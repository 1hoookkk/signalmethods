#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "trench/core/p2k.hpp"

namespace p2k = trench::core::p2k;
using nlohmann::json;

namespace {

const json& fixture() {
  static const json f = [] {
    const std::string path =
        std::string(TRENCH_SOURCE_ROOT) + "/native/trench-core/tests/fixtures/p2k_parity.json";
    std::ifstream in(path);
    return json::parse(in);
  }();
  return f;
}

p2k::CornerWords words4(const json& v) {
  p2k::CornerWords out{};
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    for (std::size_t wi = 0; wi < 4; ++wi) {
      out[si][wi] = v[si][wi].get<std::uint16_t>();
    }
  }
  return out;
}

struct Case {
  std::vector<double> target;
  p2k::CornerWords seed{};
};

Case cross_case() {
  const auto& c = fixture()["corners"][0];
  return {c["target"].get<std::vector<double>>(), words4(c["cross_seed_words"])};
}

bool corner_is_legal(const p2k::CornerWords& w) {
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    const auto [zp, zq] = p2k::pq(w[si][0], w[si][1]);
    const auto [pp, ppq] = p2k::pq(w[si][2], w[si][3]);
    if (!p2k::is_legal(zp, zq, false) || !p2k::is_legal(pp, ppq, true)) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(P2kFitterSemantics, AllFreeWatchedFitEqualsTheUnwatchedPolish) {
  const auto kase = cross_case();
  const std::vector<p2k::Seed> seeds{p2k::Seed{kase.seed}};
  const p2k::FitOptions opts;
  const auto watched = p2k::fit_corner_watched(kase.target, seeds, opts, nullptr, nullptr, nullptr);
  ASSERT_TRUE(watched.has_value());
  const auto [corner, rms] =
      p2k::polish_from_words_fine(p2k::enter(kase.seed), kase.target, opts.max_passes);
  ASSERT_EQ(watched->words, corner.w);
  ASSERT_EQ(watched->shape_rms_db, rms);
  ASSERT_FALSE(watched->stopped);
}

TEST(P2kFitterSemantics, EveryStepNamesOneSectionAndOnlyThatSectionChanges) {
  const auto kase = cross_case();
  const std::vector<p2k::Seed> seeds{p2k::Seed{kase.seed}};
  p2k::CornerWords previous = p2k::enter(kase.seed);
  std::size_t steps = 0;
  const auto watched = p2k::fit_corner_watched(
      kase.target, seeds, p2k::FitOptions{}, nullptr, nullptr,
      [&](const p2k::StepReport& r) {
        ++steps;
        for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
          if (si == r.section) {
            continue;
          }
          ASSERT_EQ(r.words[si], previous[si])
              << "step touched section " << si << " while reporting " << r.section;
        }
        ASSERT_NE(r.words[r.section], previous[r.section])
            << "a reported step must change its own section";
        previous = r.words;
      });
  ASSERT_TRUE(watched.has_value());
  ASSERT_GT(steps, 0U);
  ASSERT_EQ(watched->words, previous);
}

TEST(P2kFitterSemantics, APinnedSectionIsNeverWritten) {
  const auto kase = cross_case();
  const std::vector<p2k::Seed> seeds{p2k::Seed{kase.seed}};
  const std::uint32_t mask = p2k::kAllFree & ~(3U << (2 * 2));
  const auto watched = p2k::fit_corner_watched(
      kase.target, seeds, p2k::FitOptions{}, [mask] { return mask; }, nullptr,
      [](const p2k::StepReport& r) { ASSERT_NE(r.section, 2U); });
  ASSERT_TRUE(watched.has_value());
  ASSERT_EQ(watched->words[2], p2k::enter(kase.seed)[2]);
}

TEST(P2kFitterSemantics, APinFlippedMidFitIsExcludedFromTheNextStepOn) {
  const auto kase = cross_case();
  const std::vector<p2k::Seed> seeds{p2k::Seed{kase.seed}};
  std::atomic<std::uint32_t> mask{p2k::kAllFree};
  bool flipped = false;
  p2k::StageWords held{};
  const auto watched = p2k::fit_corner_watched(
      kase.target, seeds, p2k::FitOptions{}, [&] { return mask.load(); }, nullptr,
      [&](const p2k::StepReport& r) {
        if (!flipped) {
          flipped = true;
          held = r.words[0];
          mask.store(p2k::kAllFree & ~3U);
          return;
        }
        ASSERT_EQ(r.words[0], held) << "section 0 moved after its pin";
      });
  ASSERT_TRUE(watched.has_value());
  ASSERT_TRUE(flipped);
  ASSERT_EQ(watched->words[0], held);
}

TEST(P2kFitterSemantics, StopAndKeepReturnsTheLastAcceptedLegalState) {
  const auto kase = cross_case();
  const std::vector<p2k::Seed> seeds{p2k::Seed{kase.seed}};
  std::size_t steps = 0;
  p2k::CornerWords last = p2k::enter(kase.seed);
  const auto watched = p2k::fit_corner_watched(
      kase.target, seeds, p2k::FitOptions{}, nullptr, [&] { return steps >= 3; },
      [&](const p2k::StepReport& r) {
        ++steps;
        last = r.words;
      });
  ASSERT_TRUE(watched.has_value());
  ASSERT_TRUE(watched->stopped);
  ASSERT_EQ(steps, 3U);
  ASSERT_EQ(watched->words, last);
  ASSERT_TRUE(corner_is_legal(watched->words));
}
