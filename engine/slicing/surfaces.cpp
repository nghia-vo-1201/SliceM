#include "slicing/surfaces.h"

#include <cstddef>

#include "geometry/boolean.h"

namespace slicem {

std::vector<LayerSurfaces> detect_surfaces(const std::vector<std::vector<Polygon>>& layer_regions) {
  std::vector<LayerSurfaces> out(layer_regions.size());

  for (std::size_t i = 0; i < layer_regions.size(); ++i) {
    const std::vector<Polygon>& region = layer_regions[i];
    if (region.empty()) {
      continue;  // nothing to infill on this layer, so nothing to classify
    }

    // A missing neighbour means open air above/below, so the whole region is exposed. Passing an
    // empty clip would give the same answer, but naming the case keeps the top and bottom layers
    // from depending on the difference of a region with nothing.
    if (i + 1 < layer_regions.size()) {
      out[i].top = polygon_difference(region, layer_regions[i + 1]);
    } else {
      out[i].top = region;
    }

    if (i > 0) {
      out[i].bottom = polygon_difference(region, layer_regions[i - 1]);
    } else {
      out[i].bottom = region;
    }

    // Subtracting the two exposed sets in sequence rather than their union: both are already
    // subsets of the region, and a second difference is cheaper and shorter than a union followed
    // by one difference. The order does not matter - the result is the region minus both.
    out[i].sparse = polygon_difference(polygon_difference(region, out[i].top), out[i].bottom);
  }

  return out;
}

}  // namespace slicem
