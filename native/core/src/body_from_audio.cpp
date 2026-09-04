#include "trench/core/body_from_audio.hpp"

#include "trench/core/packed_body.hpp"

#include <Eigen/Eigenvalues>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <numbers>
#include <numeric>
#include <utility>

namespace trench::core::audio {

namespace {

constexpr std::size_t kFrame = 4096;
constexpr std::size_t kHop = 2048;

std::uint16_t word_at(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[at]) |
                                    static_cast<std::uint16_t>(bytes[at + 1] << 8));
}

std::uint32_t long_at(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  return static_cast<std::uint32_t>(bytes[at]) |
         (static_cast<std::uint32_t>(bytes[at + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[at + 2]) << 16) |
         (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
}

bool tag_at(const std::vector<std::uint8_t>& bytes, std::size_t at, const char* four) {
  if (at + 4 > bytes.size()) return false;
  return std::memcmp(bytes.data() + at, four, 4) == 0;
}

double sample_from(const std::vector<std::uint8_t>& bytes, std::size_t at,
                   std::uint16_t format, std::uint16_t bits) {
  if (format == 3 && bits == 32) {
    std::uint32_t raw = long_at(bytes, at);
    float value = 0.0F;
    std::memcpy(&value, &raw, sizeof(value));
    return static_cast<double>(value);
  }
  if (format != 1) return 0.0;
  if (bits == 16) {
    return static_cast<double>(static_cast<std::int16_t>(word_at(bytes, at))) / 32768.0;
  }
  if (bits == 24) {
    std::int32_t raw = static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(bytes[at]) << 8) |
        (static_cast<std::uint32_t>(bytes[at + 1]) << 16) |
        (static_cast<std::uint32_t>(bytes[at + 2]) << 24));
    return static_cast<double>(raw >> 8) / 8'388'608.0;
  }
  if (bits == 32) {
    return static_cast<double>(static_cast<std::int32_t>(long_at(bytes, at))) /
           2'147'483'648.0;
  }
  return 0.0;
}

std::vector<double> hann(std::size_t length) {
  std::vector<double> out(length, 1.0);
  if (length < 2) return out;
  for (std::size_t i = 0; i < length; ++i) {
    out[i] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                                  static_cast<double>(length - 1));
  }
  return out;
}

double median_of(std::vector<double> values) {
  if (values.empty()) return 0.0;
  const std::size_t middle = values.size() / 2;
  std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle),
                   values.end());
  return values[middle];
}

double floor_db_of(const AllPoleModel& model) {
  const double top_hz = std::min(20'000.0, 0.45 * model.sample_rate_hz);
  return median_of(envelope_db(
      model, trench::core::logarithmic_frequency_grid(20.0, top_hz, 256)));
}

double db_at_hz(const AllPoleModel& model, double hz) {
  const std::array<double, 1> one{hz};
  return envelope_db(model, one)[0];
}

std::vector<std::complex<double>> upper_roots(const AllPoleModel& model) {
  std::vector<std::complex<double>> out;
  if (model.a.size() < 3) return out;
  const auto order = static_cast<Eigen::Index>(model.a.size() - 1);
  Eigen::MatrixXd companion = Eigen::MatrixXd::Zero(order, order);
  for (Eigen::Index i = 0; i < order; ++i) {
    companion(0, i) = -model.a[static_cast<std::size_t>(i) + 1];
    if (i + 1 < order) companion(i + 1, i) = 1.0;
  }
  const Eigen::EigenSolver<Eigen::MatrixXd> solver(companion, false);
  for (Eigen::Index i = 0; i < solver.eigenvalues().size(); ++i) {
    const std::complex<double> root = solver.eigenvalues()[i];
    if (!(root.imag() > 0.0)) continue;
    if (!(std::abs(root) < 1.0)) continue;
    out.push_back(root);
  }
  return out;
}

std::vector<Resonance> strongest_by_hz(std::vector<std::pair<double, Resonance>> found,
                                       std::size_t count) {
  std::sort(found.begin(), found.end(),
            [](const auto& left, const auto& right) { return left.first > right.first; });
  if (found.size() > count) found.resize(count);
  std::vector<Resonance> out;
  out.reserve(found.size());
  for (const auto& entry : found) out.push_back(entry.second);
  std::sort(out.begin(), out.end(),
            [](const Resonance& left, const Resonance& right) { return left.hz < right.hz; });
  return out;
}

}

