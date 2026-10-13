#include "slicing/perimeters.h"

#include <cstddef>
#include <utility>

namespace slicem {

PerimeterResult generate_perimeters(const std::vector<Polygon>& layer_contours,
                                    const PerimeterSettings& settings) {
  PerimeterResult result;

  // Everything below relies on outer rings winding counter-clockwise and holes clockwise: it is
  // what lets the single negative offset below shrink the solid and grow its holes at once.
  std::vector<Polygon> oriented = layer_contours;
  orient_rings(oriented);

  if (settings.perimeter_count <= 0) {
    result.infill_region = std::move(oriented);
    return result;
  }

  const std::size_t count = static_cast<std::size_t>(settings.perimeter_count);
  result.loops.assign(count, std::vector<Polygon>{});

  const double width_mm = settings.extrusion_width_mm;
  // Guards NaN as well as zero and negative widths. With no width there is no centreline to
  // place, which is reported the same way as a solid too thin to hold the first loop.
  if (!(width_mm > 0.0)) {
    return result;
  }

  for (std::size_t k = 0; k < count; ++k) {
    const double delta_mm = -(0.5 * width_mm + static_cast<double>(k) * width_mm);
    std::vector<Polygon> loop = offset_rings(oriented, delta_mm);
    if (loop.empty()) {
      // offset_rings drops the self-intersecting loops an inset creates, so an empty result
      // here means the solid is thinner than 2*|delta| everywhere. Nothing further inward can
      // exist either, so stop and leave the rest of the result empty.
      return result;
    }
    result.loops[k] = std::move(loop);
  }

  result.infill_region =
      offset_rings(oriented, -(static_cast<double>(settings.perimeter_count) * width_mm));
  return result;
}

}  // namespace slicem
