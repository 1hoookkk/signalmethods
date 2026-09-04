#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace trench::core::audio {

struct Resonance {
  double hz{};
  double bw_hz{};
  double gain_db{};
};

struct AllPoleModel {
  std::vector<double> a;
  double gain{};
  double sample_rate_hz{};
};

struct MonoClip {
  std::vector<float> samples;
  double sample_rate_hz{};
};

struct SeedRules {
  double min_prominence_db{6.0};
  double min_q{10.0};
  double max_gain_db{40.0};
};

[[nodiscard]] constexpr std::size_t model_order(std::size_t count) {
  return 2 * count + 18;
}

std::optional<MonoClip> read_wav_mono(const std::filesystem::path& path);

AllPoleModel all_pole_model(std::span<const float> mono, double sample_rate_hz,
                            std::size_t order);

std::vector<double> envelope_db(const AllPoleModel& model,
                                std::span<const double> grid_hz);

std::vector<Resonance> resonances(const AllPoleModel& model, std::size_t count);

std::vector<Resonance> resonances_from_audio(std::span<const float> mono,
                                             double sample_rate_hz,
                                             std::size_t count = 6);

std::vector<Resonance> neutral_rows(const AllPoleModel& model, std::size_t count,
                                    const SeedRules& rules = {});

std::vector<float> resample(std::span<const float> mono, double from_hz, double to_hz);

std::vector<Resonance> speech_poles(std::span<const float> mono, double sample_rate_hz,
                                    std::size_t count, double model_rate_hz = 11'025.0,
                                    std::size_t order = 12);

}
