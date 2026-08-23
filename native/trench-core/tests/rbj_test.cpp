#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"
#include "trench/core/rbj.hpp"

namespace nb = trench::core::native;
namespace rbj = trench::core::rbj;

namespace {

constexpr double kSr = 48'000.0;

double cookbook_db(double b0, double b1, double b2, double a0, double a1, double a2, double hz) {
  const auto z = std::polar(1.0, -2.0 * std::numbers::pi * hz / kSr);
  return 20.0 * std::log10(std::abs((b0 + b1 * z + b2 * z * z) / (a0 + a1 * z + a2 * z * z)));
}

double shape_error(const nb::Section& section,
                   const std::vector<double>& reference_db) {
  const auto& hz = trench::core::p2k::grid().hz;
  const auto biquad = nb::biquad(nb::design(section, kSr));
  double offset = 0.0;
  std::vector<double> diff(hz.size());
  for (std::size_t i = 0; i < hz.size(); ++i) {
    diff[i] = trench::core::section_response_db(biquad, hz[i], kSr) - reference_db[i];
    offset += diff[i];
  }
  offset /= static_cast<double>(hz.size());
  double worst = 0.0;
  for (const double d : diff) worst = std::max(worst, std::abs(d - offset));
  return worst;
}

}  // namespace

TEST(Rbj, QAndBandwidthAreOneNumber) {
  EXPECT_NEAR(rbj::q_from_bandwidth_hz(1000.0, 100.0), 10.0, 1e-12);
  EXPECT_NEAR(rbj::bandwidth_hz_from_q(1000.0, 10.0), 100.0, 1e-12);
  EXPECT_NEAR(rbj::q_from_bandwidth_oct(1.0), std::sqrt(2.0), 1e-3);
  EXPECT_NEAR(rbj::bandwidth_oct_from_q(rbj::q_from_bandwidth_oct(0.37)), 0.37, 1e-12);
}

TEST(Rbj, PeakingMatchesTheCookbookAndIsUnityAtDc) {
  const double f0 = 1'234.0;
  const double q = 4.0;
  const double gain = 9.0;
  const double w0 = 2.0 * std::numbers::pi * f0 / kSr;
  const double a = std::pow(10.0, gain / 40.0);
  const double alpha = std::sin(w0) / (2.0 * q);
  const auto& hz = trench::core::p2k::grid().hz;
  std::vector<double> reference(hz.size());
  for (std::size_t i = 0; i < hz.size(); ++i) {
    reference[i] = cookbook_db(1.0 + alpha * a, -2.0 * std::cos(w0), 1.0 - alpha * a,
                               1.0 + alpha / a, -2.0 * std::cos(w0), 1.0 - alpha / a, hz[i]);
  }
  const auto section = rbj::peaking(f0, q, gain, kSr);
  EXPECT_LT(shape_error(section, reference), 1e-9);
  const auto biquad = nb::biquad(nb::design(section, kSr));
  EXPECT_NEAR(trench::core::section_response_db(biquad, 0.0, kSr), 0.0, 1e-9);
  EXPECT_NEAR(trench::core::section_response_db(biquad, f0, kSr), gain, 0.05);
  const auto* pole = std::get_if<nb::Resonant>(&section.pole);
  ASSERT_NE(pole, nullptr);
  EXPECT_NEAR(pole->hz, f0, 0.005 * f0);
}

TEST(Rbj, LowpassHighpassAndShelvesMatchTheCookbookShape) {
  const double f0 = 800.0;
  const double q = 0.7071;
  const double w0 = 2.0 * std::numbers::pi * f0 / kSr;
  const double c = std::cos(w0);
  const double alpha = std::sin(w0) / (2.0 * q);
  const auto& hz = trench::core::p2k::grid().hz;
  std::vector<double> lp(hz.size()), hp(hz.size()), ls(hz.size()), hs(hz.size());
  const double a = std::pow(10.0, 6.0 / 40.0);
  const double s = 2.0 * std::sqrt(a) * alpha;
  for (std::size_t i = 0; i < hz.size(); ++i) {
    lp[i] = cookbook_db((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha, hz[i]);
    hp[i] = cookbook_db((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha, hz[i]);
    ls[i] = cookbook_db(a * ((a + 1) - (a - 1) * c + s), 2 * a * ((a - 1) - (a + 1) * c),
                        a * ((a + 1) - (a - 1) * c - s), (a + 1) + (a - 1) * c + s,
                        -2 * ((a - 1) + (a + 1) * c), (a + 1) + (a - 1) * c - s, hz[i]);
    hs[i] = cookbook_db(a * ((a + 1) + (a - 1) * c + s), -2 * a * ((a - 1) + (a + 1) * c),
                        a * ((a + 1) + (a - 1) * c - s), (a + 1) - (a - 1) * c + s,
                        2 * ((a - 1) - (a + 1) * c), (a + 1) - (a - 1) * c - s, hz[i]);
  }
  EXPECT_LT(shape_error(rbj::lowpass(f0, q, kSr), lp), 1e-9);
  EXPECT_LT(shape_error(rbj::highpass(f0, q, kSr), hp), 1e-9);
  EXPECT_LT(shape_error(rbj::low_shelf(f0, q, 6.0, kSr), ls), 1e-9);
  EXPECT_LT(shape_error(rbj::high_shelf(f0, q, 6.0, kSr), hs), 1e-9);
  EXPECT_FALSE(rbj::highpass(f0, q, kSr).dc_stabilised);
  const auto lowpass = rbj::lowpass(f0, q, kSr);
  const auto* lowpass_zero = std::get_if<nb::RealRoots>(&lowpass.zero);
  ASSERT_NE(lowpass_zero, nullptr);
  EXPECT_TRUE(std::signbit(lowpass_zero->a_hz));
}
