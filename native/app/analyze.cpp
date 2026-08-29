#include "analyze.hpp"

#include "trench/core/measure.hpp"
#include "trench/core/native_body.hpp"

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <unsupported/Eigen/FFT>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace trench::app {
namespace {

constexpr int kPronyOrder = 12;
constexpr int kImpulseLength = 512;
constexpr int kPronyFirstRow = 13;
constexpr int kResampleTaps = 64;
constexpr int kFft = 4096;
constexpr int kHop = 1024;
constexpr double kAnalysisHz = 10'000.0;
constexpr double kAntiAliasHz = 4'700.0;
constexpr double kSegmentSeconds = 0.5;
constexpr double kHopSeconds = 0.05;
constexpr double kLowestHz = 20.0;
constexpr double kHighestHz = 4'900.0;
constexpr double kFloorDb = 80.0;
constexpr double kVoicedShare = 0.6;
constexpr double kPruneDb = 0.5;
constexpr double kSeatCeilingHz = 700.0;
constexpr double kSeatQ = 1.5;
constexpr double kFormantQ = 5.0;
constexpr double kEdgeShare = 0.9;
constexpr std::size_t kGridPoints = 240;

using Root = std::pair<double, double>;

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

std::vector<double> averagedPower(const std::vector<double>& segment) {
  Eigen::FFT<double> fft;
  fft.SetFlag(Eigen::FFT<double>::HalfSpectrum);
  std::vector<double> power(kFft / 2 + 1, 0.0);
  std::size_t frames = 0;
  std::vector<double> frame(kFft);
  std::vector<std::complex<double>> spectrum;
  const std::size_t last = segment.size() >= static_cast<std::size_t>(kFft)
                               ? segment.size() - kFft
                               : 0;
  for (std::size_t start = 0; start <= last; start += kHop) {
    for (int n = 0; n < kFft; ++n) {
      const std::size_t index = start + static_cast<std::size_t>(n);
      const double value = index < segment.size() ? segment[index] : 0.0;
      frame[static_cast<std::size_t>(n)] =
          value * (0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * n / kFft));
    }
    fft.fwd(spectrum, frame);
    for (std::size_t k = 0; k < power.size(); ++k) power[k] += std::norm(spectrum[k]);
    ++frames;
    if (last == 0) break;
  }
  if (frames > 0) {
    for (double& value : power) value /= static_cast<double>(frames);
  }
  return power;
}

std::vector<double> envelopeLogMagnitude(const std::vector<double>& segment,
                                         const std::vector<double>& power,
                                         double analysis_hz) {
  const double bin_hz = analysis_hz / kFft;
  const std::size_t bins = power.size();
  std::vector<double> log_magnitude(bins, 0.0);

  std::vector<float> mono(segment.size());
  for (std::size_t n = 0; n < segment.size(); ++n) mono[n] = static_cast<float>(segment[n]);
  trench::core::measure::Options options;
  options.fft_size = 8192;
  options.f0_low_hz = 40.0;
  options.f0_high_hz = 1'200.0;
  options.top_hz = kHighestHz;
  trench::core::measure::HarmonicEnvelope harmonic;
  bool voiced = false;
  const auto share_of = [&](double f0) {
    double near = 0.0;
    double total = 0.0;
    const double ceiling = std::min(kHighestHz, 12.5 * f0);
    for (std::size_t k = 1; k < bins; ++k) {
      const double hz = static_cast<double>(k) * bin_hz;
      if (hz < kLowestHz || hz > ceiling) continue;
      total += power[k];
      const double ratio = hz / f0;
      if (std::abs(ratio - std::round(ratio)) <= 0.15) near += power[k];
    }
    return total > 0.0 ? near / total : 0.0;
  };
  try {
    harmonic = trench::core::measure::harmonic_envelope(
        mono, analysis_hz, trench::core::measure::Source::kFlat, options);
    if (harmonic.f0_hz > 0.0) {
      double best_f0 = harmonic.f0_hz;
      double best_share = share_of(best_f0);
      for (const double divisor : {2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0}) {
        const double candidate = harmonic.f0_hz / divisor;
        if (candidate < options.f0_low_hz) continue;
        const double share = share_of(candidate);
        if (share > best_share + 0.1) {
          best_f0 = candidate;
          best_share = share;
        }
      }
      if (best_f0 != harmonic.f0_hz) {
        trench::core::measure::Options narrowed = options;
        narrowed.f0_low_hz = best_f0 * 0.93;
        narrowed.f0_high_hz = best_f0 * 1.07;
        harmonic = trench::core::measure::harmonic_envelope(
            mono, analysis_hz, trench::core::measure::Source::kFlat, narrowed);
      }
    }
    if (harmonic.f0_hz > 0.0 && harmonic.hz.size() >= 3) {
      const double share = share_of(harmonic.f0_hz);
      voiced = share >= kVoicedShare;
    }
  } catch (const std::exception&) {
    voiced = false;
  }

  if (voiced) {
    for (std::size_t k = 0; k < bins; ++k) {
      const double hz = static_cast<double>(k) * bin_hz;
      double db = harmonic.db.front();
      if (hz >= harmonic.hz.back()) {
        db = harmonic.db.back();
      } else if (hz > harmonic.hz.front()) {
        std::size_t upper = 1;
        while (upper + 1 < harmonic.hz.size() && harmonic.hz[upper] < hz) ++upper;
        const std::size_t count = harmonic.hz.size();
        const std::size_t i1 = upper - 1;
        const std::size_t i0 = i1 > 0 ? i1 - 1 : i1;
        const std::size_t i2 = upper;
        const std::size_t i3 = std::min(count - 1, upper + 1);
        const double span = harmonic.hz[i2] - harmonic.hz[i1];
        const double t = span > 0.0 ? (hz - harmonic.hz[i1]) / span : 0.0;
        const double p0 = harmonic.db[i0], p1 = harmonic.db[i1], p2 = harmonic.db[i2], p3 = harmonic.db[i3];
        db = 0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t +
                    (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t * t * t);
      }
      log_magnitude[k] = db / 20.0 * std::numbers::ln10;
    }
  } else {
    for (std::size_t k = 0; k < bins; ++k) {
      const double hz = static_cast<double>(k) * bin_hz;
      const double half_hz = std::max(0.12 * hz, 25.0);
      const auto reach = static_cast<std::size_t>(half_hz / bin_hz);
      const std::size_t lo = k > reach ? k - reach : 0;
      const std::size_t hi = std::min(bins - 1, k + reach);
      double best = 0.0;
      for (std::size_t j = lo; j <= hi; ++j) best = std::max(best, power[j]);
      log_magnitude[k] = 0.5 * std::log(best + 1e-30);
    }
  }
  const double top = *std::max_element(log_magnitude.begin(), log_magnitude.end());
  const double floor = top - kFloorDb / 20.0 * std::numbers::ln10;
  for (double& value : log_magnitude) value = std::max(value, floor);
  return log_magnitude;
}

std::vector<double> minimumPhaseImpulse(const std::vector<double>& log_magnitude) {
  Eigen::FFT<double> fft;
  std::vector<std::complex<double>> spectrum(kFft);
  for (int k = 0; k <= kFft / 2; ++k) {
    spectrum[static_cast<std::size_t>(k)] = log_magnitude[static_cast<std::size_t>(k)];
    if (k > 0 && k < kFft / 2) {
      spectrum[static_cast<std::size_t>(kFft - k)] = log_magnitude[static_cast<std::size_t>(k)];
    }
  }
  std::vector<std::complex<double>> cepstrum;
  fft.inv(cepstrum, spectrum);
  std::vector<std::complex<double>> folded(kFft, 0.0);
  folded[0] = cepstrum[0].real();
  for (int n = 1; n < kFft / 2; ++n) {
    folded[static_cast<std::size_t>(n)] = 2.0 * cepstrum[static_cast<std::size_t>(n)].real();
  }
  folded[static_cast<std::size_t>(kFft / 2)] = cepstrum[static_cast<std::size_t>(kFft / 2)].real();
  std::vector<std::complex<double>> log_spectrum;
  fft.fwd(log_spectrum, folded);
  for (auto& value : log_spectrum) value = std::exp(value);
  std::vector<std::complex<double>> impulse;
  fft.inv(impulse, log_spectrum);
  std::vector<double> response(kImpulseLength);
  const double scale = impulse[0].real() != 0.0 ? 1.0 / impulse[0].real() : 1.0;
  for (int n = 0; n < kImpulseLength; ++n) {
    response[static_cast<std::size_t>(n)] = impulse[static_cast<std::size_t>(n)].real() * scale;
  }
  return response;
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

std::vector<Root> resonances(const std::vector<std::complex<double>>& found,
                             bool stable, double sample_rate_hz) {
  std::vector<Root> physical;
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
    if (hz <= kLowestHz || hz >= kEdgeShare * kHighestHz) continue;
    if (hz / bandwidth_hz < (hz < kSeatCeilingHz ? kSeatQ : kFormantQ)) continue;
    physical.emplace_back(hz, bandwidth_hz);
  }
  std::sort(physical.begin(), physical.end(),
            [](const Root& left, const Root& right) { return left.first < right.first; });
  return physical;
}

double sectionDb(const Root& root, double hz, double sample_rate_hz) {
  const double r = std::exp(-std::numbers::pi * root.second / sample_rate_hz);
  const double theta = 2.0 * std::numbers::pi * root.first / sample_rate_hz;
  const double omega = 2.0 * std::numbers::pi * hz / sample_rate_hz;
  const std::complex<double> z1 = std::polar(1.0, -omega);
  const std::complex<double> value = 1.0 - 2.0 * r * std::cos(theta) * z1 + r * r * z1 * z1;
  return 20.0 * std::log10(std::abs(value) + 1e-20);
}

struct Fit {
  std::vector<Root> poles;
  std::vector<Root> zeros;
};

std::vector<double> modelDb(const Fit& fit, const std::vector<double>& grid_hz,
                            double sample_rate_hz) {
  std::vector<double> model(grid_hz.size(), 0.0);
  for (std::size_t i = 0; i < grid_hz.size(); ++i) {
    for (const Root& pole : fit.poles) model[i] -= sectionDb(pole, grid_hz[i], sample_rate_hz);
    for (const Root& zero : fit.zeros) model[i] += sectionDb(zero, grid_hz[i], sample_rate_hz);
  }
  return model;
}

std::vector<double> detail(const std::vector<double>& curve, std::size_t half_window) {
  std::vector<double> out(curve.size());
  for (std::size_t i = 0; i < curve.size(); ++i) {
    const std::size_t lo = i > half_window ? i - half_window : 0;
    const std::size_t hi = std::min(curve.size() - 1, i + half_window);
    double mean = 0.0;
    for (std::size_t j = lo; j <= hi; ++j) mean += curve[j];
    out[i] = curve[i] - mean / static_cast<double>(hi - lo + 1);
  }
  return out;
}

double detailError(const Fit& fit, const std::vector<double>& grid_hz,
                   const std::vector<double>& target_detail, std::size_t half_window,
                   double sample_rate_hz) {
  const std::vector<double> model = detail(modelDb(fit, grid_hz, sample_rate_hz), half_window);
  double accumulator = 0.0;
  for (std::size_t i = 0; i < grid_hz.size(); ++i) {
    const double diff = model[i] - target_detail[i];
    accumulator += diff * diff;
  }
  return std::sqrt(accumulator / static_cast<double>(grid_hz.size()));
}

double rootInfluence(const Fit& fit, bool is_pole, std::size_t index,
                     const std::vector<double>& grid_hz,
                     const std::vector<double>& target_detail, std::size_t half_window,
                     double sample_rate_hz) {
  Fit without = fit;
  auto& list = is_pole ? without.poles : without.zeros;
  list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
  const std::vector<double> full = detail(modelDb(fit, grid_hz, sample_rate_hz), half_window);
  const std::vector<double> less = detail(modelDb(without, grid_hz, sample_rate_hz), half_window);
  double accumulator = 0.0;
  for (std::size_t i = 0; i < grid_hz.size(); ++i) {
    const double diff = full[i] - less[i];
    accumulator += diff * diff;
  }
  (void)target_detail;
  return std::sqrt(accumulator / static_cast<double>(grid_hz.size()));
}

Fit pruned(Fit fit, const std::vector<double>& grid_hz,
           const std::vector<double>& target_detail, std::size_t half_window,
           double sample_rate_hz) {
  for (;;) {
    double least = std::numeric_limits<double>::infinity();
    bool least_is_pole = true;
    std::size_t least_index = 0;
    for (const bool is_pole : {true, false}) {
      const auto& list = is_pole ? fit.poles : fit.zeros;
      for (std::size_t index = 0; index < list.size(); ++index) {
        const double influence = rootInfluence(fit, is_pole, index, grid_hz, target_detail, half_window, sample_rate_hz);
        if (influence < least) {
          least = influence;
          least_is_pole = is_pole;
          least_index = index;
        }
      }
    }
    const bool over = fit.poles.size() > trench::core::native::kSections ||
                      fit.zeros.size() > trench::core::native::kSections;
    if (!std::isfinite(least) || (least >= kPruneDb && !over)) break;
    if (over) {
      const bool is_pole = fit.poles.size() > trench::core::native::kSections;
      auto& list = is_pole ? fit.poles : fit.zeros;
      double worst = std::numeric_limits<double>::infinity();
      std::size_t worst_index = 0;
      for (std::size_t index = 0; index < list.size(); ++index) {
        const double influence = rootInfluence(fit, is_pole, index, grid_hz, target_detail, half_window, sample_rate_hz);
        if (influence < worst) {
          worst = influence;
          worst_index = index;
        }
      }
      list.erase(list.begin() + static_cast<std::ptrdiff_t>(worst_index));
      continue;
    }
    auto& list = least_is_pole ? fit.poles : fit.zeros;
    list.erase(list.begin() + static_cast<std::ptrdiff_t>(least_index));
  }
  return fit;
}

}  // namespace

