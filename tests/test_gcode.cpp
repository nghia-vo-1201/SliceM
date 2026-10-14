#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "gcode/gcode_writer.h"
#include "gcode/printer_profile.h"
#include "gcode/slice_to_gcode.h"
#include "geometry/offset.h"
#include "geometry/polygon.h"
#include "slicing/infill.h"
#include "slicing/perimeters.h"
#include "slicing/slicer.h"

using namespace slicem;

namespace {

TriangleMesh make_box(double x0, double y0, double z0, double sx, double sy, double sz) {
  TriangleMesh mesh;
  const double x1 = x0 + sx;
  const double y1 = y0 + sy;
  const double z1 = z0 + sz;
  const double corners[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                                {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
  for (const auto& c : corners) {
    mesh.vertices.push_back(Point3{mm_to_units(c[0]), mm_to_units(c[1]), mm_to_units(c[2])});
  }
  const int faces[12][3] = {{0, 3, 2}, {0, 2, 1}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                            {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
  for (const auto& f : faces) {
    Triangle tri;
    tri.v[0] = f[0];
    tri.v[1] = f[1];
    tri.v[2] = f[2];
    mesh.triangles.push_back(tri);
  }
  return mesh;
}

struct Word {
  bool present = false;
  double value = 0.0;
  bool finite = true;
};

Word find_word(const std::string& line, char letter) {
  Word word;
  for (std::size_t i = 0; i < line.size(); ++i) {
    if (line[i] != letter) {
      continue;
    }
    if (i != 0 && line[i - 1] != ' ') {
      continue;
    }
    const char* start = line.c_str() + i + 1;
    char* end = nullptr;
    const double value = std::strtod(start, &end);
    if (end == start) {
      continue;
    }
    word.present = true;
    word.value = value;
    word.finite = std::isfinite(value);
    return word;
  }
  return word;
}

struct Parsed {
  std::vector<std::string> command_lines;
  int layer_change_markers = 0;
  int z_markers = 0;
  double extrusion_e_mm = 0.0;
  double retraction_e_mm = 0.0;
  int bad_e_values = 0;
  int negative_extrusions = 0;
  int moves_outside_volume = 0;
  int z_below_zero = 0;
  double max_z_mm = 0.0;
  double max_xy_feedrate_mm_s = 0.0;
  int first_extrusion_index = -1;
  int g21_index = -1;
  int g90_index = -1;
  int m83_index = -1;
  int m109_index = -1;
  int m190_index = -1;
  bool nan_or_inf_text = false;
};

Parsed parse(const std::string& text, const PrinterProfile& profile) {
  Parsed parsed;
  std::istringstream in(text);
  std::string line;
  int index = 0;
  double z = 0.0;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.rfind(";LAYER_CHANGE", 0) == 0) {
      ++parsed.layer_change_markers;
      continue;
    }
    if (line.rfind(";Z:", 0) == 0) {
      ++parsed.z_markers;
      continue;
    }
    if (line.empty() || line[0] == ';') {
      continue;
    }
    const std::string command = line.substr(0, line.find(' '));
    parsed.command_lines.push_back(line);
    // Only executable lines are scanned: "infill" in a feature marker is not an infinity.
    if (line.find("nan") != std::string::npos || line.find("inf") != std::string::npos) {
      parsed.nan_or_inf_text = true;
    }
    if (command == "G21" && parsed.g21_index < 0) {
      parsed.g21_index = index;
    } else if (command == "G90" && parsed.g90_index < 0) {
      parsed.g90_index = index;
    } else if (command == "M83" && parsed.m83_index < 0) {
      parsed.m83_index = index;
    } else if (command == "M109" && parsed.m109_index < 0) {
      parsed.m109_index = index;
    } else if (command == "M190" && parsed.m190_index < 0) {
      parsed.m190_index = index;
    } else if (command == "G0" || command == "G1") {
      const Word x = find_word(line, 'X');
      const Word y = find_word(line, 'Y');
      const Word zw = find_word(line, 'Z');
      const Word e = find_word(line, 'E');
      const Word f = find_word(line, 'F');
      if (zw.present) {
        if (!zw.finite || zw.value < 0.0 || zw.value > profile.max_print_height_mm) {
          ++parsed.z_below_zero;
        }
        z = zw.value;
        parsed.max_z_mm = std::fmax(parsed.max_z_mm, z);
      }
      if (x.present || y.present) {
        if (x.present && (!x.finite || x.value < 0.0 || x.value > profile.bed_size_x_mm)) {
          ++parsed.moves_outside_volume;
        }
        if (y.present && (!y.finite || y.value < 0.0 || y.value > profile.bed_size_y_mm)) {
          ++parsed.moves_outside_volume;
        }
        if (f.present && f.finite) {
          parsed.max_xy_feedrate_mm_s = std::fmax(parsed.max_xy_feedrate_mm_s, f.value / 60.0);
        }
      }
      if (e.present) {
        if (!e.finite) {
          ++parsed.bad_e_values;
        } else if (x.present || y.present) {
          if (e.value <= 0.0) {
            ++parsed.negative_extrusions;
          }
          parsed.extrusion_e_mm += e.value;
          if (parsed.first_extrusion_index < 0) {
            parsed.first_extrusion_index = index;
          }
        } else if (e.value < 0.0) {
          parsed.retraction_e_mm += e.value;
        }
      }
    }
    ++index;
  }
  return parsed;
}

// Recomputes filament consumption from the engine primitives and the extrusion formula written
// out longhand, so a mistake in the emitter's bookkeeping or in its own formula shows up as a
// disagreement rather than cancelling out.
double independent_filament_mm(const SliceResult& sliced, const GcodeSettings& settings) {
  const double cross_section = 3.141592653589793 * 0.875 * 0.875;
  const int layer_count = static_cast<int>(sliced.layers.size());
  double length_mm = 0.0;

  for (int i = 0; i < layer_count; ++i) {
    const Layer& layer = sliced.layers[static_cast<std::size_t>(i)];

    if (i == 0) {
      std::vector<Polygon> rings = layer.contours;
      orient_rings(rings);
      for (int loop = 0; loop < settings.skirt_loops; ++loop) {
        const double delta = settings.skirt_distance_mm + 0.5 * settings.extrusion_width_mm +
                             static_cast<double>(loop) * settings.extrusion_width_mm;
        for (const Polygon& ring : offset_rings(rings, delta)) {
          if (signed_area_mm2(ring) > 0.0 && ring.size() >= 3) {
            length_mm += perimeter_mm(ring);
          }
        }
      }
    }

    PerimeterSettings perimeter_settings;
    perimeter_settings.extrusion_width_mm = settings.extrusion_width_mm;
    perimeter_settings.perimeter_count = settings.perimeter_count;
    const PerimeterResult perimeters = generate_perimeters(layer.contours, perimeter_settings);
    for (const std::vector<Polygon>& group : perimeters.loops) {
      for (const Polygon& ring : group) {
        if (ring.size() >= 3) {
          length_mm += perimeter_mm(ring);
        }
      }
    }

    if (perimeters.infill_region.empty()) {
      continue;
    }
    const bool solid =
        i < settings.solid_layers || i >= layer_count - settings.solid_layers;
    InfillSettings infill_settings;
    infill_settings.extrusion_width_mm = settings.extrusion_width_mm;
    infill_settings.density = solid ? 1.0 : settings.infill_density;
    infill_settings.angle_degrees = i % 2 == 0 ? 45.0 : 135.0;
    for (const InfillLine& l : generate_infill(perimeters.infill_region, infill_settings)) {
      length_mm += std::hypot(units_to_mm(l.b.x - l.a.x), units_to_mm(l.b.y - l.a.y));
    }
  }

  return length_mm * settings.layer_height_mm * settings.extrusion_width_mm / cross_section;
}

std::string last_command(const Parsed& parsed) {
  return parsed.command_lines.empty() ? std::string() : parsed.command_lines.back();
}

bool contains_command(const Parsed& parsed, const std::string& prefix) {
  for (const std::string& line : parsed.command_lines) {
    if (line.rfind(prefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

struct Job {
  SliceResult sliced;
  GcodeStats stats;
  std::string text;
  Parsed parsed;
};

Job slice_and_emit(const GcodeSettings& settings, const PrinterProfile& profile) {
  Job job;
  const TriangleMesh cube = make_box(0.0, 0.0, 0.0, 20.0, 20.0, 20.0);
  SliceSettings slice_settings;
  slice_settings.layer_height_mm = settings.layer_height_mm;
  job.sliced = slice_mesh(cube, slice_settings);
  std::ostringstream out;
  job.stats = write_gcode(job.sliced, profile, settings, out);
  job.text = out.str();
  job.parsed = parse(job.text, profile);
  return job;
}

void test_cube_job_is_structurally_sound() {
  const PrinterProfile profile = mk3s_profile();
  GcodeSettings settings;
  const Job job = slice_and_emit(settings, profile);

  CHECK(job.sliced.ok);
  CHECK(job.stats.ok);
  CHECK_EQ(job.stats.error, std::string());
  CHECK_EQ(job.sliced.layers.size(), static_cast<std::size_t>(100));

  // Every emitted coordinate inside the build volume, Z never below zero.
  CHECK_EQ(job.parsed.moves_outside_volume, 0);
  CHECK_EQ(job.parsed.z_below_zero, 0);
  CHECK(job.parsed.max_z_mm <= profile.max_print_height_mm);

  // Modes set explicitly before anything is extruded.
  CHECK(job.parsed.first_extrusion_index > 0);
  CHECK(job.parsed.g21_index >= 0 && job.parsed.g21_index < job.parsed.first_extrusion_index);
  CHECK(job.parsed.g90_index >= 0 && job.parsed.g90_index < job.parsed.first_extrusion_index);
  CHECK(job.parsed.m83_index >= 0 && job.parsed.m83_index < job.parsed.first_extrusion_index);

  // Both required waits happen before the first extrusion, bed before nozzle.
  CHECK(job.parsed.m190_index >= 0 && job.parsed.m190_index < job.parsed.first_extrusion_index);
  CHECK(job.parsed.m109_index >= 0 && job.parsed.m109_index < job.parsed.first_extrusion_index);
  CHECK(job.parsed.m190_index < job.parsed.m109_index);

  // Layer markers match the slice.
  CHECK_EQ(job.parsed.layer_change_markers, static_cast<int>(job.sliced.layers.size()));
  CHECK_EQ(job.parsed.z_markers, static_cast<int>(job.sliced.layers.size()));
  CHECK_EQ(job.stats.layers, static_cast<int>(job.sliced.layers.size()));
  CHECK_NEAR(job.stats.print_z_max_mm, 20.0, 1e-9);

  // No NaN, no infinity, no non-positive extrusion.
  CHECK_EQ(job.parsed.bad_e_values, 0);
  CHECK_EQ(job.parsed.negative_extrusions, 0);
  CHECK(!job.parsed.nan_or_inf_text);

  // Feedrates within the machine limit.
  CHECK(job.parsed.max_xy_feedrate_mm_s <= profile.max_feedrate_x_mm_s);

  // Fan off for layer 1, on afterwards.
  CHECK(contains_command(job.parsed, "M107"));
  CHECK(contains_command(job.parsed, "M106 S"));

  // Ends with heaters off and steppers disabled, in that order.
  CHECK(contains_command(job.parsed, "M104 S0"));
  CHECK(contains_command(job.parsed, "M140 S0"));
  CHECK_EQ(last_command(job.parsed).rfind("M84", 0), static_cast<std::size_t>(0));
}

void test_filament_matches_an_independent_calculation() {
  const PrinterProfile profile = mk3s_profile();
  GcodeSettings settings;
  const Job job = slice_and_emit(settings, profile);
  const double expected = independent_filament_mm(job.sliced, settings);

  CHECK(expected > 0.0);
  CHECK_NEAR(job.stats.filament_used_mm, expected, 0.02 * expected);
  CHECK_NEAR(job.parsed.extrusion_e_mm, expected, 0.02 * expected);

  // Retractions are whole multiples of the profile's retraction length and nothing else.
  const double retract_count = -job.parsed.retraction_e_mm / profile.retract_length_mm;
  CHECK_NEAR(retract_count, std::round(retract_count), 1e-6);
  CHECK(retract_count >= static_cast<double>(job.sliced.layers.size()));
}

void test_centering_keeps_the_skirt_on_the_bed() {
  const PrinterProfile profile = mk3s_profile();
  GcodeSettings settings;
  const Job job = slice_and_emit(settings, profile);

  // The cube is modelled at the origin; without placement its skirt would fall off the bed.
  CHECK(job.stats.deposit_min_x_mm > 0.0);
  CHECK(job.stats.deposit_min_y_mm > 0.0);
  CHECK_NEAR(0.5 * (job.stats.deposit_min_x_mm + job.stats.deposit_max_x_mm),
             0.5 * profile.bed_size_x_mm, 0.05);
  CHECK_NEAR(0.5 * (job.stats.deposit_min_y_mm + job.stats.deposit_max_y_mm),
             0.5 * profile.bed_size_y_mm, 0.05);

  GcodeSettings unplaced = settings;
  unplaced.center_on_bed = false;
  const Job refused = slice_and_emit(unplaced, profile);
  CHECK(!refused.stats.ok);
  CHECK(!refused.stats.error.empty());
}

void test_settings_outside_the_profile_are_refused() {
  const PrinterProfile profile = mk3s_profile();

  GcodeSettings too_thick;
  too_thick.layer_height_mm = 0.4;  // above the 0.4 nozzle's 0.25 mm maximum
  CHECK(!slice_and_emit(too_thick, profile).stats.ok);

  GcodeSettings no_perimeters;
  no_perimeters.perimeter_count = 0;
  CHECK(!slice_and_emit(no_perimeters, profile).stats.ok);

  GcodeSettings silly_density;
  silly_density.infill_density = 1.5;
  CHECK(!slice_and_emit(silly_density, profile).stats.ok);
}

void test_writer_refuses_rather_than_clamps() {
  const PrinterProfile profile = mk3s_profile();

  {
    std::ostringstream out;
    GcodeWriter w(out, profile);
    CHECK(w.travel_to(10.0, 10.0, 100.0));
    CHECK(!w.travel_to(profile.bed_size_x_mm + 1.0, 10.0, 100.0));
    CHECK(!w.ok());
    CHECK(out.str().find("X251") == std::string::npos);
  }
  {
    std::ostringstream out;
    GcodeWriter w(out, profile);
    CHECK(!w.travel_to_z(-0.1, 10.0));
    CHECK(!w.ok());
  }
  {
    std::ostringstream out;
    GcodeWriter w(out, profile);
    CHECK(!w.set_temperature(Heater::kNozzle, 320.0, true));
  }
  {
    std::ostringstream out;
    GcodeWriter w(out, profile);
    CHECK(!w.set_temperature(Heater::kNozzle, 120.0, true));
  }
  {
    std::ostringstream out;
    GcodeWriter w(out, profile);
    CHECK(w.set_temperature(Heater::kNozzle, 0.0, false));
    CHECK(w.ok());
  }
  {
    std::ostringstream out;
    GcodeWriter w(out, profile);
    CHECK(!w.travel_to(10.0, 10.0, profile.max_feedrate_x_mm_s + 1.0));
  }
  {
    std::ostringstream out;
    GcodeWriter w(out, profile);
    CHECK(w.travel_to(10.0, 10.0, 100.0));
    CHECK(!w.extrude_to(20.0, 10.0, std::nan(""), 50.0));
  }
  {
    std::ostringstream out;
    GcodeWriter w(out, profile);
    CHECK(w.travel_to(10.0, 10.0, 100.0));
    CHECK(!w.extrude_to(20.0, 10.0, -0.5, 50.0));
  }
  {
    std::ostringstream out;
    GcodeWriter w(out, profile);
    CHECK(w.travel_to(10.0, 10.0, 100.0));
    CHECK(w.retract());
    CHECK(!w.extrude_to(20.0, 10.0, 0.1, 50.0));
  }
  {
    // A Buddy-family profile must not be served the legacy dialect by accident.
    PrinterProfile buddy = mk3s_profile();
    buddy.family = FirmwareFamily::kBuddy;
    std::ostringstream out;
    GcodeWriter w(out, buddy);
    CHECK(!w.set_machine_limits());
    CHECK(!w.ok());
  }
}

void test_degenerate_input_is_reported_not_emitted() {
  const PrinterProfile profile = mk3s_profile();
  GcodeSettings settings;

  SliceResult empty;
  empty.ok = true;
  std::ostringstream out;
  const GcodeStats stats = write_gcode(empty, profile, settings, out);
  CHECK(!stats.ok);
  CHECK(!stats.error.empty());
  CHECK(out.str().empty());

  SliceResult failed;
  failed.ok = false;
  failed.error = "not watertight";
  std::ostringstream out2;
  const GcodeStats stats2 = write_gcode(failed, profile, settings, out2);
  CHECK(!stats2.ok);
  CHECK(out2.str().empty());

  // A layer whose contour is a degenerate two-point ring must not produce extrusion moves.
  SliceResult sliver;
  sliver.ok = true;
  Layer layer;
  layer.index = 0;
  layer.slice_z = mm_to_units(0.1);
  layer.contours.push_back(Polygon{Point2{mm_to_units(100.0), mm_to_units(100.0)},
                                   Point2{mm_to_units(100.5), mm_to_units(100.0)}});
  sliver.layers.push_back(layer);
  GcodeSettings no_skirt = settings;
  no_skirt.skirt_loops = 0;
  std::ostringstream out3;
  const GcodeStats stats3 = write_gcode(sliver, profile, no_skirt, out3);
  CHECK(stats3.ok);
  CHECK_NEAR(stats3.deposited_volume_mm3, 0.0, 1e-12);
  const Parsed parsed3 = parse(out3.str(), profile);
  CHECK_EQ(parsed3.moves_outside_volume, 0);
  CHECK_EQ(parsed3.bad_e_values, 0);
}

void test_clearance_values_are_absent() {
  const PrinterProfile profile = mk3s_profile();
  // Owned by the toolhead clearance model. Unset means the colour-group feature is unavailable,
  // which is the intended behaviour; a number appearing here would be an invented one.
  CHECK(!profile.toolhead_clearance_height_mm.has_value());
  CHECK(!profile.toolhead_clearance_radius_mm.has_value());
}

}  // namespace

int main() {
  test_cube_job_is_structurally_sound();
  test_filament_matches_an_independent_calculation();
  test_centering_keeps_the_skirt_on_the_bed();
  test_settings_outside_the_profile_are_refused();
  test_writer_refuses_rather_than_clamps();
  test_degenerate_input_is_reported_not_emitted();
  test_clearance_values_are_absent();
  return slicem_test::summary("gcode");
}
