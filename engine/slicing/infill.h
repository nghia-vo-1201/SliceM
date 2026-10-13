#pragma once

#include <vector>

#include "geometry/polygon.h"
#include "geometry/types.h"

namespace slicem {

enum class InfillPattern { kRectilinear };

struct InfillSettings {
  InfillPattern pattern = InfillPattern::kRectilinear;
  double extrusion_width_mm = 0.45;
  double density = 0.20;  // 0..1; 1.0 means solid
  double angle_degrees = 45.0;
};

struct InfillLine {
  Point2 a;
  Point2 b;
};

inline bool operator==(const InfillLine& lhs, const InfillLine& rhs) {
  return lhs.a == rhs.a && lhs.b == rhs.b;
}

inline bool operator!=(const InfillLine& lhs, const InfillLine& rhs) {
  return !(lhs == rhs);
}

// Fills a region with extrusion centrelines.
//
// The region is expected to come from orient_rings/offset_rings, so its rings are properly
// nested with holes wound against their enclosing contour. Spans inside the solid are found by
// even-odd parity along each scan line, which needs correct nesting but not any particular
// winding direction.
//
// Lines are returned in traversal order: consecutive scan lines run in opposite directions
// (serpentine), so the sequence can be printed without a travel move between every line.
std::vector<InfillLine> generate_infill(const std::vector<Polygon>& region,
                                        const InfillSettings& settings);

}  // namespace slicem
