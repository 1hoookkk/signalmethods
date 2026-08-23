#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "trench/core/audition.hpp"
#include "trench/core/morph.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

namespace p2k = trench::core::p2k;
namespace nb = trench::core::native;

namespace {

std::vector<std::pair<std::string, std::vector<std::uint8_t>>> bank() {
  const std::filesystem::path dir = std::string(TRENCH_SOURCE_ROOT) + "/ref/presets";
  std::vector<std::filesystem::path> paths;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() == ".bin") paths.push_back(entry.path());
  }
  std::sort(paths.begin(), paths.end());
  std::vector<std::pair<std::string, std::vector<std::uint8_t>>> out;
  for (const auto& path : paths) {
    std::ifstream in(path, std::ios::binary);
    out.emplace_back(path.stem().string(),
                     std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                               std::istreambuf_iterator<char>()));
  }
  return out;
}

std::vector<std::pair<std::string, std::vector<std::uint8_t>>> engine_bank() {
  auto all = bank();
  std::erase_if(all, [](const auto& entry) { return entry.first > "P2k_032"; });
  return all;
}

std::vector<std::uint8_t> hedz() {
  for (auto& [name, bytes] : bank()) {
    if (name == "P2k_013_talking_hedz") return bytes;
  }
  return {};
}

double mean_of(const std::vector<double>& v) {
  double acc = 0.0;
  for (const double x : v) acc += x;
  return acc / static_cast<double>(v.size());
}

}  // namespace

