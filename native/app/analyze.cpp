#include "analyze.hpp"

#include "trench/core/native_body.hpp"

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>

namespace trench::app {
namespace {

// ORACLE (Tyson 2026-08-29 "analyze"): an impulse response built from poles
// (700, 80) and (2200, 150) with zeros (1400, 200) and (3100, 400) at
// 10,000 Hz returns those four roots to +-0.1 Hz in frequency and in
// bandwidth through the 4/4 Prony-Shanks solve this chain generalises.

constexpr int kLpcOrder = 24;
constexpr int kPronyOrder = 12;
constexpr int kImpulseLength = 512;
constexpr int kPronyFirstRow = 13;
constexpr int kResampleTaps = 64;
constexpr double kAnalysisHz = 10'000.0;
constexpr double kAntiAliasHz = 4'700.0;
constexpr double kSegmentSeconds = 0.5;
constexpr double kHopSeconds = 0.05;
constexpr double kLowestHz = 20.0;
constexpr double kHighestHz = 4'900.0;

double hamming(std::size_t index, std::size_t count) {
  if (count < 2) return 1.0;
  return 0.54 - 0.46 * std::cos(2.0 * std::numbers::pi *
                                static_cast<double>(index) /
                                static_cast<double>(count - 1));
}

std::vector<double> loudestSegment(std::span<const float> samples,
                                   double sample_rate_hz) {
  const std::size_t window = std::max<std::size_t>(
      1, static_cast<std::size_t>(std::llround(kSegmentSeconds * sample_rate_hz)));
  const std::size_t hop = std::max<std::size_t>(
      1, static_cast<std::size_t>(std::llround(kHopSeconds * sample_rate_hz)));
  std::size_t best = 0;
  if (samples.size() > window) {
    double loudest = -1.0;
    for (std::size_t start = 0; start + window <= samples.size(); start += hop) {
      double energy = 0.0;
      for (std::size_t n = 0; n < window; ++n) {
        const double value = samples[start + n];
        energy += value * value;
      }
      if (energy > loudest) {
        loudest = energy;
        best = start;
      }
    }
  }
  const std::size_t length = std::min(window, samples.size());
  std::vector<double> segment(length);
  double peak = 0.0;
  for (std::size_t n = 0; n < length; ++n) {
    segment[n] = samples[best + n];
    peak = std::max(peak, std::abs(segment[n]));
  }
  if (peak > 0.0) {
    for (double& value : segment) value /= peak;
  }
  return segment;
}

std::vector<double> atAnalysisRate(const std::vector<double>& segment,
                                   double sample_rate_hz) {
  if (!(sample_rate_hz > kAnalysisHz)) return segment;

  const double cutoff = kAntiAliasHz / sample_rate_hz;
  std::array<double, kResampleTaps> kernel{};
  double sum = 0.0;
  for (int tap = 0; tap < kResampleTaps; ++tap) {
    const double offset =
        static_cast<double>(tap) - 0.5 * static_cast<double>(kResampleTaps - 1);
    const double phase = 2.0 * std::numbers::pi * cutoff * offset;
    const double sinc =
        std::abs(phase) < 1e-12 ? 1.0 : std::sin(phase) / phase;
    kernel[tap] = 2.0 * cutoff * sinc *
                  hamming(static_cast<std::size_t>(tap), kResampleTaps);
    sum += kernel[tap];
  }
  if (std::abs(sum) > 1e-12) {
    for (double& tap : kernel) tap /= sum;
  }

  std::vector<double> filtered(segment.size(), 0.0);
  for (std::size_t n = 0; n < segment.size(); ++n) {
    double accumulator = 0.0;
    const int reach = static_cast<int>(std::min<std::size_t>(n, kResampleTaps - 1));
    for (int tap = 0; tap <= reach; ++tap) {
      accumulator += kernel[static_cast<std::size_t>(tap)] *
                     segment[n - static_cast<std::size_t>(tap)];
    }
    filtered[n] = accumulator;
  }

  const double step = sample_rate_hz / kAnalysisHz;
  const std::size_t frames =
      static_cast<std::size_t>(static_cast<double>(filtered.size()) / step);
  std::vector<double> resampled(frames);
  for (std::size_t n = 0; n < frames; ++n) {
    const double position = static_cast<double>(n) * step;
    const std::size_t first =
        std::min(static_cast<std::size_t>(position), filtered.size() - 1);
    const std::size_t second = std::min(first + 1, filtered.size() - 1);
    const double fraction = position - static_cast<double>(first);
    resampled[n] =
        filtered[first] + (filtered[second] - filtered[first]) * fraction;
  }
  return resampled;
}

std::vector<double> linearPrediction(const std::vector<double>& segment) {
  std::vector<double> windowed(segment.size());
  for (std::size_t n = 0; n < segment.size(); ++n) {
    windowed[n] = segment[n] * hamming(n, segment.size());
  }
  std::array<double, kLpcOrder + 1> autocorrelation{};
  for (int lag = 0; lag <= kLpcOrder; ++lag) {
    double accumulator = 0.0;
    for (std::size_t n = static_cast<std::size_t>(lag); n < windowed.size(); ++n) {
      accumulator += windowed[n] * windowed[n - static_cast<std::size_t>(lag)];
    }
    autocorrelation[static_cast<std::size_t>(lag)] = accumulator;
  }
  if (!(autocorrelation[0] > 0.0)) return {};

  Eigen::MatrixXd toeplitz(kLpcOrder, kLpcOrder);
  for (int row = 0; row < kLpcOrder; ++row) {
    for (int column = 0; column < kLpcOrder; ++column) {
      toeplitz(row, column) =
          autocorrelation[static_cast<std::size_t>(std::abs(row - column))];
    }
  }
  toeplitz += Eigen::MatrixXd::Identity(kLpcOrder, kLpcOrder) *
              (autocorrelation[0] * 1e-9);
  Eigen::VectorXd right(kLpcOrder);
  for (int row = 0; row < kLpcOrder; ++row) {
    right(row) = -autocorrelation[static_cast<std::size_t>(row + 1)];
  }
  const Eigen::VectorXd solved = toeplitz.ldlt().solve(right);
  if (!solved.allFinite()) return {};

  std::vector<double> denominator(kLpcOrder + 1);
  denominator[0] = 1.0;
  for (int index = 0; index < kLpcOrder; ++index) {
    denominator[static_cast<std::size_t>(index + 1)] = solved(index);
  }
  return denominator;
}

std::vector<double> impulseResponse(const std::vector<double>& denominator,
                                    int length) {
  std::vector<double> response(static_cast<std::size_t>(length), 0.0);
  for (int n = 0; n < length; ++n) {
    double accumulator = n == 0 ? 1.0 : 0.0;
    for (std::size_t k = 1; k < denominator.size(); ++k) {
      if (n < static_cast<int>(k)) break;
      accumulator -= denominator[k] * response[static_cast<std::size_t>(n) - k];
    }
    response[static_cast<std::size_t>(n)] = accumulator;
  }
  return response;
}

std::vector<std::complex<double>> roots(const std::vector<double>& polynomial) {
  std::size_t lead = 0;
  while (lead < polynomial.size() && std::abs(polynomial[lead]) < 1e-12) ++lead;
  if (lead + 1 >= polynomial.size()) return {};
  const Eigen::Index degree =
      static_cast<Eigen::Index>(polynomial.size() - lead - 1);
  Eigen::MatrixXd companion = Eigen::MatrixXd::Zero(degree, degree);
  for (Eigen::Index column = 0; column < degree; ++column) {
    companion(0, column) =
        -polynomial[lead + 1 + static_cast<std::size_t>(column)] /
        polynomial[lead];
  }
  for (Eigen::Index row = 1; row < degree; ++row) companion(row, row - 1) = 1.0;
  const Eigen::EigenSolver<Eigen::MatrixXd> solver(companion);
  std::vector<std::complex<double>> found;
  found.reserve(static_cast<std::size_t>(degree));
  for (Eigen::Index index = 0; index < degree; ++index) {
    found.push_back(solver.eigenvalues()(index));
  }
  return found;
}

std::vector<std::pair<double, double>> resonances(
    const std::vector<std::complex<double>>& found, bool stable,
    double sample_rate_hz) {
  std::vector<std::pair<double, double>> physical;
  for (std::complex<double> root : found) {
    const double radius = std::abs(root);
    if (stable) {
      if (radius >= 1.0) {
        if (!(radius > 0.0)) continue;
        root *= 0.999999 / radius;
      }
    } else if (radius > 1.0) {
      root = 1.0 / root;
    }
    if (root.imag() <= 1e-9) continue;
    const double hz = std::atan2(root.imag(), root.real()) /
                      (2.0 * std::numbers::pi) * sample_rate_hz;
    const double bandwidth_hz =
        -std::log(std::abs(root)) * sample_rate_hz / std::numbers::pi;
    if (!std::isfinite(hz) || !std::isfinite(bandwidth_hz)) continue;
    if (hz <= kLowestHz || hz >= kHighestHz) continue;
    physical.emplace_back(hz, bandwidth_hz);
  }
  std::sort(physical.begin(), physical.end(),
            [](const auto& left, const auto& right) {
              return left.first < right.first;
            });
  if (physical.size() > trench::core::native::kSections) {
    physical.resize(trench::core::native::kSections);
  }
  return physical;
}

}  // namespace

AnalyzeProposal analyzeSound(std::span<const float> samples,
                             double sample_rate_hz) {
  if (samples.empty() || !(sample_rate_hz > 0.0)) return {};
  const std::vector<double> segment =
      atAnalysisRate(loudestSegment(samples, sample_rate_hz), sample_rate_hz);
  const double analysis_hz =
      sample_rate_hz > kAnalysisHz ? kAnalysisHz : sample_rate_hz;
  if (segment.size() < static_cast<std::size_t>(4 * kLpcOrder)) return {};

  const std::vector<double> lpc = linearPrediction(segment);
  if (lpc.empty()) return {};
  const std::vector<double> response = impulseResponse(lpc, kImpulseLength);

  const Eigen::Index rows = kImpulseLength - kPronyFirstRow;
  Eigen::MatrixXd shifted(rows, kPronyOrder);
  Eigen::VectorXd target(rows);
  for (Eigen::Index row = 0; row < rows; ++row) {
    const int n = kPronyFirstRow + static_cast<int>(row);
    for (int column = 0; column < kPronyOrder; ++column) {
      shifted(row, column) =
          response[static_cast<std::size_t>(n - 1 - column)];
    }
    target(row) = -response[static_cast<std::size_t>(n)];
  }
  const Eigen::VectorXd solved = shifted.colPivHouseholderQr().solve(target);
  if (!solved.allFinite()) return {};
  std::vector<double> denominator(kPronyOrder + 1);
  denominator[0] = 1.0;
  for (int index = 0; index < kPronyOrder; ++index) {
    denominator[static_cast<std::size_t>(index + 1)] = solved(index);
  }

  const std::vector<double> ringing =
      impulseResponse(denominator, kImpulseLength);
  Eigen::MatrixXd basis =
      Eigen::MatrixXd::Zero(kImpulseLength, kPronyOrder + 1);
  Eigen::VectorXd measured(kImpulseLength);
  for (Eigen::Index row = 0; row < kImpulseLength; ++row) {
    measured(row) = response[static_cast<std::size_t>(row)];
    for (Eigen::Index column = 0; column <= kPronyOrder; ++column) {
      if (row < column) continue;
      basis(row, column) = ringing[static_cast<std::size_t>(row - column)];
    }
  }
  const Eigen::VectorXd weights = basis.colPivHouseholderQr().solve(measured);
  if (!weights.allFinite()) return {};
  std::vector<double> numerator(kPronyOrder + 1);
  for (int index = 0; index <= kPronyOrder; ++index) {
    numerator[static_cast<std::size_t>(index)] = weights(index);
  }

  AnalyzeProposal proposal;
  proposal.poles = resonances(roots(denominator), true, analysis_hz);
  proposal.zeros = resonances(roots(numerator), false, analysis_hz);
  return proposal;
}

}  // namespace trench::app
