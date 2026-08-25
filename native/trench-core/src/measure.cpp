#include "trench/core/measure.hpp"

#include "trench/core/packed_body.hpp"

#include <Eigen/Eigenvalues>
#include <pocketfft_hdronly.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>

namespace trench::core::measure {
namespace {

constexpr double kTau = 2.0 * std::numbers::pi;

std::vector<double> magnitude_db(std::span<const float> mono, std::size_t fft_size) {
  std::vector<double> frame(fft_size, 0.0);
  const std::size_t start = mono.size() > fft_size ? (mono.size() - fft_size) / 4 : 0;
  const std::size_t count = std::min(fft_size, mono.size() - start);
  for (std::size_t i = 0; i < count; ++i) {
    const double window =
        0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                             static_cast<double>(fft_size - 1));
    frame[i] = static_cast<double>(mono[start + i]) * window;
  }
  std::vector<std::complex<double>> spectrum(fft_size / 2 + 1);
  pocketfft::r2c(pocketfft::shape_t{fft_size}, pocketfft::stride_t{sizeof(double)},
                 pocketfft::stride_t{sizeof(std::complex<double>)}, std::size_t{0},
                 pocketfft::FORWARD, frame.data(), spectrum.data(), 1.0);
  std::vector<double> db(spectrum.size());
  for (std::size_t i = 0; i < spectrum.size(); ++i) {
    db[i] = 20.0 * std::log10(std::abs(spectrum[i]) + 1e-12);
  }
  return db;
}

struct Peak {
  double bin{};
  double db{};
};

Peak refine_peak(const std::vector<double>& db, std::size_t near, std::size_t reach) {
  std::size_t at = near - std::min(near, reach);
  const std::size_t stop = std::min(near + reach, db.size() - 1);
  for (std::size_t i = at; i <= stop; ++i) {
    if (db[i] > db[at]) at = i;
  }
  if (at == 0 || at + 1 >= db.size()) return {static_cast<double>(at), db[at]};
  const double alpha = db[at - 1];
  const double beta = db[at];
  const double gamma = db[at + 1];
  const double denominator = alpha - 2.0 * beta + gamma;
  if (!(denominator < 0.0)) return {static_cast<double>(at), beta};
  const double delta = 0.5 * (alpha - gamma) / denominator;
  const double x = std::numbers::pi * delta;
  const double lobe = std::abs(delta) < 1e-9
                          ? 1.0
                          : std::abs(std::sin(x) / x) / std::abs(1.0 - delta * delta);
  return {static_cast<double>(at) + delta, beta - 20.0 * std::log10(lobe)};
}

double comb_score(const std::vector<double>& db, double bin_hz, double f0) {
  double total = 0.0;
  for (std::size_t k = 1; k <= 20; ++k) {
    const auto bin = static_cast<std::size_t>(std::lround(static_cast<double>(k) * f0 / bin_hz));
    if (bin >= db.size()) break;
    total += db[bin];
  }
  return total;
}

}  // namespace

HarmonicEnvelope harmonic_envelope(std::span<const float> mono, double sample_rate_hz,
                                   Source source, const Options& options) {
  if (mono.empty() || !(sample_rate_hz > 0.0) || options.fft_size < 16) {
    throw std::invalid_argument("harmonic_envelope needs samples, a rate and an FFT size");
  }
  const auto db = magnitude_db(mono, options.fft_size);
  const double bin_hz = sample_rate_hz / static_cast<double>(options.fft_size);
  const auto low = static_cast<std::size_t>(options.f0_low_hz / bin_hz);
  const auto high = std::min(static_cast<std::size_t>(options.f0_high_hz / bin_hz), db.size() - 1);
  if (low >= high) throw std::invalid_argument("f0 search band is empty at this FFT size");

  std::size_t peak = low;
  for (std::size_t i = low; i <= high; ++i) {
    if (db[i] > db[peak]) peak = i;
  }
  const double coarse = static_cast<double>(peak) * bin_hz;
  double f0 = coarse;
  double best = comb_score(db, bin_hz, coarse);
  for (double candidate = coarse - bin_hz; candidate <= coarse + bin_hz; candidate += bin_hz / 64.0) {
    const double score = comb_score(db, bin_hz, candidate);
    if (score > best) {
      best = score;
      f0 = candidate;
    }
  }

  double f0_sum = 0.0;
  std::size_t f0_count = 0;
  for (std::size_t k = 1; k <= 10; ++k) {
    const auto centre = static_cast<std::size_t>(std::lround(static_cast<double>(k) * f0 / bin_hz));
    if (centre + 3 >= db.size()) break;
    f0_sum += refine_peak(db, centre, 3).bin * bin_hz / static_cast<double>(k);
    ++f0_count;
  }
  if (f0_count > 0) f0 = f0_sum / static_cast<double>(f0_count);

  HarmonicEnvelope envelope;
  envelope.f0_hz = f0;
  const double top = std::min(options.top_hz, 0.5 * sample_rate_hz);
  for (std::size_t k = 1; static_cast<double>(k) * f0 < top; ++k) {
    const auto centre = static_cast<std::size_t>(std::lround(static_cast<double>(k) * f0 / bin_hz));
    if (centre + 3 >= db.size()) break;
    const auto peak = refine_peak(db, centre, 3);
    const double correction =
        source == Source::kSawtooth ? 20.0 * std::log10(static_cast<double>(k)) : 0.0;
    envelope.hz.push_back(static_cast<double>(k) * f0);
    envelope.db.push_back(peak.db + correction);
  }
  return envelope;
}

