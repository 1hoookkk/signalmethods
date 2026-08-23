#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "trench/core/morph.hpp"
#include "trench/core/p2k.hpp"

namespace p2k = trench::core::p2k;

namespace {

std::vector<std::uint8_t> hedz() {
  const std::filesystem::path path =
      std::string(TRENCH_SOURCE_ROOT) + "/ref/presets/P2k_013_talking_hedz.bin";
  std::ifstream in(path, std::ios::binary);
  return {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(P2kMorph, TheFourCornersOfTheInteriorAreTheCorners) {
  const auto body = hedz();
  const float coords[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  for (std::size_t ci = 0; ci < 4; ++ci) {
    const auto direct = p2k::corner_response_db(p2k::rom_corner_words(body, ci));
    const auto via_lerp = p2k::morph_response_db(body, coords[ci][0], coords[ci][1]);
    for (std::size_t i = 0; i < p2k::kNpts; ++i) {
      ASSERT_EQ(direct[i], via_lerp[i]) << "corner " << ci << " point " << i;
    }
  }
}

TEST(P2kMorph, TalkingHedzInteriorStepsAreTheMeasuredOnes) {
  const auto audit = p2k::interior_audit(hedz());
  EXPECT_EQ(audit.refused, 0U);
  EXPECT_GT(audit.max_step_db, 5.0);
  EXPECT_LT(audit.max_step_db, 6.5);
  EXPECT_NEAR(audit.worst_q, 1.0F, 1e-6F);
  EXPECT_GT(audit.mean_step_db, 3.5);
  EXPECT_LT(audit.mean_step_db, 5.0);
}

TEST(P2kMorph, TalkingHedzInteriorEnvelopeIsTheMeasuredOne) {
  const auto body = hedz();
  const auto audit = p2k::interior_audit(body, p2k::grid(), 33, 33);
  EXPECT_EQ(audit.refused, 0U);
  EXPECT_NEAR(audit.max_step_db, 5.20, 0.11);
  EXPECT_NEAR(audit.mean_step_db, 3.05, 0.07);
  EXPECT_NEAR(audit.excursion_up_db, 70.27, 1.41);
  EXPECT_NEAR(audit.excursion_down_db, 96.75, 1.94);
  EXPECT_NEAR(audit.bilinear_dev_max_db, 15.04, 0.31);
  EXPECT_NEAR(audit.bilinear_dev_p95_db, 13.59, 0.28);
  EXPECT_NEAR(audit.detour_max, 6.74, 0.14);
  EXPECT_NEAR(audit.loudness_swing_db, 38.93, 0.78);
  EXPECT_NEAR(audit.loudness_beyond_corners_db, 30.06, 0.60);
  EXPECT_NEAR(audit.prefix_headroom_db, 62.35, 1.25);
  EXPECT_NEAR(audit.prefix_floor_db, -135.82, 2.72);

  EXPECT_GE(audit.excursion_up_db, 0.0);
  EXPECT_GE(audit.excursion_down_db, 0.0);
  EXPECT_LE(audit.bilinear_dev_p95_db, audit.bilinear_dev_max_db);
  EXPECT_GE(audit.detour_max, 1.0);

  double loudest = -1e9;
  for (std::size_t qi = 0; qi < 33; ++qi) {
    const float qq = static_cast<float>(qi) / 32.0F;
    for (std::size_t mi = 0; mi < 33; ++mi) {
      const float m = static_cast<float>(mi) / 32.0F;
      for (const double v : p2k::morph_response_db(body, m, qq)) {
        loudest = std::max(loudest, v);
      }
    }
  }
  EXPECT_GE(audit.prefix_headroom_db, loudest);
}
