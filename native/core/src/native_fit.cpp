#include "trench/core/native_fit.hpp"

#include <ceres/jet.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
#include <numbers>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

namespace trench::core::native {
namespace {

using Parameters = std::array<double, kFitParameterCount>;
constexpr double kTau = 2.0 * std::numbers::pi;
constexpr double kLog10 = 2.30258509299404568402;

constexpr std::size_t pole_hz_index(std::size_t section) { return section; }
constexpr std::size_t pole_bw_index(std::size_t section) { return kSections + section; }
constexpr std::size_t zero_hz_index(std::size_t section) { return 2 * kSections + section; }
constexpr std::size_t zero_bw_index(std::size_t section) { return 3 * kSections + section; }
constexpr std::size_t gain_index() { return 4 * kSections; }

bool parked(const Roots& roots) {
  const auto* real = std::get_if<RealRoots>(&roots);
  return real != nullptr && !std::isfinite(real->a_hz) && !std::isfinite(real->b_hz);
}

Resonant seed_resonant(const Roots& roots, const Resonant& fallback,
                       double sample_rate_hz) {
  if (const auto* resonant = std::get_if<Resonant>(&roots)) return *resonant;
  if (parked(roots)) return fallback;
  const auto& real = std::get<RealRoots>(roots);
  const bool both_negative = std::signbit(real.a_hz) && std::signbit(real.b_hz);
  const double hz = both_negative ? sample_rate_hz * 0.49 : 20.0;
  const double bandwidth = std::clamp(std::abs(real.a_hz) + std::abs(real.b_hz),
                                      1.0e-3, sample_rate_hz * 4.0);
  return {hz, bandwidth};
}

Parameters encode(const Corner& corner, double sample_rate_hz) {
  Parameters out{};
  const double stable_pole_bw =
      -std::log(1.0 - kPoleStabilityMargin) * sample_rate_hz / std::numbers::pi;
  for (std::size_t section = 0; section < kSections; ++section) {
    const auto pole = seed_resonant(corner.sections[section].pole,
                                    {1'000.0, sample_rate_hz * 4.0}, sample_rate_hz);
    const auto zero = seed_resonant(corner.sections[section].zero,
                                    {pole.hz, sample_rate_hz * 4.0}, sample_rate_hz);
    out[pole_hz_index(section)] = std::log(std::clamp(pole.hz, 20.0, sample_rate_hz * 0.49));
    out[pole_bw_index(section)] =
        std::log(std::clamp(pole.bw_hz, stable_pole_bw, sample_rate_hz * 4.0));
    out[zero_hz_index(section)] = std::log(std::clamp(zero.hz, 20.0, sample_rate_hz * 0.49));
    out[zero_bw_index(section)] =
        std::log(std::clamp(zero.bw_hz, 1.0e-3, sample_rate_hz * 4.0));
  }
  out[gain_index()] = corner.gain_db;
  return out;
}

void project(Parameters& parameters, double sample_rate_hz) {
  const double low_hz = std::log(20.0);
  const double high_hz = std::log(sample_rate_hz * 0.49);
  const double low_pole_bw = std::log(
      -std::log(1.0 - kPoleStabilityMargin) * sample_rate_hz / std::numbers::pi);
  const double low_zero_bw = std::log(1.0e-3);
  const double high_bw = std::log(sample_rate_hz * 4.0);
  for (std::size_t section = 0; section < kSections; ++section) {
    parameters[pole_hz_index(section)] =
        std::clamp(parameters[pole_hz_index(section)], low_hz, high_hz);
    parameters[zero_hz_index(section)] =
        std::clamp(parameters[zero_hz_index(section)], low_hz, high_hz);
    parameters[pole_bw_index(section)] =
        std::clamp(parameters[pole_bw_index(section)], low_pole_bw, high_bw);
    parameters[zero_bw_index(section)] =
        std::clamp(parameters[zero_bw_index(section)], low_zero_bw, high_bw);
  }
  parameters[gain_index()] = std::clamp(parameters[gain_index()], -120.0, 48.0);
}

Corner decode(const Parameters& parameters, const Corner& base,
              std::uint32_t freedom) {
  Corner out = base;
  for (std::size_t section = 0; section < kSections; ++section) {
    if ((freedom & pole_bit(section)) != 0U) {
      out.sections[section].pole = Resonant{
          std::exp(parameters[pole_hz_index(section)]),
          std::exp(parameters[pole_bw_index(section)])};
    }
    if ((freedom & zero_bit(section)) != 0U) {
      out.sections[section].zero = Resonant{
          std::exp(parameters[zero_hz_index(section)]),
          std::exp(parameters[zero_bw_index(section)])};
      out.sections[section].dc_stabilised = true;
    }
  }
  if ((freedom & kGainBit) != 0U) out.gain_db = parameters[gain_index()];
  return out;
}

template <typename T>
std::pair<T, T> resonant_coefficients(const T& log_hz, const T& log_bw,
                                      double sample_rate_hz, bool pole) {
  using std::cos;
  using std::exp;
  T radius = exp(-std::numbers::pi * exp(log_bw) / sample_rate_hz);
  if constexpr (std::is_same_v<T, double>) {
    if (pole) radius = std::min(radius, 1.0 - kPoleStabilityMargin);
  }
  const T theta = kTau * exp(log_hz) / sample_rate_hz;
  return {-2.0 * radius * cos(theta), radius * radius};
}

template <typename T>
T loss_of(const FitTarget& target, const Corner& base, double sample_rate_hz,
          std::uint32_t freedom, const std::array<T, kFitParameterCount>& parameters) {
  using std::log;
  std::vector<T> residual;
  std::vector<double> used_weight;
  residual.reserve(target.frequency_hz.size());
  used_weight.reserve(target.frequency_hz.size());
  T weighted_mean = T(0.0);
  double weight_sum = 0.0;
  for (std::size_t point = 0; point < target.frequency_hz.size(); ++point) {
    const double hz = target.frequency_hz[point];
    const double weight = target.weight.empty() ? 1.0 : target.weight[point];
    if (!(weight > 0.0) || hz >= sample_rate_hz * 0.5) continue;
    const double omega = kTau * hz / sample_rate_hz;
    const double c1 = std::cos(omega);
    const double s1 = std::sin(omega);
    const double c2 = std::cos(2.0 * omega);
    const double s2 = std::sin(2.0 * omega);
    T response_db = (freedom & kGainBit) != 0U ? parameters[gain_index()]
                                               : T(base.gain_db);
    for (std::size_t section = 0; section < kSections; ++section) {
      std::pair<T, T> pole;
      std::pair<T, T> zero;
      if ((freedom & pole_bit(section)) != 0U) {
        pole = resonant_coefficients(parameters[pole_hz_index(section)],
                                     parameters[pole_bw_index(section)],
                                     sample_rate_hz, true);
      } else {
        const auto fixed = design(base.sections[section], sample_rate_hz);
        pole = {T(fixed.a1), T(fixed.a2)};
      }
      if ((freedom & zero_bit(section)) != 0U) {
        zero = resonant_coefficients(parameters[zero_hz_index(section)],
                                     parameters[zero_bw_index(section)],
                                     sample_rate_hz, false);
      } else {
        const auto fixed = design(base.sections[section], sample_rate_hz);
        zero = {T(fixed.b1), T(fixed.b2)};
      }
      T scale = T(1.0);
      if (base.sections[section].dc_stabilised ||
          (freedom & zero_bit(section)) != 0U) {
        scale = (T(1.0) + pole.first + pole.second) /
                (T(1.0) + zero.first + zero.second);
      }
      const T nr = scale * (T(1.0) + zero.first * c1 + zero.second * c2);
      const T ni = scale * (-zero.first * s1 - zero.second * s2);
      const T dr = T(1.0) + pole.first * c1 + pole.second * c2;
      const T di = -pole.first * s1 - pole.second * s2;
      const T magnitude_ratio = (nr * nr + ni * ni) / (dr * dr + di * di);
      response_db += (10.0 / kLog10) * log(magnitude_ratio);
    }
    residual.push_back(response_db - target.magnitude_db[point]);
    used_weight.push_back(weight);
    weighted_mean += weight * residual.back();
    weight_sum += weight;
  }
  if (!(weight_sum > 0.0)) return T(std::numeric_limits<double>::infinity());
  if (!target.absolute_level) weighted_mean /= weight_sum;
  else weighted_mean = T(0.0);
  T loss = T(0.0);
  for (std::size_t point = 0; point < residual.size(); ++point) {
    const T aligned = residual[point] - weighted_mean;
    loss += used_weight[point] * aligned * aligned;
  }
  return loss / weight_sum;
}

struct Evaluation {
  double loss{};
  Parameters gradient{};
};

Evaluation evaluate(const FitTarget& target, const Corner& base, double sample_rate_hz,
                    std::uint32_t freedom, const Parameters& parameters,
                    bool gradient) {
  Evaluation out;
  if (!gradient) {
    out.loss = loss_of(target, base, sample_rate_hz, freedom, parameters);
    return out;
  }
  using Jet = ceres::Jet<double, static_cast<int>(kFitParameterCount)>;
  std::array<Jet, kFitParameterCount> jets;
  for (std::size_t index = 0; index < jets.size(); ++index) {
    jets[index].a = parameters[index];
    jets[index].v.setZero();
    jets[index].v[static_cast<int>(index)] = 1.0;
  }
  const Jet loss = loss_of(target, base, sample_rate_hz, freedom, jets);
  out.loss = loss.a;
  for (std::size_t index = 0; index < out.gradient.size(); ++index) {
    out.gradient[index] = loss.v[static_cast<int>(index)];
  }
  return out;
}

std::vector<std::size_t> block_indices(std::size_t section, std::uint32_t freedom) {
  std::vector<std::size_t> out;
  if ((freedom & pole_bit(section)) != 0U) {
    out.push_back(pole_hz_index(section));
    out.push_back(pole_bw_index(section));
  }
  if ((freedom & zero_bit(section)) != 0U) {
    out.push_back(zero_hz_index(section));
    out.push_back(zero_bw_index(section));
  }
  return out;
}

std::uint32_t block_mask(std::size_t section, std::uint32_t freedom) {
  return freedom & (pole_bit(section) | zero_bit(section));
}

double dot(const Parameters& lhs, const Parameters& rhs,
           const std::vector<std::size_t>& indices) {
  double out = 0.0;
  for (const auto index : indices) out += lhs[index] * rhs[index];
  return out;
}

Parameters difference(const Parameters& lhs, const Parameters& rhs) {
  Parameters out{};
  for (std::size_t index = 0; index < out.size(); ++index) out[index] = lhs[index] - rhs[index];
  return out;
}

bool stopped(const StopRequested& requested) { return requested && requested(); }

void notify(const StepObserver& observer, const Corner& corner, std::size_t section,
            FitStage stage, double loss) {
  if (observer) observer(FitStep{corner, section, stage, loss});
}

}

double fit_loss(const FitTarget& target, const Corner& corner,
                double sample_rate_hz) {
  if (!target.valid() || !(sample_rate_hz > 0.0)) {
    return std::numeric_limits<double>::infinity();
  }
  const auto parameters = encode(corner, sample_rate_hz);
  return evaluate(target, corner, sample_rate_hz, 0U, parameters, false).loss;
}

FitResult fit_corner(const FitTarget& target, const Corner& seed,
                     double sample_rate_hz, FreedomMask freedom_mask,
                     StopRequested stop_requested, StepObserver observer,
                     const FitOptions& options) {
  FitResult result;
  result.corner = seed;
  if (!target.valid() || !(sample_rate_hz > 0.0)) {
    result.initial_loss = result.final_loss = std::numeric_limits<double>::infinity();
    return result;
  }
  if (!freedom_mask) freedom_mask = [] { return kAllFree; };
  Parameters parameters = encode(seed, sample_rate_hz);
  Corner working = seed;
  project(parameters, sample_rate_hz);
  auto freedom = freedom_mask();
  auto current = evaluate(target, working, sample_rate_hz, freedom, parameters, true);
  result.initial_loss = fit_loss(target, working, sample_rate_hz);
  result.final_loss = result.initial_loss;

  Parameters first_moment{};
  Parameters second_moment{};
  for (std::size_t iteration = 1; iteration <= options.adam_steps; ++iteration) {
    if (stopped(stop_requested)) {
      result.stopped = true;
      break;
    }
    freedom = freedom_mask();
    current = evaluate(target, working, sample_rate_hz, freedom, parameters, true);
    std::size_t best_section = kSections;
    double best_norm = 0.0;
    for (std::size_t section = 0; section < kSections; ++section) {
      const auto indices = block_indices(section, freedom);
      double norm = 0.0;
      for (const auto index : indices) {
        first_moment[index] = 0.9 * first_moment[index] + 0.1 * current.gradient[index];
        second_moment[index] = 0.999 * second_moment[index] +
                               0.001 * current.gradient[index] * current.gradient[index];
        const double m_hat = first_moment[index] /
                             (1.0 - std::pow(0.9, static_cast<double>(iteration)));
        const double v_hat = second_moment[index] /
                             (1.0 - std::pow(0.999, static_cast<double>(iteration)));
        const double direction = m_hat / (std::sqrt(v_hat) + 1.0e-8);
        norm += direction * direction;
      }
      if (norm > best_norm) {
        best_norm = norm;
        best_section = section;
      }
    }
    if (best_section == kSections || !(best_norm > 0.0)) break;
    const auto indices = block_indices(best_section, freedom);
    bool accepted = false;
    for (std::size_t backtrack = 0; backtrack < 10; ++backtrack) {
      Parameters trial = parameters;
      const double rate = options.adam_learning_rate * std::pow(0.5, static_cast<double>(backtrack));
      for (const auto index : indices) {
        const double m_hat = first_moment[index] /
                             (1.0 - std::pow(0.9, static_cast<double>(iteration)));
        const double v_hat = second_moment[index] /
                             (1.0 - std::pow(0.999, static_cast<double>(iteration)));
        trial[index] -= rate * m_hat / (std::sqrt(v_hat) + 1.0e-8);
      }
      project(trial, sample_rate_hz);
      const double trial_loss =
          evaluate(target, working, sample_rate_hz, freedom, trial, false).loss;
      const auto trial_corner =
          decode(trial, working, block_mask(best_section, freedom));
      const double exact_trial_loss =
          fit_loss(target, trial_corner, sample_rate_hz);
      if (std::isfinite(trial_loss) &&
          trial_loss + options.minimum_improvement < current.loss &&
          exact_trial_loss + options.minimum_improvement < result.final_loss) {
        parameters = trial;
        working = trial_corner;
        current = evaluate(target, working, sample_rate_hz, freedom, parameters, true);
        result.corner = working;
        result.final_loss = exact_trial_loss;
        ++result.accepted_steps;
        notify(observer, result.corner, best_section, FitStage::kAdam,
               result.final_loss);
        accepted = true;
        break;
      }
    }
    if (!accepted) break;
  }

  if (!result.stopped) {
    for (std::size_t section = 0; section < kSections; ++section) {
      std::deque<Parameters> s_history;
      std::deque<Parameters> y_history;
      std::deque<double> rho_history;
      for (std::size_t iteration = 0; iteration < options.lbfgs_steps_per_section;
           ++iteration) {
        if (stopped(stop_requested)) {
          result.stopped = true;
          break;
        }
        freedom = freedom_mask();
        const auto indices = block_indices(section, freedom);
        if (indices.empty()) break;
        const auto before = evaluate(target, working, sample_rate_hz, freedom, parameters, true);
        const double exact_before = fit_loss(target, working, sample_rate_hz);
        Parameters direction{};
        Parameters q = before.gradient;
        std::vector<double> alpha(s_history.size(), 0.0);
        for (std::size_t i = s_history.size(); i-- > 0;) {
          alpha[i] = rho_history[i] * dot(s_history[i], q, indices);
          for (const auto index : indices) q[index] -= alpha[i] * y_history[i][index];
        }
        double scale = 1.0;
        if (!s_history.empty()) {
          const double yy = dot(y_history.back(), y_history.back(), indices);
          if (yy > 1.0e-18) scale = dot(s_history.back(), y_history.back(), indices) / yy;
        }
        for (const auto index : indices) direction[index] = scale * q[index];
        for (std::size_t i = 0; i < s_history.size(); ++i) {
          const double beta = rho_history[i] * dot(y_history[i], direction, indices);
          for (const auto index : indices) {
            direction[index] += s_history[i][index] * (alpha[i] - beta);
          }
        }
        for (const auto index : indices) direction[index] = -direction[index];
        const double slope = dot(before.gradient, direction, indices);
        if (!(slope < -1.0e-18)) break;
        bool accepted = false;
        for (std::size_t backtrack = 0; backtrack < 16; ++backtrack) {
          const double step = std::pow(0.5, static_cast<double>(backtrack));
          Parameters trial = parameters;
          for (const auto index : indices) trial[index] += step * direction[index];
          project(trial, sample_rate_hz);
          const auto after = evaluate(target, working, sample_rate_hz, freedom, trial, true);
          const auto trial_corner = decode(trial, working, block_mask(section, freedom));
          const double exact_after = fit_loss(target, trial_corner, sample_rate_hz);
          if (std::isfinite(after.loss) &&
              after.loss <= before.loss + 1.0e-4 * step * slope &&
              after.loss + options.minimum_improvement < before.loss &&
              exact_after + options.minimum_improvement < exact_before) {
            const Parameters s = difference(trial, parameters);
            const Parameters y = difference(after.gradient, before.gradient);
            const double sy = dot(s, y, indices);
            if (sy > 1.0e-12) {
              s_history.push_back(s);
              y_history.push_back(y);
              rho_history.push_back(1.0 / sy);
              while (s_history.size() > options.lbfgs_memory) {
                s_history.pop_front();
                y_history.pop_front();
                rho_history.pop_front();
              }
            }
            parameters = trial;
            working = trial_corner;
            result.corner = working;
            result.final_loss = exact_after;
            ++result.accepted_steps;
            notify(observer, result.corner, section, FitStage::kLbfgs,
                   result.final_loss);
            accepted = true;
            break;
          }
        }
        if (!accepted) break;
      }
      if (result.stopped) break;
    }
  }

  freedom = freedom_mask();
  if (!result.stopped && target.absolute_level && (freedom & kGainBit) != 0U) {
    Corner without_gain = working;
    const double current_gain = without_gain.gain_db;
    without_gain.gain_db = 0.0;
    double sum = 0.0;
    double weight_sum = 0.0;
    const auto response_cascade = cascade(design(without_gain, sample_rate_hz), 0.0);
    for (std::size_t point = 0; point < target.frequency_hz.size(); ++point) {
      const double weight = target.weight.empty() ? 1.0 : target.weight[point];
      if (!(weight > 0.0) || target.frequency_hz[point] >= sample_rate_hz * 0.5) continue;
      const double model = cascade_response_db(response_cascade, target.frequency_hz[point],
                                               sample_rate_hz);
      sum += weight * (target.magnitude_db[point] - model);
      weight_sum += weight;
    }
    if (weight_sum > 0.0) {
      parameters[gain_index()] = std::clamp(sum / weight_sum, -120.0, 48.0);
      if (std::abs(parameters[gain_index()] - current_gain) > 1.0e-12) {
        auto trial = working;
        trial.gain_db = parameters[gain_index()];
        const double exact_after = fit_loss(target, trial, sample_rate_hz);
        if (exact_after + options.minimum_improvement < result.final_loss) {
          working = trial;
          result.corner = working;
          result.final_loss = exact_after;
          ++result.accepted_steps;
          notify(observer, result.corner, kCornerGainStep, FitStage::kGain,
                 result.final_loss);
        }
      }
    }
  }
  result.corner = working;
  result.final_loss = fit_loss(target, working, sample_rate_hz);
  return result;
}

}
