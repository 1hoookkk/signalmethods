#include "trench/core/measure.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

namespace {

using namespace trench::core;

constexpr double kRate = 44100.0;
constexpr double kF0 = 49.14;

std::vector<float> band_limited_sawtooth(double f0, double seconds) {
  const auto count = static_cast<std::size_t>(seconds * kRate);
  std::vector<float> out(count, 0.0f);
  for (std::size_t k = 1; static_cast<double>(k) * f0 < 0.5 * kRate; ++k) {
    const double amplitude = 1.0 / static_cast<double>(k);
    for (std::size_t i = 0; i < count; ++i) {
      const double t = static_cast<double>(i) / kRate;
      out[i] += static_cast<float>(amplitude *
                                   std::sin(2.0 * std::numbers::pi * static_cast<double>(k) * f0 * t));
    }
  }
  return out;
}

std::vector<float> run_cascade(std::vector<float> x, const std::vector<PackedSection>& sections) {
  for (const auto& words : sections) {
    const auto b = section_words_to_biquad(words);
    double w1 = 0.0;
    double w2 = 0.0;
    for (auto& sample : x) {
      const double w0 = sample - b[3] * w1 - b[4] * w2;
      sample = static_cast<float>(b[0] * w0 + b[1] * w1 + b[2] * w2);
      w2 = w1;
      w1 = w0;
    }
  }
  return x;
}

std::vector<PackedSection> resonant_lowpass() {
  return {
      words_from_geometry({ConjugatePair{1200.0, 0.97}, DegeneratePair{}, 0.25}, kRate),
      words_from_geometry({ConjugatePair{400.0, 0.9}, ConjugatePair{3000.0, 0.8}, 1.0}, kRate),
  };
}

std::vector<std::uint16_t> flatten(const std::vector<PackedSection>& sections) {
  std::vector<std::uint16_t> words;
  for (const auto& s : sections) words.insert(words.end(), s.begin(), s.end());
  return words;
}

TEST(Measure, FundamentalIsFoundToWithinAHundredthOfAHertz) {
  const auto envelope =
      measure::harmonic_envelope(band_limited_sawtooth(kF0, 2.0), kRate, measure::Source::kFlat);
  EXPECT_NEAR(envelope.f0_hz, kF0, 0.01);
  EXPECT_GT(envelope.hz.size(), 300U);
  EXPECT_NEAR(envelope.hz[9], 10.0 * kF0, 0.1);
}

TEST(Measure, SawtoothCorrectionFlattensAnUnfilteredSawtooth) {
  const auto envelope = measure::harmonic_envelope(band_limited_sawtooth(kF0, 2.0), kRate,
                                                   measure::Source::kSawtooth);
  for (std::size_t k = 0; k < 40; ++k) {
    EXPECT_NEAR(envelope.db[k], envelope.db[0], 0.2) << "harmonic " << k + 1;
  }
}

TEST(Measure, FilteredSawtoothScoresAgainstItsOwnWordsUnderHalfADecibel) {
  const auto sections = resonant_lowpass();
  const auto audio = run_cascade(band_limited_sawtooth(kF0, 2.0), sections);
  const auto envelope = measure::harmonic_envelope(audio, kRate, measure::Source::kSawtooth);
  const auto& grid = p2k::grid();
  std::vector<double> hz;
  std::vector<double> weight;
  for (std::size_t i = 0; i < grid.hz.size(); ++i) {
    if (grid.hz[i] >= kF0 && grid.hz[i] <= 12000.0) {
      hz.push_back(grid.hz[i]);
      weight.push_back(grid.weight[i]);
    }
  }
  const auto target = measure::target_on_grid(envelope, hz);
  const auto report = measure::score_words(flatten(sections), kRate, hz, weight, target);
  EXPECT_LT(report.rms_db, 0.5) << "worst " << report.worst_db;
  EXPECT_LT(report.worst_db, 2.0);

  const auto wrong = measure::score_words(
      flatten({words_from_geometry({ConjugatePair{300.0, 0.97}, DegeneratePair{}, 0.25}, kRate)}),
      kRate, hz, weight, target);
  EXPECT_GT(wrong.rms_db, 3.0);
}

TEST(Measure, LpcRecoversTwoFormantsFromAPulsedVowel) {
  const std::vector<std::pair<double, double>> formants{{700.0, 80.0}, {1200.0, 100.0}};
  const auto count = static_cast<std::size_t>(0.5 * kRate);
  std::vector<float> out(count, 0.0f);
  std::vector<double> signal(count, 0.0);
  const auto period = static_cast<std::size_t>(kRate / 120.0);
  for (std::size_t i = 0; i < count; i += period) signal[i] = 1.0;
  for (int pass = 0; pass < 2; ++pass) {
    double y = 0.0;
    for (auto& x : signal) {
      y = x + 0.98 * y;
      x = y;
    }
  }
  for (const auto& [hz, bw] : formants) {
    const double r = std::exp(-std::numbers::pi * bw / kRate);
    const double a1 = -2.0 * r * std::cos(2.0 * std::numbers::pi * hz / kRate);
    const double a2 = r * r;
    double y1 = 0.0;
    double y2 = 0.0;
    for (auto& x : signal) {
      const double y = x - a1 * y1 - a2 * y2;
      y2 = y1;
      y1 = y;
      x = y;
    }
  }
  double prev = 0.0;
  for (std::size_t i = 0; i < count; ++i) {
    const double radiated = signal[i] - 0.97 * prev;
    prev = signal[i];
    out[i] = static_cast<float>(0.1 * radiated);
  }
  const auto envelope = measure::lpc_envelope(out, kRate, {.order = 8});
  ASSERT_GE(envelope.formants.size(), 2U);
  EXPECT_NEAR(envelope.formants[0].hz, 700.0, 20.0);
  EXPECT_NEAR(envelope.formants[1].hz, 1200.0, 30.0);
  EXPECT_LT(envelope.formants[0].bw_hz, 200.0);
  const auto& grid = p2k::grid();
  const auto target = measure::target_on_grid(envelope, grid.hz);
  ASSERT_EQ(target.size(), grid.hz.size());
  std::size_t at_700 = 0;
  std::size_t at_950 = 0;
  for (std::size_t i = 0; i < grid.hz.size(); ++i) {
    if (std::abs(grid.hz[i] - 700.0) < std::abs(grid.hz[at_700] - 700.0)) at_700 = i;
    if (std::abs(grid.hz[i] - 950.0) < std::abs(grid.hz[at_950] - 950.0)) at_950 = i;
  }
  EXPECT_GT(target[at_700] - target[at_950], 6.0);
}

TEST(Measure, WeightedErrorRemovesTheOffsetAndIgnoresZeroWeightPoints) {
  const std::vector<double> target{10.0, 12.0, 14.0, 100.0};
  const std::vector<double> model{0.0, 2.0, 4.0, 0.0};
  const std::vector<double> weight{1.0, 1.0, 1.0, 0.0};
  const auto report = measure::weighted_error(target, model, weight);
  EXPECT_NEAR(report.offset_db, 10.0, 1e-12);
  EXPECT_NEAR(report.rms_db, 0.0, 1e-12);
  EXPECT_NEAR(report.worst_db, 0.0, 1e-12);
}

}  // namespace
