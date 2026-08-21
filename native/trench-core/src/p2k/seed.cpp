#include "trench/core/p2k.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <tuple>

#include "trench/core/packed_body.hpp"

namespace trench::core::p2k {

namespace {

constexpr std::size_t kMaxIters = 400;
constexpr std::size_t kNvar = 4 * kStageCount - 1;

}  // namespace

std::uint64_t Rng::next_u64() {
  state_ += 0x9E37'79B9'7F4A'7C15ULL;
  std::uint64_t z = state_;
  z = (z ^ (z >> 30U)) * 0xBF58'476D'1CE4'E5B9ULL;
  z = (z ^ (z >> 27U)) * 0x94D0'49BB'1331'11EBULL;
  return z ^ (z >> 31U);
}

double Rng::uniform(double lo, double hi) {
  return lo + (hi - lo) * (static_cast<double>(next_u64() >> 11U) /
                           static_cast<double>(1ULL << 53U));
}

std::size_t Rng::below(std::size_t n) {
  return static_cast<std::size_t>(next_u64() % static_cast<std::uint64_t>(n));
}

double s6_zero_radius() { return std::sqrt(1.0 - decode_word(kS6ZeroRsqWord)); }

namespace {

using Vars = std::array<double, kNvar>;

std::pair<Vars, Vars> bounds() {
  const double lo_l = std::log(kLoHz);
  const double hi_l = std::log(kRootHiHz);
  Vars lo{};
  Vars hi{};
  std::size_t k = 0;
  for (std::size_t i = 0; i < kStageCount; ++i) {
    lo[k] = lo_l;
    hi[k] = hi_l;
    ++k;
    lo[k] = 0.05;
    hi[k] = kPoleRMax;
    ++k;
    lo[k] = lo_l;
    hi[k] = hi_l;
    ++k;
    if (i != kStageCount - 1) {
      lo[k] = 0.0;
      hi[k] = 1.0;
      ++k;
    }
  }
  return {lo, hi};
}

ContinuousCorner unpack(const Vars& x) {
  const double r496 = s6_zero_radius();
  ContinuousCorner out{};
  std::size_t k = 0;
  for (std::size_t i = 0; i < kStageCount; ++i) {
    out[i].pole_hz = std::exp(x[k]);
    ++k;
    out[i].pole_r = x[k];
    ++k;
    out[i].zero_hz = std::exp(x[k]);
    ++k;
    if (i == kStageCount - 1) {
      out[i].zero_r = r496;
    } else {
      out[i].zero_r = x[k];
      ++k;
    }
  }
  return out;
}

void stage_curve(const ContinuousStage& s, std::span<double> num, std::span<double> den,
                 std::span<double> out) {
  const Grid& g = grid();
  const double wz = 2.0 * std::numbers::pi * s.zero_hz / kSr;
  const double wp = 2.0 * std::numbers::pi * s.pole_hz / kSr;
  g.factor_db(-2.0 * s.zero_r * std::cos(wz), s.zero_r * s.zero_r, num);
  g.factor_db(-2.0 * s.pole_r * std::cos(wp), s.pole_r * s.pole_r, den);
  for (std::size_t i = 0; i < kNpts; ++i) {
    out[i] = num[i] - den[i];
  }
}

struct Model {
  std::array<std::vector<double>, kStageCount> stages;
  std::vector<double> num;
  std::vector<double> den;
  std::vector<double> total;

  Model() : num(kNpts, 0.0), den(kNpts, 0.0), total(kNpts, 0.0) {
    for (auto& s : stages) {
      s.assign(kNpts, 0.0);
    }
  }

  void rebuild(const ContinuousCorner& params) {
    for (std::size_t si = 0; si < kStageCount; ++si) {
      stage_curve(params[si], num, den, stages[si]);
    }
    sum();
  }

  void rebuild_stage(std::size_t si, const ContinuousStage& s) {
    stage_curve(s, num, den, stages[si]);
    sum();
  }

