#pragma once

#include "trench/core/fit_target.hpp"
#include "trench/core/native_body.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace trench::core::native {

inline constexpr std::size_t kFitParameterCount = 4 * kSections + 1;
inline constexpr std::size_t kCornerGainStep = kSections;

enum class FitStage { kAdam, kLbfgs, kGain };

struct FitOptions {
  std::size_t adam_steps{72};
  std::size_t lbfgs_steps_per_section{8};
  std::size_t lbfgs_memory{5};
  double adam_learning_rate{0.045};
  double minimum_improvement{1.0e-9};
};

struct FitStep {
  Corner corner;
  std::size_t section{};
  FitStage stage{FitStage::kAdam};
  double loss{};
};

struct FitResult {
  Corner corner;
  double initial_loss{};
  double final_loss{};
  std::size_t accepted_steps{};
  bool stopped{};
};

using FreedomMask = std::function<std::uint32_t()>;
using StopRequested = std::function<bool()>;
using StepObserver = std::function<void(const FitStep&)>;

double fit_loss(const FitTarget& target, const Corner& corner,
                double sample_rate_hz);

FitResult fit_corner(const FitTarget& target, const Corner& seed,
                     double sample_rate_hz, FreedomMask freedom_mask,
                     StopRequested stop_requested = {},
                     StepObserver observer = {}, const FitOptions& options = {});

}  // namespace trench::core::native
