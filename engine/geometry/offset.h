#pragma once

#include <vector>

#include "geometry/polygon.h"

namespace slicem {

// True when the point lies strictly inside the ring (ray casting, no tolerance).
bool point_in_ring(const Polygon& ring, const Point2& point);

// Normalises a layer's rings so outer boundaries wind counter-clockwise and holes wind
// clockwise, decided by nesting depth. Everything downstream assumes this convention: it is
// what lets a single offset formula shrink a solid and grow its holes at the same time.
void orient_rings(std::vector<Polygon>& rings);

struct OffsetOptions {
  // Maximum chord deviation when approximating a round join with straight segments.
  double arc_tolerance_mm = 0.005;
};

// Offsets oriented rings by delta_mm: negative shrinks the solid, positive grows it.
//
// Offsetting is not a per-vertex displacement. Moving edges inward makes them cross wherever
// the solid is thinner than 2*|delta|, producing loops that enclose the wrong region. Those
// are removed here by splitting every self-intersection and discarding the pieces that end up
// closer to the input than |delta|, so thin features vanish instead of inverting.
std::vector<Polygon> offset_rings(const std::vector<Polygon>& rings, double delta_mm,
                                  const OffsetOptions& options = OffsetOptions{});

}  // namespace slicem
