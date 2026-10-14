#include <algorithm>
#include <cmath>
#include <vector>

#include "check.h"
#include "geometry/boolean.h"
#include "geometry/offset.h"

using namespace slicem;

namespace {

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

// |area| of the rings, largest first: lets a test name "the outer ring" and "the hole" without
// depending on the order the chaining walk happens to emit them in.
std::vector<double> sorted_abs_areas(const std::vector<Polygon>& rings) {
  std::vector<double> areas;
  areas.reserve(rings.size());
  for (const Polygon& ring : rings) {
    areas.push_back(std::fabs(signed_area_mm2(ring)));
  }
  std::sort(areas.begin(), areas.end(), std::greater<double>());
  return areas;
}

int count_positive(const std::vector<Polygon>& rings) {
  int positive = 0;
  for (const Polygon& ring : rings) {
    if (signed_area_mm2(ring) > 0.0) {
      ++positive;
    }
  }
  return positive;
}

void test_disjoint_squares() {
  const std::vector<Polygon> a{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<Polygon> b{rect(20.0, 0.0, 30.0, 10.0)};

  const std::vector<Polygon> united = polygon_union(a, b);
  CHECK_EQ(united.size(), std::size_t{2});
  CHECK_NEAR(net_area(united), 200.0, 1e-9);

  CHECK(polygon_intersection(a, b).empty());

  const std::vector<Polygon> diff = polygon_difference(a, b);
  CHECK_EQ(diff.size(), std::size_t{1});
  CHECK_NEAR(net_area(diff), 100.0, 1e-9);
  CHECK(diff == a);
}

// The hardest degenerate case in a boolean: every edge of one operand lies exactly on an edge of
// the other. No crossing exists to split at, so the answer is decided purely by the shared
// boundary rule.
void test_identical_squares() {
  const std::vector<Polygon> a{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<Polygon> b{rect(0.0, 0.0, 10.0, 10.0)};

  CHECK(polygon_difference(a, b).empty());

  const std::vector<Polygon> both = polygon_intersection(a, b);
  CHECK_EQ(both.size(), std::size_t{1});
  CHECK_NEAR(net_area(both), 100.0, 1e-9);

  const std::vector<Polygon> united = polygon_union(a, b);
  CHECK_EQ(united.size(), std::size_t{1});
  CHECK_NEAR(net_area(united), 100.0, 1e-9);
}

void test_square_minus_centred_square() {
  const std::vector<Polygon> a{rect(0.0, 0.0, 20.0, 20.0)};
  const std::vector<Polygon> b{rect(5.0, 5.0, 15.0, 15.0)};

  const std::vector<Polygon> diff = polygon_difference(a, b);
  CHECK_EQ(diff.size(), std::size_t{2});
  if (diff.size() != 2) {
    return;
  }
  // 400 outside minus 100 removed: the removed ring must be wound the other way, so the signed
  // areas cancel to the real solid area rather than adding up to 500.
  CHECK_NEAR(net_area(diff), 300.0, 1e-9);
  const std::vector<double> areas = sorted_abs_areas(diff);
  CHECK_NEAR(areas[0], 400.0, 1e-9);
  CHECK_NEAR(areas[1], 100.0, 1e-9);
  CHECK_EQ(count_positive(diff), 1);
}

// The clip hangs over the subject's right edge (x from 15 to 25), so half of it removes nothing.
// The bite leaves a single ring, not a hole: 400 - 5*10.
void test_square_minus_straddling_square() {
  const std::vector<Polygon> a{rect(0.0, 0.0, 20.0, 20.0)};
  const std::vector<Polygon> b{rect(15.0, 5.0, 25.0, 15.0)};

  const std::vector<Polygon> diff = polygon_difference(a, b);
  CHECK_EQ(diff.size(), std::size_t{1});
  if (diff.size() != 1) {
    return;
  }
  CHECK_NEAR(net_area(diff), 350.0, 1e-9);
  CHECK_EQ(diff[0].size(), std::size_t{8});
}

void test_diagonal_overlap() {
  const std::vector<Polygon> a{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<Polygon> b{rect(5.0, 5.0, 15.0, 15.0)};

  const std::vector<Polygon> both = polygon_intersection(a, b);
  CHECK_EQ(both.size(), std::size_t{1});
  CHECK_NEAR(net_area(both), 25.0, 1e-9);
  if (both.size() == 1) {
    CHECK_EQ(both[0].size(), std::size_t{4});
  }

  const std::vector<Polygon> united = polygon_union(a, b);
  CHECK_EQ(united.size(), std::size_t{1});
  CHECK_NEAR(net_area(united), 175.0, 1e-9);
  if (united.size() == 1) {
    CHECK_EQ(united[0].size(), std::size_t{8});
  }

  const std::vector<Polygon> diff = polygon_difference(a, b);
  CHECK_EQ(diff.size(), std::size_t{1});
  CHECK_NEAR(net_area(diff), 75.0, 1e-9);
  if (diff.size() == 1) {
    CHECK_EQ(diff[0].size(), std::size_t{6});
  }
}

void test_clip_covers_subject() {
  const std::vector<Polygon> a{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<Polygon> b{rect(-5.0, -5.0, 15.0, 15.0)};

  CHECK(polygon_difference(a, b).empty());
  CHECK_NEAR(net_area(polygon_intersection(a, b)), 100.0, 1e-9);
  CHECK_NEAR(net_area(polygon_union(a, b)), 400.0, 1e-9);
}

// Even-odd membership is what makes this work: a point in the hole is contained by two of the
// subject's rings, so it counts as outside the subject and the clip removes nothing.
void test_clip_inside_hole_changes_nothing() {
  std::vector<Polygon> a{rect(0.0, 0.0, 20.0, 20.0), rect(5.0, 5.0, 15.0, 15.0)};
  orient_rings(a);
  const std::vector<Polygon> b{rect(7.0, 7.0, 13.0, 13.0)};

  const std::vector<Polygon> diff = polygon_difference(a, b);
  CHECK_EQ(diff.size(), std::size_t{2});
  CHECK_NEAR(net_area(diff), 300.0, 1e-9);
  CHECK_EQ(count_positive(diff), 1);

  // Nothing of the subject lies inside that clip either.
  CHECK(polygon_intersection(a, b).empty());
}

// Partially shared boundary: the clip covers the middle 10 mm of the subject's 20 mm bottom edge
// and no crossing exists anywhere, because collinear segments have no determinant. The notch is
// a single ring of 400 - 100, and the subject's bottom edge must survive on both sides of it.
void test_notch_sharing_part_of_an_edge() {
  const std::vector<Polygon> a{rect(0.0, 0.0, 20.0, 20.0)};
  const std::vector<Polygon> b{rect(5.0, 0.0, 15.0, 10.0)};

  const std::vector<Polygon> diff = polygon_difference(a, b);
  CHECK_EQ(diff.size(), std::size_t{1});
  if (diff.size() != 1) {
    return;
  }
  CHECK_NEAR(net_area(diff), 300.0, 1e-9);
  CHECK_EQ(diff[0].size(), std::size_t{8});
}

// The shape a one-sided step produces between two layers: three of the clip's four edges lie on
// the subject's boundary, two of them only partly. The answer is the uncovered half, 200.
void test_one_sided_step() {
  const std::vector<Polygon> a{rect(0.0, 0.0, 20.0, 20.0)};
  const std::vector<Polygon> b{rect(0.0, 0.0, 10.0, 20.0)};

  const std::vector<Polygon> diff = polygon_difference(a, b);
  CHECK_EQ(diff.size(), std::size_t{1});
  if (diff.size() != 1) {
    return;
  }
  CHECK_NEAR(net_area(diff), 200.0, 1e-9);
  CHECK_EQ(diff[0].size(), std::size_t{4});

  CHECK_NEAR(net_area(polygon_intersection(a, b)), 200.0, 1e-9);
  CHECK_NEAR(net_area(polygon_union(a, b)), 400.0, 1e-9);
}

// Two squares meeting along a whole edge. There the two operands' edges run opposite ways -
// their interiors are on opposite sides - so the shared edge is interior to the union and must
// disappear, leaving one 6-vertex ring rather than two squares or a seam.
void test_abutting_squares_merge() {
  const std::vector<Polygon> a{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<Polygon> b{rect(10.0, 0.0, 20.0, 10.0)};

  const std::vector<Polygon> united = polygon_union(a, b);
  CHECK_EQ(united.size(), std::size_t{1});
  CHECK_NEAR(net_area(united), 200.0, 1e-9);
  if (united.size() == 1) {
    CHECK_EQ(united[0].size(), std::size_t{6});
  }

  // They share a boundary but no area.
  CHECK(polygon_intersection(a, b).empty());
  // And removing a neighbour that only touches removes nothing.
  CHECK_NEAR(net_area(polygon_difference(a, b)), 100.0, 1e-9);
}

void test_degenerate_input() {
  const std::vector<Polygon> square{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<Polygon> nothing;

  CHECK(boolean_op(nothing, nothing, BoolOp::kUnion).empty());
  CHECK(boolean_op(nothing, nothing, BoolOp::kDifference).empty());
  CHECK(boolean_op(nothing, nothing, BoolOp::kIntersection).empty());

  CHECK(polygon_difference(nothing, square).empty());
  CHECK(polygon_intersection(nothing, square).empty());
  CHECK_NEAR(net_area(polygon_union(nothing, square)), 100.0, 1e-9);

  CHECK_NEAR(net_area(polygon_difference(square, nothing)), 100.0, 1e-9);
  CHECK(polygon_intersection(square, nothing).empty());
  CHECK_NEAR(net_area(polygon_union(square, nothing)), 100.0, 1e-9);

  // A two-point "ring" bounds no area, so it must behave exactly like an absent operand.
  const std::vector<Polygon> two_points{Polygon{Point2{0, 0}, Point2{mm_to_units(5.0), 0}}};
  CHECK(polygon_difference(two_points, square).empty());
  CHECK(polygon_intersection(two_points, square).empty());
  CHECK_NEAR(net_area(polygon_union(two_points, square)), 100.0, 1e-9);
  CHECK_NEAR(net_area(polygon_difference(square, two_points)), 100.0, 1e-9);

  // A ring with a repeated vertex carries one zero-length edge, which must not survive into the
  // result as a stub that breaks the chaining.
  const std::vector<Polygon> duplicated{Polygon{Point2{0, 0}, Point2{0, 0},
                                                Point2{mm_to_units(10.0), 0},
                                                Point2{mm_to_units(10.0), mm_to_units(10.0)},
                                                Point2{0, mm_to_units(10.0)}}};
  const std::vector<Polygon> cleaned = polygon_difference(duplicated, nothing);
  CHECK_EQ(cleaned.size(), std::size_t{1});
  CHECK_NEAR(net_area(cleaned), 100.0, 1e-9);
}

void test_far_from_origin() {
  // 500 mm out in both axes: 5e8 grid units, where a dropped int64 would show up immediately.
  const std::vector<Polygon> a{rect(500.0, 500.0, 510.0, 510.0)};
  const std::vector<Polygon> b{rect(505.0, 505.0, 515.0, 515.0)};
  CHECK_NEAR(net_area(polygon_intersection(a, b)), 25.0, 1e-9);
  CHECK_NEAR(net_area(polygon_difference(a, b)), 75.0, 1e-9);
  CHECK_NEAR(net_area(polygon_union(a, b)), 175.0, 1e-9);
}

void test_boolean_is_deterministic() {
  std::vector<Polygon> a{rect(0.0, 0.0, 20.0, 20.0), rect(5.0, 5.0, 15.0, 15.0)};
  orient_rings(a);
  const std::vector<Polygon> b{rect(10.0, 10.0, 30.0, 30.0)};

  CHECK(polygon_difference(a, b) == polygon_difference(a, b));
  CHECK(polygon_union(a, b) == polygon_union(a, b));
  CHECK(polygon_intersection(a, b) == polygon_intersection(a, b));
}

}  // namespace

int main() {
  test_disjoint_squares();
  test_identical_squares();
  test_square_minus_centred_square();
  test_square_minus_straddling_square();
  test_diagonal_overlap();
  test_clip_covers_subject();
  test_clip_inside_hole_changes_nothing();
  test_notch_sharing_part_of_an_edge();
  test_one_sided_step();
  test_abutting_squares_merge();
  test_degenerate_input();
  test_far_from_origin();
  test_boolean_is_deterministic();
  return slicem_test::summary("slicem_boolean_tests");
}
