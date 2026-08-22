#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace trench::core::measure {

enum class Source { kFlat, kSawtooth };

struct Options {
  double f0_low_hz{30.0};
  double f0_high_hz{2000.0};
  double top_hz{18000.0};
  std::size_t fft_size{32768};
};

struct HarmonicEnvelope {
  double f0_hz{};
  std::vector<double> hz;
  std::vector<double> db;
};

struct ErrorReport {
  double rms_db{};
  double worst_db{};
  double offset_db{};
};

HarmonicEnvelope harmonic_envelope(std::span<const float> mono, double sample_rate_hz,
                                   Source source, const Options& options = {});

std::vector<double> target_on_grid(const HarmonicEnvelope& envelope,
                                   std::span<const double> grid_hz);

ErrorReport weighted_error(std::span<const double> target_db, std::span<const double> model_db,
                           std::span<const double> weight);

ErrorReport score_words(std::span<const std::uint16_t> words, double sample_rate_hz,
                        std::span<const double> grid_hz, std::span<const double> weight,
                        std::span<const double> target_db);

}  // namespace trench::core::measure
