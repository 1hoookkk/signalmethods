#include "trench/core/formants.hpp"

#include <algorithm>
#include <cmath>

#include "trench/core/p2k.hpp"

namespace trench::core::p2k {

namespace {

constexpr std::array<VowelFormants, 12> kKlattTableII{{
    {"iy", {{{310, 45}, {2020, 200}, {2960, 400}}}},
    {"ih", {{{400, 50}, {1800, 100}, {2570, 140}}}},
    {"ey", {{{480, 70}, {1720, 100}, {2520, 200}}}},
    {"eh", {{{530, 60}, {1680, 90}, {2500, 200}}}},
    {"ae", {{{620, 70}, {1660, 150}, {2430, 320}}}},
    {"aa", {{{700, 130}, {1220, 70}, {2600, 160}}}},
    {"ao", {{{600, 90}, {990, 100}, {2570, 80}}}},
    {"ah", {{{620, 80}, {1220, 50}, {2550, 140}}}},
    {"ow", {{{540, 80}, {1100, 70}, {2300, 70}}}},
    {"uh", {{{450, 80}, {1100, 100}, {2350, 80}}}},
    {"uw", {{{350, 65}, {1250, 110}, {2200, 140}}}},
    {"er", {{{470, 100}, {1270, 60}, {1540, 110}}}},
}};

double bw_oct_of(double fc_hz, double bw_hz) {
  return 2.0 * std::asinh(bw_hz / (2.0 * fc_hz)) / std::log(2.0);
}

}  // namespace

std::span<const VowelFormants> klatt_vowels() { return kKlattTableII; }

const VowelFormants* klatt_vowel(std::string_view symbol) {
  for (const auto& v : kKlattTableII) {
    if (v.symbol == symbol) return &v;
  }
  return nullptr;
}

FormantRecipe rows_from_formants(std::span<const Formant> formants, double tilt_lo_hz,
                                 double tilt_hi_hz) {
  FormantRecipe out;
  out.rows[0] = {SectionType::kHighPass, tilt_hi_hz, 0.05, 0.0};
  std::size_t row = 1;
  for (std::size_t i = 0; i < formants.size() && row < 5; ++i, ++row) {
    const auto& f = formants[i];
    const double bw_oct = bw_oct_of(f.hz, f.bw_hz);
    out.rows[row] = {SectionType::kEq, f.hz, bw_oct, 20.0 * std::log10(f.hz / f.bw_hz)};
  }
  for (; row < 5; ++row) {
    out.rows[row] = {SectionType::kOff, 18000.0, 1.0, 0.0};
  }
  out.rows[5] = {SectionType::kLowPass, tilt_lo_hz, 0.8, 0.0};
  return out;
}

std::array<std::array<std::uint16_t, 4>, 6> words_from_recipe(const FormantRecipe& recipe,
                                                             double sample_rate_hz) {
  std::array<std::array<std::uint16_t, 4>, 6> out{};
  const auto identity = identity_words();
  for (std::size_t si = 0; si < 6; ++si) {
    out[si] = words_from_param(recipe.rows[si], identity[si], si, sample_rate_hz);
  }
  return out;
}

std::vector<SpectralPeak> peaks_of_envelope(std::span<const double> hz, std::span<const double> db,
                                            std::size_t max_peaks) {
  std::vector<SpectralPeak> found;
  const std::size_t n = std::min(hz.size(), db.size());
  for (std::size_t i = 2; i + 2 < n; ++i) {
    if (db[i] > db[i - 1] && db[i] >= db[i + 1] && db[i] >= db[i - 2] && db[i] >= db[i + 2]) {
      std::size_t lo = i;
      while (lo > 0 && db[lo] > db[i] - 3.0) --lo;
      std::size_t hi = i;
      while (hi + 1 < n && db[hi] > db[i] - 3.0) ++hi;
      found.push_back({hz[i], db[i], std::max(hz[hi] - hz[lo], 1.0)});
    }
  }
  std::sort(found.begin(), found.end(),
            [](const SpectralPeak& a, const SpectralPeak& b) { return a.db > b.db; });
  if (found.size() > max_peaks) found.resize(max_peaks);
  std::sort(found.begin(), found.end(),
            [](const SpectralPeak& a, const SpectralPeak& b) { return a.hz < b.hz; });
  return found;
}

}  // namespace trench::core::p2k
