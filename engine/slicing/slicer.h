#pragma once

#include <string>
#include <vector>

#include "geometry/polygon.h"
#include "slicing/mesh.h"

namespace slicem {

struct SliceSettings {
  double layer_height_mm = 0.2;
};

struct Layer {
  int index = 0;
  int64_t slice_z = 0;
  std::vector<Polygon> contours;
  int open_contours = 0;
};

struct SliceResult {
  std::vector<Layer> layers;
  int total_open_contours = 0;
  bool ok = false;
  std::string error;
};

SliceResult slice_mesh(const TriangleMesh& mesh, const SliceSettings& settings);

// Single plane against a caller-supplied candidate list. Exposed so tests can drive one
// plane directly, including the degenerate cases that are awkward to reach through
// slice_mesh's layer arithmetic.
std::vector<Polygon> slice_plane(const TriangleMesh& mesh, const std::vector<int>& candidates,
                                 int64_t plane_z, int* open_contours);

}  // namespace slicem
