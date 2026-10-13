#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "check.h"
#include "geometry/offset.h"
#include "slicing/infill.h"

using namespace slicem;

namespace {

constexpr double kWidth = 0.45;

// Endpoints land exactly on the region boundary, where point_in_ring (strict, no tolerance) is
// free to answer either way. "Inside or on" therefore needs an explicit on-boundary arm. The
// slack is 1e-6 mm = 1000 grid units, which is four orders of magnitude above the ~2 nm a
// rotate/snap round trip can introduce and four orders below any real feature.
constexpr double kOnBoundaryToleranceMm = 1e-6;

Polygon rect(double x0, double y0, double x1, double y1) {
  return Polygon{Point2{mm_to_units(x0), mm_to_units(y0)}, Point2{mm_to_units(x1), mm_to_units(y0)},
                 Point2{mm_to_units(x1), mm_to_units(y1)}, Point2{mm_to_units(x0), mm_to_units(y1)}};
}

InfillSettings settings(double width_mm, double density, double angle_degrees) {
  InfillSettings s;
  s.extrusion_width_mm = width_mm;
  s.density = density;
  s.angle_degrees = angle_degrees;
  return s;
}

double line_length_mm(const InfillLine& line) {
  return std::hypot(units_to_mm(line.b.x - line.a.x), units_to_mm(line.b.y - line.a.y));
}

double total_length_mm(const std::vector<InfillLine>& lines) {
  double total = 0.0;
  for (const InfillLine& line : lines) {
    total += line_length_mm(line);
  }
  return total;
}

double point_segment_distance_mm(const Point2& p, const Point2& a, const Point2& b) {
  const double px = units_to_mm(p.x);
  const double py = units_to_mm(p.y);
  const double ax = units_to_mm(a.x);
  const double ay = units_to_mm(a.y);
  const double dx = units_to_mm(b.x) - ax;
  const double dy = units_to_mm(b.y) - ay;
  const double len_sq = dx * dx + dy * dy;
  double t = 0.0;
  if (len_sq > 0.0) {
    t = ((px - ax) * dx + (py - ay) * dy) / len_sq;
    t = std::max(0.0, std::min(1.0, t));
  }
  return std::hypot(px - (ax + t * dx), py - (ay + t * dy));
}

bool on_ring_boundary(const Polygon& ring, const Point2& p) {
  for (std::size_t i = 0; i < ring.size(); ++i) {
    const Point2& a = ring[i];
    const Point2& b = ring[(i + 1) % ring.size()];
    if (point_segment_distance_mm(p, a, b) <= kOnBoundaryToleranceMm) {
      return true;
    }
  }
  return false;
}

bool inside_or_on(const Polygon& ring, const Point2& p) {
  return point_in_ring(ring, p) || on_ring_boundary(ring, p);
}

// point_in_ring is a strict ray cast with no tolerance, so for a point exactly on an edge its
// answer depends on which side the ray leaves from: a point on a hole's left edge comes back
// "inside", one on the right edge "outside". A span that stops dead on a hole wall is correct,
// so the predicate that matters is strict interiority with the boundary excluded explicitly.
bool strictly_inside(const Polygon& ring, const Point2& p) {
  return point_in_ring(ring, p) && !on_ring_boundary(ring, p);
}

Point2 along(const InfillLine& line, double fraction) {
  const double ax = static_cast<double>(line.a.x);
  const double ay = static_cast<double>(line.a.y);
  return Point2{static_cast<int64_t>(std::llround(ax + fraction * static_cast<double>(line.b.x - line.a.x))),
                static_cast<int64_t>(std::llround(ay + fraction * static_cast<double>(line.b.y - line.a.y)))};
}

// Solid fill of a 10x10 square at 0 degrees. Spacing is w/1.0 = 0.45 and scan lines sit on the
// lattice y = 0.225 + k*0.45, so the ones inside [0, 10] are k = 0..21: 22 lines, each spanning
// the full 10 mm. Total 220 mm against the area/width ideal of 100/0.45 = 222.22 - 1.0% short,
// which is the half-spacing margin left at each edge.
void test_solid_square_at_zero_degrees() {
  const std::vector<Polygon> region{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<InfillLine> lines = generate_infill(region, settings(kWidth, 1.0, 0.0));

  CHECK_EQ(lines.size(), std::size_t{22});
  if (lines.empty()) {
    return;
  }
  for (const InfillLine& line : lines) {
    CHECK_EQ(line.a.y, line.b.y);
    CHECK_NEAR(line_length_mm(line), 10.0, 1e-6);
  }
  CHECK_NEAR(total_length_mm(lines), 220.0, 1e-6);

  const double ideal = 100.0 / kWidth;
  CHECK(std::fabs(total_length_mm(lines) - ideal) / ideal < 0.02);

  // First and last scan line positions.
  CHECK_EQ(lines.front().a.y, mm_to_units(0.225));
  CHECK_EQ(lines.back().a.y, mm_to_units(0.225 + 21.0 * 0.45));

  // Serpentine: line 0 runs +x, line 1 runs -x, line 2 runs +x again.
  CHECK(lines[0].a.x < lines[0].b.x);
  CHECK(lines[1].a.x > lines[1].b.x);
  CHECK(lines[2].a.x < lines[2].b.x);
  // and the end of one line is the start of the next, so no travel is needed between them.
  CHECK_EQ(lines[0].b.x, lines[1].a.x);
  CHECK_EQ(lines[1].b.x, lines[2].a.x);
}

// Density only changes the spacing. At 0.25 the spacing is 0.45/0.25 = 1.8 mm, lattice
// y = 0.9 + k*1.8, giving k = 0..5 inside [0, 10]: 6 lines of 10 mm.
void test_density_sets_spacing() {
  const std::vector<Polygon> region{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<InfillLine> lines = generate_infill(region, settings(kWidth, 0.25, 0.0));

  CHECK_EQ(lines.size(), std::size_t{6});
  CHECK_NEAR(total_length_mm(lines), 60.0, 1e-6);
  if (lines.size() == 6) {
    CHECK_EQ(lines[1].a.y - lines[0].a.y, mm_to_units(1.8));
  }
}

// 90 degrees is the same problem transposed: 22 vertical lines of 10 mm.
void test_solid_square_at_ninety_degrees() {
  const std::vector<Polygon> region{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<InfillLine> lines = generate_infill(region, settings(kWidth, 1.0, 90.0));

  CHECK_EQ(lines.size(), std::size_t{22});
  CHECK_NEAR(total_length_mm(lines), 220.0, 1e-6);
  for (const InfillLine& line : lines) {
    CHECK_EQ(line.a.x, line.b.x);
    CHECK_NEAR(line_length_mm(line), 10.0, 1e-6);
    CHECK(inside_or_on(region[0], line.a));
    CHECK(inside_or_on(region[0], line.b));
  }
}

// 45 degrees over the same square. In the scan frame the square becomes a diamond with
// v in [-7.0711, 7.0711] and width 14.1421 - 2*|v| at height v. The lattice
// v = 0.225 + k*0.45 puts 32 lines in range (k = -16..15), but the outermost pair at
// |v| = 6.975 spans only 14.1421 - 13.95 = 0.192 mm, under the 0.225 mm minimum, so those two
// are dropped: 30 lines. Their total length is 30*14.1421 - 2*sum(|v|) where sum over the 30
// kept lines of |v| is 101.25, giving 424.264 - 202.5 = 221.764 mm (ideal 222.22).
void test_solid_square_at_forty_five_degrees() {
  const std::vector<Polygon> region{rect(0.0, 0.0, 10.0, 10.0)};
  const std::vector<InfillLine> lines = generate_infill(region, settings(kWidth, 1.0, 45.0));

  CHECK_EQ(lines.size(), std::size_t{30});
  CHECK_NEAR(total_length_mm(lines), 221.764, 1e-3);

  const double ideal = 100.0 / kWidth;
  CHECK(std::fabs(total_length_mm(lines) - ideal) / ideal < 0.02);

  for (const InfillLine& line : lines) {
    CHECK(inside_or_on(region[0], line.a));
    CHECK(inside_or_on(region[0], line.b));
    CHECK(line_length_mm(line) >= 0.5 * kWidth);
  }
}

// A 20 mm square with a 4 mm square hole, w = 0.45 at density 0.5 (spacing 0.9), 0 degrees.
// Lattice y = 0.45 + k*0.9 gives 22 scan lines in [0, 20]; the four at y = 8.55, 9.45, 10.35
// and 11.25 fall inside the hole's 8..12 band and split into two spans each. So 26 lines, and
// the total drops from 22*20 = 440 by the 4 mm of hole on each of four lines: 424 mm.
void test_hole_is_never_crossed() {
  std::vector<Polygon> region{rect(0.0, 0.0, 20.0, 20.0), rect(8.0, 8.0, 12.0, 12.0)};
  orient_rings(region);
  const Polygon& outer = region[0];
  const Polygon& hole = region[1];
  CHECK(signed_area_mm2(outer) > 0.0);
  CHECK(signed_area_mm2(hole) < 0.0);

  const std::vector<InfillLine> lines = generate_infill(region, settings(kWidth, 0.5, 0.0));

  CHECK_EQ(lines.size(), std::size_t{26});
  CHECK_NEAR(total_length_mm(lines), 424.0, 1e-6);

  for (const InfillLine& line : lines) {
    // Midpoint, plus the quarter points, must all be outside the hole.
    CHECK(!point_in_ring(hole, along(line, 0.5)));
    CHECK(!point_in_ring(hole, along(line, 0.25)));
    CHECK(!point_in_ring(hole, along(line, 0.75)));
    CHECK(inside_or_on(outer, line.a));
    CHECK(inside_or_on(outer, line.b));
    // Endpoints may sit exactly on a hole wall - that is where a split span stops - but never
    // in its interior.
    CHECK(!strictly_inside(hole, line.a));
    CHECK(!strictly_inside(hole, line.b));
  }

  // The scan line at y = 8.55 is the first one split by the hole and is walked backwards
  // (lattice index 9 is odd), so both of its spans must run -x and arrive in descending order.
  std::vector<InfillLine> split;
  for (const InfillLine& line : lines) {
    if (line.a.y == mm_to_units(8.55) && line.b.y == mm_to_units(8.55)) {
      split.push_back(line);
    }
  }
  CHECK_EQ(split.size(), std::size_t{2});
  if (split.size() == 2) {
    CHECK(split[0].a.x > split[0].b.x);
    CHECK(split[1].a.x > split[1].b.x);
    CHECK(split[0].b.x >= split[1].a.x);
    CHECK_EQ(split[0].a.x, mm_to_units(20.0));
    CHECK_EQ(split[0].b.x, mm_to_units(12.0));
    CHECK_EQ(split[1].a.x, mm_to_units(8.0));
    CHECK_EQ(split[1].b.x, mm_to_units(0.0));
  }
}

// Spans narrower than half an extrusion width are blobs, not extrusions, and must be dropped.
// A 0.1 mm x 10 mm sliver scanned at 0 degrees gets 22 scan lines, every one of them only
// 0.1 mm long - under the 0.225 mm minimum - so nothing survives.
void test_sliver_spans_are_dropped() {
  const std::vector<Polygon> region{rect(0.0, 0.0, 0.1, 10.0)};
  const std::vector<InfillLine> lines = generate_infill(region, settings(kWidth, 1.0, 0.0));
  CHECK(lines.empty());

  // Scanned at 90 degrees the sliver's whole extent along the scan axis is 0.1 mm, which is
  // less than the half-spacing offset of the first lattice line, so there is no scan line at
  // all. Either way the answer is nothing, never a blob.
  const std::vector<InfillLine> rotated = generate_infill(region, settings(kWidth, 1.0, 90.0));
  CHECK(rotated.empty());
}

void test_degenerate_settings() {
  const std::vector<Polygon> region{rect(0.0, 0.0, 10.0, 10.0)};

  CHECK(generate_infill({}, settings(kWidth, 1.0, 0.0)).empty());
  CHECK(generate_infill(region, settings(kWidth, 0.0, 0.0)).empty());
  CHECK(generate_infill(region, settings(kWidth, -0.5, 0.0)).empty());
  CHECK(generate_infill(region, settings(0.0, 1.0, 0.0)).empty());
  CHECK(generate_infill(region, settings(-0.45, 1.0, 0.0)).empty());

  // Density above 1 clamps to solid.
  CHECK(generate_infill(region, settings(kWidth, 4.0, 0.0)) ==
        generate_infill(region, settings(kWidth, 1.0, 0.0)));

  // A two-point ring bounds no area.
  const std::vector<Polygon> degenerate{Polygon{Point2{0, 0}, Point2{mm_to_units(5.0), 0}}};
  CHECK(generate_infill(degenerate, settings(kWidth, 1.0, 0.0)).empty());

  // An empty ring mixed in with a real one must not disturb the real one.
  const std::vector<Polygon> mixed{Polygon{}, rect(0.0, 0.0, 10.0, 10.0)};
  CHECK(generate_infill(mixed, settings(kWidth, 1.0, 0.0)) ==
        generate_infill(region, settings(kWidth, 1.0, 0.0)));
}

// Angles outside [0, 90) need no special handling. 180 degrees is 0 degrees transposed in sign
// and gives the same 22 lines. -45 and 135 degrees both put the diamond's long axis along the
// scan normal, v in [0, 14.1421] (or its negation): the lattice fits 31 lines there, and the
// outermost span is 2*0.225 = 0.45 mm, just over the 0.225 mm minimum, so none are dropped.
// Their total length is 2*(sum of min(v_i, 14.1421 - v_i)) = 222.214 mm.
void test_angle_wrapping() {
  const std::vector<Polygon> region{rect(0.0, 0.0, 10.0, 10.0)};
  CHECK_EQ(generate_infill(region, settings(kWidth, 1.0, 180.0)).size(), std::size_t{22});
  CHECK_NEAR(total_length_mm(generate_infill(region, settings(kWidth, 1.0, 180.0))), 220.0, 1e-6);

  const std::vector<InfillLine> minus45 = generate_infill(region, settings(kWidth, 1.0, -45.0));
  CHECK_EQ(minus45.size(), std::size_t{31});
  CHECK_NEAR(total_length_mm(minus45), 222.214, 1e-3);

  CHECK_EQ(generate_infill(region, settings(kWidth, 1.0, 135.0)).size(), std::size_t{31});
}

// Coordinates far from the origin: the pattern is anchored to the origin lattice, so the line
// count can shift by one, but the geometry must stay valid and inside the region.
void test_far_from_origin() {
  const std::vector<Polygon> region{rect(250.0, 250.0, 260.0, 260.0)};
  const std::vector<InfillLine> lines = generate_infill(region, settings(kWidth, 1.0, 0.0));
  CHECK(lines.size() >= std::size_t{21});
  CHECK(lines.size() <= std::size_t{23});
  for (const InfillLine& line : lines) {
    CHECK_NEAR(line_length_mm(line), 10.0, 1e-6);
    CHECK(inside_or_on(region[0], line.a));
    CHECK(inside_or_on(region[0], line.b));
  }
}

void test_infill_is_deterministic() {
  std::vector<Polygon> region{rect(0.0, 0.0, 20.0, 20.0), rect(8.0, 8.0, 12.0, 12.0)};
  orient_rings(region);
  const InfillSettings s = settings(kWidth, 0.33, 37.0);
  const std::vector<InfillLine> first = generate_infill(region, s);
  const std::vector<InfillLine> second = generate_infill(region, s);
  CHECK(!first.empty());
  CHECK(first == second);
}

}  // namespace

int main() {
  test_solid_square_at_zero_degrees();
  test_density_sets_spacing();
  test_solid_square_at_ninety_degrees();
  test_solid_square_at_forty_five_degrees();
  test_hole_is_never_crossed();
  test_sliver_spans_are_dropped();
  test_degenerate_settings();
  test_angle_wrapping();
  test_far_from_origin();
  test_infill_is_deterministic();
  return slicem_test::summary("slicem_infill_tests");
}
