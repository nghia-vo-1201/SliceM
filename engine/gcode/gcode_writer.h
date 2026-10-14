#pragma once

#include <ostream>
#include <string>

#include "gcode/printer_profile.h"

namespace slicem {

enum class Heater { kNozzle, kBed };

// Emits G-code for one job and tracks the machine state the commands imply: position,
// feedrate, retraction, and the relative-E total.
//
// Every operation validates before it writes and returns false on refusal, leaving a sticky
// error behind. A refusal is never a clamp: a move silently pulled back to the edge of the
// build volume prints something other than the model, which is worse than no file at all.
// Once an operation has been refused the writer emits nothing further.
//
// Only the commands below are available, and each one traces to the dialect research doc for
// the profile's firmware family. The order they are composed in belongs to the caller.
class GcodeWriter {
 public:
  GcodeWriter(std::ostream& out, const PrinterProfile& profile);

  bool ok() const { return error_.empty(); }
  const std::string& error() const { return error_; }

  void comment(const std::string& text);
  // Viewer markers such as TYPE:Perimeter, which have to appear as ";TYPE:Perimeter" with no
  // space after the semicolon for a viewer to recognise them.
  void marker(const std::string& text);
  void blank_line();

  // G21 / G90 / M83. Set explicitly, never inherited from whatever the last job left behind.
  bool set_units_and_modes();
  // M201 / M203 / M204 / M205, from the profile's machine limits. Legacy family only.
  bool set_machine_limits();
  // M862.3 model check and M862.1 nozzle-diameter check. Legacy family only.
  bool emit_compatibility_checks();

  bool set_temperature(Heater heater, double celsius, bool wait);
  bool set_fan_percent(int percent);
  bool reset_extruder();
  bool set_flow_percent(double percent);
  // M900 K0: linear advance explicitly off. SliceM has no tuned K value, and a K left over
  // from a previous job would change extrusion behind the extrusion maths' back.
  bool disable_linear_advance();
  bool home_axes();
  bool mesh_bed_level();
  bool dwell_until_moves_finish();
  bool disable_steppers();

  bool travel_to(double x_mm, double y_mm, double speed_mm_s);
  bool travel_to_z(double z_mm, double speed_mm_s);
  bool extrude_to(double x_mm, double y_mm, double e_mm, double speed_mm_s);
  bool retract();
  bool unretract();
  bool layer_change(double print_z_mm, double layer_height_mm);

  double x_mm() const { return x_; }
  double y_mm() const { return y_; }
  double z_mm() const { return z_; }
  bool position_known() const { return position_known_; }
  bool retracted() const { return retracted_; }
  double net_e_mm() const { return net_e_mm_; }
  double extruded_e_mm() const { return extruded_e_mm_; }
  int layer_change_count() const { return layer_change_count_; }
  const PrinterProfile& profile() const { return profile_; }

 private:
  bool fail(const std::string& message);
  bool require_legacy(const char* command);
  bool check_finite(double value, const char* what);
  bool check_xy(double x, double y);
  bool check_z(double z);
  bool check_xy_speed(double speed_mm_s);
  bool check_z_speed(double speed_mm_s);
  void write_feedrate_if_changed(double speed_mm_s);

  std::ostream& out_;
  PrinterProfile profile_;
  std::string error_;

  double x_ = 0.0;
  double y_ = 0.0;
  double z_ = 0.0;
  bool position_known_ = false;
  double feedrate_mm_s_ = -1.0;
  bool retracted_ = false;
  double net_e_mm_ = 0.0;
  double extruded_e_mm_ = 0.0;
  int layer_change_count_ = 0;
};

}  // namespace slicem
