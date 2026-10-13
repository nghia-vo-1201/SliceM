#pragma once

#include <vector>

#include "geometry/types.h"

namespace slicem {

using Polygon = std::vector<Point2>;

// Shoelace area in mm^2. Positive when the ring winds counter-clockwise, so the sign also
// serves to tell an outer contour from a hole once orientation is established.
double signed_area_mm2(const Polygon& poly);

double perimeter_mm(const Polygon& poly);

}  // namespace slicem
