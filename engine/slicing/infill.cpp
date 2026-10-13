#include "slicing/infill.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace slicem {
namespace {

constexpr double kPi = 3.14159265358979323846;

// A span shorter than half an extrusion width cannot be extruded as a line: the nozzle would
// drop a blob roughly one width across instead of drawing anything. This is the only tolerance
// in the file, and it is a physical threshold rather than a numerical one.
//
// The scan needs no numerical tolerance because the rotated ring coordinates are quantised back
// onto the 1 nm grid of types.h before any comparison is made, so the above/below/on decision
// per scan line is an exact integer comparison - the same half-open rule point_in_ring uses.
constexpr double kMinSpanWidthFraction = 0.5;

// One ring expressed in the scan frame: the problem is rotated by -angle so that scan lines are
// axis-aligned, which keeps the scan itself integral and leaves rotation the only float work.
struct RotatedRing {
  std::vector<int64_t> u;
  std::vector<int64_t> v;
};

long double to_ld(int64_t value) {
  return static_cast<long double>(value);
}

int64_t round_to_units(double value) {
  return static_cast<int64_t>(std::llround(value));
}

// Rotates a scan-frame point back by +angle and snaps it to the integer grid.
Point2 from_scan_frame(int64_t u, int64_t v, double cos_a, double sin_a) {
  const double du = static_cast<double>(u);
  const double dv = static_cast<double>(v);
  return Point2{round_to_units(du * cos_a - dv * sin_a),
                round_to_units(du * sin_a + dv * cos_a)};
}

}  // namespace

std::vector<InfillLine> generate_infill(const std::vector<Polygon>& region,
                                        const InfillSettings& settings) {
  if (settings.pattern != InfillPattern::kRectilinear) {
    // Nothing else is implemented yet; returning nothing beats guessing a pattern.
    return {};
  }
  // The !(x > 0) form rejects NaN along with zero and negative values.
  if (region.empty() || !(settings.density > 0.0) || !(settings.extrusion_width_mm > 0.0)) {
    return {};
  }

  const double density = std::min(settings.density, 1.0);
  // At density 1 the lines sit exactly one extrusion width apart, which is solid fill.
  const double spacing_mm = settings.extrusion_width_mm / density;
  const int64_t spacing = std::max<int64_t>(1, mm_to_units(spacing_mm));
  const int64_t min_span =
      mm_to_units(settings.extrusion_width_mm * kMinSpanWidthFraction);

  const double angle = settings.angle_degrees * kPi / 180.0;
  const double cos_a = std::cos(angle);
  const double sin_a = std::sin(angle);

  std::vector<RotatedRing> rings;
  rings.reserve(region.size());
  bool have_bounds = false;
  int64_t v_min = 0;
  int64_t v_max = 0;

  for (const Polygon& ring : region) {
    if (ring.size() < 3) {
      // A ring with fewer than three vertices bounds no area, so it can contribute no span.
      continue;
    }
    RotatedRing rotated;
    rotated.u.reserve(ring.size());
    rotated.v.reserve(ring.size());
    for (const Point2& p : ring) {
      const double x = static_cast<double>(p.x);
      const double y = static_cast<double>(p.y);
      const int64_t u = round_to_units(x * cos_a + y * sin_a);
      const int64_t v = round_to_units(-x * sin_a + y * cos_a);
      rotated.u.push_back(u);
      rotated.v.push_back(v);
      if (!have_bounds) {
        v_min = v;
        v_max = v;
        have_bounds = true;
      } else {
        v_min = std::min(v_min, v);
        v_max = std::max(v_max, v);
      }
    }
    rings.push_back(std::move(rotated));
  }
  if (!have_bounds) {
    return {};
  }

  // Scan lines sit on the fixed lattice v = i*spacing + spacing/2, anchored to the origin
  // rather than to the region. That keeps the pattern aligned between regions of the same
  // layer, and the half-spacing offset keeps a line off the lattice line through the origin,
  // which is where an axis-aligned region's own edge most often lies.
  const int64_t half = spacing / 2;
  const int64_t first_index = ceil_div(v_min - half, spacing);
  const int64_t last_index = floor_div(v_max - half, spacing);

  std::vector<InfillLine> lines;
  std::vector<long double> crossings;

  for (int64_t i = first_index; i <= last_index; ++i) {
    const int64_t v = i * spacing + half;

    crossings.clear();
    for (const RotatedRing& ring : rings) {
      const std::size_t n = ring.u.size();
      for (std::size_t a = 0, b = n - 1; a < n; b = a++) {
        const int64_t va = ring.v[a];
        const int64_t vb = ring.v[b];
        // Half-open rule: an edge counts only if exactly one endpoint is strictly above the
        // scan line. A vertex exactly on the line is therefore crossed once, a horizontal edge
        // on the line not at all, and a vertex merely touching from one side twice at the same
        // place - a zero-length span the length filter below discards. The crossing count is
        // always even, so even-odd pairing can never run off the end.
        if ((va > v) == (vb > v)) {
          continue;
        }
        const long double t = to_ld(v - va) / to_ld(vb - va);
        crossings.push_back(to_ld(ring.u[a]) + t * to_ld(ring.u[b] - ring.u[a]));
      }
    }
    if (crossings.size() < 2) {
      continue;
    }
    std::sort(crossings.begin(), crossings.end());

    // Serpentine: every other scan line is walked backwards, spans included, so the emitted
    // sequence stays monotone in the scan direction and needs no travel move between lines.
    const bool backwards = ((i - first_index) % 2) != 0;
    const std::size_t span_count = crossings.size() / 2;
    for (std::size_t s = 0; s < span_count; ++s) {
      const std::size_t span = backwards ? (span_count - 1 - s) : s;
      const int64_t u0 = static_cast<int64_t>(std::llroundl(crossings[span * 2]));
      const int64_t u1 = static_cast<int64_t>(std::llroundl(crossings[span * 2 + 1]));
      if (u1 - u0 < min_span) {
        continue;
      }
      Point2 start = from_scan_frame(u0, v, cos_a, sin_a);
      Point2 end = from_scan_frame(u1, v, cos_a, sin_a);
      if (backwards) {
        std::swap(start, end);
      }
      lines.push_back(InfillLine{start, end});
    }
  }

  return lines;
}

}  // namespace slicem
