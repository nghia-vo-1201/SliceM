#pragma once

#include <optional>
#include <string>

namespace slicem {

// The two firmware families take different dialects. Nothing may be carried from one to the
// other, so the family is part of the profile and every emitter asserts on it.
enum class FirmwareFamily { kLegacy, kBuddy };

struct PrinterProfile {
  std::string name;
  // Argument of the legacy family's M862.3 model check.
  std::string model_code;
  FirmwareFamily family = FirmwareFamily::kLegacy;

  double bed_size_x_mm = 0.0;
  double bed_size_y_mm = 0.0;
  double max_print_height_mm = 0.0;

  double nozzle_diameter_mm = 0.0;
  double filament_diameter_mm = 0.0;
  double min_layer_height_mm = 0.0;
  double max_layer_height_mm = 0.0;

  double max_feedrate_x_mm_s = 0.0;
  double max_feedrate_y_mm_s = 0.0;
  double max_feedrate_z_mm_s = 0.0;
  double max_feedrate_e_mm_s = 0.0;

  double max_accel_x_mm_s2 = 0.0;
  double max_accel_y_mm_s2 = 0.0;
  double max_accel_z_mm_s2 = 0.0;
  double max_accel_e_mm_s2 = 0.0;
  double max_accel_extruding_mm_s2 = 0.0;
  double max_accel_retracting_mm_s2 = 0.0;

  double jerk_x_mm_s = 0.0;
  double jerk_y_mm_s = 0.0;
  double jerk_z_mm_s = 0.0;
  double jerk_e_mm_s = 0.0;
  double min_extruding_rate_mm_s = 0.0;
  double min_travel_rate_mm_s = 0.0;

  double travel_speed_mm_s = 0.0;
  double travel_speed_z_mm_s = 0.0;
  double max_print_speed_mm_s = 0.0;
  double max_volumetric_speed_mm3_s = 0.0;

  double retract_length_mm = 0.0;
  double retract_speed_mm_s = 0.0;
  double deretract_speed_mm_s = 0.0;
  double retract_lift_mm = 0.0;

  // Working temperatures this profile is allowed to use, plus the range a setpoint must fall
  // inside. The range is the span observed across the vendor filament presets for this machine,
  // not a firmware limit: the printer profile carries no max-temperature key and the firmware
  // limits have not been read. Treating the observed span as the declared limit keeps the
  // emitter from ever producing a setpoint no shipped filament asks for.
  double nozzle_temp_first_layer_c = 0.0;
  double nozzle_temp_other_layers_c = 0.0;
  double bed_temp_first_layer_c = 0.0;
  double bed_temp_other_layers_c = 0.0;
  double nozzle_temp_min_c = 0.0;
  double nozzle_temp_max_c = 0.0;
  double bed_temp_min_c = 0.0;
  double bed_temp_max_c = 0.0;

  // Open-frame machines have no chamber at all; an enclosed one must never inherit this.
  bool has_chamber = false;

  // Owned by the toolhead clearance model, not by this file. Left unset here: the colour-group
  // feature is simply unavailable while they are empty, which is the intended failure mode.
  // A substituted "plausible" value is a crashed gantry.
  std::optional<double> toolhead_clearance_height_mm;
  std::optional<double> toolhead_clearance_radius_mm;
};

// Original Prusa i3 MK3S / MK3S+, 0.4 nozzle, PLA working temperatures.
// Every value traces to research/printer-profiles/mk3s.md.
inline PrinterProfile mk3s_profile() {
  PrinterProfile p;
  p.name = "Original Prusa i3 MK3S";
  p.model_code = "MK3S";
  p.family = FirmwareFamily::kLegacy;

  p.bed_size_x_mm = 250.0;
  p.bed_size_y_mm = 210.0;
  p.max_print_height_mm = 210.0;

  p.nozzle_diameter_mm = 0.4;
  p.filament_diameter_mm = 1.75;
  p.min_layer_height_mm = 0.07;
  p.max_layer_height_mm = 0.25;

  p.max_feedrate_x_mm_s = 200.0;
  p.max_feedrate_y_mm_s = 200.0;
  p.max_feedrate_z_mm_s = 12.0;
  p.max_feedrate_e_mm_s = 120.0;

  p.max_accel_x_mm_s2 = 1000.0;
  p.max_accel_y_mm_s2 = 1000.0;
  p.max_accel_z_mm_s2 = 200.0;
  p.max_accel_e_mm_s2 = 5000.0;
  p.max_accel_extruding_mm_s2 = 1250.0;
  p.max_accel_retracting_mm_s2 = 1250.0;

  p.jerk_x_mm_s = 8.0;
  p.jerk_y_mm_s = 8.0;
  p.jerk_z_mm_s = 0.4;
  p.jerk_e_mm_s = 4.5;
  p.min_extruding_rate_mm_s = 0.0;
  p.min_travel_rate_mm_s = 0.0;

  p.travel_speed_mm_s = 180.0;
  p.travel_speed_z_mm_s = 12.0;
  p.max_print_speed_mm_s = 200.0;
  p.max_volumetric_speed_mm3_s = 15.0;

  p.retract_length_mm = 0.8;
  p.retract_speed_mm_s = 35.0;
  p.deretract_speed_mm_s = 35.0;
  p.retract_lift_mm = 0.4;

  p.nozzle_temp_first_layer_c = 215.0;
  p.nozzle_temp_other_layers_c = 210.0;
  p.bed_temp_first_layer_c = 60.0;
  p.bed_temp_other_layers_c = 60.0;
  p.nozzle_temp_min_c = 195.0;
  p.nozzle_temp_max_c = 285.0;
  p.bed_temp_min_c = 50.0;
  p.bed_temp_max_c = 110.0;

  p.has_chamber = false;

  return p;
}

inline double filament_cross_section_mm2(const PrinterProfile& profile) {
  const double r = profile.filament_diameter_mm * 0.5;
  return 3.14159265358979323846 * r * r;
}

}  // namespace slicem
