#include "gcode/gcode_writer.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace slicem {
namespace {

// Two setpoints closer together than this are the same setpoint. Used only to recognise
// "heater off" (0) without comparing a double to zero for equality.
constexpr double kTempEpsilonC = 0.5;

// A feedrate difference below this cannot be expressed in the mm/min field anyway.
constexpr double kFeedrateEpsilonMmS = 1e-6;

std::string num(double value, int decimals) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
  std::string text(buffer);
  if (text.find('.') == std::string::npos) {
    return text;
  }
  while (!text.empty() && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  if (text == "-0") {
    return "0";
  }
  return text;
}

std::string coord(double mm) {
  return num(mm, 3);
}

std::string efield(double mm) {
  return num(mm, 5);
}

std::string feed(double mm_s) {
  return num(mm_s * 60.0, 3);
}

}  // namespace

GcodeWriter::GcodeWriter(std::ostream& out, const PrinterProfile& profile)
    : out_(out), profile_(profile) {}

bool GcodeWriter::fail(const std::string& message) {
  if (error_.empty()) {
    error_ = message;
  }
  return false;
}

bool GcodeWriter::require_legacy(const char* command) {
  if (profile_.family != FirmwareFamily::kLegacy) {
    return fail(std::string(command) + " is a legacy-family command and this profile (" +
                profile_.name + ") is not in that family");
  }
  return true;
}

bool GcodeWriter::check_finite(double value, const char* what) {
  if (!std::isfinite(value)) {
    return fail(std::string("non-finite ") + what);
  }
  return true;
}

bool GcodeWriter::check_xy(double x, double y) {
  if (!check_finite(x, "X coordinate") || !check_finite(y, "Y coordinate")) {
    return false;
  }
  if (x < 0.0 || x > profile_.bed_size_x_mm || y < 0.0 || y > profile_.bed_size_y_mm) {
    return fail("move to X" + coord(x) + " Y" + coord(y) + " is outside the build volume (0.." +
                coord(profile_.bed_size_x_mm) + " by 0.." + coord(profile_.bed_size_y_mm) + ")");
  }
  return true;
}

bool GcodeWriter::check_z(double z) {
  if (!check_finite(z, "Z coordinate")) {
    return false;
  }
  if (z < 0.0 || z > profile_.max_print_height_mm) {
    return fail("move to Z" + coord(z) + " is outside the build volume (0.." +
                coord(profile_.max_print_height_mm) + ")");
  }
  return true;
}

bool GcodeWriter::check_xy_speed(double speed_mm_s) {
  if (!check_finite(speed_mm_s, "feedrate")) {
    return false;
  }
  const double limit = std::fmin(profile_.max_feedrate_x_mm_s, profile_.max_feedrate_y_mm_s);
  if (speed_mm_s <= 0.0 || speed_mm_s > limit) {
    return fail("XY feedrate " + num(speed_mm_s, 3) + " mm/s is outside (0, " + num(limit, 3) +
                "]");
  }
  return true;
}

bool GcodeWriter::check_z_speed(double speed_mm_s) {
  if (!check_finite(speed_mm_s, "feedrate")) {
    return false;
  }
  if (speed_mm_s <= 0.0 || speed_mm_s > profile_.max_feedrate_z_mm_s) {
    return fail("Z feedrate " + num(speed_mm_s, 3) + " mm/s is outside (0, " +
                num(profile_.max_feedrate_z_mm_s, 3) + "]");
  }
  return true;
}

void GcodeWriter::write_feedrate_if_changed(double speed_mm_s) {
  if (std::fabs(speed_mm_s - feedrate_mm_s_) > kFeedrateEpsilonMmS) {
    out_ << " F" << feed(speed_mm_s);
    feedrate_mm_s_ = speed_mm_s;
  }
}

void GcodeWriter::comment(const std::string& text) {
  if (!ok()) {
    return;
  }
  out_ << "; " << text << "\n";
}

void GcodeWriter::marker(const std::string& text) {
  if (!ok()) {
    return;
  }
  out_ << ";" << text << "\n";
}

void GcodeWriter::blank_line() {
  if (!ok()) {
    return;
  }
  out_ << "\n";
}

bool GcodeWriter::set_units_and_modes() {
  if (!ok()) {
    return false;
  }
  out_ << "G21 ; millimetre units\n";
  out_ << "G90 ; absolute XYZ\n";
  out_ << "M83 ; relative extrusion\n";
  return true;
}

