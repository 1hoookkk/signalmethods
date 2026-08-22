#include "trench/core/measure.hpp"

#include "trench/core/packed_body.hpp"

#include <pocketfft_hdronly.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>

namespace trench::core::measure {
namespace {

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
