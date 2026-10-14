#include "geometry/boolean.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "geometry/offset.h"
#include "geometry/segment_ops.h"

namespace slicem {
namespace {

using segment_ops::chain_segments;
using segment_ops::Segment;
using segment_ops::split_at_intersections;

// A ring this small cannot be real geometry: the coordinate grid is 1 nm, so the smallest
// triangle expressible on it already has an area around 5e-13 mm^2, and anything up to 1e-9 mm^2
// (a square of side 32 nm) is a rounding artefact of the split points rather than a region. The
// offsetter drops rings with the same threshold; both must agree or a sliver surviving one stage
// would be deleted by the next and the result would depend on the call order.
constexpr double kMinRingAreaMm2 = 1e-9;

// What the midpoint test is allowed to decide for a piece. Pieces lying on a stretch of boundary
// shared by both operands are settled before the test, because there the midpoint is exactly on
// the other operand's boundary and "inside" is then an artefact of the ray-casting rule rather
// than a fact about the geometry.
enum class Verdict { kClassify, kKeep, kDrop };

void emit_ring_edges(const std::vector<Polygon>& rings, std::vector<Segment>& out) {
  for (const Polygon& ring : rings) {
    // Fewer than three vertices bounds no area at all, so such a "ring" contributes nothing to
    // either operand. Dropping it here is what keeps a 2-point input from producing a stray
    // there-and-back chain later.
    if (ring.size() < 3) {
      continue;
    }
    for (std::size_t i = 0; i < ring.size(); ++i) {
      const Point2& a = ring[i];
      const Point2& b = ring[(i + 1) % ring.size()];
      if (a != b) {  // duplicate consecutive vertices carry no edge
        out.push_back(Segment{a, b});
      }
    }
  }
}

// Even-odd membership in a whole operand: inside when an odd number of its rings contain the
// point. This is the rule that makes a hole a hole without needing to know which outer ring owns
// it, and it is why a clip sitting inside a hole of the subject counts as outside the subject.
bool inside_region(const std::vector<Polygon>& rings, const Point2& point) {
  int containing = 0;
  for (const Polygon& ring : rings) {
    if (ring.size() < 3) {
      continue;
    }
    if (point_in_ring(ring, point)) {
      ++containing;
    }
  }
  return (containing % 2) == 1;
}

// Midpoints are what get classified rather than endpoints: an endpoint is shared with the
// neighbouring piece and frequently lies exactly on the other operand's boundary, where no
// inside/outside answer is meaningful. floor_div keeps the rounding direction uniform for
// negative coordinates, so a piece far from the origin behaves like one near it.
Point2 piece_midpoint(const Segment& s) {
  return Point2{floor_div(s.a.x + s.b.x, 2), floor_div(s.a.y + s.b.y, 2)};
}

bool point_less(const Point2& a, const Point2& b) {
  return a.x != b.x ? a.x < b.x : a.y < b.y;
}

// (b - a) x (p - a), in grid units squared.
//
// Exact in int64 for coordinate differences up to 3e9 units, i.e. 3 m - an order of magnitude
// past any build volume, and the same bound that keeps the rest of the integer pipeline safe.
// Being exact is the point: this decides collinearity, and a tolerance here would make the
// answer depend on how far from the origin the model sits.
int64_t cross(const Point2& a, const Point2& b, const Point2& p) {
  return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

bool collinear_segments(const Segment& s, const Segment& t) {
  return cross(s.a, s.b, t.a) == 0 && cross(s.a, s.b, t.b) == 0;
}

// For a point already known to be collinear with a-b: does it lie strictly inside the segment?
// Compared along whichever axis the segment actually spans, so the test stays exact integer
// arithmetic with no products at all.
bool strictly_inside_segment(const Point2& a, const Point2& b, const Point2& p) {
  if (a.x != b.x) {
    return p.x > std::min(a.x, b.x) && p.x < std::max(a.x, b.x);
  }
  if (a.y != b.y) {
    return p.y > std::min(a.y, b.y) && p.y < std::max(a.y, b.y);
  }
  return false;
}

// Splits every segment at the endpoints of the collinear segments that overlap it.
//
// This is what split_at_intersections cannot do: two collinear segments have a zero determinant,
// so they report no crossing, and a stretch of boundary shared by both operands survives as two
// pieces with different endpoints. The shared-boundary rule below can only recognise a shared
// stretch when the two operands' pieces are endpoint-identical, so the overlap has to be cut out
// first. Without this pass a subject edge partially covered by a clip edge - a one-sided step
// between two layers, the most ordinary shape there is - loses its boundary and chains open.
//
// Only collinear overlaps are cut. A vertex that merely touches the middle of a non-parallel
// edge is left alone: the edge there still has one unambiguous continuation, and splitting it
// would hand the chaining walk a four-way junction to guess at.
std::vector<Segment> split_at_collinear_overlaps(const std::vector<Segment>& segments,
                                                 std::vector<std::size_t>& source_index) {
  std::vector<std::vector<Point2>> cuts(segments.size());

  for (std::size_t i = 0; i < segments.size(); ++i) {
    for (std::size_t j = 0; j < segments.size(); ++j) {
      if (i == j || !collinear_segments(segments[i], segments[j])) {
        continue;
      }
      for (const Point2& p : {segments[j].a, segments[j].b}) {
        if (strictly_inside_segment(segments[i].a, segments[i].b, p)) {
          cuts[i].push_back(p);
        }
      }
    }
  }

  std::vector<Segment> out;
  out.reserve(segments.size() * 2);
  source_index.clear();
  source_index.reserve(segments.size() * 2);

  for (std::size_t i = 0; i < segments.size(); ++i) {
    const Segment& segment = segments[i];
    // Along the spanned axis the cut points are strictly monotone, so ordering by that one
    // coordinate orders them along the segment exactly, with no parameter to round.
    const bool by_x = segment.a.x != segment.b.x;
    const bool ascending = by_x ? segment.b.x > segment.a.x : segment.b.y > segment.a.y;
    std::sort(cuts[i].begin(), cuts[i].end(), [by_x, ascending](const Point2& p, const Point2& q) {
      const int64_t pk = by_x ? p.x : p.y;
      const int64_t qk = by_x ? q.x : q.y;
      return ascending ? pk < qk : pk > qk;
    });

    Point2 cursor = segment.a;
    for (const Point2& cut : cuts[i]) {
      if (cut != cursor) {  // repeated cut points collapse: equal key plus collinear means equal
        out.push_back(Segment{cursor, cut});
        source_index.push_back(i);
        cursor = cut;
      }
    }
    if (segment.b != cursor) {
      out.push_back(Segment{cursor, segment.b});
      source_index.push_back(i);
    }
  }
  return out;
}

// One piece keyed by its endpoints as an unordered pair, so that a subject piece and a clip piece
// that trace the same stretch of boundary land next to each other after sorting no matter which
// way round each is directed.
struct OverlapKey {
  Point2 lo;
  Point2 hi;
  std::size_t piece = 0;
  bool from_subject = false;
};

bool overlap_key_less(const OverlapKey& a, const OverlapKey& b) {
  if (a.lo != b.lo) {
    return point_less(a.lo, b.lo);
  }
  if (a.hi != b.hi) {
    return point_less(a.hi, b.hi);
  }
  if (a.from_subject != b.from_subject) {
    return a.from_subject && !b.from_subject;  // subject representative first
  }
  return a.piece < b.piece;  // total order: no two records compare equal
}

// Decides the pieces that lie on boundary shared by both operands.
//
// Along such a stretch the two operands' edges either run the same way - their interiors are on
// the same side, so the operands locally coincide - or opposite ways - their interiors are on
// opposite sides, so they merely touch. That distinction, not a containment test, is what says
// whether the shared boundary survives:
//
//   same direction      union keeps it once, intersection keeps it once, difference deletes it
//                       (locally subject == clip, so subject - clip is locally empty)
//   opposite direction  union and intersection delete it (the regions glue along it / share no
//                       area), difference keeps the subject's copy unchanged
//
// Exactly one copy is ever kept, always the subject's, so the shared boundary is not emitted
// twice; a duplicated edge would give the chaining walk two equal choices at one vertex.
void resolve_shared_boundary(const std::vector<Segment>& pieces,
                             const std::vector<bool>& from_subject, BoolOp op,
                             std::vector<Verdict>& verdict) {
  std::vector<OverlapKey> keys;
  keys.reserve(pieces.size());
  for (std::size_t i = 0; i < pieces.size(); ++i) {
    const Point2& a = pieces[i].a;
    const Point2& b = pieces[i].b;
    const bool a_first = point_less(a, b);
    keys.push_back(OverlapKey{a_first ? a : b, a_first ? b : a, i, from_subject[i]});
  }
  std::sort(keys.begin(), keys.end(), overlap_key_less);

  std::size_t start = 0;
  while (start < keys.size()) {
    std::size_t end = start + 1;
    while (end < keys.size() && keys[end].lo == keys[start].lo && keys[end].hi == keys[start].hi) {
      ++end;
    }

    std::size_t subject_piece = pieces.size();
    std::size_t clip_piece = pieces.size();
    for (std::size_t k = start; k < end; ++k) {
      if (keys[k].from_subject) {
        if (subject_piece == pieces.size()) {
          subject_piece = keys[k].piece;
        }
      } else if (clip_piece == pieces.size()) {
        clip_piece = keys[k].piece;
      }
    }

    // Only a stretch walked by both operands needs this treatment. A repeated edge within one
    // operand is a self-overlapping input and is left to the midpoint test.
    if (subject_piece != pieces.size() && clip_piece != pieces.size()) {
      const bool same_direction = pieces[subject_piece].a == pieces[clip_piece].a;
      bool keep_subject_copy = false;
      switch (op) {
        case BoolOp::kUnion:
        case BoolOp::kIntersection:
          keep_subject_copy = same_direction;
          break;
        case BoolOp::kDifference:
          keep_subject_copy = !same_direction;
          break;
      }
      for (std::size_t k = start; k < end; ++k) {
        verdict[keys[k].piece] = Verdict::kDrop;
      }
      if (keep_subject_copy) {
        verdict[subject_piece] = Verdict::kKeep;
      }
    }

    start = end;
  }
}

}  // namespace

std::vector<Polygon> boolean_op(const std::vector<Polygon>& subject,
                                const std::vector<Polygon>& clip, BoolOp op) {
  if (subject.empty() && clip.empty()) {
    return {};
  }

  std::vector<Polygon> subject_rings = subject;
  std::vector<Polygon> clip_rings = clip;
  orient_rings(subject_rings);
  orient_rings(clip_rings);

  std::vector<Segment> segments;
  emit_ring_edges(subject_rings, segments);
  const std::size_t subject_segment_count = segments.size();
  emit_ring_edges(clip_rings, segments);
  if (segments.empty()) {
    return {};
  }

  // Two splitting passes, in this order: collinear overlaps first, because they are decided from
  // the original endpoints, then the transversal crossings, which introduce new ones.
  std::vector<std::size_t> overlap_source;
  const std::vector<Segment> split_once = split_at_collinear_overlaps(segments, overlap_source);

  std::vector<std::size_t> crossing_source;
  const std::vector<Segment> pieces = split_at_intersections(split_once, &crossing_source);

  std::vector<bool> from_subject(pieces.size(), false);
  for (std::size_t i = 0; i < pieces.size(); ++i) {
    from_subject[i] = overlap_source[crossing_source[i]] < subject_segment_count;
  }

  std::vector<Verdict> verdict(pieces.size(), Verdict::kClassify);
  resolve_shared_boundary(pieces, from_subject, op, verdict);

  std::vector<Segment> kept;
  kept.reserve(pieces.size());
  for (std::size_t i = 0; i < pieces.size(); ++i) {
    if (verdict[i] == Verdict::kDrop) {
      continue;
    }
    bool keep = true;
    if (verdict[i] == Verdict::kClassify) {
      const Point2 midpoint = piece_midpoint(pieces[i]);
      const bool inside_other =
          inside_region(from_subject[i] ? clip_rings : subject_rings, midpoint);
      switch (op) {
        case BoolOp::kUnion:
          keep = !inside_other;
          break;
        case BoolOp::kIntersection:
          keep = inside_other;
          break;
        case BoolOp::kDifference:
          keep = from_subject[i] ? !inside_other : inside_other;
          break;
      }
    }
    if (!keep) {
      continue;
    }
    Segment piece = pieces[i];
    // The clip boundary surviving a difference bounds removed area, so it has to be walked the
    // other way round: wound opposite to the ring that encloses it, which is what makes it read
    // as a hole instead of as a second solid.
    if (op == BoolOp::kDifference && !from_subject[i]) {
      std::swap(piece.a, piece.b);
    }
    kept.push_back(piece);
  }

  std::vector<Polygon> result = chain_segments(kept);

  result.erase(std::remove_if(result.begin(), result.end(),
                              [](const Polygon& ring) {
                                return std::fabs(signed_area_mm2(ring)) < kMinRingAreaMm2;
                              }),
               result.end());

  orient_rings(result);
  return result;
}

}  // namespace slicem