  void sum() {
    std::fill(total.begin(), total.end(), 0.0);
    for (std::size_t si = 0; si < kStageCount; ++si) {
      for (std::size_t i = 0; i < kNpts; ++i) {
        total[i] += stages[si][i];
      }
    }
  }
};

double residuals(std::span<const double> target, std::span<const double> model,
                 std::span<const double> sw, std::span<double> out, std::span<double> tmp) {
  const Grid& g = grid();
  for (std::size_t i = 0; i < kNpts; ++i) {
    out[i] = target[i] - model[i];
  }
  for (std::size_t i = 0; i < kNpts; ++i) {
    tmp[i] = g.weight[i] * out[i];
  }
  const double m = psum(tmp.subspan(0, kNpts)) / g.weight_sum;
  for (std::size_t i = 0; i < kNpts; ++i) {
    out[i] = sw[i] * (out[i] - m);
  }
  for (std::size_t i = 0; i < kNpts; ++i) {
    tmp[i] = out[i] * out[i];
  }
  return psum(tmp.subspan(0, kNpts)) / static_cast<double>(kNpts);
}

std::optional<std::vector<double>> solve_normal(std::vector<double>& a, std::vector<double>& b,
                                                std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t pivot = i;
    for (std::size_t k = i + 1; k < n; ++k) {
      if (std::abs(a[k * n + i]) > std::abs(a[pivot * n + i])) {
        pivot = k;
      }
    }
    if (std::abs(a[pivot * n + i]) < 1e-14) {
      return std::nullopt;
    }
    if (pivot != i) {
      for (std::size_t j = 0; j < n; ++j) {
        std::swap(a[i * n + j], a[pivot * n + j]);
      }
      std::swap(b[i], b[pivot]);
    }
    for (std::size_t k = i + 1; k < n; ++k) {
      const double f = a[k * n + i] / a[i * n + i];
      for (std::size_t j = i; j < n; ++j) {
        a[k * n + j] -= f * a[i * n + j];
      }
      b[k] -= f * b[i];
    }
  }
  std::vector<double> x(n, 0.0);
  for (std::size_t ii = n; ii > 0; --ii) {
    const std::size_t i = ii - 1;
    double acc = b[i];
    for (std::size_t j = i + 1; j < n; ++j) {
      acc -= a[i * n + j] * x[j];
    }
    x[i] = acc / a[i * n + i];
  }
  return x;
}

std::size_t var_stage(std::size_t k) {
  std::size_t i = 0;
  std::size_t acc = 0;
  for (;;) {
    const std::size_t width = i == kStageCount - 1 ? 3 : 4;
    if (k < acc + width) {
      return i;
    }
    acc += width;
    ++i;
  }
}

std::pair<Vars, double> refine(Vars x0, std::span<const double> target,
                               std::span<const double> sw) {
  const auto [lo, hi] = bounds();
  Vars x = x0;
  for (std::size_t k = 0; k < kNvar; ++k) {
    x[k] = std::clamp(x[k], lo[k] + 1e-9, hi[k] - 1e-9);
  }

  Model model;
  model.rebuild(unpack(x));

  std::vector<double> resid(kNpts, 0.0);
  std::vector<double> tmp(kNpts, 0.0);
  std::vector<double> trial_resid(kNpts, 0.0);
  double cost = residuals(target, model.total, sw, resid, tmp);

  std::vector<double> jac(kNpts * kNvar, 0.0);
  double lambda = 1e-2;
  Model probe;
  std::size_t stalled = 0;

  for (std::size_t iter = 0; iter < kMaxIters; ++iter) {
    for (std::size_t k = 0; k < kNvar; ++k) {
      const double h = 1e-6 * std::max(std::abs(x[k]), 1.0);
      Vars xp = x;
      xp[k] = x[k] + h > hi[k] ? std::max(x[k] - h, lo[k]) : x[k] + h;
      const double step = xp[k] - x[k];
      if (step == 0.0) {
        for (std::size_t i = 0; i < kNpts; ++i) {
          jac[i * kNvar + k] = 0.0;
        }
        continue;
      }
      const std::size_t si = var_stage(k);
      probe.stages = model.stages;
      const ContinuousCorner pp = unpack(xp);
      probe.rebuild_stage(si, pp[si]);
      residuals(target, probe.total, sw, trial_resid, tmp);
      for (std::size_t i = 0; i < kNpts; ++i) {
        jac[i * kNvar + k] = (trial_resid[i] - resid[i]) / step;
      }
    }

    std::vector<double> jtj(kNvar * kNvar, 0.0);
    std::vector<double> jtr(kNvar, 0.0);
    for (std::size_t i = 0; i < kNpts; ++i) {
      const std::size_t row = i * kNvar;
      const double ri = resid[i];
      for (std::size_t p = 0; p < kNvar; ++p) {
        const double jp = jac[row + p];
        jtr[p] += jp * ri;
        for (std::size_t q = p; q < kNvar; ++q) {
          jtj[p * kNvar + q] += jp * jac[row + q];
        }
      }
    }
    for (std::size_t p = 0; p < kNvar; ++p) {
      for (std::size_t q = 0; q < p; ++q) {
        jtj[p * kNvar + q] = jtj[q * kNvar + p];
      }
    }

    std::vector<double> a = jtj;
    for (std::size_t p = 0; p < kNvar; ++p) {
      a[p * kNvar + p] += lambda * (jtj[p * kNvar + p] + 1e-9);
    }
    std::vector<double> b = jtr;
    const auto delta = solve_normal(a, b, kNvar);
    if (!delta) {
      lambda *= 4.0;
      if (lambda > 1e8) {
        break;
      }
      continue;
    }

    Vars xt = x;
    for (std::size_t k = 0; k < kNvar; ++k) {
      xt[k] = std::clamp(x[k] + (*delta)[k], lo[k], hi[k]);
    }
    probe.rebuild(unpack(xt));
    const double trial_cost = residuals(target, probe.total, sw, trial_resid, tmp);

    if (trial_cost < cost) {
      stalled = cost - trial_cost < 1e-12 * std::max(cost, 1e-12) ? stalled + 1 : 0;
      x = xt;
      model.stages = probe.stages;
      model.total = probe.total;
      resid = trial_resid;
      cost = trial_cost;
      lambda *= 0.3;
    } else {
      ++stalled;
      lambda *= 2.5;
      if (lambda > 1e8) {
        break;
      }
    }
    if (stalled >= 12) {
      break;
    }
  }

  return {x, cost};
}

std::vector<double> sqrt_weights() {
  const Grid& g = grid();
  const double w_mean = psum(g.weight) / static_cast<double>(kNpts);
  std::vector<double> sw;
  sw.reserve(kNpts);
  for (const double w : g.weight) {
    sw.push_back(std::sqrt(w / w_mean));
  }
  return sw;
}

Vars random_start(Rng& rng) {
  const double lo_l = std::log(kLoHz);
  const double hi_l = std::log(kRootHiHz);
  Vars x0{};
  std::size_t k = 0;
  for (std::size_t i = 0; i < kStageCount; ++i) {
    x0[k] = rng.uniform(lo_l, hi_l);
    ++k;
    x0[k] = rng.uniform(0.6, std::min(0.99, kPoleRMax));
    ++k;
    x0[k] = rng.uniform(lo_l, hi_l);
    ++k;
    if (i != kStageCount - 1) {
      x0[k] = rng.uniform(0.3, 0.99);
      ++k;
    }
  }
  return x0;
}

}  // namespace

