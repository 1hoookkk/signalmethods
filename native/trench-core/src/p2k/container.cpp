#include "trench/core/p2k.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "trench/core/packed_body.hpp"

namespace trench::core::p2k {

const std::vector<std::uint16_t>& lattice_words() {
  static const std::vector<std::uint16_t> words = [] {
    std::vector<std::uint16_t> v;
    v.reserve(256 + 16);
    for (std::size_t b = 0; b < 256; ++b) {
      v.push_back(word_of(b));
    }
    for (std::size_t b = 0xF0; b < 256; ++b) {
      v.push_back(static_cast<std::uint16_t>((b << 8U) | kE15AltLow));
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
  }();
  return words;
}

const std::vector<double>& lattice_decoded() {
  static const std::vector<double> decoded = [] {
    std::vector<double> v;
    v.reserve(lattice_words().size());
    for (const auto w : lattice_words()) {
      v.push_back(decode_word(w));
    }
    return v;
  }();
  return decoded;
}

std::size_t lattice_len() { return lattice_words().size(); }

bool magnitude_admissible(std::size_t index, bool is_pole) {
  return !is_pole ||
         static_cast<std::size_t>(lattice_words()[index] >> 8U) <= kMaxMagByte;
}

std::size_t nearest_lattice(double value) {
  const auto& lat = lattice_decoded();
  const auto it = std::lower_bound(lat.begin(), lat.end(), value);
  const std::size_t i = std::clamp<std::size_t>(
      static_cast<std::size_t>(it - lat.begin()), 1, lat.size() - 1);
  const double lo = lat[i - 1];
  const double hi = lat[i];
  return std::abs(value - lo) <= std::abs(value - hi) ? i - 1 : i;
}

std::size_t nearest_lattice_word(std::uint16_t word) {
  const auto& lat = lattice_words();
  const auto it = std::lower_bound(lat.begin(), lat.end(), word);
  const std::size_t i = static_cast<std::size_t>(it - lat.begin());
  if (i < lat.size() && lat[i] == word) {
    return i;
  }
  const std::size_t lo = i == 0 ? 0 : i - 1;
  const std::size_t hi = std::min(i, lat.size() - 1);
  const auto dl = std::abs(static_cast<std::int32_t>(word) - static_cast<std::int32_t>(lat[lo]));
  const auto dh = std::abs(static_cast<std::int32_t>(word) - static_cast<std::int32_t>(lat[hi]));
  return dl <= dh ? lo : hi;
}

std::pair<double, double> pq(std::uint16_t w_mag, std::uint16_t w_rsq) {
  const double d_mag = decode_word(w_mag);
  const double d_rsq = decode_word(w_rsq);
  return {4.0 * d_mag + d_rsq - 2.0, 1.0 - d_rsq};
}

double pair_radius(double p, double q) {
  const double disc = p * p - 4.0 * q;
  if (disc < 0.0) {
    return std::sqrt(std::max(q, 0.0));
  }
  const double s = std::sqrt(disc);
  return std::max(std::abs(0.5 * (-p + s)), std::abs(0.5 * (-p - s)));
}

double pole_radius_ceiling() {
  static const double ceiling = std::sqrt(1.0 - decode_word(kPoleCeilingRsqWord));
  return ceiling;
}

bool is_legal(double p, double q, bool is_pole) {
  const double r = pair_radius(p, q);
  return is_pole ? r <= pole_radius_ceiling() : r <= 1.0;
}

std::pair<std::uint16_t, std::uint16_t> words_from_root(double hz, double r) {
  const auto& lat = lattice_decoded();
  const auto& words = lattice_words();
  const double rc = std::clamp(r, 0.0, 0.999999);
  const std::size_t i_rsq = nearest_lattice(1.0 - rc * rc);
  const double d_rsq_q = lat[i_rsq];
  const double p = -2.0 * std::sqrt(std::max(1.0 - d_rsq_q, 0.0)) *
                   std::cos(2.0 * std::numbers::pi * hz / kSr);
  const std::size_t i_mag = nearest_lattice((p + 2.0 - d_rsq_q) / 4.0);
  return {words[i_mag], words[i_rsq]};
}

std::uint16_t nearest_gain_word(double scale) {
  const double want = std::max(scale, 0.0) / 4.0;
  std::uint16_t best = 0;
  double err = std::numeric_limits<double>::infinity();
  for (std::uint32_t w = 0; w <= 0xFFFF; ++w) {
    const double d = std::abs(decode_word(static_cast<std::uint16_t>(w)) - want);
    if (d < err) {
      err = d;
      best = static_cast<std::uint16_t>(w);
    }
  }
  return best;
}

namespace {

std::optional<std::size_t> mag_index_within(double d_rsq, double want, bool is_pole) {
  const auto& lat = lattice_decoded();
  std::optional<std::size_t> best;
  for (std::size_t i = 0; i < lat.size(); ++i) {
    if (!magnitude_admissible(i, is_pole) ||
        !is_legal(4.0 * lat[i] + d_rsq - 2.0, 1.0 - d_rsq, is_pole)) {
      continue;
    }
    if (!best || std::abs(lat[i] - want) < std::abs(lat[*best] - want)) {
      best = i;
    }
  }
  return best;
}

std::pair<std::uint16_t, std::uint16_t> seat_root(std::uint16_t w_mag, std::uint16_t w_rsq,
                                                  bool is_pole, bool rsq_is_fixed) {
  const auto [p, q] = pq(w_mag, w_rsq);
  if (is_legal(p, q, is_pole)) {
    return {w_mag, w_rsq};
  }
  const auto& lat = lattice_decoded();
  const auto& words = lattice_words();
  const double want = decode_word(w_mag);
  if (rsq_is_fixed) {
    const double d_rsq = decode_word(w_rsq);
    if (const auto i = mag_index_within(d_rsq, want, is_pole)) {
      return {words[*i], w_rsq};
    }
    return {w_mag, w_rsq};
  }
  const std::size_t start = nearest_lattice_word(w_rsq);
  for (std::size_t step = 0; step < lat.size(); ++step) {
    for (const std::size_t i_rsq : {start + step, start - step}) {
      if (i_rsq >= lat.size()) {
        continue;
      }
      const double d_rsq = lat[i_rsq];
      if (const auto i_mag = mag_index_within(d_rsq, want, is_pole)) {
        return {words[*i_mag], words[i_rsq]};
      }
    }
  }
  return {w_mag, w_rsq};
}

}  // namespace

CornerWords enter(const CornerWords& words) {
  CornerWords out = words;
  for (std::size_t si = 0; si < kStageCount; ++si) {
    const bool zero_fixed = si == kStageCount - 1;
    const auto [zm, zr] = seat_root(out[si][0], out[si][1], false, zero_fixed);
    out[si][0] = zm;
    out[si][1] = zr;
    const auto [pm, pr] = seat_root(out[si][2], out[si][3], true, false);
    out[si][2] = pm;
    out[si][3] = pr;
  }
  return out;
}

}  // namespace trench::core::p2k
