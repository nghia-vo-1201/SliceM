#include "gcode/slice_to_gcode.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

#include "gcode/gcode_writer.h"
#include "geometry/offset.h"
#include "geometry/polygon.h"
#include "slicing/infill.h"
#include "slicing/perimeters.h"
#include "slicem_version.h"

namespace slicem {
namespace {

// A setpoint difference below this is the same setpoint, so no mid-print command is emitted
// for it. Matches the tolerance the writer uses to recognise "heater off".
constexpr double kTempEpsilonC = 0.5;

// Vertical gap between the finished part and the toolhead during the end-of-job park travel.
constexpr double kEndLiftClearanceMm = 1.0;

// Extra Z the nozzle is taken to after parking, purely so the part is easy to reach.
constexpr double kEndAccessRaiseMm = 30.0;

enum class Feature { kSkirt, kPerimeter, kExternalPerimeter, kSolidInfill, kInternalInfill };

const char* feature_name(Feature feature) {
  switch (feature) {
    case Feature::kSkirt:
      return "Skirt";
    case Feature::kPerimeter:
      return "Perimeter";
    case Feature::kExternalPerimeter:
      return "External perimeter";
    case Feature::kSolidInfill:
      return "Solid infill";
    case Feature::kInternalInfill:
      return "Internal infill";
  }
  return "Custom";
}

// The order features are printed in within a layer is decided here and nowhere else.
//
// Perimeters run innermost first and finish on the external perimeter, then infill follows.
// Ending a layer on the external perimeter is the usual quality choice; this first version
// keeps one uniform order so revisiting it is a single-function edit.
std::vector<std::size_t> perimeter_emission_order(std::size_t loop_count) {
  std::vector<std::size_t> order;
  order.reserve(loop_count);
  for (std::size_t i = loop_count; i > 0; --i) {
    order.push_back(i - 1);
  }
  return order;
}

double feature_speed(Feature feature, bool first_layer, const GcodeSettings& settings) {
  if (first_layer) {
    return settings.first_layer_speed_mm_s;
  }
  switch (feature) {
    case Feature::kSkirt:
      return settings.first_layer_speed_mm_s;
    case Feature::kExternalPerimeter:
      return settings.external_perimeter_speed_mm_s;
    case Feature::kPerimeter:
      return settings.perimeter_speed_mm_s;
    case Feature::kSolidInfill:
      return settings.solid_infill_speed_mm_s;
    case Feature::kInternalInfill:
      return settings.infill_speed_mm_s;
  }
  return settings.perimeter_speed_mm_s;
}

// Only ever lowers a speed. A feedrate reduced to stay inside a machine limit still prints the
// intended path, which is why capping is legitimate here where clamping a coordinate is not.
double capped_speed(double requested, const PrinterProfile& profile,
                    const GcodeSettings& settings) {
  double speed = std::fmin(requested, profile.max_print_speed_mm_s);
  speed = std::fmin(speed, std::fmin(profile.max_feedrate_x_mm_s, profile.max_feedrate_y_mm_s));
  const double mm3_per_mm = settings.layer_height_mm * settings.extrusion_width_mm;
  if (profile.max_volumetric_speed_mm3_s > 0.0 && mm3_per_mm > 0.0) {
    speed = std::fmin(speed, profile.max_volumetric_speed_mm3_s / mm3_per_mm);
  }
  return speed;
}

bool layer_is_solid(int index, int layer_count, const GcodeSettings& settings) {
  return index < settings.solid_layers || index >= layer_count - settings.solid_layers;
}

double layer_infill_angle(int index) {
  return index % 2 == 0 ? 45.0 : 135.0;
}

std::vector<Polygon> build_skirt(const std::vector<Polygon>& contours,
                                 const GcodeSettings& settings) {
  std::vector<Polygon> skirt;
  if (settings.skirt_loops <= 0) {
    return skirt;
  }
  std::vector<Polygon> rings = contours;
  orient_rings(rings);
  for (int loop = 0; loop < settings.skirt_loops; ++loop) {
    const double delta = settings.skirt_distance_mm + 0.5 * settings.extrusion_width_mm +
                         static_cast<double>(loop) * settings.extrusion_width_mm;
    for (const Polygon& ring : offset_rings(rings, delta)) {
      // Growing the solid shrinks its holes, so only the outward boundaries are skirt.
      if (signed_area_mm2(ring) > 0.0) {
        skirt.push_back(ring);
      }
    }
  }
  return skirt;
}

struct Bounds2 {
  bool valid = false;
  int64_t min_x = 0;
  int64_t max_x = 0;
  int64_t min_y = 0;
  int64_t max_y = 0;

