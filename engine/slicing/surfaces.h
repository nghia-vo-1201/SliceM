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

// The region of each layer that must be printed solid, given a shell `shell_layers` thick.
//
// One solid layer under a ceiling is not enough: it is laid across the gaps of the sparse infill
// below it and sags into them. The shell is therefore thickened by projecting every exposed
// surface back into the layers that have to carry it - a top surface downwards, a bottom surface
// upwards - and a layer is solid wherever any projection lands on it:
//
//   solid[i] = region[i] & ( top[i] | top[i+1] | ... | top[i+shell-1]
//                          | bottom[i] | bottom[i-1] | ... | bottom[i-shell+1] )
//
// `shell_layers` of 0 returns empty regions throughout, which prints a part with open surfaces -
// a legitimate request for a vase, and wrong for anything else.
//
// The input is the same per-layer infill regions `detect_surfaces` takes.
std::vector<std::vector<Polygon>> solid_shell_regions(
    const std::vector<std::vector<Polygon>>& layer_regions, int shell_layers);

}  // namespace slicem
