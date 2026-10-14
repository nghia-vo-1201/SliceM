#include <algorithm>
#include <cmath>
#include <vector>

#include "check.h"
#include "geometry/offset.h"

using namespace slicem;

namespace {

constexpr double kPi = 3.14159265358979323846;

Polygon rect(double x0, double y0, double x1, double y1) {
  return Polygon{Point2{mm_to_units(x0), mm_to_units(y0)}, Point2{mm_to_units(x1), mm_to_units(y0)},
                 Point2{mm_to_units(x1), mm_to_units(y1)}, Point2{mm_to_units(x0), mm_to_units(y1)}};
}

double net_area(const std::vector<Polygon>& rings) {
  double total = 0.0;
  for (const Polygon& ring : rings) {
    total += signed_area_mm2(ring);
  }
  return total;
}

// Shrinking a convex corner needs no join: the two displaced edges overrun each other and the
// crossing itself is the new corner, so the result must be an exact 18 mm square.
void test_square_shrinks_to_exact_square() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0)};
  const std::vector<Polygon> out = offset_rings(input, -1.0);

  CHECK_EQ(out.size(), std::size_t{1});
  if (out.size() != 1) {
    return;
  }
  CHECK_EQ(out[0].size(), std::size_t{4});
  CHECK_NEAR(signed_area_mm2(out[0]), 324.0, 1e-6);
  CHECK(signed_area_mm2(out[0]) > 0.0);
}

// Growing the same square rounds every corner, so the area is the square plus a rib along each
// edge plus exactly one full circle's worth of corner: 400 + 80 + pi.
void test_square_grows_with_round_joins() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0)};
  const std::vector<Polygon> out = offset_rings(input, 1.0);

  CHECK_EQ(out.size(), std::size_t{1});
  if (out.size() != 1) {
    return;
  }
  CHECK_NEAR(signed_area_mm2(out[0]), 400.0 + 80.0 + kPi, 0.05);
}

void test_hole_grows_as_solid_shrinks() {
  std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0), rect(5.0, 5.0, 15.0, 15.0)};
  orient_rings(input);

  CHECK(signed_area_mm2(input[0]) > 0.0);
  CHECK(signed_area_mm2(input[1]) < 0.0);

  const std::vector<Polygon> out = offset_rings(input, -1.0);
  CHECK_EQ(out.size(), std::size_t{2});

  // Outer becomes a sharp 18 mm square (324); the hole opens to 12 mm with corners rounded at
  // radius 1, giving 140 + pi. The solid is what remains between them.
  CHECK_NEAR(net_area(out), 324.0 - (140.0 + kPi), 0.05);
}

// The property that matters most: a feature thinner than twice the offset must disappear, not
// re-emerge inside out. This is the case naive per-vertex offsetting gets wrong.
void test_thin_feature_collapses() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 10.0, 1.0)};
  const std::vector<Polygon> out = offset_rings(input, -0.6);
  CHECK(out.empty());
}

void test_partial_collapse_keeps_thick_part() {
  // An L of two arms: one 4 mm wide, one 0.8 mm wide. Shrinking by 0.5 must keep the thick arm
  // and drop the thin one.
  std::vector<Polygon> input{Polygon{
      Point2{mm_to_units(0.0), mm_to_units(0.0)}, Point2{mm_to_units(4.0), mm_to_units(0.0)},
      Point2{mm_to_units(4.0), mm_to_units(10.0)}, Point2{mm_to_units(4.8), mm_to_units(10.0)},
      Point2{mm_to_units(4.8), mm_to_units(10.8)}, Point2{mm_to_units(0.0), mm_to_units(10.8)}}};
  orient_rings(input);

  const std::vector<Polygon> out = offset_rings(input, -0.5);
  CHECK(!out.empty());
  const double area = net_area(out);
  CHECK(area > 0.0);
  // The 4 mm arm shrinks to 3 mm wide; the 0.8 mm stub cannot survive a 0.5 mm inset.
  CHECK(area < 4.0 * 10.8);
}

void test_disjoint_islands_stay_separate() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 10.0, 10.0), rect(20.0, 0.0, 30.0, 10.0)};
  const std::vector<Polygon> out = offset_rings(input, -1.0);

  CHECK_EQ(out.size(), std::size_t{2});
  CHECK_NEAR(net_area(out), 2.0 * 64.0, 1e-6);
}

void test_zero_offset_is_identity() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0)};
  const std::vector<Polygon> out = offset_rings(input, 0.0);
  CHECK_EQ(out.size(), std::size_t{1});
  CHECK(out == input);
}