  void add(const Point2& p) {
    if (!valid) {
      min_x = max_x = p.x;
      min_y = max_y = p.y;
      valid = true;
      return;
    }
    min_x = std::min(min_x, p.x);
    max_x = std::max(max_x, p.x);
    min_y = std::min(min_y, p.y);
    max_y = std::max(max_y, p.y);
  }
};

Bounds2 slice_bounds(const SliceResult& sliced) {
  Bounds2 bounds;
  for (const Layer& layer : sliced.layers) {
    for (const Polygon& contour : layer.contours) {
      for (const Point2& point : contour) {
        bounds.add(point);
      }
    }
  }
  return bounds;
}

class JobEmitter {
 public:
  JobEmitter(GcodeWriter& writer, const PrinterProfile& profile, const GcodeSettings& settings,
             double offset_x_mm, double offset_y_mm, GcodeStats& stats)
      : w_(writer),
        profile_(profile),
        settings_(settings),
        offset_x_mm_(offset_x_mm),
        offset_y_mm_(offset_y_mm),
        cross_section_mm2_(filament_cross_section_mm2(profile)),
        stats_(stats) {}

  void set_layer(double print_z_mm) { print_z_mm_ = print_z_mm; }

  bool emit_rings(const std::vector<Polygon>& rings, Feature feature, double speed) {
    if (rings.empty()) {
      return true;
    }
    mark(feature);
    for (const Polygon& ring : rings) {
      if (!emit_closed_ring(ring, speed)) {
        return false;
      }
    }
    return true;
  }

  bool emit_lines(const std::vector<InfillLine>& lines, Feature feature, double speed) {
    if (lines.empty()) {
      return true;
    }
    mark(feature);
    for (const InfillLine& line : lines) {
      if (!(have_last_ && line.a == last_point_ && !w_.retracted())) {
        if (!start_path(line.a)) {
          return false;
        }
      }
      if (!extrude_to(line.b, speed)) {
        return false;
      }
    }
    return true;
  }

 private:
  void mark(Feature feature) {
    w_.marker(std::string("TYPE:") + feature_name(feature));
    std::ostringstream width;
    width << "WIDTH:" << settings_.extrusion_width_mm;
    w_.marker(width.str());
  }

  double to_x(const Point2& p) const { return units_to_mm(p.x) + offset_x_mm_; }
  double to_y(const Point2& p) const { return units_to_mm(p.y) + offset_y_mm_; }

  bool start_path(const Point2& target) {
    const double x = to_x(target);
    const double y = to_y(target);
    if (!w_.position_known()) {
      // Nothing has been deposited yet, so there is nothing to collide with and nothing to
      // ooze onto: the first approach needs neither a retraction nor a hop.
      if (!w_.travel_to(x, y, travel_speed()) || !w_.unretract()) {
        return false;
      }
      have_last_ = true;
      last_point_ = target;
      return true;
    }
    const double distance = std::hypot(x - w_.x_mm(), y - w_.y_mm());
    if (distance > settings_.retract_before_travel_mm) {
      if (!w_.retract()) {
        return false;
      }
      const double hop_z = print_z_mm_ + profile_.retract_lift_mm;
      const bool hop = hop_z <= profile_.max_print_height_mm;
      if (hop && !w_.travel_to_z(hop_z, profile_.travel_speed_z_mm_s)) {
        return false;
      }
      if (!w_.travel_to(x, y, travel_speed())) {
        return false;
      }
      if (hop && !w_.travel_to_z(print_z_mm_, profile_.travel_speed_z_mm_s)) {
        return false;
      }
    } else if (!w_.travel_to(x, y, travel_speed())) {
      return false;
    }
    if (!w_.unretract()) {
      return false;
    }
    have_last_ = true;
    last_point_ = target;
    return true;
  }

