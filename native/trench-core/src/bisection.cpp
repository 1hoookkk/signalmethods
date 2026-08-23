#include "trench/core/bisection.hpp"

#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace trench::core::bisect {
namespace {

constexpr std::array<std::array<std::size_t, 4>, 3> kLevels{
    {{3, 0, 0, 0}, {1, 5, 0, 0}, {0, 2, 4, 6}}};
constexpr std::array<std::size_t, 3> kLevelSize{1, 2, 4};

std::string number(double value) {
  std::ostringstream out;
  out << std::setprecision(9) << value;
  return out.str();
}

}  // namespace

double knob_of_point(std::size_t point) {
  return static_cast<double>(point + 1) / 8.0;
}

double median_of(std::vector<double> values) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const auto n = values.size();
  if (n % 2 == 1) return values[n / 2];
  return 0.5 * (values[n / 2 - 1] + values[n / 2]);
}

Session::Session(std::uint64_t seed) : seed_(seed) { build_level(); }

void Session::build_level() {
  queue_.clear();
  at_ = 0;
  if (level_ >= kLevels.size()) return;
  const auto lower = [this](std::size_t point) {
    for (std::size_t i = point; i-- > 0;) {
      if (!repeats_[i].empty()) return median_[i];
    }
    return 0.0;
  };
  const auto upper = [this](std::size_t point) {
    for (std::size_t i = point + 1; i < kPointCount; ++i) {
      if (!repeats_[i].empty()) return median_[i];
    }
    return 1.0;
  };
  std::vector<Trial> trials;
  for (std::size_t i = 0; i < kLevelSize[level_]; ++i) {
    const auto point = kLevels[level_][i];
    const Trial trial{point, lower(point), upper(point)};
    for (std::size_t r = 0; r < kRepeats; ++r) trials.push_back(trial);
  }
  std::mt19937_64 rng(seed_ + level_);
  std::shuffle(trials.begin(), trials.end(), rng);
  queue_ = std::move(trials);
  ++level_;
}

bool Session::finished() const noexcept { return queue_.empty() && at_ == 0; }

const Trial& Session::current() const {
  if (at_ >= queue_.size()) throw std::logic_error("bisection session finished");
  return queue_[at_];
}

double Session::midpoint() const {
  const auto& trial = current();
  return 0.5 * (trial.lo + trial.hi);
}

void Session::accept(double internal) {
  const auto& trial = current();
  const auto point = trial.point;
  repeats_[point].push_back(internal);
  median_[point] = median_of(repeats_[point]);
  ++at_;
  if (at_ >= queue_.size()) build_level();
}

std::array<double, kPointCount> Session::medians() const { return median_; }

const std::array<std::vector<double>, kPointCount>& Session::repeats() const noexcept {
  return repeats_;
}

bool Session::monotone() const {
  for (std::size_t i = 1; i < kPointCount; ++i) {
    if (!(median_[i] > median_[i - 1])) return false;
  }
  return true;
}

std::string curve_json(const std::string& axis, const std::string& body,
                       const std::array<double, kPointCount>& points,
                       const std::array<std::vector<double>, kPointCount>& repeats,
                       bool monotone) {
  std::ostringstream out;
  out << "{\"axis\":\"" << axis << "\",\"body\":\"" << body << "\",\"points\":[";
  for (std::size_t i = 0; i < kPointCount; ++i) {
    if (i) out << ',';
    out << "{\"knob\":" << number(knob_of_point(i)) << ",\"internal\":" << number(points[i])
        << '}';
  }
  out << "],\"repeats\":{";
  for (std::size_t i = 0; i < kPointCount; ++i) {
    if (i) out << ',';
    out << '"' << number(knob_of_point(i)) << "\":[";
    for (std::size_t r = 0; r < repeats[i].size(); ++r) {
      if (r) out << ',';
      out << number(repeats[i][r]);
    }
    out << ']';
  }
  out << '}';
  if (!monotone) out << ",\"monotone\":false";
  out << "}\n";
  return out.str();
}

}  // namespace trench::core::bisect