std::optional<MonoClip> read_wav_mono(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return std::nullopt;
  const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),
                                        std::istreambuf_iterator<char>()};
  if (bytes.size() < 44) return std::nullopt;
  if (!tag_at(bytes, 0, "RIFF") || !tag_at(bytes, 8, "WAVE")) return std::nullopt;

  std::uint16_t format = 0;
  std::uint16_t channels = 0;
  std::uint16_t bits = 0;
  std::uint32_t rate = 0;
  bool have_format = false;
  std::size_t data_at = 0;
  std::size_t data_bytes = 0;
  bool have_data = false;

  std::size_t at = 12;
  while (at + 8 <= bytes.size()) {
    const std::uint32_t size = long_at(bytes, at + 4);
    const std::size_t body = at + 8;
    if (body > bytes.size()) break;
    const std::size_t available = std::min<std::size_t>(size, bytes.size() - body);
    if (tag_at(bytes, at, "fmt ") && available >= 16) {
      format = word_at(bytes, body);
      channels = word_at(bytes, body + 2);
      rate = long_at(bytes, body + 4);
      bits = word_at(bytes, body + 14);
      if (format == 0xFFFE) {
        if (available < 40) return std::nullopt;
        format = word_at(bytes, body + 24);
      }
      have_format = true;
    } else if (tag_at(bytes, at, "data")) {
      data_at = body;
      data_bytes = available;
      have_data = true;
    }
    const std::size_t step = static_cast<std::size_t>(size) + (size & 1u);
    if (step == 0) break;
    at = body + step;
  }

  if (!have_format || !have_data) return std::nullopt;
  if (channels == 0 || rate == 0) return std::nullopt;
  if (format != 1 && format != 3) return std::nullopt;
  if (format == 3 && bits != 32) return std::nullopt;
  if (format == 1 && bits != 16 && bits != 24 && bits != 32) return std::nullopt;

  const std::size_t width = static_cast<std::size_t>(bits) / 8u;
  const std::size_t stride = width * channels;
  if (stride == 0) return std::nullopt;
  const std::size_t frames = data_bytes / stride;

  MonoClip clip;
  clip.sample_rate_hz = static_cast<double>(rate);
  clip.samples.resize(frames);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    double sum = 0.0;
    for (std::size_t channel = 0; channel < channels; ++channel) {
      sum += sample_from(bytes, data_at + frame * stride + channel * width, format, bits);
    }
    clip.samples[frame] =
        static_cast<float>(sum / static_cast<double>(channels));
  }
  return clip;
}

AllPoleModel all_pole_model(std::span<const float> mono, double sample_rate_hz,
                            std::size_t order) {
  AllPoleModel model;
  model.sample_rate_hz = sample_rate_hz;
  model.a.assign(order + 1, 0.0);
  if (order == 0 || mono.empty() || !(sample_rate_hz > 0.0)) return model;
  model.a[0] = 1.0;

  std::vector<double> x(mono.begin(), mono.end());
  const double mean =
      std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(x.size());
  double peak = 0.0;
  for (double& value : x) {
    value -= mean;
    peak = std::max(peak, std::abs(value));
  }
  if (!(peak > 0.0)) return model;
  for (double& value : x) value /= peak;

  const std::size_t length = std::min(kFrame, x.size());
  const std::vector<double> window = hann(length);
  std::vector<double> r(order + 1, 0.0);
  std::vector<double> frame(length, 0.0);
  std::size_t frames = 0;
  for (std::size_t start = 0; start + length <= x.size(); start += kHop) {
    for (std::size_t i = 0; i < length; ++i) frame[i] = x[start + i] * window[i];
    for (std::size_t lag = 0; lag <= order && lag < length; ++lag) {
      double acc = 0.0;
      for (std::size_t i = lag; i < length; ++i) acc += frame[i] * frame[i - lag];
      r[lag] += acc;
    }
    ++frames;
    if (length < kFrame) break;
  }
  if (frames == 0) return model;
  for (double& value : r) value /= static_cast<double>(frames);
  if (!(r[0] > 0.0)) return model;
  r[0] *= 1.0 + 1e-6;

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
    if (!(error > 0.0)) {
      error = 1e-30;
      break;
    }
  }
  model.a = a;
  model.gain = std::sqrt(std::max(error, 1e-30));
  return model;
}

