#include <gtest/gtest.h>

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
