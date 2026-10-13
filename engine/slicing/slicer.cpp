#include "slicing/slicer.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace slicem {
namespace {

struct Segment {
  Point2 start;
  Point2 end;
};

Point2 plane_crossing(const Point3& p, const Point3& q, int64_t plane_z) {
  // Always interpolate from the strictly-below endpoint toward the on-or-above one. Two
  // triangles sharing an edge then feed identical operands in identical order, so both
  // compute a bit-identical crossing point and chaining can match endpoints exactly rather
  // than within a tolerance.
  const bool p_below = p.z < plane_z;
  const Point3& lo = p_below ? p : q;
  const Point3& hi = p_below ? q : p;
  const double t = static_cast<double>(plane_z - lo.z) / static_cast<double>(hi.z - lo.z);
  return Point2{lo.x + std::llround(t * static_cast<double>(hi.x - lo.x)),
                lo.y + std::llround(t * static_cast<double>(hi.y - lo.y))};
}

}  // namespace

std::vector<Polygon> slice_plane(const TriangleMesh& mesh, const std::vector<int>& candidates,
                                 int64_t plane_z, int* open_contours) {
  std::vector<Segment> segments;
  segments.reserve(candidates.size());

  for (const int tri_index : candidates) {
    const Triangle& tri = mesh.triangles[static_cast<std::size_t>(tri_index)];
    const Point3* v[3] = {&mesh.vertices[static_cast<std::size_t>(tri.v[0])],
                          &mesh.vertices[static_cast<std::size_t>(tri.v[1])],
                          &mesh.vertices[static_cast<std::size_t>(tri.v[2])]};

    // A vertex exactly on the plane counts as above. That single convention makes a triangle
    // yield either no segment or exactly one, keeps the interpolation denominator non-zero,
    // and removes coplanar facets and grazing vertices as special cases entirely.
    bool below[3];
    int below_count = 0;
    for (int k = 0; k < 3; ++k) {
      below[k] = v[k]->z < plane_z;
      if (below[k]) {
        ++below_count;
      }
    }
    if (below_count == 0 || below_count == 3) {
      continue;
    }

    Point2 start{};
    Point2 end{};
    bool has_start = false;
    bool has_end = false;
    for (int k = 0; k < 3; ++k) {
      const int next = (k + 1) % 3;
      if (below[k] == below[next]) {
        continue;
      }
      const Point2 crossing = plane_crossing(*v[k], *v[next], plane_z);
      // Which side of the segment a crossing lands on is decided by the direction of the
      // triangle's own edge, so a consistently wound mesh yields consistently oriented
      // contours across the whole layer.
      if (below[k]) {
        end = crossing;
        has_end = true;
      } else {
        start = crossing;
        has_start = true;
      }
    }
    if (has_start && has_end && start != end) {
      segments.push_back(Segment{start, end});
    }
  }

  std::unordered_map<Point2, std::vector<int>, Point2Hash> by_start;
  by_start.reserve(segments.size() * 2);
  for (std::size_t i = 0; i < segments.size(); ++i) {
    by_start[segments[i].start].push_back(static_cast<int>(i));
  }

  std::vector<char> used(segments.size(), 0);
  std::vector<Polygon> contours;
  int open = 0;

  // Walk order is driven by segment index, and each bucket keeps insertion order, so the
  // output is a deterministic function of triangle order. The hash map is only ever queried,
  // never iterated, which is what keeps bucket ordering out of the result.
  for (std::size_t i = 0; i < segments.size(); ++i) {
    if (used[i]) {
      continue;
    }
    used[i] = 1;
    const Point2 first = segments[i].start;
    Polygon poly;
    poly.push_back(first);
    Point2 cursor = segments[i].end;
    bool closed = false;

    while (true) {
      if (cursor == first) {
        closed = true;
        break;
      }
      poly.push_back(cursor);
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
      cursor = segments[static_cast<std::size_t>(next)].end;
    }

    if (closed && poly.size() >= 3) {
      contours.push_back(std::move(poly));
    } else {
      // An unclosed chain means the mesh is not watertight along this plane.
      ++open;
    }
  }

  if (open_contours != nullptr) {
    *open_contours = open;
  }
  return contours;
}

SliceResult slice_mesh(const TriangleMesh& mesh, const SliceSettings& settings) {
  SliceResult result;

  const int64_t layer_height = mm_to_units(settings.layer_height_mm);
  if (layer_height <= 0) {
    result.error = "layer height must be positive";
    return result;
  }
  if (mesh.empty()) {
    result.ok = true;
    return result;
  }

  const Bounds3 bounds = compute_bounds(mesh);
  const int64_t base_z = bounds.min.z;
  const int64_t total_height = bounds.max.z - base_z;
  if (total_height <= 0) {
    result.ok = true;
    return result;
  }

  // Floor, not round: a remainder thinner than one layer cannot be deposited, so it is
  // dropped rather than printed at the wrong height.
  const int64_t layer_count = total_height / layer_height;
  if (layer_count <= 0) {
    result.ok = true;
    return result;
  }

  const int64_t half_layer = layer_height / 2;

  // Planes sit at the middle of each layer. Mid-layer sampling keeps them clear of the flat
  // faces of axis-aligned models, which is exactly where coincident-vertex degeneracy would
  // otherwise concentrate.
  const auto plane_z_for = [&](int64_t index) {
    return base_z + index * layer_height + half_layer;
  };

  // Triangles are bucketed by the span of layers they cross. Scanning every triangle for
  // every layer would be quadratic on tall models.
  std::vector<std::vector<int>> buckets(static_cast<std::size_t>(layer_count));
  for (std::size_t t = 0; t < mesh.triangles.size(); ++t) {
    const Triangle& tri = mesh.triangles[t];
    int64_t tri_min = mesh.vertices[static_cast<std::size_t>(tri.v[0])].z;
    int64_t tri_max = tri_min;
    for (int k = 1; k < 3; ++k) {
      const int64_t z = mesh.vertices[static_cast<std::size_t>(tri.v[k])].z;
      tri_min = std::min(tri_min, z);
      tri_max = std::max(tri_max, z);
    }

    int64_t first = ceil_div(tri_min - base_z - half_layer, layer_height);
    int64_t last = floor_div(tri_max - base_z - half_layer, layer_height);
    first = std::max<int64_t>(first, 0);
    last = std::min<int64_t>(last, layer_count - 1);
    for (int64_t i = first; i <= last; ++i) {
      buckets[static_cast<std::size_t>(i)].push_back(static_cast<int>(t));
    }
  }

  result.layers.resize(static_cast<std::size_t>(layer_count));
  for (int64_t i = 0; i < layer_count; ++i) {
    Layer& layer = result.layers[static_cast<std::size_t>(i)];
    layer.index = static_cast<int>(i);
    layer.slice_z = plane_z_for(i);
    layer.contours = slice_plane(mesh, buckets[static_cast<std::size_t>(i)], layer.slice_z,
                                 &layer.open_contours);
    result.total_open_contours += layer.open_contours;
  }

  result.ok = true;
  return result;
}

}  // namespace slicem
