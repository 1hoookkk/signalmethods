#pragma once

#include "trench/core/fit_target.hpp"

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

struct Formant {
  double hz{};
  double bw_hz{};
};

struct LpcOptions {
  std::size_t order{0};
  double pre_emphasis{0.97};
  double max_bw_hz{500.0};
  double low_hz{90.0};
  std::size_t frame{32768};
};

struct LpcEnvelope {
  double gain{};
  std::vector<double> a;
  double sample_rate_hz{};
  std::vector<Formant> formants;
};

LpcEnvelope lpc_envelope(std::span<const float> mono, double sample_rate_hz,
                         const LpcOptions& options = {});

std::vector<double> target_on_grid(const LpcEnvelope& envelope, std::span<const double> grid_hz);

struct TransferOptions {
  std::size_t fft_size{4096};
  std::size_t hop_size{2048};
  std::size_t points{512};
  double low_hz{20.0};
  double top_hz{20'000.0};
  double smoothing_octaves{1.0 / 6.0};
};

FitTarget transfer_function(std::span<const float> input,
                            std::span<const float> output,
                            double sample_rate_hz,
                            const TransferOptions& options = {});

ErrorReport weighted_error(std::span<const double> target_db, std::span<const double> model_db,
                           std::span<const double> weight);

ErrorReport score_words(std::span<const std::uint16_t> words, double sample_rate_hz,
                        std::span<const double> grid_hz, std::span<const double> weight,
                        std::span<const double> target_db);

}  // namespace trench::core::measure
