#pragma once

#include <vector>

#include "geometry/types.h"

namespace slicem {

struct Triangle {
  int v[3] = {0, 0, 0};
};

struct Bounds3 {
  Point3 min{};
  Point3 max{};
  bool valid = false;
};

// Vertices are welded and quantised at load time, so two triangles sharing an edge share
// vertex indices. Every downstream guarantee about exact endpoint matching rests on that.
struct TriangleMesh {
  std::vector<Point3> vertices;
  std::vector<Triangle> triangles;

  bool empty() const { return triangles.empty(); }
};

Bounds3 compute_bounds(const TriangleMesh& mesh);

}  // namespace slicem
