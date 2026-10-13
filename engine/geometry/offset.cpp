#include "geometry/offset.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace slicem {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct Segment {
  Point2 a;
  Point2 b;
};

struct SplitPoint {
  long double t = 0.0L;
  Point2 point;
};

long double to_ld(int64_t v) { return static_cast<long double>(v); }

// Interior crossings only. Endpoint contacts are deliberately ignored: offset segments that
// merely meet at a shared join point are already correctly chained, and treating that as an
// intersection would split them into zero-length stubs.
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

double point_segment_distance_mm(const Point2& p, const Point2& a, const Point2& b) {
  const double px = units_to_mm(p.x);
  const double py = units_to_mm(p.y);
  const double ax = units_to_mm(a.x);
  const double ay = units_to_mm(a.y);
  const double bx = units_to_mm(b.x);
  const double by = units_to_mm(b.y);
  const double dx = bx - ax;
  const double dy = by - ay;
  const double len_sq = dx * dx + dy * dy;
  double t = 0.0;
  if (len_sq > 0.0) {
    t = ((px - ax) * dx + (py - ay) * dy) / len_sq;
    t = std::max(0.0, std::min(1.0, t));
  }
  const double cx = ax + t * dx;
  const double cy = ay + t * dy;
  return std::hypot(px - cx, py - cy);
}

double distance_to_rings_mm(const std::vector<Polygon>& rings, const Point2& p) {
  double best = 1e30;
  for (const Polygon& ring : rings) {
    for (std::size_t i = 0; i < ring.size(); ++i) {
      const Point2& a = ring[i];
      const Point2& b = ring[(i + 1) % ring.size()];
      best = std::min(best, point_segment_distance_mm(p, a, b));
      if (best == 0.0) {
        return best;
      }
    }
  }
  return best;
}

void append_arc(std::vector<Segment>& out, const Point2& centre, const Point2& from,
                const Point2& to, double radius_units, double arc_tolerance_mm, bool counter_clockwise) {
  const double radius_mm = units_to_mm(static_cast<int64_t>(radius_units));
  double step = kPi / 8.0;
  if (radius_mm > arc_tolerance_mm && arc_tolerance_mm > 0.0) {
    step = 2.0 * std::acos(1.0 - arc_tolerance_mm / radius_mm);
  }
  step = std::max(step, 1e-3);

  const double start = std::atan2(to_ld(from.y - centre.y), to_ld(from.x - centre.x));
  const double end = std::atan2(to_ld(to.y - centre.y), to_ld(to.x - centre.x));
  double sweep = end - start;
  if (counter_clockwise) {
    while (sweep <= 0.0) sweep += 2.0 * kPi;
  } else {
    while (sweep >= 0.0) sweep -= 2.0 * kPi;
  }

  const int steps = std::max(1, static_cast<int>(std::ceil(std::fabs(sweep) / step)));
  Point2 previous = from;
  for (int i = 1; i <= steps; ++i) {
    Point2 next = to;
    if (i < steps) {
      const double angle = start + sweep * (static_cast<double>(i) / static_cast<double>(steps));
      next = Point2{centre.x + static_cast<int64_t>(std::llround(std::cos(angle) * radius_units)),
                    centre.y + static_cast<int64_t>(std::llround(std::sin(angle) * radius_units))};
    }
    if (next != previous) {
      out.push_back(Segment{previous, next});
    }
    previous = next;
  }
}

// Raw offset outline for one ring: each edge displaced along its outward normal, with round
// joins inserted wherever the displacement opens a gap. The result self-intersects wherever
// the solid is thinner than 2*|delta|; that is expected and resolved by the caller.
void emit_offset_outline(const Polygon& ring, double delta_units, double arc_tolerance_mm,
                         std::vector<Segment>& out) {
  const std::size_t n = ring.size();
  if (n < 3) {
    return;
  }

  std::vector<Segment> edges;
  std::vector<std::size_t> edge_end_vertex;
  edges.reserve(n);
  edge_end_vertex.reserve(n);

  for (std::size_t i = 0; i < n; ++i) {
    const Point2& p = ring[i];
    const Point2& q = ring[(i + 1) % n];
    const double dx = static_cast<double>(q.x - p.x);
    const double dy = static_cast<double>(q.y - p.y);
    const double length = std::hypot(dx, dy);
    if (length == 0.0) {
      continue;
    }
    // Outward normal for a counter-clockwise ring. Holes are wound clockwise, so the same
    // expression points out of the solid for them too, which is why one formula covers both.
    const double nx = dy / length;
    const double ny = -dx / length;
    const int64_t ox = static_cast<int64_t>(std::llround(nx * delta_units));
    const int64_t oy = static_cast<int64_t>(std::llround(ny * delta_units));
    edges.push_back(Segment{Point2{p.x + ox, p.y + oy}, Point2{q.x + ox, q.y + oy}});
    edge_end_vertex.push_back((i + 1) % n);
  }

  if (edges.empty()) {
    return;
  }

  for (std::size_t i = 0; i < edges.size(); ++i) {
    out.push_back(edges[i]);

    const Segment& current = edges[i];
    const Segment& next = edges[(i + 1) % edges.size()];
    if (current.b == next.a) {
      continue;
    }

    const Point2& corner = ring[edge_end_vertex[i]];
    const double cross = static_cast<double>(current.b.x - current.a.x) *
                             static_cast<double>(next.b.y - next.a.y) -
                         static_cast<double>(current.b.y - current.a.y) *
                             static_cast<double>(next.b.x - next.a.x);
    // A gap opens only when the turn and the offset direction agree in sign; the opposite
    // case makes the two offset edges overlap, which the intersection pass cleans up.
    if (cross * delta_units > 0.0) {
      append_arc(out, corner, current.b, next.a, std::fabs(delta_units), arc_tolerance_mm,
                 delta_units > 0.0);
    }
  }
}