std::vector<double> envelope_db(const AllPoleModel& model,
                                std::span<const double> grid_hz) {
  std::vector<double> out;
  out.reserve(grid_hz.size());
  for (const double hz : grid_hz) {
    const double w = 2.0 * std::numbers::pi * hz / model.sample_rate_hz;
    std::complex<double> denominator{0.0, 0.0};
    for (std::size_t k = 0; k < model.a.size(); ++k) {
      denominator += model.a[k] * std::polar(1.0, -w * static_cast<double>(k));
    }
    const double magnitude = std::max(std::abs(denominator), 1e-30);
    out.push_back(20.0 * std::log10(std::max(model.gain / magnitude, 1e-30)));
  }
  return out;
}

std::vector<Resonance> resonances(const AllPoleModel& model, std::size_t count) {
  std::vector<Resonance> out;
  if (count == 0 || model.a.size() < 3 || !(model.sample_rate_hz > 0.0)) return out;
  const std::size_t order = model.a.size() - 1;

  const double top_hz = std::min(20'000.0, 0.45 * model.sample_rate_hz);
  const double floor_db = median_of(envelope_db(
      model, trench::core::logarithmic_frequency_grid(20.0, top_hz, 256)));

  Eigen::MatrixXd companion = Eigen::MatrixXd::Zero(
      static_cast<Eigen::Index>(order), static_cast<Eigen::Index>(order));
  for (std::size_t i = 0; i < order; ++i) {
    companion(0, static_cast<Eigen::Index>(i)) = -model.a[i + 1];
    if (i + 1 < order) {
      companion(static_cast<Eigen::Index>(i + 1), static_cast<Eigen::Index>(i)) = 1.0;
    }
  }
  const Eigen::EigenSolver<Eigen::MatrixXd> solver(companion, false);

  std::vector<std::pair<double, Resonance>> found;
  for (Eigen::Index i = 0; i < solver.eigenvalues().size(); ++i) {
    const std::complex<double> root = solver.eigenvalues()[i];
    if (!(root.imag() > 0.0)) continue;
    const double magnitude = std::abs(root);
    if (!(magnitude < 1.0)) continue;
    const double hz = std::arg(root) * model.sample_rate_hz / (2.0 * std::numbers::pi);
    if (!(hz >= 30.0) || !(hz <= 0.45 * model.sample_rate_hz)) continue;
    const double bw_hz = -std::log(magnitude) * model.sample_rate_hz / std::numbers::pi;
    const std::array<double, 1> one{hz};
    const double prominence = envelope_db(model, one)[0] - floor_db;
    Resonance resonance;
    resonance.hz = hz;
    resonance.bw_hz = bw_hz;
    resonance.gain_db = std::clamp(prominence, 3.0, 24.0);
    found.emplace_back(prominence, resonance);
  }
  std::sort(found.begin(), found.end(),
            [](const auto& left, const auto& right) { return left.first > right.first; });
  if (found.size() > count) found.resize(count);
  out.reserve(found.size());
  for (const auto& entry : found) out.push_back(entry.second);
  std::sort(out.begin(), out.end(),
            [](const Resonance& left, const Resonance& right) { return left.hz < right.hz; });
  return out;
}

std::vector<Resonance> resonances_from_audio(std::span<const float> mono,
                                             double sample_rate_hz,
                                             std::size_t count) {
  return resonances(all_pole_model(mono, sample_rate_hz, model_order(count)), count);
}

std::vector<Resonance> neutral_rows(const AllPoleModel& model, std::size_t count,
                                    const SeedRules& rules) {
  if (count == 0 || model.a.size() < 3 || !(model.sample_rate_hz > 0.0)) return {};
  const double floor_db = floor_db_of(model);
  const double top_hz = 0.45 * model.sample_rate_hz;
  std::vector<std::pair<double, Resonance>> found;
  for (const std::complex<double>& root : upper_roots(model)) {
    const double hz = std::arg(root) * model.sample_rate_hz / (2.0 * std::numbers::pi);
    if (!(hz >= 30.0) || !(hz <= top_hz)) continue;
    const double bw_hz =
        -std::log(std::abs(root)) * model.sample_rate_hz / std::numbers::pi;
    if (!(bw_hz <= hz)) continue;
    const double prominence = db_at_hz(model, hz) - floor_db;
    if (!(prominence >= rules.min_prominence_db)) continue;
    Resonance resonance;
    resonance.hz = hz;
    resonance.bw_hz = rules.min_q > 0.0 ? std::min(bw_hz, hz / rules.min_q) : bw_hz;
    resonance.gain_db = std::clamp(prominence, 3.0, rules.max_gain_db);
    found.emplace_back(prominence, resonance);
  }
  return strongest_by_hz(std::move(found), count);
}

std::vector<float> resample(std::span<const float> mono, double from_hz, double to_hz) {
  if (mono.empty() || !(from_hz > 0.0) || !(to_hz > 0.0)) return {};
  if (from_hz == to_hz) return {mono.begin(), mono.end()};

  constexpr std::size_t kTaps = 127;
  constexpr std::size_t kCentre = kTaps / 2;
  const double cutoff = std::min(0.45 * to_hz / from_hz, 0.49);
  const std::vector<double> window = hann(kTaps);
  std::vector<double> taps(kTaps, 0.0);
  double sum = 0.0;
  for (std::size_t i = 0; i < kTaps; ++i) {
    const double n = static_cast<double>(i) - static_cast<double>(kCentre);
    const double phase = 2.0 * std::numbers::pi * cutoff * n;
    const double sinc = std::abs(n) < 1e-12 ? 1.0 : std::sin(phase) / phase;
    taps[i] = 2.0 * cutoff * sinc * window[i];
    sum += taps[i];
  }
  if (sum > 0.0) {
    for (double& tap : taps) tap /= sum;
  }

  std::vector<double> filtered(mono.size(), 0.0);
  const auto length = static_cast<std::ptrdiff_t>(mono.size());
  for (std::ptrdiff_t n = 0; n < length; ++n) {
    double acc = 0.0;
    for (std::size_t k = 0; k < kTaps; ++k) {
      const std::ptrdiff_t at =
          n + static_cast<std::ptrdiff_t>(k) - static_cast<std::ptrdiff_t>(kCentre);
      if (at < 0 || at >= length) continue;
      acc += taps[k] * static_cast<double>(mono[static_cast<std::size_t>(at)]);
    }
    filtered[static_cast<std::size_t>(n)] = acc;
  }

  const double step = from_hz / to_hz;
  std::vector<float> out;
  out.reserve(static_cast<std::size_t>(static_cast<double>(mono.size()) / step) + 1);
  for (std::size_t i = 0;; ++i) {
    const double position = static_cast<double>(i) * step;
    const auto base = static_cast<std::size_t>(position);
    if (base + 1 >= filtered.size()) break;
    const double frac = position - static_cast<double>(base);
    out.push_back(static_cast<float>(filtered[base] * (1.0 - frac) +
                                     filtered[base + 1] * frac));
  }
  return out;
}

std::vector<Resonance> speech_poles(std::span<const float> mono, double sample_rate_hz,
                                    std::size_t count, double model_rate_hz,
                                    std::size_t order) {
  if (count == 0 || order < 2 || !(model_rate_hz > 0.0)) return {};
  const std::vector<float> voiced = resample(mono, sample_rate_hz, model_rate_hz);
  if (voiced.empty()) return {};
  const AllPoleModel model = all_pole_model(voiced, model_rate_hz, order);
  if (model.a.size() < 3) return {};
  const double floor_db = floor_db_of(model);
  const double top_hz = 0.45 * model_rate_hz;
  std::vector<std::pair<double, Resonance>> found;
  for (const std::complex<double>& root : upper_roots(model)) {
    const double hz = std::arg(root) * model_rate_hz / (2.0 * std::numbers::pi);
    if (!(hz >= 60.0) || !(hz <= top_hz)) continue;
    const double prominence = db_at_hz(model, hz) - floor_db;
    Resonance resonance;
    resonance.hz = hz;
    resonance.bw_hz = -std::log(std::abs(root)) * model_rate_hz / std::numbers::pi;
    resonance.gain_db = prominence;
    found.emplace_back(prominence, resonance);
  }
  return strongest_by_hz(std::move(found), count);
}

}
