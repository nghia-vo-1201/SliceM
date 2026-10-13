#include "slicing/mesh.h"

namespace slicem {

Bounds3 compute_bounds(const TriangleMesh& mesh) {
  Bounds3 bounds;
  for (const Triangle& tri : mesh.triangles) {
    for (int k = 0; k < 3; ++k) {
      const Point3& p = mesh.vertices[static_cast<std::size_t>(tri.v[k])];
      if (!bounds.valid) {
        bounds.min = p;
        bounds.max = p;
        bounds.valid = true;
        continue;
      }
      if (p.x < bounds.min.x) bounds.min.x = p.x;
      if (p.y < bounds.min.y) bounds.min.y = p.y;
      if (p.z < bounds.min.z) bounds.min.z = p.z;
      if (p.x > bounds.max.x) bounds.max.x = p.x;
      if (p.y > bounds.max.y) bounds.max.y = p.y;
      if (p.z > bounds.max.z) bounds.max.z = p.z;
    }
  }
  return bounds;
}

}  // namespace slicem