std::vector<Segment> split_at_intersections(const std::vector<Segment>& segments) {
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

  std::vector<Segment> out;
  out.reserve(segments.size() * 2);
  for (std::size_t i = 0; i < segments.size(); ++i) {
    std::sort(splits[i].begin(), splits[i].end(),
              [](const SplitPoint& a, const SplitPoint& b) { return a.t < b.t; });
    Point2 cursor = segments[i].a;
    for (const SplitPoint& split : splits[i]) {
      if (split.point != cursor) {
        out.push_back(Segment{cursor, split.point});
        cursor = split.point;
      }
    }
    if (segments[i].b != cursor) {
      out.push_back(Segment{cursor, segments[i].b});
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

}  // namespace

bool point_in_ring(const Polygon& ring, const Point2& point) {
  bool inside = false;
  const std::size_t n = ring.size();
  for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
    const Point2& a = ring[i];
    const Point2& b = ring[j];
    if ((a.y > point.y) != (b.y > point.y)) {
      const long double t = to_ld(point.y - a.y) / to_ld(b.y - a.y);
      const long double x = to_ld(a.x) + t * to_ld(b.x - a.x);
      if (to_ld(point.x) < x) {
        inside = !inside;
      }
    }
  }
  return inside;
}

void orient_rings(std::vector<Polygon>& rings) {
  for (std::size_t i = 0; i < rings.size(); ++i) {
    if (rings[i].size() < 3) {
      continue;
    }
    int depth = 0;
    for (std::size_t j = 0; j < rings.size(); ++j) {
      if (i == j || rings[j].size() < 3) {
        continue;
      }
      if (point_in_ring(rings[j], rings[i][0])) {
        ++depth;
      }
    }
    const bool should_be_ccw = (depth % 2) == 0;
    const bool is_ccw = signed_area_mm2(rings[i]) > 0.0;
    if (is_ccw != should_be_ccw) {
      std::reverse(rings[i].begin(), rings[i].end());
    }
  }
}

std::vector<Polygon> offset_rings(const std::vector<Polygon>& rings, double delta_mm,
                                  const OffsetOptions& options) {
  if (rings.empty()) {
    return {};
  }
  if (delta_mm == 0.0) {
    return rings;
  }

  const double delta_units = delta_mm * static_cast<double>(kUnitsPerMm);

  std::vector<Segment> outline;
  for (const Polygon& ring : rings) {
    emit_offset_outline(ring, delta_units, options.arc_tolerance_mm, outline);
  }
  if (outline.empty()) {
    return {};
  }

  const std::vector<Segment> pieces = split_at_intersections(outline);

  // Valid offset geometry sits exactly |delta| away from the input. Anything closer belongs to
  // a loop that offsetting turned inside out, so discarding by distance is what makes features
  // thinner than 2*|delta| disappear instead of reappearing with inverted winding.
  //
  // The threshold is slackened by the arc tolerance because a chord approximating a round join
  // legitimately sits up to that far inside the true offset radius.
  const double threshold = std::fabs(delta_mm) - options.arc_tolerance_mm - 1e-3;

  std::vector<Segment> kept;
  kept.reserve(pieces.size());
  for (const Segment& piece : pieces) {
    const Point2 midpoint{(piece.a.x + piece.b.x) / 2, (piece.a.y + piece.b.y) / 2};
    if (distance_to_rings_mm(rings, midpoint) >= threshold) {
      kept.push_back(piece);
    }
  }

  std::vector<Polygon> result = chain_segments(kept);

  result.erase(std::remove_if(result.begin(), result.end(),
                              [](const Polygon& ring) {
                                return std::fabs(signed_area_mm2(ring)) < 1e-9;
                              }),
               result.end());

  orient_rings(result);
  return result;
}

}  // namespace slicem
