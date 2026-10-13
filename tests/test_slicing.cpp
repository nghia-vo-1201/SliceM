#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

#include "check.h"
#include "slicing/slicer.h"
#include "slicing/stl_reader.h"

using namespace slicem;

namespace {

void append_box(TriangleMesh& mesh, double x0, double y0, double z0, double sx, double sy,
                double sz) {
  const int base = static_cast<int>(mesh.vertices.size());
  const double x1 = x0 + sx;
  const double y1 = y0 + sy;
  const double z1 = z0 + sz;
  const double corners[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                                {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
  for (const auto& c : corners) {
    mesh.vertices.push_back(Point3{mm_to_units(c[0]), mm_to_units(c[1]), mm_to_units(c[2])});
  }
  // Outward-facing winding: bottom, top, -y, +x, +y, -x.
  const int faces[12][3] = {{0, 3, 2}, {0, 2, 1}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                            {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
  for (const auto& f : faces) {
    Triangle tri;
    tri.v[0] = base + f[0];
    tri.v[1] = base + f[1];
    tri.v[2] = base + f[2];
    mesh.triangles.push_back(tri);
  }
}

TriangleMesh make_box(double x0, double y0, double z0, double sx, double sy, double sz) {
  TriangleMesh mesh;
  append_box(mesh, x0, y0, z0, sx, sy, sz);
  return mesh;
}

std::vector<int> all_triangles(const TriangleMesh& mesh) {
  std::vector<int> indices(mesh.triangles.size());
  for (std::size_t i = 0; i < indices.size(); ++i) {
    indices[i] = static_cast<int>(i);
  }
  return indices;
}

double abs_area(const Polygon& poly) { return std::fabs(signed_area_mm2(poly)); }

double layer_area(const Layer& layer) {
  double total = 0.0;
  for (const Polygon& contour : layer.contours) {
    total += abs_area(contour);
  }
  return total;
}

bool write_ascii_stl(const std::string& path, const TriangleMesh& mesh) {
  std::ofstream out(path);
  if (!out) {
    return false;
  }
  out << std::fixed << std::setprecision(6);
  out << "solid slicem_test\n";
  for (const Triangle& tri : mesh.triangles) {
    out << "  facet normal 0 0 0\n    outer loop\n";
    for (int k = 0; k < 3; ++k) {
      const Point3& p = mesh.vertices[static_cast<std::size_t>(tri.v[k])];
      out << "      vertex " << units_to_mm(p.x) << " " << units_to_mm(p.y) << " "
          << units_to_mm(p.z) << "\n";
    }
    out << "    endloop\n  endfacet\n";
  }
  out << "endsolid slicem_test\n";
  return true;
}

// A 20 mm cube at 0.2 mm has an analytically known answer: 100 layers, one contour each,
// 400 mm^2 every time. Anything else is a bug, not a tolerance question.
void test_cube_against_analytic_oracle() {
  const TriangleMesh cube = make_box(0.0, 0.0, 0.0, 20.0, 20.0, 20.0);
  SliceSettings settings;
  settings.layer_height_mm = 0.2;

  const SliceResult result = slice_mesh(cube, settings);
  CHECK(result.ok);
  CHECK_EQ(result.layers.size(), std::size_t{100});
  CHECK_EQ(result.total_open_contours, 0);

  bool every_layer_single_contour = true;
  double worst_area_error = 0.0;
  for (const Layer& layer : result.layers) {
    if (layer.contours.size() != 1) {
      every_layer_single_contour = false;
      continue;
    }
    worst_area_error = std::max(worst_area_error, std::fabs(abs_area(layer.contours[0]) - 400.0));
  }
  CHECK(every_layer_single_contour);
  CHECK_NEAR(worst_area_error, 0.0, 1e-9);
}

void test_two_disjoint_columns() {
  TriangleMesh mesh;
  append_box(mesh, 0.0, 0.0, 0.0, 5.0, 5.0, 10.0);
  append_box(mesh, 10.0, 0.0, 0.0, 5.0, 5.0, 10.0);

  SliceSettings settings;
  settings.layer_height_mm = 0.5;
  const SliceResult result = slice_mesh(mesh, settings);

  CHECK(result.ok);
  CHECK_EQ(result.layers.size(), std::size_t{20});
  CHECK_EQ(result.total_open_contours, 0);

  bool every_layer_two_contours = true;
  for (const Layer& layer : result.layers) {
    if (layer.contours.size() != 2) {
      every_layer_two_contours = false;
    }
  }
  CHECK(every_layer_two_contours);
  CHECK_NEAR(layer_area(result.layers.front()), 50.0, 1e-9);
}

// The on-plane convention is the slicer's single most load-bearing numeric decision, so it
// gets asserted directly rather than through slice_mesh's mid-layer sampling.
void test_vertex_exactly_on_plane() {
  const TriangleMesh cube = make_box(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
  const std::vector<int> candidates = all_triangles(cube);

  int open = 0;
  const std::vector<Polygon> at_bottom = slice_plane(cube, candidates, 0, &open);
  CHECK(at_bottom.empty());
  CHECK_EQ(open, 0);

  open = 0;
  const std::vector<Polygon> at_top = slice_plane(cube, candidates, mm_to_units(10.0), &open);
  CHECK_EQ(at_top.size(), std::size_t{1});
  CHECK_EQ(open, 0);
  if (at_top.size() == 1) {
    CHECK_NEAR(abs_area(at_top[0]), 100.0, 1e-9);
  }
}

void test_single_triangle_yields_open_chain() {
  TriangleMesh mesh;
  mesh.vertices = {Point3{0, 0, 0}, Point3{mm_to_units(10.0), 0, 0},
                   Point3{0, 0, mm_to_units(10.0)}};
  Triangle tri;
  tri.v[0] = 0;
  tri.v[1] = 1;
  tri.v[2] = 2;
  mesh.triangles.push_back(tri);

  int open = 0;
  const std::vector<Polygon> contours =
      slice_plane(mesh, all_triangles(mesh), mm_to_units(5.0), &open);
  CHECK(contours.empty());
  CHECK_EQ(open, 1);
}

void test_non_watertight_mesh_is_reported() {
  TriangleMesh mesh = make_box(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
  mesh.triangles.pop_back();

  SliceSettings settings;
  settings.layer_height_mm = 1.0;
  const SliceResult result = slice_mesh(mesh, settings);

  CHECK(result.ok);
  CHECK(result.total_open_contours > 0);
}

void test_empty_and_invalid_input() {
  const TriangleMesh empty;
  SliceSettings settings;
  const SliceResult empty_result = slice_mesh(empty, settings);
  CHECK(empty_result.ok);
  CHECK(empty_result.layers.empty());

  SliceSettings bad;
  bad.layer_height_mm = 0.0;
  const SliceResult bad_result = slice_mesh(make_box(0.0, 0.0, 0.0, 10.0, 10.0, 10.0), bad);
  CHECK(!bad_result.ok);
  CHECK(!bad_result.error.empty());

  // Shorter than one layer: nothing is depositable, and that must not become a phantom layer.
  SliceSettings coarse;
  coarse.layer_height_mm = 1.0;
  const SliceResult thin = slice_mesh(make_box(0.0, 0.0, 0.0, 10.0, 10.0, 0.4), coarse);
  CHECK(thin.ok);
  CHECK(thin.layers.empty());
}

void test_output_is_deterministic() {
  const TriangleMesh mesh = make_box(0.0, 0.0, 0.0, 20.0, 20.0, 5.0);
  SliceSettings settings;
  settings.layer_height_mm = 0.2;

  const SliceResult first = slice_mesh(mesh, settings);
  const SliceResult second = slice_mesh(mesh, settings);

  CHECK_EQ(first.layers.size(), second.layers.size());
  bool identical = first.layers.size() == second.layers.size();
  for (std::size_t i = 0; identical && i < first.layers.size(); ++i) {
    identical = first.layers[i].contours == second.layers[i].contours;
  }
  CHECK(identical);
}

void test_ascii_stl_welds_shared_vertices() {
  const TriangleMesh cube = make_box(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
  const std::string path = "slicem_test_cube.stl";
  CHECK(write_ascii_stl(path, cube));

  const StlLoadResult loaded = load_stl(path);
  CHECK(loaded.ok);
  CHECK(!loaded.binary);
  CHECK_EQ(loaded.mesh.triangles.size(), std::size_t{12});
  // 36 vertex records collapse onto the cube's 8 distinct corners.
  CHECK_EQ(loaded.mesh.vertices.size(), std::size_t{8});
  CHECK_EQ(loaded.duplicate_vertices_welded, 28);

  SliceSettings settings;
  settings.layer_height_mm = 0.5;
  const SliceResult result = slice_mesh(loaded.mesh, settings);
  CHECK(result.ok);
  CHECK_EQ(result.layers.size(), std::size_t{20});
  CHECK_EQ(result.total_open_contours, 0);

  std::remove(path.c_str());
}

}  // namespace

int main() {
  test_cube_against_analytic_oracle();
  test_two_disjoint_columns();
  test_vertex_exactly_on_plane();
  test_single_triangle_yields_open_chain();
  test_non_watertight_mesh_is_reported();
  test_empty_and_invalid_input();
  test_output_is_deterministic();
  test_ascii_stl_welds_shared_vertices();
  return slicem_test::summary("slicem_tests");
}
