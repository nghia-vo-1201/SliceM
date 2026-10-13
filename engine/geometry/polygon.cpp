#include "geometry/polygon.h"

#include <cmath>

namespace slicem {

double signed_area_mm2(const Polygon& poly) {
  if (poly.size() < 3) {
    return 0.0;
  }
  // Accumulated in millimetre doubles rather than units: the exact integer cross product of
  // two bed-sized coordinates overflows int64, and area is only ever reported, never used to
  // make a topological decision.
  double sum = 0.0;
  for (std::size_t i = 0; i < poly.size(); ++i) {
    const Point2& a = poly[i];
    const Point2& b = poly[(i + 1) % poly.size()];
    sum += units_to_mm(a.x) * units_to_mm(b.y) - units_to_mm(b.x) * units_to_mm(a.y);
  }
  return sum * 0.5;
}

double perimeter_mm(const Polygon& poly) {
  if (poly.size() < 2) {
    return 0.0;
  }
  double total = 0.0;
  for (std::size_t i = 0; i < poly.size(); ++i) {
    const Point2& a = poly[i];
    const Point2& b = poly[(i + 1) % poly.size()];
    const double dx = units_to_mm(b.x) - units_to_mm(a.x);
    const double dy = units_to_mm(b.y) - units_to_mm(a.y);
    total += std::sqrt(dx * dx + dy * dy);
  }
  return total;
}

}  // namespace slicem