  bool extrude_to(const Point2& target, double speed) {
    const double x = to_x(target);
    const double y = to_y(target);
    const double length = std::hypot(x - w_.x_mm(), y - w_.y_mm());
    if (length <= 0.0) {
      return true;
    }
    const double volume = length * settings_.layer_height_mm * settings_.extrusion_width_mm;
    if (!w_.extrude_to(x, y, volume / cross_section_mm2_, speed)) {
      return false;
    }
    stats_.extruded_path_length_mm += length;
    stats_.deposited_volume_mm3 += volume;
    if (!bbox_valid_) {
      stats_.deposit_min_x_mm = stats_.deposit_max_x_mm = x;
      stats_.deposit_min_y_mm = stats_.deposit_max_y_mm = y;
      bbox_valid_ = true;
    }
    stats_.deposit_min_x_mm = std::fmin(stats_.deposit_min_x_mm, x);
    stats_.deposit_max_x_mm = std::fmax(stats_.deposit_max_x_mm, x);
    stats_.deposit_min_y_mm = std::fmin(stats_.deposit_min_y_mm, y);
    stats_.deposit_max_y_mm = std::fmax(stats_.deposit_max_y_mm, y);
    have_last_ = true;
    last_point_ = target;
    return true;
  }

  bool emit_closed_ring(const Polygon& ring, double speed) {
    if (ring.size() < 3) {
      return true;
    }
    if (!start_path(ring.front())) {
      return false;
    }
    for (std::size_t i = 1; i < ring.size(); ++i) {
      if (!extrude_to(ring[i], speed)) {
        return false;
      }
    }
    return extrude_to(ring.front(), speed);
  }

  double travel_speed() const { return profile_.travel_speed_mm_s; }