bool GcodeWriter::set_machine_limits() {
  if (!ok() || !require_legacy("M201/M203/M204/M205")) {
    return false;
  }
  out_ << "M201 X" << num(profile_.max_accel_x_mm_s2, 0) << " Y"
       << num(profile_.max_accel_y_mm_s2, 0) << " Z" << num(profile_.max_accel_z_mm_s2, 0) << " E"
       << num(profile_.max_accel_e_mm_s2, 0) << " ; maximum acceleration\n";
  out_ << "M203 X" << num(profile_.max_feedrate_x_mm_s, 0) << " Y"
       << num(profile_.max_feedrate_y_mm_s, 0) << " Z" << num(profile_.max_feedrate_z_mm_s, 0)
       << " E" << num(profile_.max_feedrate_e_mm_s, 0) << " ; maximum feedrate\n";
  out_ << "M204 S" << num(profile_.max_accel_extruding_mm_s2, 0) << " T"
       << num(profile_.max_accel_retracting_mm_s2, 0) << " ; print and retract acceleration\n";
  out_ << "M205 X" << num(profile_.jerk_x_mm_s, 2) << " Y" << num(profile_.jerk_y_mm_s, 2) << " Z"
       << num(profile_.jerk_z_mm_s, 2) << " E" << num(profile_.jerk_e_mm_s, 2) << " ; jerk\n";
  out_ << "M205 S" << num(profile_.min_extruding_rate_mm_s, 0) << " T"
       << num(profile_.min_travel_rate_mm_s, 0) << " ; minimum feedrates\n";
  return true;
}

bool GcodeWriter::emit_compatibility_checks() {
  if (!ok() || !require_legacy("M862.x")) {
    return false;
  }
  if (profile_.model_code.empty()) {
    return fail("profile has no model code for the M862.3 check");
  }
  out_ << "M862.3 P \"" << profile_.model_code << "\" ; printer model check\n";
  out_ << "M862.1 P" << num(profile_.nozzle_diameter_mm, 2) << " ; nozzle diameter check\n";
  return true;
}

bool GcodeWriter::set_temperature(Heater heater, double celsius, bool wait) {
  if (!ok() || !check_finite(celsius, "temperature")) {
    return false;
  }
  const bool off = celsius < kTempEpsilonC;
  const double low = heater == Heater::kNozzle ? profile_.nozzle_temp_min_c : profile_.bed_temp_min_c;
  const double high =
      heater == Heater::kNozzle ? profile_.nozzle_temp_max_c : profile_.bed_temp_max_c;
  if (!off && (celsius < low || celsius > high)) {
    return fail("temperature " + num(celsius, 1) + " C is outside the profile's declared range " +
                num(low, 1) + ".." + num(high, 1) + " C");
  }
  if (off && wait) {
    return fail("refusing to wait for a heater that is being switched off");
  }
  const double value = off ? 0.0 : celsius;
  if (heater == Heater::kNozzle) {
    out_ << (wait ? "M109 S" : "M104 S") << num(value, 1)
         << (wait ? " ; wait for nozzle\n" : " ; set nozzle temperature\n");
  } else {
    out_ << (wait ? "M190 S" : "M140 S") << num(value, 1)
         << (wait ? " ; wait for bed\n" : " ; set bed temperature\n");
  }
  return true;
}

bool GcodeWriter::set_fan_percent(int percent) {
  if (!ok()) {
    return false;
  }
  if (percent < 0 || percent > 100) {
    return fail("fan speed " + std::to_string(percent) + "% is outside 0..100");
  }
  if (percent == 0) {
    out_ << "M107 ; fan off\n";
    return true;
  }
  const int pwm = static_cast<int>(std::lround(static_cast<double>(percent) * 255.0 / 100.0));
  out_ << "M106 S" << pwm << " ; fan " << percent << "%\n";
  return true;
}

bool GcodeWriter::reset_extruder() {
  if (!ok()) {
    return false;
  }
  out_ << "G92 E0 ; reset extruder position\n";
  return true;
}

bool GcodeWriter::set_flow_percent(double percent) {
  if (!ok() || !check_finite(percent, "flow percentage")) {
    return false;
  }
  if (percent <= 0.0 || percent > 200.0) {
    return fail("flow " + num(percent, 1) + "% is outside (0, 200]");
  }
  out_ << "M221 S" << num(percent, 1) << " ; extrusion flow\n";
  return true;
}

bool GcodeWriter::disable_linear_advance() {
  if (!ok() || !require_legacy("M900")) {
    return false;
  }
  out_ << "M900 K0 ; linear advance off\n";
  return true;
}

bool GcodeWriter::home_axes() {
  if (!ok() || !require_legacy("G28 W")) {
    return false;
  }
  out_ << "G28 W ; home all axes without mesh levelling\n";
  // The homed position is not asserted here: every move the writer emits afterwards is
  // absolute, so nothing downstream may rely on where homing left the toolhead.
  position_known_ = false;
  feedrate_mm_s_ = -1.0;
  return true;
}

bool GcodeWriter::mesh_bed_level() {
  if (!ok() || !require_legacy("G80")) {
    return false;
  }
  out_ << "G80 ; mesh bed levelling\n";
  position_known_ = false;
  return true;
}

bool GcodeWriter::dwell_until_moves_finish() {
  if (!ok()) {
    return false;
  }
  out_ << "G4 ; wait for all moves to finish\n";
  return true;
}

