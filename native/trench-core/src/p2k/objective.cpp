#include "trench/core/p2k.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace trench::core::p2k {

double erb_hz(double f) { return 24.7 * (4.37 * f / 1000.0 + 1.0); }

double psum(std::span<const double> a) {
  const std::size_t n = a.size();
  if (n < 8) {
    double res = 0.0;
    for (const double v : a) {
      res += v;
    }
    return res;
  }
  if (n <= 128) {
    std::array<double, 8> r{a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]};
    std::size_t i = 8;
    while (i + 8 <= n) {
      for (std::size_t k = 0; k < 8; ++k) {
        r[k] += a[i + k];
      }
      i += 8;
    }
    double res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    while (i < n) {
      res += a[i];
      ++i;
    }
    return res;
  }
  std::size_t n2 = n / 2;
  n2 -= n2 % 8;
  return psum(a.subspan(0, n2)) + psum(a.subspan(n2));
}

const Grid& grid() {
  static const Grid g = make_grid(PerceptualSpace{});
  return g;
}

Grid make_grid(const PerceptualSpace& space) {
  Grid out;
  {
    const double lo = std::clamp(space.lo_hz, 1.0, kSr * 0.499);
    const double hi = std::clamp(space.hi_hz, lo * 1.0001, kSr * 0.499);
    const double ratio = hi / lo;
    out.hz.reserve(kNpts);
    for (std::size_t i = 0; i < kNpts; ++i) {
      out.hz.push_back(lo * std::pow(ratio, static_cast<double>(i) /
                                               static_cast<double>(kNpts - 1)));
    }
    out.z1r.reserve(kNpts);
    out.z1i.reserve(kNpts);
    out.z2r.reserve(kNpts);
    out.z2i.reserve(kNpts);
    for (const double f : out.hz) {
      const double w = 2.0 * std::numbers::pi * f / kSr;
      const double cr = std::cos(-w);
      const double ci = std::sin(-w);
      out.z1r.push_back(cr);
      out.z1i.push_back(ci);
      out.z2r.push_back(cr * cr - ci * ci);
      out.z2i.push_back(cr * ci + ci * cr);
    }
    std::vector<double> raw;
    raw.reserve(kNpts);
    for (const double f : out.hz) {
      double w = space.weight == PerceptualSpace::Weight::kErb ? f / erb_hz(f) : 1.0;
      for (const auto& band : space.emphasis) {
        if (f >= band.lo_hz && f <= band.hi_hz) {
          w *= band.gain;
        }
      }
      raw.push_back(w);
    }
    const double total = psum(raw);
    out.weight.reserve(kNpts);
    for (const double w : raw) {
      out.weight.push_back(w * (static_cast<double>(kNpts) / total));
    }
    out.weight_sum = psum(out.weight);
    const double octaves = std::log2(out.hz.back() / out.hz.front());
    const double bins_per_octave = static_cast<double>(kNpts - 1) / octaves;
    out.smooth_bins = space.smooth_octaves > 0.0
                          ? static_cast<std::size_t>(std::lround(space.smooth_octaves * bins_per_octave))
                          : 0;
  }
  return out;
}

namespace {

void box_smooth(std::span<const double> in, std::span<double> out, std::size_t half) {
  double acc = 0.0;
  std::size_t lo = 0;
  std::size_t hi = 0;
  for (std::size_t i = 0; i < kNpts; ++i) {
    const std::size_t want_lo = i > half ? i - half : 0;
    const std::size_t want_hi = std::min(i + half + 1, kNpts);
    while (hi < want_hi) {
      acc += in[hi];
      ++hi;
    }
    while (lo < want_lo) {
      acc -= in[lo];
      ++lo;
    }
    out[i] = acc / static_cast<double>(hi - lo);
  }
}

}  // namespace

void Grid::factor_db(double p, double q, std::span<double> out) const {
  for (std::size_t i = 0; i < kNpts; ++i) {
    const double re = 1.0 + p * z1r[i] + q * z2r[i];
    const double im = p * z1i[i] + q * z2i[i];
    out[i] = 20.0 * std::log10(std::max(std::hypot(re, im), 1e-12));
  }
}

double Grid::weighted_var(std::span<const double> resid_in, std::span<double> scratch) const {
  thread_local std::vector<double> smoothed(kNpts, 0.0);
  std::span<const double> resid = resid_in;
  if (smooth_bins > 0) {
    box_smooth(resid_in, smoothed, smooth_bins / 2);
    resid = smoothed;
  }
  for (std::size_t i = 0; i < kNpts; ++i) {
    scratch[i] = weight[i] * resid[i];
  }
  const double m = psum(scratch.subspan(0, kNpts)) / weight_sum;
  for (std::size_t i = 0; i < kNpts; ++i) {
    const double d = resid[i] - m;
    scratch[i] = weight[i] * d * d;
  }
  return psum(scratch.subspan(0, kNpts)) / weight_sum;
}

double Grid::residual_var(std::span<const double> target, std::span<const double> model,
                          std::span<double> scratch) const {
  std::vector<double> resid(kNpts, 0.0);
  for (std::size_t i = 0; i < kNpts; ++i) {
    resid[i] = target[i] - model[i];
  }
  return weighted_var(resid, scratch);
}

double Grid::variation(std::span<const double> resid) const {
  double acc = 0.0;
  for (std::size_t i = 1; i < kNpts; ++i) {
    acc += std::abs(resid[i] - resid[i - 1]);
  }
  return acc;
}

double Grid::unweighted_var(std::span<const double> resid) const {
  double m = 0.0;
  for (std::size_t i = 0; i < kNpts; ++i) {
    m += resid[i];
  }
  m /= static_cast<double>(kNpts);
  double v = 0.0;
  for (std::size_t i = 0; i < kNpts; ++i) {
    const double d = resid[i] - m;
    v += d * d;
  }
  return v / static_cast<double>(kNpts);
}

double Grid::score(std::span<const double> resid, Cost cost, std::span<double> scratch) const {
  switch (cost) {
    case Cost::kWeightedVar:
      return weighted_var(resid, scratch);
    case Cost::kVariation:
      return variation(resid);
    case Cost::kUnweighted:
      return unweighted_var(resid);
  }
  return 0.0;
}

AbsoluteError absolute_error(std::span<const double> target, std::span<const double> written) {
  const Grid& g = grid();
  AbsoluteError out;
  out.worst_hz = g.hz[0];
  double sq = 0.0;
  double sum = 0.0;
  for (std::size_t i = 0; i < kNpts; ++i) {
    const double d = target[i] - written[i];
    if (std::abs(d) > out.max_db) {
      out.max_db = std::abs(d);
      out.worst_hz = g.hz[i];
    }
    sq += d * d;
    sum += d;
  }
  out.offset_db = sum / static_cast<double>(kNpts);
  double sq_after = 0.0;
  for (std::size_t i = 0; i < kNpts; ++i) {
    const double d = target[i] - written[i] - out.offset_db;
    out.max_after_offset_db = std::max(out.max_after_offset_db, std::abs(d));
    sq_after += d * d;
  }
  out.rms_db = std::sqrt(sq / static_cast<double>(kNpts));
  out.rms_after_offset_db = std::sqrt(sq_after / static_cast<double>(kNpts));
  return out;
}

}  // namespace trench::core::p2k