std::vector<double> target_on_grid(const HarmonicEnvelope& envelope,
                                   std::span<const double> grid_hz) {
  if (envelope.hz.empty()) throw std::invalid_argument("envelope has no harmonics");
  std::vector<double> raw(grid_hz.size());
  for (std::size_t i = 0; i < grid_hz.size(); ++i) {
    const double f = grid_hz[i];
    if (f <= envelope.hz.front()) {
      raw[i] = envelope.db.front();
    } else if (f >= envelope.hz.back()) {
      raw[i] = envelope.db.back();
    } else {
      const auto upper = std::upper_bound(envelope.hz.begin(), envelope.hz.end(), f);
      const auto index = static_cast<std::size_t>(upper - envelope.hz.begin());
      const double f0 = envelope.hz[index - 1];
      const double f1 = envelope.hz[index];
      const double t = (f - f0) / (f1 - f0);
      raw[i] = envelope.db[index - 1] + t * (envelope.db[index] - envelope.db[index - 1]);
    }
  }
  const double ratio = std::pow(2.0, 1.0 / 6.0);
  std::vector<double> smooth(grid_hz.size());
  for (std::size_t i = 0; i < grid_hz.size(); ++i) {
    const auto lo = std::lower_bound(grid_hz.begin(), grid_hz.end(), grid_hz[i] / ratio);
    auto hi = std::lower_bound(grid_hz.begin(), grid_hz.end(), grid_hz[i] * ratio);
    if (hi <= lo) hi = lo + 1;
    double sum = 0.0;
    for (auto it = lo; it != hi; ++it) sum += raw[static_cast<std::size_t>(it - grid_hz.begin())];
    smooth[i] = sum / static_cast<double>(hi - lo);
  }
  return smooth;
}

