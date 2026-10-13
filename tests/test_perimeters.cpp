#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "check.h"
#include "slicing/perimeters.h"

using namespace slicem;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kWidth = 0.45;

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

PerimeterSettings settings(double width_mm, int count) {
  PerimeterSettings s;
  s.extrusion_width_mm = width_mm;
  s.perimeter_count = count;
  return s;
}

// A 20 mm square with w = 0.45 and two perimeters. Each centreline is inset by half a width
// plus one width per preceding perimeter, and a square inset by d has side 20 - 2*d:
//   perimeter 0: d = 0.225            -> side 19.55, area 382.2025
//   perimeter 1: d = 0.225 + 0.45     -> side 18.65, area 347.8225
//   infill:      d = 2 * 0.45 = 0.9   -> side 18.20, area 331.2400
// Shrinking a convex corner needs no join arc - the displaced edges cross and the crossing is
// the new corner - so all three are exact squares and the areas can be pinned hard.
void test_square_two_perimeters() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0)};
  const PerimeterResult result = generate_perimeters(input, settings(kWidth, 2));

  CHECK_EQ(result.loops.size(), std::size_t{2});
  if (result.loops.size() != 2) {
    return;
  }
  CHECK_EQ(result.loops[0].size(), std::size_t{1});
  CHECK_EQ(result.loops[1].size(), std::size_t{1});
  CHECK_EQ(result.infill_region.size(), std::size_t{1});
  if (result.loops[0].size() != 1 || result.loops[1].size() != 1 ||
      result.infill_region.size() != 1) {
    return;
  }

  CHECK_EQ(result.loops[0][0].size(), std::size_t{4});
  CHECK_EQ(result.loops[1][0].size(), std::size_t{4});
  CHECK_EQ(result.infill_region[0].size(), std::size_t{4});

  CHECK_NEAR(signed_area_mm2(result.loops[0][0]), 382.2025, 1e-6);
  CHECK_NEAR(signed_area_mm2(result.loops[1][0]), 347.8225, 1e-6);
  CHECK_NEAR(signed_area_mm2(result.infill_region[0]), 331.24, 1e-6);

  // Consecutive centrelines are exactly one extrusion width apart, so their perimeters differ
  // by 8*w for a square (4 sides, each losing 2*w of length).
  CHECK_NEAR(perimeter_mm(result.loops[0][0]) - perimeter_mm(result.loops[1][0]), 8.0 * kWidth,
             1e-6);
  CHECK_NEAR(perimeter_mm(result.loops[0][0]), 4.0 * 19.55, 1e-6);
}

// One perimeter only: the infill region then starts one full width in, not half a width.
void test_single_perimeter() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0)};
  const PerimeterResult result = generate_perimeters(input, settings(kWidth, 1));

  CHECK_EQ(result.loops.size(), std::size_t{1});
  CHECK_EQ(result.infill_region.size(), std::size_t{1});
  if (result.loops.size() != 1 || result.loops[0].size() != 1 ||
      result.infill_region.size() != 1) {
    return;
  }
  // centreline side 20 - 0.45 = 19.55; infill side 20 - 2*0.45 = 19.1 -> 364.81
  CHECK_NEAR(signed_area_mm2(result.loops[0][0]), 382.2025, 1e-6);
  CHECK_NEAR(signed_area_mm2(result.infill_region[0]), 364.81, 1e-6);
}

