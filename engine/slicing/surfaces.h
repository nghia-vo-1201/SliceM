#pragma once

#include <vector>

#include "geometry/polygon.h"

namespace slicem {

struct LayerSurfaces {
  std::vector<Polygon> top;     // nothing above -> needs a solid top
  std::vector<Polygon> bottom;  // nothing below -> needs a solid bottom
  std::vector<Polygon> sparse;  // the rest: sparse infill
};

// Splits every layer's infill region into the parts that face open air and the part buried
// between neighbours.
//
// The input is one entry per layer, bottom layer first, each holding the region left for infill
// on that layer - that is, PerimeterResult::infill_region. Regions, not contours: a surface only
// has to be solid where infill would otherwise be laid, since the perimeters already close the
// outside edge.
//
//   top[i]    = region[i] - region[i+1]   (whatever the layer above does not cover)
//   bottom[i] = region[i] - region[i-1]   (whatever the layer below does not support)
//   sparse[i] = region[i] - top[i] - bottom[i]
//
// The first and last layers have no neighbour on one side, so the whole region is bottom
// respectively top - a one-layer part is therefore entirely both, and its sparse region is empty.
// Top and bottom can overlap on a layer thin enough to be exposed in both directions; sparse is
// defined by subtracting both, so the three sets always tile the region without overlap.
std::vector<LayerSurfaces> detect_surfaces(const std::vector<std::vector<Polygon>>& layer_regions);

}  // namespace slicem