LpcEnvelope lpc_envelope(std::span<const float> mono, double sample_rate_hz,
                         const LpcOptions& options) {
  if (mono.size() < 64 || !(sample_rate_hz > 0.0)) {
    throw std::invalid_argument("lpc_envelope needs samples and a rate");
  }
  const std::size_t order =
      options.order > 0 ? options.order
                        : static_cast<std::size_t>(std::lround(2.0 + sample_rate_hz / 1000.0));
  const std::size_t length = std::min(options.frame, mono.size());
  const std::size_t start = mono.size() > length ? (mono.size() - length) / 4 : 0;
  std::vector<double> frame(length);
  for (std::size_t i = 0; i < length; ++i) {
    const double x = mono[start + i];
    const double previous = i > 0 ? mono[start + i - 1] : 0.0;
    const double window =
        0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                             static_cast<double>(length - 1));
    frame[i] = (x - options.pre_emphasis * previous) * window;
  }
  std::vector<double> r(order + 1, 0.0);
  for (std::size_t lag = 0; lag <= order; ++lag) {
    double acc = 0.0;
    for (std::size_t i = lag; i < length; ++i) acc += frame[i] * frame[i - lag];
    r[lag] = acc;
  }
  if (!(r[0] > 0.0)) throw std::invalid_argument("lpc_envelope needs a non-silent clip");

  std::vector<double> a(order + 1, 0.0);
  std::vector<double> previous(order + 1, 0.0);
  a[0] = 1.0;
  double error = r[0];
  for (std::size_t i = 1; i <= order; ++i) {
    double acc = r[i];
    for (std::size_t j = 1; j < i; ++j) acc += a[j] * r[i - j];
    const double k = -acc / error;
    previous = a;
    for (std::size_t j = 1; j < i; ++j) a[j] = previous[j] + k * previous[i - j];
    a[i] = k;
    error *= 1.0 - k * k;
    if (!(error > 0.0)) break;
  }

  LpcEnvelope out;
  out.gain = std::sqrt(std::max(error, 1e-30) / static_cast<double>(length));
  out.a = a;
  out.sample_rate_hz = sample_rate_hz;

  Eigen::MatrixXd companion = Eigen::MatrixXd::Zero(static_cast<Eigen::Index>(order),
                                                    static_cast<Eigen::Index>(order));
  for (std::size_t i = 0; i < order; ++i) {
    companion(0, static_cast<Eigen::Index>(i)) = -a[i + 1];
    if (i + 1 < order) companion(static_cast<Eigen::Index>(i + 1), static_cast<Eigen::Index>(i)) = 1.0;
  }
  const Eigen::EigenSolver<Eigen::MatrixXd> solver(companion, false);
  for (Eigen::Index i = 0; i < solver.eigenvalues().size(); ++i) {
    const std::complex<double> root = solver.eigenvalues()[i];
    if (root.imag() <= 0.0) continue;
    const double hz = std::arg(root) * sample_rate_hz / (2.0 * std::numbers::pi);
    const double bw = -std::log(std::abs(root)) * sample_rate_hz / std::numbers::pi;
    if (hz < options.low_hz || hz > 0.49 * sample_rate_hz || bw > options.max_bw_hz) continue;
    out.formants.push_back({hz, bw});
  }
  std::sort(out.formants.begin(), out.formants.end(),
            [](const Formant& lhs, const Formant& rhs) { return lhs.hz < rhs.hz; });
  return out;
}

std::vector<double> target_on_grid(const LpcEnvelope& envelope, std::span<const double> grid_hz) {
  std::vector<double> out;
  out.reserve(grid_hz.size());
  for (const double hz : grid_hz) {
    const double w = 2.0 * std::numbers::pi * hz / envelope.sample_rate_hz;
    std::complex<double> denominator{0.0, 0.0};
    for (std::size_t k = 0; k < envelope.a.size(); ++k) {
      denominator += envelope.a[k] * std::polar(1.0, -w * static_cast<double>(k));
    }
    out.push_back(20.0 * std::log10(envelope.gain / std::max(std::abs(denominator), 1e-12)));
  }
  return out;
}