  GcodeWriter& w_;
  const PrinterProfile& profile_;
  const GcodeSettings& settings_;
  double offset_x_mm_ = 0.0;
  double offset_y_mm_ = 0.0;
  double cross_section_mm2_ = 1.0;
  GcodeStats& stats_;
  double print_z_mm_ = 0.0;
  bool have_last_ = false;
  Point2 last_point_{};
  bool bbox_valid_ = false;
};

void emit_header_comments(GcodeWriter& w, const PrinterProfile& profile,
                          const GcodeSettings& settings) {
  std::ostringstream line;
  w.comment(std::string("generated by SliceM ") + SLICEM_VERSION);
  w.comment("printer: " + profile.name + " (legacy firmware family)");
  line << "nozzle " << profile.nozzle_diameter_mm << " mm, filament "
       << profile.filament_diameter_mm << " mm";
  w.comment(line.str());
  line.str("");
  line << "layer height " << settings.layer_height_mm << " mm, extrusion width "
       << settings.extrusion_width_mm << " mm";
  w.comment(line.str());
  line.str("");
  line << "perimeters " << settings.perimeter_count << ", infill density "
       << settings.infill_density << ", solid layers top and bottom " << settings.solid_layers;
  w.comment(line.str());
  line.str("");
  line << "nozzle " << profile.nozzle_temp_first_layer_c << " C first layer, "
       << profile.nozzle_temp_other_layers_c << " C after; bed "
       << profile.bed_temp_first_layer_c << " C";
  w.comment(line.str());
  w.comment("extrusion is relative (M83); coordinates absolute (G90)");
  w.blank_line();
}

// Initialisation, in the order research/gcode-dialect-legacy-mk3.md section 2 requires.
//
// Deliberately absent, because no researched value exists for them: M73 (needs a print-time
// estimate SliceM cannot produce yet), M115 (declaring an unverified firmware version makes the
// printer stall and prompt for an upgrade), M907 (motor current is a tuning value this project
// has not established), and the windowed form of G80 (the probeable sub-area is undocumented,
// so the full default mesh is used). The nozzle is primed by the skirt rather than by a purge
// line in front of the bed, which would sit outside the build volume.
bool emit_start_sequence(GcodeWriter& w, const PrinterProfile& profile) {
  w.marker("TYPE:Custom");
  return w.set_machine_limits() && w.emit_compatibility_checks() && w.set_units_and_modes() &&
         w.set_temperature(Heater::kNozzle, profile.nozzle_temp_first_layer_c, false) &&
         w.set_temperature(Heater::kBed, profile.bed_temp_first_layer_c, false) &&
         w.set_temperature(Heater::kBed, profile.bed_temp_first_layer_c, true) &&
         w.set_temperature(Heater::kNozzle, profile.nozzle_temp_first_layer_c, true) &&
         w.home_axes() && w.mesh_bed_level() && w.reset_extruder() && w.set_flow_percent(100.0) &&
         w.disable_linear_advance() && w.set_fan_percent(0);
}

// End of job, in the order research/gcode-dialect-legacy-mk3.md section 4 requires: retract,
// fan off, lift clear of the part, park, dwell, reset what the start sequence set, heaters off,
// steppers off.
bool emit_end_sequence(GcodeWriter& w, const PrinterProfile& profile, double top_z_mm) {
  w.blank_line();
  w.marker("TYPE:Custom");
  if (!w.retract() || !w.set_fan_percent(0)) {
    return false;
  }
  const double lift_z = top_z_mm + kEndLiftClearanceMm;
  if (!w.travel_to_z(lift_z, profile.travel_speed_z_mm_s)) {
    return false;
  }
  // Nozzle to the left edge with the bed rolled forward, which on this machine presents the
  // part to the user and keeps the hot nozzle off it.
  if (!w.travel_to(0.0, profile.bed_size_y_mm - 10.0, profile.travel_speed_mm_s)) {
    return false;
  }
  const double raise_z = std::fmin(top_z_mm + kEndAccessRaiseMm, profile.max_print_height_mm);
  if (raise_z > lift_z && !w.travel_to_z(raise_z, profile.travel_speed_z_mm_s)) {
    return false;
  }
  return w.dwell_until_moves_finish() && w.set_flow_percent(100.0) &&
         w.disable_linear_advance() && w.set_temperature(Heater::kNozzle, 0.0, false) &&
         w.set_temperature(Heater::kBed, 0.0, false) && w.set_fan_percent(0) &&
         w.disable_steppers();
}

}  // namespace

GcodeStats write_gcode(const SliceResult& sliced, const PrinterProfile& profile,
                       const GcodeSettings& settings, std::ostream& out) {
  GcodeStats stats;

  if (!sliced.ok) {
    stats.error = "slicing failed: " + sliced.error;
    return stats;
  }
  if (sliced.layers.empty()) {
    stats.error = "nothing to print: the slice has no layers";
    return stats;
  }
  if (profile.family != FirmwareFamily::kLegacy) {
    stats.error = "only the legacy MK3/MK3S dialect is implemented";
    return stats;
  }
  if (!(settings.layer_height_mm >= profile.min_layer_height_mm &&
        settings.layer_height_mm <= profile.max_layer_height_mm)) {
    stats.error = "layer height is outside the profile's range";
    return stats;
  }
  if (!(settings.extrusion_width_mm > 0.0)) {
    stats.error = "extrusion width must be positive";
    return stats;
  }
  if (settings.perimeter_count < 1) {
    stats.error = "at least one perimeter is required";
    return stats;
  }
  if (!(settings.infill_density >= 0.0 && settings.infill_density <= 1.0)) {
    stats.error = "infill density must be between 0 and 1";
    return stats;
  }
  if (settings.fan_percent < 0 || settings.fan_percent > 100) {
    stats.error = "fan percentage must be between 0 and 100";
    return stats;
  }
  if (settings.solid_layers < 0) {
    stats.error = "solid layer count cannot be negative";
    return stats;
  }

  const int layer_count = static_cast<int>(sliced.layers.size());
  const double top_z_mm = static_cast<double>(layer_count) * settings.layer_height_mm;
  // The end sequence has to lift clear of the finished part, and every z-hop sits one lift
  // above its layer. Both have to fit, and that is checked before a single line is written.
  if (top_z_mm + std::fmax(kEndLiftClearanceMm, profile.retract_lift_mm) >
      profile.max_print_height_mm) {
    stats.error = "the part is too tall to park and lift clear of inside the build volume";
    return stats;
  }

  const Bounds2 bounds = slice_bounds(sliced);
  if (!bounds.valid) {
    stats.error = "nothing to print: the slice has no contours";
    return stats;
  }

  double offset_x_mm = 0.0;
  double offset_y_mm = 0.0;
  if (settings.center_on_bed) {
    offset_x_mm = 0.5 * profile.bed_size_x_mm -
                  0.5 * (units_to_mm(bounds.min_x) + units_to_mm(bounds.max_x));
    offset_y_mm = 0.5 * profile.bed_size_y_mm -
                  0.5 * (units_to_mm(bounds.min_y) + units_to_mm(bounds.max_y));
  }

  // The skirt is the outermost thing printed, so the placed model plus the skirt offset is what
  // has to fit on the bed. Failing here rather than at the first refused move keeps a
  // half-written file off disk.
  const double margin = settings.skirt_loops > 0
                            ? settings.skirt_distance_mm +
                                  static_cast<double>(settings.skirt_loops) *
                                      settings.extrusion_width_mm
                            : 0.0;
  const double placed_min_x = units_to_mm(bounds.min_x) + offset_x_mm - margin;
  const double placed_max_x = units_to_mm(bounds.max_x) + offset_x_mm + margin;
  const double placed_min_y = units_to_mm(bounds.min_y) + offset_y_mm - margin;
  const double placed_max_y = units_to_mm(bounds.max_y) + offset_y_mm + margin;
  if (placed_min_x < 0.0 || placed_min_y < 0.0 || placed_max_x > profile.bed_size_x_mm ||
      placed_max_y > profile.bed_size_y_mm) {
    stats.error = "the model and its skirt do not fit inside the build volume";
    return stats;
  }

  GcodeWriter w(out, profile);
  emit_header_comments(w, profile, settings);
  if (!emit_start_sequence(w, profile)) {
    stats.error = w.error();
    return stats;
  }

  JobEmitter emitter(w, profile, settings, offset_x_mm, offset_y_mm, stats);

  for (int i = 0; i < layer_count; ++i) {
    const Layer& layer = sliced.layers[static_cast<std::size_t>(i)];
    const bool first_layer = i == 0;
    const double print_z_mm = static_cast<double>(i + 1) * settings.layer_height_mm;

    w.blank_line();
    if (!first_layer && !w.retract()) {
      stats.error = w.error();
      return stats;
    }
    if (!w.layer_change(print_z_mm, settings.layer_height_mm)) {
      stats.error = w.error();
      return stats;
    }
    if (i == 1) {
      if (std::fabs(profile.nozzle_temp_other_layers_c - profile.nozzle_temp_first_layer_c) >
              kTempEpsilonC &&
          !w.set_temperature(Heater::kNozzle, profile.nozzle_temp_other_layers_c, false)) {
        stats.error = w.error();
        return stats;
      }
      if (std::fabs(profile.bed_temp_other_layers_c - profile.bed_temp_first_layer_c) >
              kTempEpsilonC &&
          !w.set_temperature(Heater::kBed, profile.bed_temp_other_layers_c, false)) {
        stats.error = w.error();
        return stats;
      }
      if (!w.set_fan_percent(settings.fan_percent)) {
        stats.error = w.error();
        return stats;
      }
    }
    emitter.set_layer(print_z_mm);
    stats.print_z_max_mm = print_z_mm;

    if (first_layer) {
      const std::vector<Polygon> skirt = build_skirt(layer.contours, settings);
      if (!emitter.emit_rings(skirt, Feature::kSkirt,
                              capped_speed(feature_speed(Feature::kSkirt, true, settings), profile,
                                           settings))) {
        stats.error = w.error();
        return stats;
      }
    }

    PerimeterSettings perimeter_settings;
    perimeter_settings.extrusion_width_mm = settings.extrusion_width_mm;
    perimeter_settings.perimeter_count = settings.perimeter_count;
    const PerimeterResult perimeters = generate_perimeters(layer.contours, perimeter_settings);

    for (const std::size_t index : perimeter_emission_order(perimeters.loops.size())) {
      const Feature feature = index == 0 ? Feature::kExternalPerimeter : Feature::kPerimeter;
      const double speed = capped_speed(feature_speed(feature, first_layer, settings), profile,
                                        settings);
      if (!emitter.emit_rings(perimeters.loops[index], feature, speed)) {
        stats.error = w.error();
        return stats;
      }
    }

    if (!perimeters.infill_region.empty()) {
      const bool solid = layer_is_solid(i, layer_count, settings);
      InfillSettings infill_settings;
      infill_settings.pattern = InfillPattern::kRectilinear;
      infill_settings.extrusion_width_mm = settings.extrusion_width_mm;
      infill_settings.density = solid ? 1.0 : settings.infill_density;
      infill_settings.angle_degrees = layer_infill_angle(i);
      if (infill_settings.density > 0.0) {
        const Feature feature = solid ? Feature::kSolidInfill : Feature::kInternalInfill;
        const double speed = capped_speed(feature_speed(feature, first_layer, settings), profile,
                                          settings);
        const std::vector<InfillLine> lines =
            generate_infill(perimeters.infill_region, infill_settings);
        if (!emitter.emit_lines(lines, feature, speed)) {
          stats.error = w.error();
          return stats;
        }
      }
    }
  }

  if (!emit_end_sequence(w, profile, stats.print_z_max_mm)) {
    stats.error = w.error();
    return stats;
  }

  stats.layers = w.layer_change_count();
  stats.filament_used_mm = stats.deposited_volume_mm3 / filament_cross_section_mm2(profile);
  stats.filament_used_cm3 = stats.deposited_volume_mm3 / 1000.0;

  w.blank_line();
  std::ostringstream line;
  line << "filament used [mm] " << stats.filament_used_mm;
  w.comment(line.str());
  line.str("");
  line << "filament used [cm3] " << stats.filament_used_cm3;
  w.comment(line.str());
  line.str("");
  line << "layers " << stats.layers << ", top z " << stats.print_z_max_mm << " mm";
  w.comment(line.str());

  if (!w.ok()) {
    stats.error = w.error();
    return stats;
  }
  stats.ok = true;
  return stats;
}

GcodeStats write_gcode_file(const SliceResult& sliced, const PrinterProfile& profile,
                            const GcodeSettings& settings, const std::string& path) {
  GcodeStats stats;
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    stats.error = "cannot open " + path + " for writing";
    return stats;
  }
  stats = write_gcode(sliced, profile, settings, out);
  out.flush();
  if (!out) {
    stats.ok = false;
    if (stats.error.empty()) {
      stats.error = "writing " + path + " failed";
    }
  }
  return stats;
}

}  // namespace slicem