bool GcodeWriter::disable_steppers() {
  if (!ok()) {
    return false;
  }
  out_ << "M84 ; disable steppers\n";
  position_known_ = false;
  return true;
}

bool GcodeWriter::travel_to(double x_mm, double y_mm, double speed_mm_s) {
  if (!ok() || !check_xy(x_mm, y_mm) || !check_xy_speed(speed_mm_s)) {
    return false;
  }
  out_ << "G1 X" << coord(x_mm) << " Y" << coord(y_mm);
  write_feedrate_if_changed(speed_mm_s);
  out_ << "\n";
  x_ = x_mm;
  y_ = y_mm;
  position_known_ = true;
  return true;
}

bool GcodeWriter::travel_to_z(double z_mm, double speed_mm_s) {
  if (!ok() || !check_z(z_mm) || !check_z_speed(speed_mm_s)) {
    return false;
  }
  out_ << "G1 Z" << coord(z_mm);
  write_feedrate_if_changed(speed_mm_s);
  out_ << "\n";
  z_ = z_mm;
  return true;
}

bool GcodeWriter::extrude_to(double x_mm, double y_mm, double e_mm, double speed_mm_s) {
  if (!ok() || !check_xy(x_mm, y_mm) || !check_xy_speed(speed_mm_s) ||
      !check_finite(e_mm, "extrusion length")) {
    return false;
  }
  if (!position_known_) {
    return fail("extrusion move from an unknown position");
  }
  if (retracted_) {
    return fail("extrusion move while retracted");
  }
  if (e_mm <= 0.0) {
    return fail("extrusion move with non-positive E (" + efield(e_mm) + ")");
  }
  const double length = std::hypot(x_mm - x_, y_mm - y_);
  if (length <= 0.0) {
    return fail("extrusion move of zero length");
  }
  const double e_speed = speed_mm_s * e_mm / length;
  if (e_speed > profile_.max_feedrate_e_mm_s) {
    return fail("extruder feedrate " + num(e_speed, 3) + " mm/s exceeds the limit " +
                num(profile_.max_feedrate_e_mm_s, 3) + " mm/s");
  }
  out_ << "G1 X" << coord(x_mm) << " Y" << coord(y_mm) << " E" << efield(e_mm);
  write_feedrate_if_changed(speed_mm_s);
  out_ << "\n";
  x_ = x_mm;
  y_ = y_mm;
  net_e_mm_ += e_mm;
  extruded_e_mm_ += e_mm;
  return true;
}

bool GcodeWriter::retract() {
  if (!ok()) {
    return false;
  }
  if (retracted_) {
    return true;
  }
  const double length = profile_.retract_length_mm;
  if (!(length > 0.0)) {
    return fail("profile declares no retraction length");
  }
  if (profile_.retract_speed_mm_s > profile_.max_feedrate_e_mm_s) {
    return fail("retraction speed exceeds the extruder feedrate limit");
  }
  out_ << "G1 E-" << efield(length) << " F" << feed(profile_.retract_speed_mm_s) << "\n";
  // The bare-F form is not used for retraction, so the tracked feedrate must follow the
  // F this line carried or the next move would omit its own F.
  feedrate_mm_s_ = profile_.retract_speed_mm_s;
  net_e_mm_ -= length;
  retracted_ = true;
  return true;
}

bool GcodeWriter::unretract() {
  if (!ok()) {
    return false;
  }
  if (!retracted_) {
    return true;
  }
  const double length = profile_.retract_length_mm;
  const double speed = profile_.deretract_speed_mm_s > 0.0 ? profile_.deretract_speed_mm_s
                                                           : profile_.retract_speed_mm_s;
  if (speed > profile_.max_feedrate_e_mm_s) {
    return fail("deretraction speed exceeds the extruder feedrate limit");
  }
  out_ << "G1 E" << efield(length) << " F" << feed(speed) << "\n";
  feedrate_mm_s_ = speed;
  net_e_mm_ += length;
  retracted_ = false;
  return true;
}

bool GcodeWriter::layer_change(double print_z_mm, double layer_height_mm) {
  if (!ok() || !check_z(print_z_mm)) {
    return false;
  }
  if (!check_finite(layer_height_mm, "layer height")) {
    return false;
  }
  if (layer_height_mm < profile_.min_layer_height_mm ||
      layer_height_mm > profile_.max_layer_height_mm) {
    return fail("layer height " + num(layer_height_mm, 3) + " mm is outside the profile range " +
                num(profile_.min_layer_height_mm, 3) + ".." + num(profile_.max_layer_height_mm, 3) +
                " mm");
  }
  out_ << ";LAYER_CHANGE\n";
  out_ << ";Z:" << coord(print_z_mm) << "\n";
  out_ << ";HEIGHT:" << coord(layer_height_mm) << "\n";
  ++layer_change_count_;
  return travel_to_z(print_z_mm, profile_.travel_speed_z_mm_s);
}

}  // namespace slicem
