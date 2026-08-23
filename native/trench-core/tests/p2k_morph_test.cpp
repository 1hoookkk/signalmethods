#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "trench/core/morph.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

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
}

TEST(P2kMorph, TheWordLerpLetsInteriorDcDriftOffTheCorners) {
  const std::filesystem::path dir = std::string(TRENCH_SOURCE_ROOT) + "/ref/presets";
  double worst_db = 0.0;
  std::string worst_body;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() != ".bin") continue;
    std::ifstream in(entry.path(), std::ios::binary);
    const std::vector<std::uint8_t> body((std::istreambuf_iterator<char>(in)),
                                         std::istreambuf_iterator<char>());
    const auto corners = p2k::body_corners(body);
    const auto dc_of = [](const p2k::StoredCorner& words) {
      double dc = 0.0;
      for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
        dc += p2k::stage_db(trench::core::section_words_to_biquad(words[si]), 0.0);
      }
      return dc;
    };
    double corner_lo = 1e9;
    double corner_hi = -1e9;
    for (const auto& corner : corners) {
      corner_lo = std::min(corner_lo, dc_of(corner));
      corner_hi = std::max(corner_hi, dc_of(corner));
    }
    double lo = 1e9;
    double hi = -1e9;
    for (std::size_t qi = 0; qi < 33; ++qi) {
      for (std::size_t mi = 0; mi < 33; ++mi) {
        const double dc = dc_of(p2k::interpolate_plane(corners, mi / 32.0F, qi / 32.0F));
        lo = std::min(lo, dc);
        hi = std::max(hi, dc);
      }
    }
    const double beyond = std::max(corner_lo - lo, hi - corner_hi);
    if (beyond > worst_db) {
      worst_db = beyond;
      worst_body = entry.path().stem().string();
    }
  }
  EXPECT_GT(worst_db, 4.0) << worst_body;
}
