#include "slicing/stl_reader.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace slicem {
namespace {

constexpr std::size_t kBinaryHeaderSize = 80;
constexpr std::size_t kBinaryCountSize = 4;
constexpr std::size_t kBinaryRecordSize = 50;

uint32_t read_u32_le(const unsigned char* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

float read_f32_le(const unsigned char* p) {
  const uint32_t bits = read_u32_le(p);
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

class VertexWelder {
 public:
  explicit VertexWelder(TriangleMesh& mesh) : mesh_(mesh) {}

  int add(double x_mm, double y_mm, double z_mm) {
    const Point3 quantised{mm_to_units(x_mm), mm_to_units(y_mm), mm_to_units(z_mm)};
    const auto it = index_.find(quantised);
    if (it != index_.end()) {
      ++welded_;
      return it->second;
    }
    const int index = static_cast<int>(mesh_.vertices.size());
    mesh_.vertices.push_back(quantised);
    index_.emplace(quantised, index);
    return index;
  }

  int welded() const { return welded_; }

 private:
  TriangleMesh& mesh_;
  std::unordered_map<Point3, int, Point3Hash> index_;
  int welded_ = 0;
};

bool push_triangle(TriangleMesh& mesh, const int idx[3], int& skipped) {
  // After quantisation a triangle with two coincident corners has no area and no usable
  // edge direction, so it can only add noise to contour chaining.
  if (idx[0] == idx[1] || idx[1] == idx[2] || idx[0] == idx[2]) {
    ++skipped;
    return false;
  }
  Triangle tri;
  tri.v[0] = idx[0];
  tri.v[1] = idx[1];
  tri.v[2] = idx[2];
  mesh.triangles.push_back(tri);
  return true;
}

bool looks_binary(const std::vector<unsigned char>& buf) {
  if (buf.size() < kBinaryHeaderSize + kBinaryCountSize) {
    return false;
  }
  const uint32_t count = read_u32_le(buf.data() + kBinaryHeaderSize);
  const std::size_t expected =
      kBinaryHeaderSize + kBinaryCountSize + static_cast<std::size_t>(count) * kBinaryRecordSize;
  // Size agreement is the only reliable discriminator: a binary STL's 80-byte header may
  // itself begin with the word "solid", so the leading token proves nothing.
  return expected == buf.size();
}

void parse_binary(const std::vector<unsigned char>& buf, StlLoadResult& out) {
  const uint32_t count = read_u32_le(buf.data() + kBinaryHeaderSize);
  VertexWelder welder(out.mesh);
  out.mesh.triangles.reserve(count);
  for (uint32_t i = 0; i < count; ++i) {
    const unsigned char* record =
        buf.data() + kBinaryHeaderSize + kBinaryCountSize + static_cast<std::size_t>(i) * kBinaryRecordSize;
    int idx[3];
    for (int k = 0; k < 3; ++k) {
      const unsigned char* vertex = record + 12 + k * 12;
      idx[k] = welder.add(read_f32_le(vertex), read_f32_le(vertex + 4), read_f32_le(vertex + 8));
    }
    ++out.triangles_read;
    push_triangle(out.mesh, idx, out.degenerate_triangles_skipped);
  }
  out.duplicate_vertices_welded = welder.welded();
  out.ok = true;
}

void parse_ascii(const std::vector<unsigned char>& buf, StlLoadResult& out) {
  std::string text(reinterpret_cast<const char*>(buf.data()), buf.size());
  std::istringstream in(text);
  VertexWelder welder(out.mesh);
  std::string token;
  int pending[3];
  int pending_count = 0;
  while (in >> token) {
    if (token != "vertex") {
      continue;
    }
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (!(in >> x >> y >> z)) {
      out.error = "truncated vertex record in ASCII STL";
      return;
    }
    pending[pending_count++] = welder.add(x, y, z);
    if (pending_count == 3) {
      pending_count = 0;
      ++out.triangles_read;
      push_triangle(out.mesh, pending, out.degenerate_triangles_skipped);
    }
  }
  if (pending_count != 0) {
    out.error = "ASCII STL vertex count is not a multiple of three";
    return;
  }
  out.duplicate_vertices_welded = welder.welded();
  out.ok = true;
}

}  // namespace

StlLoadResult load_stl(const std::string& path) {
  StlLoadResult out;

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    out.error = "cannot open file: " + path;
    return out;
  }

  std::vector<unsigned char> buf((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
  if (buf.empty()) {
    out.error = "file is empty: " + path;
    return out;
  }

  out.binary = looks_binary(buf);
  if (out.binary) {
    parse_binary(buf, out);
  } else {
    parse_ascii(buf, out);
  }

  if (out.ok && out.mesh.triangles.empty()) {
    out.ok = false;
    out.error = "no usable triangles in file";
  }
  return out;
}

}  // namespace slicem