TEST(NativeBody, RootsSurviveAnyRateRoundTrip) {
  for (const double sr : {44'100.0, 48'000.0, 96'000.0}) {
    const nb::Roots cases[] = {nb::Resonant{1'234.5, 80.0}, nb::RealRoots{300.0, -5'000.0},
                               nb::RealRoots{std::numeric_limits<double>::infinity(),
                                             std::numeric_limits<double>::infinity()}};
    for (const auto& roots : cases) {
      const auto [p, q] = nb::coefficients_of(roots, sr);
      const auto back = nb::roots_from_coefficients(p, q, sr);
      if (const auto* res = std::get_if<nb::Resonant>(&roots)) {
        const auto* got = std::get_if<nb::Resonant>(&back);
        ASSERT_NE(got, nullptr);
        EXPECT_NEAR(got->hz, res->hz, 1e-9);
        EXPECT_NEAR(got->bw_hz, res->bw_hz, 1e-9);
      } else {
        const auto& real = std::get<nb::RealRoots>(roots);
        const auto* got = std::get_if<nb::RealRoots>(&back);
        ASSERT_NE(got, nullptr);
        const double lo = std::min(real.a_hz, real.b_hz);
        const double hi = std::max(real.a_hz, real.b_hz);
        EXPECT_NEAR(std::min(got->a_hz, got->b_hz), lo, 1e-6 * std::max(1.0, std::abs(lo)));
        EXPECT_NEAR(std::max(got->a_hz, got->b_hz), hi, 1e-6 * std::max(1.0, std::abs(hi)));
      }
    }
  }
}

TEST(NativeBody, EveryFactoryCornerNullsAgainstItsDecodedResponse) {
  double worst_shape = 0.0;
  double worst_level = 0.0;
  std::string worst_level_at;
  for (const auto& [name, body] : bank()) {
    const auto imported = nb::import_p2k(body);
    for (std::size_t ci = 0; ci < 4; ++ci) {
      const auto words = p2k::rom_corner_words(body, ci);
      trench::core::Cascade packed_cascade{};
      for (auto& s : packed_cascade) s = {1.0, 0.0, 0.0, 0.0, 0.0};
      for (std::size_t si = 0; si < nb::kSections; ++si) {
        packed_cascade[si] = trench::core::section_words_to_biquad(words[si]);
      }
      const auto design = nb::design(imported.corners[ci], p2k::kSr);
      const auto float_cascade = nb::cascade(design, imported.corners[ci].gain_db);
      const auto& hz = p2k::grid().hz;
      if (!std::isfinite(trench::core::cascade_response_db(packed_cascade, hz[0], p2k::kSr))) {
        continue;
      }
      std::vector<double> diff(hz.size());
      for (std::size_t i = 0; i < diff.size(); ++i) {
        diff[i] = trench::core::cascade_response_db(float_cascade, hz[i], p2k::kSr) -
                  trench::core::cascade_response_db(packed_cascade, hz[i], p2k::kSr);
      }
      const double level = mean_of(diff);
      for (const double d : diff) worst_shape = std::max(worst_shape, std::abs(d - level));
      if (std::abs(level) > worst_level) {
        worst_level = std::abs(level);
        worst_level_at = name + " corner " + std::to_string(ci);
      }
      for (std::size_t si = 0; si < nb::kSections; ++si) {
        const auto geometry = trench::core::geometry_from_words(words[si], p2k::kSr);
        const bool packed_conjugate =
            std::holds_alternative<trench::core::ConjugatePair>(geometry.pole);
        EXPECT_EQ(nb::pole_is_conjugate(design[si]), packed_conjugate) << name << ci << si;
      }
    }
  }
  EXPECT_LT(worst_shape, 1e-4);
  EXPECT_LT(worst_level, 1e-4) << worst_level_at;
}

TEST(NativeBody, TheCornersOfTheBlendAreTheCorners) {
  const auto body = nb::import_p2k(hedz());
  const double coords[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  for (std::size_t ci = 0; ci < 4; ++ci) {
    const auto direct = nb::design(body.corners[ci], 48'000.0);
    const auto blended = nb::blend(body, coords[ci][0], coords[ci][1], 48'000.0);
    for (std::size_t si = 0; si < nb::kSections; ++si) {
      EXPECT_EQ(direct[si].b1, blended[si].b1);
      EXPECT_EQ(direct[si].b2, blended[si].b2);
      EXPECT_EQ(direct[si].a1, blended[si].a1);
      EXPECT_EQ(direct[si].a2, blended[si].a2);
    }
  }
}

TEST(NativeBody, TheInteriorIsStableConjugateAndUnityAtDc) {
  for (const auto& [name, body] : engine_bank()) {
    const auto imported = nb::import_p2k(body);
    for (const double sr : {44'100.0, 48'000.0}) {
      std::array<std::array<bool, 4>, nb::kSections> conjugate{};
      for (std::size_t ci = 0; ci < 4; ++ci) {
        const auto d = nb::design(imported.corners[ci], sr);
        for (std::size_t si = 0; si < nb::kSections; ++si) {
          conjugate[si][ci] = nb::pole_is_conjugate(d[si]);
        }
      }
      for (int mi = 0; mi <= 16; ++mi) {
        for (int qi = 0; qi <= 16; ++qi) {
          const auto d = nb::blend(imported, mi / 16.0, qi / 16.0, sr);
          for (std::size_t si = 0; si < nb::kSections; ++si) {
            ASSERT_TRUE(nb::is_stable(d[si])) << name << " " << si << " " << mi << " " << qi;
            const bool all_conjugate = conjugate[si][0] && conjugate[si][1] &&
                                       conjugate[si][2] && conjugate[si][3];
            if (all_conjugate) {
              ASSERT_TRUE(nb::pole_is_conjugate(d[si])) << name << " " << si;
            }
          }
          bool stabilised = true;
          for (const auto& c : d) {
            stabilised = stabilised && c.dc_stabilised && !nb::pole_is_marginal(c);
          }
          if (!stabilised) continue;
          const double dc = trench::core::cascade_response_db(nb::cascade(d), 0.0, sr);
          ASSERT_NEAR(dc, 0.0, 1e-9) << name << " " << mi << " " << qi;
          const double gain = nb::blend_gain_db(imported, mi / 16.0, qi / 16.0);
          const double dc_with_gain =
              trench::core::cascade_response_db(nb::cascade(d, gain), 0.0, sr);
          ASSERT_NEAR(dc_with_gain, gain, 1e-9)
              << name << " " << mi << " " << qi << " corner gains "
              << imported.corners[0].gain_db << " " << imported.corners[1].gain_db << " "
              << imported.corners[2].gain_db << " " << imported.corners[3].gain_db;
        }
      }
    }
  }
}

TEST(NativeBody, TalkingHedzFloatInteriorEnvelope) {
  const auto audit = p2k::interior_audit(nb::import_p2k(hedz()), p2k::grid(), 33, 33);
  EXPECT_EQ(audit.refused, 0U);
  EXPECT_NEAR(audit.max_step_db, 5.28, 0.11);
  EXPECT_NEAR(audit.mean_step_db, 3.08, 0.07);
  EXPECT_NEAR(audit.excursion_up_db, 69.43, 1.39);
  EXPECT_NEAR(audit.excursion_down_db, 93.25, 1.87);
  EXPECT_NEAR(audit.bilinear_dev_max_db, 14.45, 0.29);
  EXPECT_NEAR(audit.bilinear_dev_p95_db, 12.81, 0.26);
  EXPECT_NEAR(audit.detour_max, 6.70, 0.14);
  EXPECT_NEAR(audit.loudness_swing_db, 39.46, 0.79);
  EXPECT_NEAR(audit.loudness_beyond_corners_db, 30.59, 0.61);
  EXPECT_NEAR(audit.prefix_headroom_db, 107.60, 2.15);
  EXPECT_NEAR(audit.prefix_floor_db, -144.90, 2.90);
}

TEST(NativeBody, MotionThroughTheBlendStaysFinite) {
  constexpr double kSr = 48'000.0;
  constexpr std::size_t kN = 48'000;
  constexpr std::size_t kBlock = 512;
  std::vector<float> noise(kN);
  {
    std::uint32_t seed = 0x1BADF00D;
    double y = 0.0;
    const double alpha = 1.0 - std::exp(-2.0 * std::numbers::pi * 500.0 / kSr);
    float peak = 0.0F;
    for (auto& v : noise) {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      const double white = (seed / 4294967295.0) * 2.0 - 1.0;
      y += alpha * (white - y);
      v = static_cast<float>(y);
      peak = std::max(peak, std::abs(v));
    }
    for (auto& v : noise) v = v / peak * 0.25F;
  }
  const auto render = [&](const nb::Body& body, double q, auto morph_at) {
    trench::core::CascadeRunner runner;
    auto out = noise;
    float peak = 0.0F;
    std::size_t non_finite = 0;
    for (std::size_t off = 0; off < kN; off += kBlock) {
      const std::size_t len = std::min(kBlock, kN - off);
      const double m = morph_at(off / kSr);
      runner.set_target(nb::cascade(nb::blend(body, m, q, kSr), nb::blend_gain_db(body, m, q)));
      runner.process(std::span<float>(out).subspan(off, len));
      for (std::size_t i = off; i < off + len; ++i) {
        if (!std::isfinite(out[i])) ++non_finite;
        peak = std::max(peak, std::abs(out[i]));
      }
    }
    return std::pair{peak, non_finite};
  };
  double worst_motion = -1e9;
  std::string worst_name;
  for (const auto& [name, bytes] : engine_bank()) {
    const auto body = nb::import_p2k(bytes);
    for (const double q : {0.0, 1.0}) {
      float frozen = 0.0F;
      for (int k = 0; k < 17; ++k) {
        const double m = k / 16.0;
        const auto [peak, bad] = render(body, q, [m](double) { return m; });
        ASSERT_EQ(bad, 0U) << name;
        frozen = std::max(frozen, peak);
      }
      const auto [sq20, bad20] = render(body, q, [](double t) {
        return std::fmod(t * 20.0, 1.0) < 0.5 ? 0.0 : 1.0;
      });
      const auto [sin5, bad5] = render(body, q, [](double t) {
        return 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * 5.0 * t);
      });
      ASSERT_EQ(bad20 + bad5, 0U) << name;
      const double motion = 20.0 * std::log10(std::max(sq20, sin5) / std::max(frozen, 1e-15F));
      if (motion > worst_motion) {
        worst_motion = motion;
        worst_name = name;
      }
    }
  }
  EXPECT_LT(worst_motion, 80.0) << worst_name;
}
