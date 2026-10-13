#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>

namespace slicem {

// All internal geometry is quantised onto this grid: 1 unit = 1 nm. A 300 mm bed spans
// 3e11 units, four orders of magnitude inside int64, so coordinate arithmetic cannot
// overflow at realistic model sizes. Quantising once at load time is what lets shared mesh
// edges compare bit-exactly later, which is why contour chaining needs no tolerance at all.
inline constexpr int64_t kUnitsPerMm = 1000000;

inline int64_t mm_to_units(double mm) {
  return static_cast<int64_t>(std::llround(mm * static_cast<double>(kUnitsPerMm)));
}

inline double units_to_mm(int64_t units) {
  return static_cast<double>(units) / static_cast<double>(kUnitsPerMm);
}

struct Point2 {
  int64_t x = 0;
  int64_t y = 0;
};

struct Point3 {
  int64_t x = 0;
  int64_t y = 0;
  int64_t z = 0;
};

inline bool operator==(const Point2& a, const Point2& b) {
  return a.x == b.x && a.y == b.y;
}

inline bool operator!=(const Point2& a, const Point2& b) {
  return !(a == b);
}

inline bool operator==(const Point3& a, const Point3& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool operator!=(const Point3& a, const Point3& b) {
  return !(a == b);
}

namespace detail {

inline uint64_t mix64(uint64_t v) {
  v += 0x9e3779b97f4a7c15ull;
  v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ull;
  v = (v ^ (v >> 27)) * 0x94d049bb133111ebull;
  return v ^ (v >> 31);
}

}  // namespace detail

struct Point2Hash {
  std::size_t operator()(const Point2& p) const {
    const uint64_t h = detail::mix64(static_cast<uint64_t>(p.x)) ^
                       (detail::mix64(static_cast<uint64_t>(p.y)) * 0x9e3779b97f4a7c15ull);
    return static_cast<std::size_t>(h);
  }
};

struct Point3Hash {
  std::size_t operator()(const Point3& p) const {
    const uint64_t h = detail::mix64(static_cast<uint64_t>(p.x)) ^
                       (detail::mix64(static_cast<uint64_t>(p.y)) * 0x9e3779b97f4a7c15ull) ^
                       (detail::mix64(static_cast<uint64_t>(p.z)) * 0xc2b2ae3d27d4eb4full);
    return static_cast<std::size_t>(h);
  }
};

// Floor/ceil division for signed integers. The built-in / truncates toward zero, which
// would silently pick the wrong layer bucket for geometry below the origin.
inline int64_t floor_div(int64_t a, int64_t b) {
  const int64_t q = a / b;
  return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

inline int64_t ceil_div(int64_t a, int64_t b) {
  const int64_t q = a / b;
  return (a % b != 0 && ((a < 0) == (b < 0))) ? q + 1 : q;
}

}  // namespace slicem