std::pair<ContinuousCorner, double> continuous_from(std::span<const double> target,
                                                    const ContinuousCorner& start) {
  const auto sw = sqrt_weights();
  Vars x0{};
  std::size_t k = 0;
  for (std::size_t i = 0; i < kStageCount; ++i) {
    x0[k] = std::log(std::max(start[i].pole_hz, kLoHz));
    ++k;
    x0[k] = start[i].pole_r;
    ++k;
    x0[k] = std::log(std::max(start[i].zero_hz, kLoHz));
    ++k;
    if (i != kStageCount - 1) {
      x0[k] = start[i].zero_r;
      ++k;
    }
  }
  const auto [x, cost] = refine(x0, target, sw);
  return {unpack(x), std::sqrt(std::max(cost, 0.0))};
}

std::optional<std::pair<ContinuousCorner, double>> continuous_capacity(
    std::span<const double> target, Rng& rng, std::size_t starts) {
  const auto sw = sqrt_weights();
  std::optional<std::pair<Vars, double>> best;
  for (std::size_t i = 0; i < starts; ++i) {
    const auto [x, cost] = refine(random_start(rng), target, sw);
    if (!best || cost < best->second) {
      best.emplace(x, cost);
    }
  }
  if (!best) {
    return std::nullopt;
  }
  return std::pair{unpack(best->first), std::sqrt(std::max(best->second, 0.0))};
}

std::optional<ContinuousCorner> continuous_seed(std::span<const double> target, Rng& rng,
                                                std::size_t starts) {
  const auto sw = sqrt_weights();
  std::optional<std::pair<Vars, double>> best;
  for (std::size_t i = 0; i < starts; ++i) {
    const auto [x, cost] = refine(random_start(rng), target, sw);
    if (!best || cost < best->second) {
      best.emplace(x, cost);
    }
  }
  if (!best) {
    return std::nullopt;
  }
  return unpack(best->first);
}

CornerWords words_from_continuous(const ContinuousCorner& seed) {
  CornerWords w{};
  for (std::size_t si = 0; si < kStageCount; ++si) {
    const auto [zm, zr] = words_from_root(seed[si].zero_hz, seed[si].zero_r);
    w[si][0] = zm;
    w[si][1] = si == kStageCount - 1 ? kS6ZeroRsqWord : zr;
    const auto [pm, pr] = words_from_root(seed[si].pole_hz, seed[si].pole_r);
    w[si][2] = pm;
    w[si][3] = pr;
  }
  return w;
}

