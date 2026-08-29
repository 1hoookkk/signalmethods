#pragma once

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace trench::test {

struct Failure : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct Case {
  const char* name;
  void (*run)();
};

std::vector<Case>& registry();

struct Register {
  Register(const char* name, void (*run)()) { registry().push_back({name, run}); }
};

[[noreturn]] inline void fail(const char* file, int line, const std::string& what) {
  std::ostringstream out;
  out << file << ":" << line << " " << what;
  throw Failure(out.str());
}

}  // namespace trench::test

#define TRENCH_TEST(name)                                                    \
  static void name();                                                        \
  static const trench::test::Register name##_registration{#name, &name};    \
  static void name()

#define CHECK(condition)                                                     \
  do {                                                                       \
    if (!(condition)) trench::test::fail(__FILE__, __LINE__, "CHECK " #condition); \
  } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                              \
  do {                                                                       \
    const double a_ = static_cast<double>(actual);                           \
    const double e_ = static_cast<double>(expected);                         \
    if (!(std::abs(a_ - e_) <= (tolerance))) {                               \
      std::ostringstream m_;                                                 \
      m_ << "CHECK_NEAR " #actual " = " << a_ << " vs " #expected " = " << e_ \
         << " (tolerance " << (tolerance) << ")";                            \
      trench::test::fail(__FILE__, __LINE__, m_.str());                      \
    }                                                                        \
  } while (0)
