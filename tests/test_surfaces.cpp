#include <cmath>
#include <vector>

#include "check.h"
#include "geometry/polygon.h"
#include "slicing/surfaces.h"

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

int count_positive(const std::vector<Polygon>& rings) {
  int positive = 0;
  for (const Polygon& ring : rings) {
    if (signed_area_mm2(ring) > 0.0) {
      ++positive;
    }
  }
  return positive;
}

// A prism has no overhang and no ceiling except at its two ends, so only the first and last
// layers may need solid infill.
void test_straight_prism() {
  const std::vector<Polygon> region{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<std::vector<Polygon>> layers(5, region);

  const std::vector<LayerSurfaces> out = detect_surfaces(layers);
  CHECK_EQ(out.size(), std::size_t{5});
  if (out.size() != 5) {
    return;
  }

  CHECK(out[0].top.empty());
  CHECK_NEAR(net_area(out[0].bottom), 100.0, 1e-9);
  CHECK_EQ(out[0].bottom.size(), std::size_t{1});
  CHECK(out[0].sparse.empty());

  for (std::size_t i = 1; i + 1 < out.size(); ++i) {
    CHECK(out[i].top.empty());
    CHECK(out[i].bottom.empty());
    CHECK_EQ(out[i].sparse.size(), std::size_t{1});
    CHECK_NEAR(net_area(out[i].sparse), 100.0, 1e-9);
  }

  CHECK_NEAR(net_area(out[4].top), 100.0, 1e-9);
  CHECK_EQ(out[4].top.size(), std::size_t{1});
  CHECK(out[4].bottom.empty());
  CHECK(out[4].sparse.empty());
}

// A 20 mm column carrying a concentric 10 mm column. The last wide layer is roofed everywhere
// except the 5 mm ring around the narrow column, and that ring is exactly its top surface.
void test_step_down_makes_top_surface() {
  const Polygon wide = rect(0.0, 0.0, 20.0, 20.0);
  const Polygon narrow = rect(5.0, 5.0, 15.0, 15.0);
  const std::vector<std::vector<Polygon>> layers{
      std::vector<Polygon>{wide}, std::vector<Polygon>{wide}, std::vector<Polygon>{narrow},
      std::vector<Polygon>{narrow}};

  const std::vector<LayerSurfaces> out = detect_surfaces(layers);
  CHECK_EQ(out.size(), std::size_t{4});
  if (out.size() != 4) {
    return;
  }

  // 400 - 100: an outer ring with the narrow column's footprint as a hole, so two rings whose
  // signed areas cancel to the ring's real area.
  CHECK_EQ(out[1].top.size(), std::size_t{2});
  CHECK_NEAR(net_area(out[1].top), 300.0, 1e-9);
  CHECK_EQ(count_positive(out[1].top), 1);
  // What the column does cover stays sparse.
  CHECK_EQ(out[1].sparse.size(), std::size_t{1});
  CHECK_NEAR(net_area(out[1].sparse), 100.0, 1e-9);
  CHECK(out[1].bottom.empty());

  // No other middle layer is exposed upwards.
  CHECK(out[0].top.empty());
  CHECK(out[2].top.empty());
  // Nor downwards: the narrow column sits entirely on the wide one.
  CHECK(out[2].bottom.empty());
  CHECK_NEAR(net_area(out[2].sparse), 100.0, 1e-9);

  // First and last layer still face air.
  CHECK_NEAR(net_area(out[0].bottom), 400.0, 1e-9);
  CHECK_NEAR(net_area(out[3].top), 100.0, 1e-9);
}

// The same shape upside down: the overhanging ring has nothing below it, so it is a bottom
// surface and no top surface appears anywhere in the middle.
void test_step_up_makes_bottom_surface() {
  const Polygon wide = rect(0.0, 0.0, 20.0, 20.0);
  const Polygon narrow = rect(5.0, 5.0, 15.0, 15.0);
  const std::vector<std::vector<Polygon>> layers{
      std::vector<Polygon>{narrow}, std::vector<Polygon>{narrow}, std::vector<Polygon>{wide},
      std::vector<Polygon>{wide}};

  const std::vector<LayerSurfaces> out = detect_surfaces(layers);
  CHECK_EQ(out.size(), std::size_t{4});
  if (out.size() != 4) {
    return;
  }

  CHECK_EQ(out[2].bottom.size(), std::size_t{2});
  CHECK_NEAR(net_area(out[2].bottom), 300.0, 1e-9);
  CHECK_EQ(count_positive(out[2].bottom), 1);
  CHECK_EQ(out[2].sparse.size(), std::size_t{1});
  CHECK_NEAR(net_area(out[2].sparse), 100.0, 1e-9);
  CHECK(out[2].top.empty());

  CHECK(out[1].top.empty());
  CHECK(out[1].bottom.empty());
  CHECK_NEAR(net_area(out[1].sparse), 100.0, 1e-9);
}

// A step on one side only, which is what an ordinary staircase looks like: the upper region
// shares three stretches of the lower region's boundary. The top surface is the uncovered half.
void test_one_sided_step() {
  const std::vector<std::vector<Polygon>> layers{std::vector<Polygon>{rect(0.0, 0.0, 20.0, 20.0)},
                                                 std::vector<Polygon>{rect(0.0, 0.0, 20.0, 20.0)},
                                                 std::vector<Polygon>{rect(0.0, 0.0, 10.0, 20.0)},
                                                 std::vector<Polygon>{rect(0.0, 0.0, 10.0, 20.0)}};

  const std::vector<LayerSurfaces> out = detect_surfaces(layers);
  CHECK_EQ(out.size(), std::size_t{4});
  if (out.size() != 4) {
    return;
  }

  CHECK_EQ(out[1].top.size(), std::size_t{1});
  CHECK_NEAR(net_area(out[1].top), 200.0, 1e-9);
  CHECK_EQ(out[1].sparse.size(), std::size_t{1});
  CHECK_NEAR(net_area(out[1].sparse), 200.0, 1e-9);
  CHECK(out[1].bottom.empty());

  CHECK(out[2].top.empty());
  CHECK(out[2].bottom.empty());
  CHECK_NEAR(net_area(out[2].sparse), 200.0, 1e-9);
}

void test_single_layer_is_top_and_bottom() {
  const std::vector<std::vector<Polygon>> layers{std::vector<Polygon>{rect(0.0, 0.0, 10.0, 10.0)}};

  const std::vector<LayerSurfaces> out = detect_surfaces(layers);
  CHECK_EQ(out.size(), std::size_t{1});
  if (out.size() != 1) {
    return;
  }
  CHECK_NEAR(net_area(out[0].top), 100.0, 1e-9);
  CHECK_NEAR(net_area(out[0].bottom), 100.0, 1e-9);
  CHECK(out[0].sparse.empty());
}

void test_degenerate_input() {
  CHECK(detect_surfaces({}).empty());

  // A layer whose perimeters consumed everything leaves no infill region, so it has no surfaces
  // of any kind - and must not make its neighbours' classification throw or wander.
  const std::vector<std::vector<Polygon>> layers{std::vector<Polygon>{rect(0.0, 0.0, 10.0, 10.0)},
                                                 std::vector<Polygon>{},
                                                 std::vector<Polygon>{rect(0.0, 0.0, 10.0, 10.0)}};
  const std::vector<LayerSurfaces> out = detect_surfaces(layers);
  CHECK_EQ(out.size(), std::size_t{3});
  if (out.size() != 3) {
    return;
  }
  CHECK(out[1].top.empty());
  CHECK(out[1].bottom.empty());
  CHECK(out[1].sparse.empty());
  // Layer 0 is roofed by nothing, so all of it is top as well as bottom.
  CHECK_NEAR(net_area(out[0].top), 100.0, 1e-9);
  CHECK_NEAR(net_area(out[0].bottom), 100.0, 1e-9);
  CHECK(out[0].sparse.empty());
  // Layer 2 stands on nothing.
  CHECK_NEAR(net_area(out[2].bottom), 100.0, 1e-9);
  CHECK_NEAR(net_area(out[2].top), 100.0, 1e-9);
}

void test_surfaces_are_deterministic() {
  const std::vector<std::vector<Polygon>> layers{std::vector<Polygon>{rect(0.0, 0.0, 20.0, 20.0)},
                                                 std::vector<Polygon>{rect(5.0, 5.0, 15.0, 15.0)}};
  const std::vector<LayerSurfaces> first = detect_surfaces(layers);
  const std::vector<LayerSurfaces> second = detect_surfaces(layers);
  CHECK_EQ(first.size(), second.size());
  for (std::size_t i = 0; i < first.size() && i < second.size(); ++i) {
    CHECK(first[i].top == second[i].top);
    CHECK(first[i].bottom == second[i].bottom);
    CHECK(first[i].sparse == second[i].sparse);
  }
}

}  // namespace

int main() {
  test_straight_prism();
  test_step_down_makes_top_surface();
  test_step_up_makes_bottom_surface();
  test_one_sided_step();
  test_single_layer_is_top_and_bottom();
  test_degenerate_input();
  test_surfaces_are_deterministic();
  return slicem_test::summary("slicem_surface_tests");
}