void test_orientation_normalises_nesting() {
  // Deliberately wrong winding on both: outer clockwise, hole counter-clockwise.
  Polygon outer = rect(0.0, 0.0, 20.0, 20.0);
  Polygon hole = rect(5.0, 5.0, 15.0, 15.0);
  std::reverse(outer.begin(), outer.end());
  std::vector<Polygon> rings{outer, hole};

  orient_rings(rings);
  CHECK(signed_area_mm2(rings[0]) > 0.0);
  CHECK(signed_area_mm2(rings[1]) < 0.0);
}

void test_degenerate_input() {
  CHECK(offset_rings({}, -1.0).empty());

  const std::vector<Polygon> two_points{Polygon{Point2{0, 0}, Point2{mm_to_units(5.0), 0}}};
  CHECK(offset_rings(two_points, -1.0).empty());

  // Shrinking by more than the half-width of anything present leaves nothing at all.
  const std::vector<Polygon> small{rect(0.0, 0.0, 2.0, 2.0)};
  CHECK(offset_rings(small, -5.0).empty());
}

// A square carrying extra vertices along its edges must offset to exactly the same thing as the
// bare square, at every distance. The failure this pins is narrow and was shipped once: at a
// distance equal to the collinear vertex's distance from the corner, two displaced edges meet at
// the endpoint of a third, which is a T-junction rather than a crossing. The splitting pass used
// to ignore those, the chaining walk turned into the leftover spur, and the ring was dropped
// whole - a 20 mm cube lost all infill on the two layers where its facet diagonals cross.
void test_collinear_vertices_do_not_change_the_offset() {
  const std::vector<Polygon> plain{rect(0.0, 0.0, 20.0, 20.0)};
  const std::vector<Polygon> with_collinear{Polygon{
      Point2{mm_to_units(0.9), mm_to_units(0.0)}, Point2{mm_to_units(20.0), mm_to_units(0.0)},
      Point2{mm_to_units(20.0), mm_to_units(0.9)}, Point2{mm_to_units(20.0), mm_to_units(20.0)},
      Point2{mm_to_units(19.1), mm_to_units(20.0)}, Point2{mm_to_units(0.0), mm_to_units(20.0)},
      Point2{mm_to_units(0.0), mm_to_units(19.1)}, Point2{mm_to_units(0.0), mm_to_units(0.0)}}};

  CHECK_NEAR(net_area(plain), 400.0, 1e-9);
  CHECK_NEAR(net_area(with_collinear), 400.0, 1e-9);

  // -0.9 is the one that used to fail: it is exactly where the extra vertices sit.
  for (const double delta : {-0.225, -0.45, -0.675, -0.9, -1.0, -2.0, 1.0}) {
    const std::vector<Polygon> expected = offset_rings(plain, delta);
    const std::vector<Polygon> actual = offset_rings(with_collinear, delta);
    CHECK_EQ(actual.size(), expected.size());
    CHECK_NEAR(net_area(actual), net_area(expected), 1e-6);
  }

  CHECK_NEAR(net_area(offset_rings(with_collinear, -0.9)), 331.24, 1e-6);
}

// The same hazard from the other side: a T-junction between two rings of one operand. The spur
// is a separate ring touching the square's edge at a single point, which the splitting pass has
// to notice even though nothing crosses anything.
void test_touching_rings_still_offset() {
  std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0), rect(20.0, 8.0, 26.0, 12.0)};
  orient_rings(input);
  const std::vector<Polygon> out = offset_rings(input, -1.0);
  CHECK(!out.empty());
  CHECK(net_area(out) > 0.0);
}

void test_offset_is_deterministic() {
  std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0), rect(5.0, 5.0, 15.0, 15.0)};
  orient_rings(input);
  CHECK(offset_rings(input, -1.0) == offset_rings(input, -1.0));
}

}  // namespace

int main() {
  test_square_shrinks_to_exact_square();
  test_square_grows_with_round_joins();
  test_hole_grows_as_solid_shrinks();
  test_thin_feature_collapses();
  test_partial_collapse_keeps_thick_part();
  test_disjoint_islands_stay_separate();
  test_zero_offset_is_identity();
  test_orientation_normalises_nesting();
  test_degenerate_input();
  test_collinear_vertices_do_not_change_the_offset();
  test_touching_rings_still_offset();
  test_offset_is_deterministic();
  return slicem_test::summary("slicem_offset_tests");
}
