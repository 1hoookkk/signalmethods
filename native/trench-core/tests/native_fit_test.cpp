#include "trench/core/native_fit.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace native = trench::core::native;

namespace {

native::Corner example_corner() {
  native::Corner corner;
  constexpr std::array<double, native::kSections> pole_hz{
      180.0, 430.0, 950.0, 2'100.0, 4'800.0, 9'500.0};
  constexpr std::array<double, native::kSections> zero_hz{
      240.0, 620.0, 1'300.0, 2'900.0, 6'300.0, 12'000.0};
  for (std::size_t section = 0; section < native::kSections; ++section) {
    corner.sections[section].pole = native::Resonant{pole_hz[section], 90.0 + 35.0 * section};
    corner.sections[section].zero = native::Resonant{zero_hz[section], 260.0 + 60.0 * section};
    corner.sections[section].dc_stabilised = true;
  }
  corner.gain_db = -4.25;
  return corner;
}

trench::core::FitTarget target_from(const native::Corner& corner, double sample_rate_hz,
                                    bool absolute = true) {
  trench::core::FitTarget target;
  target.frequency_hz = trench::core::logarithmic_frequency_grid(
      30.0, sample_rate_hz * 0.47, 96);
  target.weight.assign(target.frequency_hz.size(), 1.0);
  const auto cascade = native::cascade(native::design(corner, sample_rate_hz), corner.gain_db);
  for (const double hz : target.frequency_hz) {
    target.magnitude_db.push_back(
        trench::core::cascade_response_db(cascade, hz, sample_rate_hz));
  }
  target.kind = trench::core::TargetKind::kTransferFunction;
  target.absolute_level = absolute;
  return target;
}

TEST(NativeFit, LossIsTheExactCompleteRuntimeCascadeMagnitude) {
  constexpr double rate = 48'000.0;
  auto corner = example_corner();
  corner.sections[4].pole = native::Resonant{4'800.0, 1.0e-12};
  const auto target = target_from(corner, rate);
  EXPECT_NEAR(native::fit_loss(target, corner, rate), 0.0, 1.0e-15);
}

TEST(NativeFit, AdamMovesOnlyTheFreeNativeZeroAndLowersCascadeError) {
  constexpr double rate = 48'000.0;
  const auto wanted = example_corner();
  auto seed = wanted;
  seed.sections[2].zero = native::Resonant{4'600.0, 1'800.0};
  const auto target = target_from(wanted, rate, false);
  std::uint32_t mask = native::zero_bit(2);
  const auto poles_before = seed.sections;
  std::vector<native::FitStep> steps;
  native::FitOptions options;
  options.adam_steps = 32;
  options.lbfgs_steps_per_section = 0;
  const auto result = native::fit_corner(
      target, seed, rate, [&mask] { return mask; }, {},
      [&steps](const native::FitStep& step) { steps.push_back(step); }, options);
  ASSERT_FALSE(steps.empty());
  EXPECT_LT(result.final_loss, result.initial_loss);
  for (const auto& step : steps) {
    EXPECT_EQ(step.section, 2U);
    EXPECT_EQ(step.stage, native::FitStage::kAdam);
  }
  for (std::size_t section = 0; section < native::kSections; ++section) {
    EXPECT_EQ(result.corner.sections[section].pole, poles_before[section].pole);
    if (section != 2) {
      EXPECT_EQ(result.corner.sections[section].zero, poles_before[section].zero);
    }
  }
  EXPECT_EQ(result.corner.gain_db, seed.gain_db);
}

TEST(NativeFit, ParkedZerosEnterOneSectionAtATimeOnlyAfterExactLossImproves) {
  constexpr double rate = 48'000.0;
  const auto wanted = example_corner();
  auto seed = wanted;
  const native::RealRoots parked{std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity()};
  std::uint32_t mask = 0U;
  for (std::size_t section = 0; section < native::kSections; ++section) {
    seed.sections[section].zero = parked;
    mask |= native::zero_bit(section);
  }
  const auto target = target_from(wanted, rate, false);
  auto previous = seed;
  double previous_loss = native::fit_loss(target, seed, rate);
  std::size_t observed = 0;
  native::FitOptions options;
  options.adam_steps = 12;
  options.lbfgs_steps_per_section = 0;
  const auto result = native::fit_corner(
      target, seed, rate, [mask] { return mask; }, {},
      [&](const native::FitStep& step) {
        std::size_t changed = 0;
        for (std::size_t section = 0; section < native::kSections; ++section) {
          if (step.corner.sections[section] != previous.sections[section]) {
            ++changed;
            EXPECT_EQ(section, step.section);
          }
        }
        EXPECT_EQ(changed, 1U);
        EXPECT_LT(step.loss, previous_loss);
        previous = step.corner;
        previous_loss = step.loss;
        ++observed;
      },
      options);
  EXPECT_GT(observed, 0U);
  EXPECT_LT(result.final_loss, result.initial_loss);
}

TEST(NativeFit, LbfgsRefinesOneSectionAtATimeAndALivePinHoldsItsLastState) {
  constexpr double rate = 44'100.0;
  const auto wanted = example_corner();
  auto seed = wanted;
  seed.sections[0].zero = native::Resonant{700.0, 900.0};
  seed.sections[1].zero = native::Resonant{1'900.0, 1'100.0};
  const auto target = target_from(wanted, rate, false);
  std::uint32_t mask = native::zero_bit(0) | native::zero_bit(1);
  std::optional<native::Roots> held;
  native::FitOptions options;
  options.adam_steps = 0;
  options.lbfgs_steps_per_section = 8;
  const auto result = native::fit_corner(
      target, seed, rate, [&mask] { return mask; }, {},
      [&](const native::FitStep& step) {
        EXPECT_EQ(step.stage, native::FitStage::kLbfgs);
        EXPECT_LT(step.section, native::kSections);
        if (step.section == 0 && !held) {
          held = step.corner.sections[0].zero;
          mask &= ~native::zero_bit(0);
        }
      },
      options);
  ASSERT_TRUE(held.has_value());
  EXPECT_EQ(result.corner.sections[0].zero, *held);
  EXPECT_LT(result.final_loss, result.initial_loss);
}

TEST(NativeFit, AbsoluteTargetAdjustsTheSingleCornerGainSeparately) {
  constexpr double rate = 48'000.0;
  const auto wanted = example_corner();
  auto seed = wanted;
  seed.gain_db = 7.0;
  const auto target = target_from(wanted, rate, true);
  native::FitOptions options;
  options.adam_steps = 0;
  options.lbfgs_steps_per_section = 0;
  std::vector<native::FitStep> steps;
  const auto result = native::fit_corner(
      target, seed, rate, [] { return native::kGainBit; }, {},
      [&](const native::FitStep& step) { steps.push_back(step); }, options);
  ASSERT_EQ(steps.size(), 1U);
  EXPECT_EQ(steps.front().stage, native::FitStage::kGain);
  EXPECT_EQ(steps.front().section, native::kCornerGainStep);
  EXPECT_NEAR(result.corner.gain_db, wanted.gain_db, 1.0e-10);
  EXPECT_NEAR(result.final_loss, 0.0, 1.0e-18);
}

}  // namespace
