#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

#include "gcode/printer_profile.h"
#include "gcode/slice_to_gcode.h"
#include "slicing/slicer.h"
#include "slicing/stl_reader.h"

namespace {

void print_usage() {
  std::cout << "slicem - SliceM engine harness\n\n"
               "usage: slicem [options] <model.stl>\n\n"
               "options:\n"
               "  --layer-height <mm>     layer height (default 0.2)\n"
               "  --per-layer             print one row per layer\n"
               "  --gcode <out.gcode>     write MK3S G-code to this file\n"
               "  --perimeters <n>        perimeter count (default 2)\n"
               "  --infill-density <d>    sparse infill density 0..1 (default 0.2)\n"
               "  --extrusion-width <mm>  extrusion width (default 0.45)\n"
               "  -h, --help              this message\n";
}

double total_area_mm2(const slicem::Layer& layer) {
  double total = 0.0;
  for (const slicem::Polygon& contour : layer.contours) {
    total += std::fabs(slicem::signed_area_mm2(contour));
  }
  return total;
}

}  // namespace

int main(int argc, char** argv) {
  std::string path;
  std::string gcode_path;
  double layer_height_mm = 0.2;
  bool per_layer = false;
  slicem::GcodeSettings gcode_settings;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      print_usage();
      return 0;
    }
    if (arg == "--per-layer") {
      per_layer = true;
    } else if (arg == "--layer-height" && i + 1 < argc) {
      layer_height_mm = std::atof(argv[++i]);
    } else if (arg == "--gcode" && i + 1 < argc) {
      gcode_path = argv[++i];
    } else if (arg == "--perimeters" && i + 1 < argc) {
      gcode_settings.perimeter_count = std::atoi(argv[++i]);
    } else if (arg == "--infill-density" && i + 1 < argc) {
      gcode_settings.infill_density = std::atof(argv[++i]);
    } else if (arg == "--extrusion-width" && i + 1 < argc) {
      gcode_settings.extrusion_width_mm = std::atof(argv[++i]);
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << "\n";
      return 2;
    } else {
      path = arg;
    }
  }

  if (path.empty()) {
    print_usage();
    return 2;
  }

  const slicem::StlLoadResult loaded = slicem::load_stl(path);
  if (!loaded.ok) {
    std::cerr << "error: " << loaded.error << "\n";
    return 1;
  }

  const slicem::Bounds3 bounds = slicem::compute_bounds(loaded.mesh);
  std::cout << std::fixed << std::setprecision(3);
  std::cout << "model          " << path << "\n"
            << "format         " << (loaded.binary ? "binary STL" : "ASCII STL") << "\n"
            << "triangles      " << loaded.mesh.triangles.size() << " (read "
            << loaded.triangles_read << ", skipped degenerate "
            << loaded.degenerate_triangles_skipped << ")\n"
            << "vertices       " << loaded.mesh.vertices.size() << " (welded "
            << loaded.duplicate_vertices_welded << " duplicates)\n"
            << "bounds (mm)    x " << slicem::units_to_mm(bounds.min.x) << " .. "
            << slicem::units_to_mm(bounds.max.x) << "   y "
            << slicem::units_to_mm(bounds.min.y) << " .. "
            << slicem::units_to_mm(bounds.max.y) << "   z "
            << slicem::units_to_mm(bounds.min.z) << " .. "
            << slicem::units_to_mm(bounds.max.z) << "\n";

  slicem::SliceSettings settings;
  settings.layer_height_mm = layer_height_mm;

  const slicem::SliceResult sliced = slicem::slice_mesh(loaded.mesh, settings);
  if (!sliced.ok) {
    std::cerr << "error: " << sliced.error << "\n";
    return 1;
  }

  std::size_t contour_count = 0;
  double area_sum = 0.0;
  for (const slicem::Layer& layer : sliced.layers) {
    contour_count += layer.contours.size();
    area_sum += total_area_mm2(layer);
  }

  std::cout << "layer height   " << layer_height_mm << " mm\n"
            << "layers         " << sliced.layers.size() << "\n"
            << "contours       " << contour_count << "\n"
            << "open contours  " << sliced.total_open_contours;
  if (sliced.total_open_contours > 0) {
    std::cout << "   <-- mesh is not watertight at these planes";
  }
  std::cout << "\n";

  if (!sliced.layers.empty()) {
    std::cout << "mean area      " << area_sum / static_cast<double>(sliced.layers.size())
              << " mm^2 per layer\n";
  }

  if (!gcode_path.empty()) {
    gcode_settings.layer_height_mm = layer_height_mm;
    const slicem::PrinterProfile profile = slicem::mk3s_profile();
    const slicem::GcodeStats stats =
        slicem::write_gcode_file(sliced, profile, gcode_settings, gcode_path);
    if (!stats.ok) {
      std::cerr << "error: g-code not written: " << stats.error << "\n";
      return 1;
    }
    std::cout << "\ng-code         " << gcode_path << "\n"
              << "profile        " << profile.name << " (legacy family)\n"
              << "emitted layers " << stats.layers << "\n"
              << "top z          " << stats.print_z_max_mm << " mm\n"
              << "path length    " << stats.extruded_path_length_mm << " mm\n"
              << "deposited      " << stats.deposited_volume_mm3 << " mm^3\n"
              << "filament used  " << stats.filament_used_mm << " mm ("
              << stats.filament_used_cm3 << " cm^3)\n"
              << "deposit bbox   x " << stats.deposit_min_x_mm << " .. "
              << stats.deposit_max_x_mm << "   y " << stats.deposit_min_y_mm << " .. "
              << stats.deposit_max_y_mm << "\n"
              << "NOT VALIDATED ON HARDWARE - run the gcode-validation pass before printing\n";
  }

  if (per_layer) {
    std::cout << "\n  layer        z (mm)   contours     area (mm^2)   open\n";
    for (const slicem::Layer& layer : sliced.layers) {
      std::cout << std::setw(7) << layer.index << std::setw(14)
                << slicem::units_to_mm(layer.slice_z) << std::setw(11) << layer.contours.size()
                << std::setw(16) << total_area_mm2(layer) << std::setw(7) << layer.open_contours
                << "\n";
    }
  }

  return 0;
}