StoredCorner rom_corner_words(std::span<const std::uint8_t> body, std::size_t corner) {
  if (body.size() != 240) {
    throw std::invalid_argument("a P2K body is 240 bytes");
  }
  if (corner >= 4) {
    throw std::invalid_argument("a P2K body has 4 corners");
  }
  StoredCorner out{};
  for (std::size_t si = 0; si < kStageCount; ++si) {
    for (std::size_t wi = 0; wi < kWordCount; ++wi) {
      const std::size_t off = corner * 60 + si * 10 + wi * 2;
      out[si][wi] = static_cast<std::uint16_t>(body[off]) |
                    static_cast<std::uint16_t>(static_cast<std::uint16_t>(body[off + 1]) << 8U);
    }
  }
  return out;
}

CornerWords rom_seed(const StoredCorner& words) {
  CornerWords w{};
  for (std::size_t si = 0; si < kStageCount; ++si) {
    for (std::size_t wi = 0; wi < 4; ++wi) {
      w[si][wi] = lattice_words()[nearest_lattice_word(words[si][wi])];
    }
  }
  w[5][1] = kS6ZeroRsqWord;
  return w;
}

bool pole_is_legal(const StageWords& w) {
  const auto [p, q] = pq(w[2], w[3]);
  return is_legal(p, q, true);
}

namespace {

ContinuousStage stage(double pole_hz, double pole_r, double zero_hz, double zero_r) {
  return {pole_hz, pole_r, zero_hz, zero_r};
}

double spread(double lo, double hi, std::size_t i) {
  return lo * std::pow(hi / lo, static_cast<double>(i) / static_cast<double>(kStageCount - 1));
}

std::vector<std::pair<double, double>> peaks_of(std::span<const double> target, bool want_max) {
  const Grid& g = grid();
  std::vector<std::pair<double, double>> found;
  for (std::size_t i = 2; i < kNpts - 2; ++i) {
    const double v = target[i];
    const bool is =
        want_max ? v > target[i - 1] && v > target[i + 1] && v >= target[i - 2] &&
                       v >= target[i + 2]
                 : v < target[i - 1] && v < target[i + 1] && v <= target[i - 2] &&
                       v <= target[i + 2];
    if (is) {
      found.emplace_back(g.hz[i], v);
    }
  }
  std::stable_sort(found.begin(), found.end(), [want_max](const auto& a, const auto& b) {
    return want_max ? b.second < a.second : a.second < b.second;
  });
  return found;
}

}  // namespace

