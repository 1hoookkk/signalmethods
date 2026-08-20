#include "trench/core/packed_body.hpp"

#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace trench::core {
namespace {

std::complex<double> evaluate(const Biquad& section, double frequency_hz,
                              double sample_rate_hz) {
  const double omega = 2.0 * std::numbers::pi * frequency_hz / sample_rate_hz;
  const auto z1 = std::polar(1.0, -omega);
  const auto z2 = z1 * z1;
  const auto numerator = section[0] + section[1] * z1 + section[2] * z2;
  const auto denominator = 1.0 + section[3] * z1 + section[4] * z2;
  return numerator / denominator;
}

double to_db(const std::complex<double>& response) {
  return 20.0 * std::log10(std::abs(response));
}

}  // namespace

double section_response_db(const Biquad& section, double frequency_hz,
                           double sample_rate_hz) {
  return to_db(evaluate(section, frequency_hz, sample_rate_hz));
}

double cascade_response_db(std::span<const Biquad> sections, double frequency_hz,
                           double sample_rate_hz) {
  std::complex<double> response{1.0, 0.0};
  for (const auto& section : sections) {
    response *= evaluate(section, frequency_hz, sample_rate_hz);
  }
  return to_db(response);
}

std::vector<double> marginal_contribution_db(
    std::span<const Biquad> sections, std::size_t section_index,
    std::span<const double> frequencies_hz, double sample_rate_hz) {
  if (section_index >= sections.size()) {
    throw std::out_of_range("section index is outside the cascade");
  }
  std::vector<double> result;
  result.reserve(frequencies_hz.size());
  std::vector<Biquad> without;
  without.reserve(sections.size() - 1);
  for (std::size_t i = 0; i < sections.size(); ++i) {
    if (i != section_index) without.push_back(sections[i]);
  }
  for (const auto frequency : frequencies_hz) {
    result.push_back(cascade_response_db(sections, frequency, sample_rate_hz) -
                     cascade_response_db(without, frequency, sample_rate_hz));
  }
  return result;
}

std::vector<std::vector<double>> marginal_contributions_db(
    std::span<const Biquad> sections, std::span<const double> frequencies_hz,
    double sample_rate_hz) {
  std::vector<std::vector<double>> result(
      sections.size(), std::vector<double>(frequencies_hz.size()));
  std::vector<std::complex<double>> prefix(sections.size() + 1);
  std::vector<std::complex<double>> suffix(sections.size() + 1);
  for (std::size_t point = 0; point < frequencies_hz.size(); ++point) {
    prefix[0] = {1.0, 0.0};
    for (std::size_t section = 0; section < sections.size(); ++section) {
      prefix[section + 1] =
          prefix[section] * evaluate(sections[section], frequencies_hz[point], sample_rate_hz);
    }
    suffix[sections.size()] = {1.0, 0.0};
    for (std::size_t section = sections.size(); section-- > 0;) {
      suffix[section] =
          evaluate(sections[section], frequencies_hz[point], sample_rate_hz) * suffix[section + 1];
    }
    const auto full_db = to_db(prefix.back());
    for (std::size_t section = 0; section < sections.size(); ++section) {
      const auto without_db = to_db(prefix[section] * suffix[section + 1]);
      result[section][point] = full_db - without_db;
    }
  }
  return result;
}

std::vector<double> logarithmic_frequency_grid(double low_hz, double high_hz,
                                               std::size_t point_count) {
  if (!(low_hz > 0.0 && high_hz > low_hz) || point_count < 2) {
    throw std::invalid_argument("frequency grid needs 0 < low < high and at least two points");
  }
  std::vector<double> result(point_count);
  const double ratio = high_hz / low_hz;
  for (std::size_t i = 0; i < point_count; ++i) {
    const double fraction = static_cast<double>(i) / static_cast<double>(point_count - 1);
    result[i] = low_hz * std::pow(ratio, fraction);
  }
  return result;
}

}  // namespace trench::core
