#pragma once

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

namespace slicem_test {

inline int g_checks = 0;
inline int g_failures = 0;

inline void record(bool ok, const std::string& expr, const char* file, int line,
                   const std::string& detail) {
  ++g_checks;
  if (ok) {
    return;
  }
  ++g_failures;
  std::cout << "FAIL  " << file << ":" << line << "  " << expr << "\n";
  if (!detail.empty()) {
    std::cout << "        " << detail << "\n";
  }
}

template <typename A, typename B>
std::string describe(const A& a, const B& b) {
  std::ostringstream os;
  os << "got " << a << ", expected " << b;
  return os.str();
}

inline int summary(const char* name) {
  std::cout << name << ": " << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
  return g_failures == 0 ? 0 : 1;
}

}  // namespace slicem_test

#define CHECK(cond) slicem_test::record((cond), #cond, __FILE__, __LINE__, "")

#define CHECK_EQ(a, b) \
  slicem_test::record((a) == (b), #a " == " #b, __FILE__, __LINE__, slicem_test::describe((a), (b)))

#define CHECK_NEAR(a, b, tol)                                                      \
  slicem_test::record(std::fabs((a) - (b)) <= (tol), #a " ~= " #b, __FILE__, __LINE__, \
                      slicem_test::describe((a), (b)))
