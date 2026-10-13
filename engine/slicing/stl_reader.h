#pragma once

#include <string>

#include "slicing/mesh.h"

namespace slicem {

struct StlLoadResult {
  TriangleMesh mesh;
  bool ok = false;
  std::string error;
  bool binary = false;
  int triangles_read = 0;
  int degenerate_triangles_skipped = 0;
  int duplicate_vertices_welded = 0;
};

StlLoadResult load_stl(const std::string& path);

}  // namespace slicem