// A square with a square hole must produce a loop around the outside and a loop around the
// hole at every perimeter index, and the hole's loop must wind against the outer one.
void test_square_with_hole() {
  // Deliberately handed in unoriented; generate_perimeters is responsible for fixing nesting.
  const std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0), rect(5.0, 5.0, 15.0, 15.0)};
  const PerimeterResult result = generate_perimeters(input, settings(kWidth, 2));

  CHECK_EQ(result.loops.size(), std::size_t{2});
  if (result.loops.size() != 2) {
    return;
  }

  for (std::size_t k = 0; k < 2; ++k) {
    CHECK_EQ(result.loops[k].size(), std::size_t{2});
    if (result.loops[k].size() != 2) {
      continue;
    }
    int positive = 0;
    int negative = 0;
    for (const Polygon& ring : result.loops[k]) {
      if (signed_area_mm2(ring) > 0.0) {
        ++positive;
      } else {
        ++negative;
      }
    }
    // Exactly one outer loop and one hole loop, wound against each other.
    CHECK_EQ(positive, 1);
    CHECK_EQ(negative, 1);
  }
  CHECK_EQ(result.infill_region.size(), std::size_t{2});

  double outer = 0.0;
  double hole = 0.0;
  for (const Polygon& ring : result.loops[0]) {
    const double area = signed_area_mm2(ring);
    if (area > 0.0) {
      outer = area;
    } else {
      hole = -area;
    }
  }
  // Outer centreline: exact 19.55 square.
  CHECK_NEAR(outer, 382.2025, 1e-6);
  // Hole centreline: the 10 mm hole grows by 0.225 to 10.45 (109.2025) but its corners are
  // convex from the hole's side, so they take round joins of radius 0.225 and the square loses
  // (4 - pi)*0.225^2 = 0.04346 there. The arc is approximated by chords sitting slightly inside
  // the true radius, which removes a further ~0.004. Expect 109.155.
  CHECK_NEAR(hole, 10.45 * 10.45 - (4.0 - kPi) * 0.225 * 0.225, 0.01);
  CHECK_NEAR(hole, 109.155, 0.01);
  CHECK(hole < 10.45 * 10.45);

  // Solid area between the two centrelines of the external perimeter.
  CHECK_NEAR(net_area(result.loops[0]), outer - hole, 1e-9);
}

// A 0.8 mm wide strip is thinner than 2*w = 0.9, so it can carry the external perimeter
// (which needs 2*0.225 = 0.45 of width) but nothing beyond it. Generation must stop there
// instead of emitting the inside-out loops a naive inset would give.
void test_thin_region_stops_early() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 10.0, 0.8)};
  const PerimeterResult result = generate_perimeters(input, settings(kWidth, 3));

  CHECK_EQ(result.loops.size(), std::size_t{3});
  if (result.loops.size() != 3) {
    return;
  }
  CHECK(!result.loops[0].empty());
  CHECK(result.loops[1].empty());
  CHECK(result.loops[2].empty());
  CHECK(result.infill_region.empty());

  // Surviving centreline: 9.55 x 0.35 = 3.3425 mm^2, and still wound counter-clockwise.
  CHECK_EQ(result.loops[0].size(), std::size_t{1});
  if (result.loops[0].size() != 1) {
    return;
  }
  CHECK_NEAR(signed_area_mm2(result.loops[0][0]), 9.55 * 0.35, 1e-6);
  CHECK(signed_area_mm2(result.loops[0][0]) > 0.0);
}

// Narrower than one extrusion width: not even the external perimeter fits.
void test_region_too_thin_for_any_perimeter() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 10.0, 0.3)};
  const PerimeterResult result = generate_perimeters(input, settings(kWidth, 2));

  CHECK_EQ(result.loops.size(), std::size_t{2});
  if (result.loops.size() != 2) {
    return;
  }
  CHECK(result.loops[0].empty());
  CHECK(result.loops[1].empty());
  CHECK(result.infill_region.empty());
}

void test_zero_perimeter_count_passes_region_through() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0), rect(5.0, 5.0, 15.0, 15.0)};
  const PerimeterResult result = generate_perimeters(input, settings(kWidth, 0));

  CHECK(result.loops.empty());
  CHECK_EQ(result.infill_region.size(), std::size_t{2});
  if (result.infill_region.size() != 2) {
    return;
  }
  // The input unchanged apart from orientation: outer counter-clockwise, hole clockwise.
  CHECK(signed_area_mm2(result.infill_region[0]) > 0.0);
  CHECK(signed_area_mm2(result.infill_region[1]) < 0.0);
  CHECK_NEAR(net_area(result.infill_region), 400.0 - 100.0, 1e-9);
}

void test_negative_perimeter_count_behaves_like_zero() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0)};
  const PerimeterResult result = generate_perimeters(input, settings(kWidth, -4));
  CHECK(result.loops.empty());
  CHECK_EQ(result.infill_region.size(), std::size_t{1});
  CHECK_NEAR(net_area(result.infill_region), 400.0, 1e-9);
}

