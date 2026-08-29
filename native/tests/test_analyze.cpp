#include "harness.hpp"

#include "analyze.hpp"
#include "trench/audio/audio_boundary.hpp"

#include <filesystem>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <utility>
#include <vector>

namespace {

constexpr double kRate = 44'100.0;
constexpr double kPitchHz = 110.0;

struct Root {
  double hz;
  double bw_hz;
};

constexpr std::array<Root, 4> kPoles{{{280.0, 60.0}, {1250.0, 120.0}, {2300.0, 160.0}, {3300.0, 250.0}}};
constexpr Root kAntiformant{900.0, 150.0};

std::vector<float> nasal(double seconds) {
  const auto frames = static_cast<std::size_t>(seconds * kRate);
  std::vector<double> x(frames, 0.0);
  const double period = kRate / kPitchHz;
  for (double t = 0.0; t < static_cast<double>(frames); t += period) {
    x[static_cast<std::size_t>(t)] = 1.0;
  }
  std::uint32_t seed = 12345u;
  for (double& v : x) {
    seed = seed * 1664525u + 1013904223u;
    v += 1.0e-4 * (static_cast<double>(seed >> 8) / 16777216.0 - 0.5);
  }
  const auto section = [](double hz, double bw) {
    const double r = std::exp(-std::numbers::pi * bw / kRate);
    const double c = -2.0 * r * std::cos(2.0 * std::numbers::pi * hz / kRate);
    return std::pair{c, r * r};
  };
  {
    const auto [b1, b2] = section(kAntiformant.hz, kAntiformant.bw_hz);
    std::vector<double> y(frames);
    for (std::size_t n = 0; n < frames; ++n) {
      y[n] = x[n] + (n >= 1 ? b1 * x[n - 1] : 0.0) + (n >= 2 ? b2 * x[n - 2] : 0.0);
    }
    x.swap(y);
  }
  for (const Root& pole : kPoles) {
    const auto [a1, a2] = section(pole.hz, pole.bw_hz);
    for (std::size_t n = 0; n < frames; ++n) {
      x[n] -= (n >= 1 ? a1 * x[n - 1] : 0.0) + (n >= 2 ? a2 * x[n - 2] : 0.0);
    }
  }
  double peak = 0.0;
  for (const double v : x) peak = std::max(peak, std::abs(v));
  std::vector<float> out(frames);
  for (std::size_t n = 0; n < frames; ++n) out[n] = static_cast<float>(0.5 * x[n] / peak);
  return out;
}

const std::pair<double, double>* nearest(const std::vector<std::pair<double, double>>& found, double hz) {
  const std::pair<double, double>* best = nullptr;
  for (const auto& root : found) {
    if (best == nullptr || std::abs(std::log2(root.first / hz)) < std::abs(std::log2(best->first / hz))) {
      best = &root;
    }
  }
  return best;
}

}  // namespace

TRENCH_TEST(analyze_recovers_nasal_poles_and_antiformant) {
  const auto sound = nasal(1.0);
  const auto proposal = trench::app::analyzeSound(sound, kRate);
  std::printf("poles:");
  for (const auto& p : proposal.poles) std::printf("  %.0f/%.0f", p.first, p.second);
  std::printf("\nzeros:");
  for (const auto& z : proposal.zeros) std::printf("  %.0f/%.0f", z.first, z.second);
  std::printf("\n");
  for (const Root& pole : kPoles) {
    const auto* got = nearest(proposal.poles, pole.hz);
    CHECK(got != nullptr);
    std::printf("pole %.0f/%.0f -> %.1f/%.1f\n", pole.hz, pole.bw_hz, got->first, got->second);
    CHECK_NEAR(got->first, pole.hz, 0.05 * pole.hz);
    CHECK(got->second > 0.5 * pole.bw_hz && got->second < 2.0 * pole.bw_hz);
  }
  const auto* zero = nearest(proposal.zeros, kAntiformant.hz);
  CHECK(zero != nullptr);
  std::printf("antiformant %.0f/%.0f -> %.1f/%.1f\n", kAntiformant.hz, kAntiformant.bw_hz, zero->first, zero->second);
  CHECK_NEAR(zero->first, kAntiformant.hz, 0.06 * kAntiformant.hz);
  CHECK(zero->second < 4.0 * kAntiformant.bw_hz);
  CHECK(proposal.zeros.size() <= 2);
}

TRENCH_TEST(analyze_finds_the_recorded_ah_formants) {
  const auto clip = trench::audio::decode_mono(
      std::filesystem::path(TRENCH_SOURCE_ROOT) / "recipes" / "recordings" / "test-vowel-ah.wav");
  CHECK(clip.has_value());
  const auto proposal = trench::app::analyzeSound(clip->samples, clip->sample_rate_hz);
  std::printf("poles:");
  for (const auto& p : proposal.poles) std::printf("  %.0f/%.0f", p.first, p.second);
  std::printf("\nzeros:");
  for (const auto& z : proposal.zeros) std::printf("  %.0f/%.0f", z.first, z.second);
  std::printf("\n");
  constexpr std::array<std::pair<double, double>, 3> kFormants{{{680.0, 0.12}, {1070.0, 0.10}, {2600.0, 0.08}}};
  for (const auto& [hz, tolerance] : kFormants) {
    const auto* got = nearest(proposal.poles, hz);
    CHECK(got != nullptr);
    CHECK_NEAR(got->first, hz, tolerance * hz);
    CHECK(got->second > 20.0 && got->second < 400.0);
  }
  CHECK(proposal.poles.size() <= 6);
  CHECK(proposal.zeros.size() <= 6);
}