AnalyzeProposal analyzeSound(std::span<const float> samples,
                             double sample_rate_hz) {
  if (samples.empty() || !(sample_rate_hz > 0.0)) return {};
  const std::vector<double> segment =
      atAnalysisRate(loudestSegment(samples, sample_rate_hz), sample_rate_hz);
  const double analysis_hz =
      sample_rate_hz > kAnalysisHz ? kAnalysisHz : sample_rate_hz;
  if (segment.size() < static_cast<std::size_t>(4 * kPronyOrder)) return {};

  const std::vector<double> power = averagedPower(segment);
  const std::vector<double> log_magnitude =
      envelopeLogMagnitude(segment, power, analysis_hz);
  const std::vector<double> response = minimumPhaseImpulse(log_magnitude);

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

  Fit fit{resonances(roots(denominator), true, analysis_hz),
          resonances(roots(numerator), false, analysis_hz)};

  const std::vector<double> grid_hz =
      trench::core::logarithmic_frequency_grid(60.0, kHighestHz, kGridPoints);
  std::vector<double> target_db(grid_hz.size());
  const double bin_hz = analysis_hz / kFft;
  for (std::size_t i = 0; i < grid_hz.size(); ++i) {
    const double position = grid_hz[i] / bin_hz;
    const std::size_t lower = std::min(static_cast<std::size_t>(position), log_magnitude.size() - 2);
    const double fraction = position - static_cast<double>(lower);
    const double value = log_magnitude[lower] + fraction * (log_magnitude[lower + 1] - log_magnitude[lower]);
    target_db[i] = 20.0 * value / std::numbers::ln10;
  }
  const std::size_t half_window = kGridPoints / 12;
  const std::vector<double> target_detail = detail(target_db, half_window);
  fit = pruned(std::move(fit), grid_hz, target_detail, half_window, analysis_hz);

  AnalyzeProposal proposal;
  proposal.poles = std::move(fit.poles);
  proposal.zeros = std::move(fit.zeros);
  return proposal;
}

}  // namespace trench::app
