#include "geometry/segment_ops.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace slicem {
namespace segment_ops {
namespace {

struct SplitPoint {
  long double t = 0.0L;
  Point2 point;
};

// Exact on the integer grid. Every coordinate reaching this point is a build-volume coordinate
// plus at most an offset distance, so it stays below 1e9 units and the products below stay two
// orders of magnitude inside int64_t's range.
bool point_strictly_inside(const Segment& s, const Point2& p, long double& t) {
  const int64_t dx = s.b.x - s.a.x;
  const int64_t dy = s.b.y - s.a.y;
  const int64_t px = p.x - s.a.x;
  const int64_t py = p.y - s.a.y;
  if (dx * py - dy * px != 0) {
    return false;
  }
  const int64_t along = dx * px + dy * py;
  const int64_t length_sq = dx * dx + dy * dy;
  if (along <= 0 || along >= length_sq) {
    return false;
  }
  t = static_cast<long double>(along) / static_cast<long double>(length_sq);
  return true;
}

}  // namespace

long double to_ld(int64_t v) { return static_cast<long double>(v); }

bool interior_intersection(const Segment& s, const Segment& t, Point2& out, long double& ts,
                           long double& tt) {
  const long double x1 = to_ld(s.a.x);
  const long double y1 = to_ld(s.a.y);
  const long double x2 = to_ld(s.b.x);
  const long double y2 = to_ld(s.b.y);
  const long double x3 = to_ld(t.a.x);
  const long double y3 = to_ld(t.a.y);
  const long double x4 = to_ld(t.b.x);
  const long double y4 = to_ld(t.b.y);

  const long double denom = (x2 - x1) * (y4 - y3) - (y2 - y1) * (x4 - x3);
  if (denom == 0.0L) {
    return false;
  }
  ts = ((x3 - x1) * (y4 - y3) - (y3 - y1) * (x4 - x3)) / denom;
  tt = ((x3 - x1) * (y2 - y1) - (y3 - y1) * (x2 - x1)) / denom;
  if (ts <= 0.0L || ts >= 1.0L || tt <= 0.0L || tt >= 1.0L) {
    return false;
  }
  out = Point2{static_cast<int64_t>(std::llroundl(x1 + ts * (x2 - x1))),
               static_cast<int64_t>(std::llroundl(y1 + ts * (y2 - y1)))};
  return true;
}

std::vector<Segment> split_at_intersections(const std::vector<Segment>& segments,
                                            std::vector<std::size_t>* source_index) {
  std::vector<std::vector<SplitPoint>> splits(segments.size());

  for (std::size_t i = 0; i < segments.size(); ++i) {
    for (std::size_t j = i + 1; j < segments.size(); ++j) {
      Point2 crossing;
      long double ti = 0.0L;
      long double tj = 0.0L;
      if (interior_intersection(segments[i], segments[j], crossing, ti, tj)) {
        // The same rounded point is handed to both sides, so the two halves still share an
        // endpoint exactly and can be re-chained without tolerance.
        splits[i].push_back(SplitPoint{ti, crossing});
        splits[j].push_back(SplitPoint{tj, crossing});
      }
    }
  }

  // A segment that merely *ends* on another's interior is a T-junction, not a crossing, so the
  // test above rejects it - both parameters have to be strictly inside. It still has to be split:
  // an unsplit crossbar leaves the arrangement non-planar, and the chaining walk then turns into
  // the stem and never closes the ring, so the whole loop is silently dropped.
  //
  // This is not a rare degeneracy. A triangulated box slices into contours carrying collinear
  // vertices wherever a facet diagonal meets the plane, and offsetting such a contour by exactly
  // the collinear vertex's distance from the corner lands a displaced endpoint precisely on
  // another displaced edge. Inset by 0.9 mm, a 20 mm cube lost two whole layers of infill to it.
  for (std::size_t i = 0; i < segments.size(); ++i) {
    for (std::size_t j = 0; j < segments.size(); ++j) {
      if (i == j) {
        continue;
      }
      long double t = 0.0L;
      if (point_strictly_inside(segments[i], segments[j].a, t)) {
        splits[i].push_back(SplitPoint{t, segments[j].a});
      }
      if (point_strictly_inside(segments[i], segments[j].b, t)) {
        splits[i].push_back(SplitPoint{t, segments[j].b});
      }
    }
  }

  std::vector<Segment> out;
  out.reserve(segments.size() * 2);
  if (source_index != nullptr) {
    source_index->clear();
    source_index->reserve(segments.size() * 2);
  }
  for (std::size_t i = 0; i < segments.size(); ++i) {
    std::sort(splits[i].begin(), splits[i].end(),
              [](const SplitPoint& a, const SplitPoint& b) { return a.t < b.t; });
    Point2 cursor = segments[i].a;
    for (const SplitPoint& split : splits[i]) {
      if (split.point != cursor) {
        out.push_back(Segment{cursor, split.point});
        if (source_index != nullptr) {
          source_index->push_back(i);
        }
        cursor = split.point;
      }
    }
    if (segments[i].b != cursor) {
      out.push_back(Segment{cursor, segments[i].b});
      if (source_index != nullptr) {
        source_index->push_back(i);
      }
    }
  }
  return out;
}

std::vector<Polygon> chain_segments(const std::vector<Segment>& segments) {
  std::unordered_map<Point2, std::vector<int>, Point2Hash> by_start;
  by_start.reserve(segments.size() * 2);
  for (std::size_t i = 0; i < segments.size(); ++i) {
    by_start[segments[i].a].push_back(static_cast<int>(i));
  }

  std::vector<char> used(segments.size(), 0);
  std::vector<Polygon> rings;

  for (std::size_t i = 0; i < segments.size(); ++i) {
    if (used[i]) {
      continue;
    }
    used[i] = 1;
    const Point2 first = segments[i].a;
    Polygon ring;
    ring.push_back(first);
    Point2 cursor = segments[i].b;
    bool closed = false;

    while (true) {
      if (cursor == first) {
        closed = true;
        break;
      }
      ring.push_back(cursor);
      int next = -1;
      const auto it = by_start.find(cursor);
      if (it != by_start.end()) {
        for (const int candidate : it->second) {
          if (!used[static_cast<std::size_t>(candidate)]) {
            next = candidate;
            break;
          }
        }
      }
      if (next < 0) {
        break;
      }
      used[static_cast<std::size_t>(next)] = 1;
      cursor = segments[static_cast<std::size_t>(next)].b;
    }

    if (closed && ring.size() >= 3) {
      rings.push_back(std::move(ring));
    }
  }
  return rings;
}

}  // namespace segment_ops
}  // namespace slicem
