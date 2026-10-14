#pragma once

#include <vector>

#include "geometry/polygon.h"

namespace slicem {

enum class BoolOp { kUnion, kIntersection, kDifference };

// Boolean operations on sets of rings, where each operand is a whole region: its outer contours
// and the holes inside them, interpreted even-odd (a point is in the region when an odd number
// of the operand's rings contain it).
//
// Both operands are re-oriented internally - outer rings counter-clockwise, holes clockwise, by
// nesting depth - so the caller may pass rings in any winding. The result comes back in that
// same convention, which is what every later stage (offset, infill) assumes.
//
// Method: emit every ring edge of both operands as a directed segment, split all segments at
// their mutual crossings, keep or drop each resulting piece by testing its midpoint against the
// other operand, then walk the kept pieces back into closed rings. This is the same pipeline the
// offsetter uses and it shares its primitives, so "the same point" means the same thing in both.
// The classification rules follow the standard boundary-classification formulation of polygon
// clipping (Martinez, Rueda and Feito, "A new algorithm for computing Boolean operations on
// polygons", Computers & Geosciences 35, 2009); the sweep line is deliberately not used here -
// an O(n^2) split is enough for one slice layer's ring count and keeps the code auditable.
//
// Rings whose area falls below the degeneracy floor are dropped, so an empty result is returned
// as an empty vector rather than as a zero-area sliver.
std::vector<Polygon> boolean_op(const std::vector<Polygon>& subject,
                                const std::vector<Polygon>& clip, BoolOp op);

inline std::vector<Polygon> polygon_difference(const std::vector<Polygon>& a,
                                               const std::vector<Polygon>& b) {
  return boolean_op(a, b, BoolOp::kDifference);
}

inline std::vector<Polygon> polygon_union(const std::vector<Polygon>& a,
                                          const std::vector<Polygon>& b) {
  return boolean_op(a, b, BoolOp::kUnion);
}

inline std::vector<Polygon> polygon_intersection(const std::vector<Polygon>& a,
                                                 const std::vector<Polygon>& b) {
  return boolean_op(a, b, BoolOp::kIntersection);
}

}  // namespace slicem