std::vector<std::pair<std::string_view, ContinuousCorner>> topological_seeds(
    std::span<const double> target) {
  const double r496 = s6_zero_radius();
  std::vector<std::pair<std::string_view, ContinuousCorner>> out;

  {
    ContinuousCorner c{};
    for (std::size_t i = 0; i < kStageCount; ++i) {
      const double f = spread(60.0, 12'000.0, i);
      c[i] = stage(f, 0.90, f, i == kStageCount - 1 ? r496 : 0.50);
    }
    out.emplace_back("even neutral", c);
  }
  {
    ContinuousCorner c{};
    for (std::size_t i = 0; i < kStageCount; ++i) {
      const double f = spread(60.0, 900.0, i);
      c[i] = stage(f, 0.90 + 0.016 * static_cast<double>(i), spread(2'000.0, 16'000.0, i),
                   i == kStageCount - 1 ? r496 : 0.70);
    }
    out.emplace_back("stacked lowpass", c);
  }
  {
    ContinuousCorner c{};
    for (std::size_t i = 0; i < kStageCount; ++i) {
      const double f = spread(1'500.0, 14'000.0, i);
      c[i] = stage(f, 0.90, spread(40.0, 400.0, i), i == kStageCount - 1 ? r496 : 0.70);
    }
    out.emplace_back("stacked highpass", c);
  }
  {
    ContinuousCorner c{};
    for (std::size_t i = 0; i < kStageCount; ++i) {
      const double f = spread(300.0, 3'500.0, i);
      c[i] = stage(f, 0.95, f * 1.6, i == kStageCount - 1 ? r496 : 0.85);
    }
    out.emplace_back("bandpass cluster", c);
  }
  {
    ContinuousCorner c{};
    for (std::size_t i = 0; i < kStageCount; ++i) {
      const double f = spread(80.0, 14'000.0, i);
      c[i] = stage(f, 0.985 - 0.09 * static_cast<double>(i), f * 9.0,
                   i == kStageCount - 1 ? r496 : 0.60);
    }
    out.emplace_back("descending tilt", c);
  }

  const auto hi = peaks_of(target, true);
  const auto lo = peaks_of(target, false);
  if (!hi.empty()) {
    {
      ContinuousCorner c{};
      for (std::size_t i = 0; i < kStageCount; ++i) {
        const auto p = i < hi.size() ? hi[i] : std::pair{spread(80.0, 12'000.0, i), 0.0};
        const auto z = i < lo.size() ? lo[i] : std::pair{p.first * 1.5, 0.0};
        const double depth = std::min(std::abs(p.second - z.second), 60.0);
        c[i] = stage(std::clamp(p.first, kLoHz, kRootHiHz), std::min(0.90 + depth / 600.0, 0.995),
                     std::clamp(z.first, kLoHz, kRootHiHz),
                     i == kStageCount - 1 ? r496 : std::min(0.70 + depth / 300.0, 0.99));
      }
      out.emplace_back("target topology", c);
    }
    {
      ContinuousCorner c{};
      for (std::size_t i = 0; i < kStageCount; ++i) {
        const auto p = i < hi.size() ? hi[i] : std::pair{spread(80.0, 12'000.0, i), 0.0};
        const auto z = i < lo.size() ? lo[i] : std::pair{p.first * 1.5, 0.0};
        c[i] = stage(std::clamp(p.first, kLoHz, kRootHiHz), 0.985,
                     std::clamp(z.first, kLoHz, kRootHiHz), i == kStageCount - 1 ? r496 : 0.96);
      }
      out.emplace_back("target topology, tight", c);
    }
  }
  return out;
}

std::optional<std::tuple<std::string_view, ContinuousCorner, double>> continuous_best(
    std::span<const double> target) {
  std::optional<std::tuple<std::string_view, ContinuousCorner, double>> best;
  for (const auto& [name, start] : topological_seeds(target)) {
    const auto [refined, rms] = continuous_from(target, start);
    if (!best || rms < std::get<2>(*best)) {
      best.emplace(name, refined, rms);
    }
  }
  return best;
}

CornerWords identity_words() {
  const std::uint16_t a = lattice_words()[128];
  const std::uint16_t b = lattice_words()[200];
  CornerWords w;
  w.fill({a, b, a, b});
  w[5][1] = kS6ZeroRsqWord;
  w[5][3] = b;
  return w;
}

CornerWords peel_seed(std::span<const double> target) {
  return peel_seed_by(target, Cost::kWeightedVar);
}

CornerWords peel_seed_by(std::span<const double> target, Cost cost) {
  const Grid& g = grid();
  Corner c = Corner::from_words(identity_words());
  Scratch s;
  std::vector<double> total(kNpts, 0.0);

  for (std::size_t si = 0; si < kStageCount; ++si) {
    c.total_into(total);
    double mean = 0.0;
    for (std::size_t i = 0; i < kNpts; ++i) {
      mean += g.weight[i] * (target[i] - total[i]);
    }
    mean /= g.weight_sum;

    std::size_t peak_i = 0;
    double peak_v = 0.0;
    for (std::size_t i = 0; i < kNpts; ++i) {
      const double d = target[i] - total[i] - mean;
      if (std::abs(d) > std::abs(peak_v)) {
        peak_v = d;
        peak_i = i;
      }
    }
    const double hz = std::clamp(g.hz[peak_i], kLoHz, kRootHiHz);
    const double prominence = std::min(std::abs(peak_v), 48.0);
    const double tight = std::min(0.90 + prominence / 500.0, 0.995);

    const double pole_hz = hz;
    const double zero_hz = hz;
    const double pole_r = peak_v >= 0.0 ? tight : 0.55;
    const double zero_r = peak_v >= 0.0 ? 0.55 : std::min(tight, 0.99);
    const auto [zm, zr] = words_from_root(zero_hz, zero_r);
    const auto [pm, pr] = words_from_root(pole_hz, pole_r);
    c.w[si][0] = zm;
    c.w[si][1] = si == kStageCount - 1 ? kS6ZeroRsqWord : zr;
    c.w[si][2] = pm;
    c.w[si][3] = pr;
    c.refresh(si);

    double best = corner_cost(c, target, cost, s);
    for (std::size_t pass = 0; pass < 3; ++pass) {
      const auto [v, moved] = stage_moves_cost(c, target, si, best, cost, s);
      best = v;
      if (!moved) {
        break;
      }
    }
  }
  return enter(c.w);
}

}  // namespace trench::core::p2k
