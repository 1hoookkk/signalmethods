#include "trench/core/p2k.hpp"

#include <algorithm>

namespace trench::core::p2k {

Corner Corner::identity(std::size_t seed_byte, std::size_t rsq_byte) {
  const std::uint16_t a = word_of(seed_byte);
  const std::uint16_t b = word_of(rsq_byte);
  CornerWords w;
  w.fill({a, b, a, b});
  w[5][1] = kS6ZeroRsqWord;
  w[5][3] = kS6ZeroRsqWord;
  return from_words(w);
}

Corner Corner::from_words(const CornerWords& w) {
  Corner c;
  c.w = w;
  c.num_.assign(kStageCount * kNpts, 0.0);
  c.den_.assign(kStageCount * kNpts, 0.0);
  for (std::size_t si = 0; si < kStageCount; ++si) {
    c.refresh(si);
  }
  return c;
}

void Corner::refresh(std::size_t si) {
  const Grid& g = grid();
  const auto [zp, zq] = pq(w[si][0], w[si][1]);
  const auto [pp, ppq] = pq(w[si][2], w[si][3]);
  g.factor_db(zp, zq, std::span<double>(num_).subspan(si * kNpts, kNpts));
  g.factor_db(pp, ppq, std::span<double>(den_).subspan(si * kNpts, kNpts));
}

std::span<const double> Corner::num(std::size_t si) const {
  return std::span<const double>(num_).subspan(si * kNpts, kNpts);
}

std::span<const double> Corner::den(std::size_t si) const {
  return std::span<const double>(den_).subspan(si * kNpts, kNpts);
}

void Corner::stage_db_into(std::size_t si, std::span<double> out) const {
  const auto n = num(si);
  const auto d = den(si);
  for (std::size_t i = 0; i < kNpts; ++i) {
    out[i] = n[i] - d[i];
  }
}

void Corner::total_into(std::span<double> out) const {
  std::fill(out.begin(), out.begin() + kNpts, 0.0);
  for (std::size_t si = 0; si < kStageCount; ++si) {
    const auto n = num(si);
    const auto d = den(si);
    for (std::size_t i = 0; i < kNpts; ++i) {
      out[i] += n[i] - d[i];
    }
  }
}

std::vector<double> Corner::total() const {
  std::vector<double> out(kNpts, 0.0);
  total_into(out);
  return out;
}

}  // namespace trench::core::p2k
