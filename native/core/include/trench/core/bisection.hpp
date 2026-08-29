#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace trench::core::bisect {

inline constexpr std::size_t kPointCount = 7;
inline constexpr std::size_t kRepeats = 3;

[[nodiscard]] double knob_of_point(std::size_t point);

struct Trial {
  std::size_t point{};
  double lo{};
  double hi{};
};

class Session {
 public:
  explicit Session(std::uint64_t seed);

  [[nodiscard]] bool finished() const noexcept;
  [[nodiscard]] const Trial& current() const;
  [[nodiscard]] double midpoint() const;
  void accept(double internal);

  [[nodiscard]] std::array<double, kPointCount> medians() const;
  [[nodiscard]] const std::array<std::vector<double>, kPointCount>& repeats() const noexcept;
  [[nodiscard]] bool monotone() const;

 private:
  void build_level();

  std::array<std::vector<double>, kPointCount> repeats_{};
  std::array<double, kPointCount> median_{};
  std::vector<Trial> queue_{};
  std::size_t at_{};
  std::size_t level_{};
  std::uint64_t seed_{};
};

[[nodiscard]] double median_of(std::vector<double> values);

[[nodiscard]] std::string curve_json(
    const std::string& axis, const std::string& body,
    const std::array<double, kPointCount>& points,
    const std::array<std::vector<double>, kPointCount>& repeats, bool monotone);

}