void test_two_disjoint_islands_each_get_loops() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 10.0, 10.0), rect(20.0, 0.0, 30.0, 10.0)};
  const PerimeterResult result = generate_perimeters(input, settings(kWidth, 2));

  CHECK_EQ(result.loops.size(), std::size_t{2});
  if (result.loops.size() != 2) {
    return;
  }
  CHECK_EQ(result.loops[0].size(), std::size_t{2});
  CHECK_EQ(result.loops[1].size(), std::size_t{2});
  // Two 9.55 squares, then two 8.65 squares.
  CHECK_NEAR(net_area(result.loops[0]), 2.0 * 9.55 * 9.55, 1e-6);
  CHECK_NEAR(net_area(result.loops[1]), 2.0 * 8.65 * 8.65, 1e-6);
  CHECK_NEAR(net_area(result.infill_region), 2.0 * 8.2 * 8.2, 1e-6);
}

void test_degenerate_input() {
  const PerimeterResult empty = generate_perimeters({}, settings(kWidth, 2));
  CHECK_EQ(empty.loops.size(), std::size_t{2});
  CHECK(empty.loops[0].empty());
  CHECK(empty.infill_region.empty());

  const PerimeterResult empty_no_loops = generate_perimeters({}, settings(kWidth, 0));
  CHECK(empty_no_loops.loops.empty());
  CHECK(empty_no_loops.infill_region.empty());

  // A two-point contour bounds no area.
  const std::vector<Polygon> degenerate{Polygon{Point2{0, 0}, Point2{mm_to_units(5.0), 0}}};
  const PerimeterResult thin = generate_perimeters(degenerate, settings(kWidth, 2));
  CHECK(thin.loops[0].empty());
  CHECK(thin.infill_region.empty());

  // A non-positive extrusion width has no centreline to place.
  const std::vector<Polygon> square{rect(0.0, 0.0, 20.0, 20.0)};
  const PerimeterResult no_width = generate_perimeters(square, settings(0.0, 2));
  CHECK_EQ(no_width.loops.size(), std::size_t{2});
  CHECK(no_width.loops[0].empty());
  CHECK(no_width.loops[1].empty());
  CHECK(no_width.infill_region.empty());
}

// Coordinates far from the origin must not change the answer: the same square translated by
// 250 mm gives identical areas.
void test_offset_from_origin_is_translation_invariant() {
  const std::vector<Polygon> here{rect(0.0, 0.0, 20.0, 20.0)};
  const std::vector<Polygon> far{rect(250.0, 250.0, 270.0, 270.0)};
  const PerimeterResult a = generate_perimeters(here, settings(kWidth, 2));
  const PerimeterResult b = generate_perimeters(far, settings(kWidth, 2));

  CHECK_EQ(a.loops.size(), b.loops.size());
  CHECK_NEAR(net_area(a.loops[0]), net_area(b.loops[0]), 1e-9);
  CHECK_NEAR(net_area(a.loops[1]), net_area(b.loops[1]), 1e-9);
  CHECK_NEAR(net_area(a.infill_region), net_area(b.infill_region), 1e-9);
}

void test_perimeters_are_deterministic() {
  const std::vector<Polygon> input{rect(0.0, 0.0, 20.0, 20.0), rect(5.0, 5.0, 15.0, 15.0)};
  const PerimeterResult a = generate_perimeters(input, settings(kWidth, 3));
  const PerimeterResult b = generate_perimeters(input, settings(kWidth, 3));
  CHECK(a.loops == b.loops);
  CHECK(a.infill_region == b.infill_region);
}

}  // namespace

int main() {
  test_square_two_perimeters();
  test_single_perimeter();
  test_square_with_hole();
  test_thin_region_stops_early();
  test_region_too_thin_for_any_perimeter();
  test_zero_perimeter_count_passes_region_through();
  test_negative_perimeter_count_behaves_like_zero();
  test_two_disjoint_islands_each_get_loops();
  test_degenerate_input();
  test_offset_from_origin_is_translation_invariant();
  test_perimeters_are_deterministic();
  return slicem_test::summary("slicem_perimeter_tests");
}