FitTarget transfer_function(std::span<const float> input,
                            std::span<const float> output,
                            double sample_rate_hz,
                            const TransferOptions& options) {
  if (input.size() != output.size() || input.empty() || !(sample_rate_hz > 0.0) ||
      options.fft_size < 32 || options.hop_size == 0 || options.points < 2) {
    throw std::invalid_argument("transfer_function needs equal channels, a rate and a grid");
  }
  const std::size_t bins = options.fft_size / 2 + 1;
  std::vector<double> sxx(bins, 0.0);
  std::vector<double> syy(bins, 0.0);
  std::vector<std::complex<double>> sxy(bins, {0.0, 0.0});
  std::vector<double> in_frame(options.fft_size, 0.0);
  std::vector<double> out_frame(options.fft_size, 0.0);
  std::vector<std::complex<double>> in_spectrum(bins);
  std::vector<std::complex<double>> out_spectrum(bins);
  const std::size_t final_start = input.size() > options.fft_size
                                      ? input.size() - options.fft_size
                                      : 0;
  for (std::size_t start = 0;; start = std::min(start + options.hop_size, final_start)) {
    std::fill(in_frame.begin(), in_frame.end(), 0.0);
    std::fill(out_frame.begin(), out_frame.end(), 0.0);
    const auto count = std::min(options.fft_size, input.size() - start);
    for (std::size_t index = 0; index < count; ++index) {
      const double window =
          0.5 - 0.5 * std::cos(kTau * static_cast<double>(index) /
                               static_cast<double>(options.fft_size - 1));
      in_frame[index] = input[start + index] * window;
      out_frame[index] = output[start + index] * window;
    }
    pocketfft::r2c(pocketfft::shape_t{options.fft_size},
                   pocketfft::stride_t{sizeof(double)},
                   pocketfft::stride_t{sizeof(std::complex<double>)}, std::size_t{0},
                   pocketfft::FORWARD, in_frame.data(), in_spectrum.data(), 1.0);
    pocketfft::r2c(pocketfft::shape_t{options.fft_size},
                   pocketfft::stride_t{sizeof(double)},
                   pocketfft::stride_t{sizeof(std::complex<double>)}, std::size_t{0},
                   pocketfft::FORWARD, out_frame.data(), out_spectrum.data(), 1.0);
    for (std::size_t bin = 0; bin < bins; ++bin) {
      sxx[bin] += std::norm(in_spectrum[bin]);
      syy[bin] += std::norm(out_spectrum[bin]);
      sxy[bin] += std::conj(in_spectrum[bin]) * out_spectrum[bin];
    }
    if (start == final_start) break;
  }

  FitTarget target;
  target.frequency_hz = logarithmic_frequency_grid(
      options.low_hz, std::min(options.top_hz, sample_rate_hz * 0.49),
      options.points);
  target.magnitude_db.reserve(target.frequency_hz.size());
  target.phase_rad.reserve(target.frequency_hz.size());
  target.weight.reserve(target.frequency_hz.size());
  const double bin_hz = sample_rate_hz / static_cast<double>(options.fft_size);
  const double half_band = options.smoothing_octaves * 0.5;
  for (const double hz : target.frequency_hz) {
    const double low = hz * std::pow(2.0, -half_band);
    const double high = hz * std::pow(2.0, half_band);
    const auto first = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::floor(low / bin_hz)), 1, bins - 1);
    const auto last = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::ceil(high / bin_hz)), first + 1, bins);
    double input_power = 0.0;
    double output_power = 0.0;
    std::complex<double> cross{0.0, 0.0};
    for (std::size_t bin = first; bin < last; ++bin) {
      input_power += sxx[bin];
      output_power += syy[bin];
      cross += sxy[bin];
    }
    const auto transfer = cross / std::max(input_power, 1.0e-30);
    const double coherence = std::norm(cross) /
                             std::max(input_power * output_power, 1.0e-30);
    target.magnitude_db.push_back(20.0 * std::log10(std::max(std::abs(transfer), 1.0e-15)));
    target.phase_rad.push_back(std::arg(transfer));
    target.weight.push_back(std::clamp(coherence, 0.0, 1.0));
  }
  target.kind = TargetKind::kTransferFunction;
  target.absolute_level = true;
  if (!target.valid()) throw std::runtime_error("transfer estimate is not finite");
  return target;
}

ErrorReport weighted_error(std::span<const double> target_db, std::span<const double> model_db,
                           std::span<const double> weight) {
  if (target_db.size() != model_db.size() || target_db.size() != weight.size() || target_db.empty()) {
    throw std::invalid_argument("weighted_error needs equal, non-empty spans");
  }
  double weight_sum = 0.0;
  double offset = 0.0;
  for (std::size_t i = 0; i < target_db.size(); ++i) {
    weight_sum += weight[i];
    offset += weight[i] * (target_db[i] - model_db[i]);
  }
  if (!(weight_sum > 0.0)) throw std::invalid_argument("weights sum to zero");
  offset /= weight_sum;
  double square = 0.0;
  double worst = 0.0;
  for (std::size_t i = 0; i < target_db.size(); ++i) {
    const double d = target_db[i] - model_db[i] - offset;
    square += weight[i] * d * d;
    if (weight[i] > 0.0) worst = std::max(worst, std::abs(d));
  }
  return {std::sqrt(square / weight_sum), worst, offset};
}

ErrorReport score_words(std::span<const std::uint16_t> words, double sample_rate_hz,
                        std::span<const double> grid_hz, std::span<const double> weight,
                        std::span<const double> target_db) {
  if (words.size() % kCoefficientCount != 0 || words.empty()) {
    throw std::invalid_argument("words must be a non-empty multiple of 5");
  }
  std::vector<Biquad> cascade;
  for (std::size_t section = 0; section < words.size() / kCoefficientCount; ++section) {
    PackedSection packed{};
    for (std::size_t word = 0; word < kCoefficientCount; ++word) {
      packed[word] = words[section * kCoefficientCount + word];
    }
    cascade.push_back(section_words_to_biquad(packed));
  }
  std::vector<double> model(grid_hz.size());
  for (std::size_t i = 0; i < grid_hz.size(); ++i) {
    model[i] = cascade_response_db(cascade, grid_hz[i], sample_rate_hz);
  }
  return weighted_error(target_db, model, weight);
}

}  // namespace trench::core::measure
