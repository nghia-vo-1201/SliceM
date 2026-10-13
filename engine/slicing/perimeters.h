#pragma once

#include <vector>

#include "geometry/offset.h"
#include "geometry/polygon.h"

namespace slicem {

struct PerimeterSettings {
  double extrusion_width_mm = 0.45;
  int perimeter_count = 2;
};

struct PerimeterResult {
  // loops[0] is the external perimeter, loops[1] the next one inward, and so on.
  // Each entry holds the closed centreline rings for that perimeter index.
  std::vector<std::vector<Polygon>> loops;
  // Region left for infill after all perimeters are accounted for.
  std::vector<Polygon> infill_region;
};

// Places perimeter centrelines inside a layer's contours.
//
// The centreline is what is computed, not the extrusion edge: material is deposited along it
// with width w, so perimeter k sits at -(w/2 + k*w) from the input and the external
// perimeter's outer edge lands exactly on the model boundary. The infill region starts a full
// perimeter_count * w inside, which is where the innermost extrusion's inner edge ends.
//
// A perimeter that offsets away entirely means the solid was too thin to hold it. Generation
// stops at that index - the remaining entries stay empty and the infill region is empty too -
// rather than emitting the inverted loops a naive offset would produce there.
PerimeterResult generate_perimeters(const std::vector<Polygon>& layer_contours,
                                    const PerimeterSettings& settings);

}  // namespace slicem
