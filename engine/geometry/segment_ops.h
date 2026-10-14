#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "geometry/polygon.h"

namespace slicem {
// Internal to engine/geometry: the directed-segment machinery shared by the offsetter and the
// polygon boolean operations. Both algorithms have the same shape - emit directed segments,
// split them at their mutual crossings, discard the pieces that fall on the wrong side, re-chain
// what is left - and they must agree exactly on what "the same point" and "the same chain" mean.
// Keeping one copy of these primitives is what guarantees that.
namespace segment_ops {

struct Segment {
  Point2 a;
  Point2 b;
};

long double to_ld(int64_t v);

// Interior crossings only. Endpoint contacts are deliberately ignored: segments that merely meet
// at a shared join point are already correctly chained, and treating that as an intersection
// would split them into zero-length stubs.
//
// Returns the crossing rounded onto the integer grid, plus the parameter along each input
// segment. Collinear and parallel pairs report no intersection (zero denominator); callers that
// care about overlapping collinear edges must handle that case themselves.
bool interior_intersection(const Segment& s, const Segment& t, Point2& out, long double& ts,
                           long double& tt);

// Splits every segment at every interior crossing with every other segment. The same rounded
// crossing point is handed to both sides, so the two halves still share an endpoint bit-exactly
// and the pieces can be re-chained without any tolerance.
//
// When source_index is non-null it is resized to the number of returned pieces and filled with
// the index of the input segment each piece came from, which is how a caller tells the pieces of
// one operand from the pieces of another after a joint split.
std::vector<Segment> split_at_intersections(const std::vector<Segment>& segments,
                                            std::vector<std::size_t>* source_index = nullptr);

// Walks the segments into closed rings: from each unused segment, repeatedly follow an unused
// segment starting exactly where the previous one ended, until the start point is reached again.
// Chains that cannot be closed are discarded - an open chain means some upstream stage produced
// geometry that does not bound a region, and emitting it would corrupt everything downstream.
//
// Deterministic for a given input order: the outer walk visits segments by index and the
// candidate lists are kept in insertion order, so no hash iteration order can reach the result.
std::vector<Polygon> chain_segments(const std::vector<Segment>& segments);

}  // namespace segment_ops
}  // namespace slicem
