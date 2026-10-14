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

std::vector<std::vector<Polygon>> solid_shell_regions(
    const std::vector<std::vector<Polygon>>& layer_regions, int shell_layers) {
  std::vector<std::vector<Polygon>> solid(layer_regions.size());
  if (shell_layers <= 0) {
    return solid;
  }

  const std::vector<LayerSurfaces> surfaces = detect_surfaces(layer_regions);
  const int count = static_cast<int>(layer_regions.size());

  for (int i = 0; i < count; ++i) {
    const std::size_t index = static_cast<std::size_t>(i);
    if (layer_regions[index].empty()) {
      continue;
    }

    std::vector<Polygon> exposed;
    // An empty operand is never handed to the boolean: the first contributing set is adopted
    // outright, so a layer with a single contributing surface costs no boolean at all.
    const auto accumulate = [&exposed](const std::vector<Polygon>& rings) {
      if (rings.empty()) {
        return;
      }
      exposed = exposed.empty() ? rings : polygon_union(exposed, rings);
    };

    for (int k = 0; k < shell_layers; ++k) {
      if (i + k < count) {
        accumulate(surfaces[static_cast<std::size_t>(i + k)].top);
      }
      if (i - k >= 0) {
        accumulate(surfaces[static_cast<std::size_t>(i - k)].bottom);
      }
    }

    if (!exposed.empty()) {
      solid[index] = polygon_intersection(layer_regions[index], exposed);
    }
  }

  return solid;
}

}  // namespace slicem
