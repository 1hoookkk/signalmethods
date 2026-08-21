#include "trench/core/p2k.hpp"
#include "trench/core/p2k_push.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace p2k = trench::core::p2k;

namespace {

std::vector<std::uint8_t> body_bytes() {
  const auto path = std::filesystem::path(TRENCH_SOURCE_ROOT) /
                    "ref/presets/P2k_013_talking_hedz.bin";
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

p2k::StoredCorner fixture_corner() { return p2k::rom_corner_words(body_bytes(), 0); }

}  // namespace

TEST(P2kPush, EveryCandidateIsOneLegalRungFromTheCurrentWord) {
  const auto corner = fixture_corner();
  const auto push = p2k::compute_push(corner, p2k::kAllFree);
  EXPECT_GT(push.candidate_count, 0U);

  std::size_t recount = 0;
  const auto& words = p2k::lattice_words();
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    for (const bool is_pole : {false, true}) {
      const std::size_t mag_wi = is_pole ? 2 : 0;
      const std::size_t rsq_wi = is_pole ? 3 : 1;
      for (const auto wi : {mag_wi, rsq_wi}) {
        if (si == 5 && wi == 1) continue;
        const auto index = p2k::nearest_lattice_word(corner[si][wi]);
        for (const int step : {-1, 1}) {
          const auto neighbour = static_cast<std::ptrdiff_t>(index) + step;
          if (neighbour < 0 ||
              neighbour >= static_cast<std::ptrdiff_t>(p2k::lattice_len()))
            continue;
          const auto word = words[static_cast<std::size_t>(neighbour)];
          if (word == corner[si][wi]) continue;
          const auto mag = wi == mag_wi ? word : corner[si][mag_wi];
          const auto rsq = wi == rsq_wi ? word : corner[si][rsq_wi];
          if (!p2k::magnitude_admissible(p2k::nearest_lattice_word(mag), is_pole))
            continue;
          const auto [p, q] = p2k::pq(mag, rsq);
          if (!p2k::is_legal(p, q, is_pole)) continue;
          ++recount;
        }
      }
    }
  }
  EXPECT_EQ(push.candidate_count, recount);
}

TEST(P2kPush, AFullyPinnedMaskYieldsNothing) {
  const auto push = p2k::compute_push(fixture_corner(), 0U);
  EXPECT_EQ(push.candidate_count, 0U);
  EXPECT_TRUE(push.directions.empty());
}

TEST(P2kPush, PinningOnePoleRemovesExactlyThatRootsCandidates) {
  const auto corner = fixture_corner();
  const auto all = p2k::compute_push(corner, p2k::kAllFree);
  const auto masked =
      p2k::compute_push(corner, p2k::kAllFree & ~p2k::pole_bit(2));
  EXPECT_LT(masked.candidate_count, all.candidate_count);

  const auto zero_only = p2k::compute_push(corner, p2k::zero_bit(2));
  const auto pole_only = p2k::compute_push(corner, p2k::pole_bit(2));
  EXPECT_EQ(all.candidate_count - masked.candidate_count, pole_only.candidate_count);
  EXPECT_GT(zero_only.candidate_count, 0U);
}

TEST(P2kPush, DirectionsAreWeightedUnitAndMeanFreeWithDescendingSigma) {
  const auto push = p2k::compute_push(fixture_corner(), p2k::kAllFree);
  ASSERT_FALSE(push.directions.empty());
  const auto& g = p2k::grid();
  double previous = std::numeric_limits<double>::infinity();
  for (const auto& direction : push.directions) {
    EXPECT_LE(direction.sigma, previous);
    previous = direction.sigma;
    double weighted_mean = 0.0;
    double weighted_norm = 0.0;
    for (std::size_t k = 0; k < p2k::kNpts; ++k) {
      weighted_mean += g.weight[k] * direction.curve[k];
      weighted_norm += g.weight[k] * direction.curve[k] * direction.curve[k];
    }
    EXPECT_NEAR(weighted_mean / g.weight_sum, 0.0, 1e-9);
    EXPECT_NEAR(std::sqrt(weighted_norm), 1.0, 1e-9);
  }
}

TEST(P2kPush, DirectionsDieWhenTheWordsMove) {
  auto corner = fixture_corner();
  const auto before = p2k::compute_push(corner, p2k::kAllFree);
  const auto index = p2k::nearest_lattice_word(corner[0][2]);
  corner[0][2] = p2k::lattice_words()[index - 1];
  const auto after = p2k::compute_push(corner, p2k::kAllFree);
  ASSERT_FALSE(before.directions.empty());
  ASSERT_FALSE(after.directions.empty());
  double difference = 0.0;
  for (std::size_t k = 0; k < p2k::kNpts; ++k) {
    difference += std::abs(std::abs(before.directions[0].curve[k]) -
                           std::abs(after.directions[0].curve[k]));
  }
  EXPECT_GT(difference, 1e-6);
}

TEST(P2kPush, TheLockedSixthStageZeroRadiusContributesNoCandidates) {
  const auto corner = fixture_corner();
  const auto zero_only = p2k::compute_push(corner, p2k::zero_bit(5));
  const auto& words = p2k::lattice_words();
  const auto index = p2k::nearest_lattice_word(corner[5][0]);
  std::size_t expected = 0;
  for (const int step : {-1, 1}) {
    const auto neighbour = static_cast<std::ptrdiff_t>(index) + step;
    if (neighbour < 0 || neighbour >= static_cast<std::ptrdiff_t>(p2k::lattice_len()))
      continue;
    const auto word = words[static_cast<std::size_t>(neighbour)];
    if (word == corner[5][0]) continue;
    if (!p2k::magnitude_admissible(p2k::nearest_lattice_word(word), false)) continue;
    const auto [p, q] = p2k::pq(word, corner[5][1]);
    if (!p2k::is_legal(p, q, false)) continue;
    ++expected;
  }
  EXPECT_EQ(zero_only.candidate_count, expected);
}

TEST(P2kPush, TheComputationIsDeterministic) {
  const auto corner = fixture_corner();
  const auto a = p2k::compute_push(corner, p2k::kAllFree);
  const auto b = p2k::compute_push(corner, p2k::kAllFree);
  ASSERT_EQ(a.directions.size(), b.directions.size());
  ASSERT_EQ(a.candidate_count, b.candidate_count);
  for (std::size_t di = 0; di < a.directions.size(); ++di) {
    EXPECT_EQ(a.directions[di].sigma, b.directions[di].sigma);
    for (std::size_t k = 0; k < p2k::kNpts; ++k) {
      EXPECT_EQ(a.directions[di].curve[k], b.directions[di].curve[k]);
    }
  }
}
