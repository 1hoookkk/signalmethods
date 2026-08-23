#include "trench/core/rbj.hpp"

#include <cmath>
#include <numbers>

namespace trench::core::rbj {

namespace {

struct Cookbook {
  double b0, b1, b2, a0, a1, a2;
};

native::Section section_of(const Cookbook& c, double sample_rate_hz, bool dc_stabilised = true) {
  return {native::roots_from_coefficients(c.a1 / c.a0, c.a2 / c.a0, sample_rate_hz),
          native::roots_from_coefficients(c.b1 / c.b0, c.b2 / c.b0, sample_rate_hz),
          dc_stabilised};
}

struct Trig {
  double cos_w0;
  double alpha;
};

Trig trig(double f0_hz, double q, double sample_rate_hz) {
  const double w0 = 2.0 * std::numbers::pi * f0_hz / sample_rate_hz;
  return {std::cos(w0), std::sin(w0) / (2.0 * q)};
}

}  // namespace

double q_from_bandwidth_hz(double f0_hz, double bw_hz) { return f0_hz / bw_hz; }

double bandwidth_hz_from_q(double f0_hz, double q) { return f0_hz / q; }

double q_from_bandwidth_oct(double bw_oct) {
  return 1.0 / (2.0 * std::sinh(std::numbers::ln2 / 2.0 * bw_oct));
}

double bandwidth_oct_from_q(double q) {
  return 2.0 / std::numbers::ln2 * std::asinh(1.0 / (2.0 * q));
}

native::Section peaking(double f0_hz, double q, double gain_db, double sample_rate_hz) {
  const auto [c, alpha] = trig(f0_hz, q, sample_rate_hz);
  const double a = std::pow(10.0, gain_db / 40.0);
  return section_of({1.0 + alpha * a, -2.0 * c, 1.0 - alpha * a, 1.0 + alpha / a, -2.0 * c,
                     1.0 - alpha / a},
                    sample_rate_hz);
}

native::Section lowpass(double f0_hz, double q, double sample_rate_hz) {
  const auto [c, alpha] = trig(f0_hz, q, sample_rate_hz);
  return section_of({(1.0 - c) / 2.0, 1.0 - c, (1.0 - c) / 2.0, 1.0 + alpha, -2.0 * c,
                     1.0 - alpha},
                    sample_rate_hz);
}

native::Section highpass(double f0_hz, double q, double sample_rate_hz) {
  const auto [c, alpha] = trig(f0_hz, q, sample_rate_hz);
  return section_of({(1.0 + c) / 2.0, -(1.0 + c), (1.0 + c) / 2.0, 1.0 + alpha, -2.0 * c,
                     1.0 - alpha},
                    sample_rate_hz, false);
}

native::Section low_shelf(double f0_hz, double q, double gain_db, double sample_rate_hz) {
  const auto [c, alpha] = trig(f0_hz, q, sample_rate_hz);
  const double a = std::pow(10.0, gain_db / 40.0);
  const double s = 2.0 * std::sqrt(a) * alpha;
  return section_of({a * ((a + 1.0) - (a - 1.0) * c + s), 2.0 * a * ((a - 1.0) - (a + 1.0) * c),
                     a * ((a + 1.0) - (a - 1.0) * c - s), (a + 1.0) + (a - 1.0) * c + s,
                     -2.0 * ((a - 1.0) + (a + 1.0) * c), (a + 1.0) + (a - 1.0) * c - s},
                    sample_rate_hz);
}

native::Section high_shelf(double f0_hz, double q, double gain_db, double sample_rate_hz) {
  const auto [c, alpha] = trig(f0_hz, q, sample_rate_hz);
  const double a = std::pow(10.0, gain_db / 40.0);
  const double s = 2.0 * std::sqrt(a) * alpha;
  return section_of({a * ((a + 1.0) + (a - 1.0) * c + s), -2.0 * a * ((a - 1.0) + (a + 1.0) * c),
                     a * ((a + 1.0) + (a - 1.0) * c - s), (a + 1.0) - (a - 1.0) * c + s,
                     2.0 * ((a - 1.0) - (a + 1.0) * c), (a + 1.0) - (a - 1.0) * c - s},
                    sample_rate_hz);
}

}  // namespace trench::core::rbj
