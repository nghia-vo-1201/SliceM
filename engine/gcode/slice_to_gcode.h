#pragma once

#include <ostream>
#include <string>

#include "gcode/printer_profile.h"
#include "slicing/slicer.h"

namespace slicem {

struct GcodeSettings {
  double layer_height_mm = 0.2;
  double extrusion_width_mm = 0.45;
  int perimeter_count = 2;
  double infill_density = 0.20;

  // Thickness, in layers, of the solid shell grown beneath every detected top surface and above
  // every bottom surface. 0 leaves all surfaces sparse, which prints the part open.
  int solid_layers = 3;

  double first_layer_speed_mm_s = 20.0;
  double external_perimeter_speed_mm_s = 25.0;
  double perimeter_speed_mm_s = 45.0;
  double solid_infill_speed_mm_s = 45.0;
  double infill_speed_mm_s = 60.0;

  int skirt_loops = 1;
  double skirt_distance_mm = 3.0;

  // Travels shorter than this stay un-retracted: the ooze over such a short hop is less
  // damaging than the extra retraction cycle.
  double retract_before_travel_mm = 2.0;

  // Fan is off on layer 1 and runs at this percentage from layer 2.
  int fan_percent = 100;

  bool center_on_bed = true;
};

struct GcodeStats {
  bool ok = false;
  std::string error;
  int layers = 0;
  double print_z_max_mm = 0.0;
  double extruded_path_length_mm = 0.0;
  double deposited_volume_mm3 = 0.0;
  double filament_used_mm = 0.0;
  double filament_used_cm3 = 0.0;
  double deposit_min_x_mm = 0.0;
  double deposit_max_x_mm = 0.0;
  double deposit_min_y_mm = 0.0;
  double deposit_max_y_mm = 0.0;
};

// Turns sliced layers into a complete G-code job for one profile.
//
// Any refusal by the writer (a move outside the build volume, a temperature outside the
// profile's range, a limit exceeded) aborts the whole job and is reported in GcodeStats::error.
// A partially written stream is not a usable file and must not be sent to a printer.
GcodeStats write_gcode(const SliceResult& sliced, const PrinterProfile& profile,
                       const GcodeSettings& settings, std::ostream& out);

GcodeStats write_gcode_file(const SliceResult& sliced, const PrinterProfile& profile,
                            const GcodeSettings& settings, const std::string& path);

}  // namespace slicem
