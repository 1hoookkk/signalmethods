#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "trench/core/p2k.hpp"
#include "trench/core/role.hpp"

namespace p2k = trench::core::p2k;

namespace {

const std::vector<std::vector<std::uint8_t>>& bank() {
  static const auto bodies = [] {
    const std::filesystem::path dir = std::string(TRENCH_SOURCE_ROOT) + "/ref/presets";
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      if (entry.path().extension() == ".bin") {
        names.push_back(entry.path().filename().string());
      }
    }
    std::sort(names.begin(), names.end());
    std::vector<std::vector<std::uint8_t>> out;
    for (const auto& name : names) {
      std::ifstream in(dir / name, std::ios::binary);
      out.emplace_back((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    return out;
  }();
  return bodies;
}

trench::core::PackedSection section(const p2k::StoredCorner& corner, std::size_t si) {
  trench::core::PackedSection out{};
  for (std::size_t wi = 0; wi < p2k::kWordCount; ++wi) {
    out[wi] = corner[si][wi];
  }
  return out;
}

}  // namespace

TEST(P2kRole, TheBankRoleCensusIsTheMeasuredOne) {
  ASSERT_EQ(bank().size(), 33U);
  std::map<std::size_t, std::size_t> tilts_per_corner;
  std::array<std::map<p2k::Role, std::size_t>, 6> per_slot;
  std::size_t peak_rows = 0;
  for (const auto& body : bank()) {
    for (std::size_t ci = 0; ci < 4; ++ci) {
      const auto corner = p2k::rom_corner_words(body, ci);
      std::size_t tilts = 0;
      for (std::size_t si = 0; si < 6; ++si) {
        const auto role = p2k::role_of(section(corner, si));
        ++per_slot[si][role];
        tilts += role == p2k::Role::kTilt ? 1U : 0U;
        peak_rows += role == p2k::Role::kPeak || role == p2k::Role::kPeakNotch ? 1U : 0U;
      }
      ++tilts_per_corner[tilts];
    }
  }
  EXPECT_EQ(tilts_per_corner[0], 12U);
  EXPECT_EQ(tilts_per_corner[1], 59U);
  EXPECT_EQ(tilts_per_corner[2], 39U);
  EXPECT_EQ(tilts_per_corner[3], 20U);
  EXPECT_EQ(tilts_per_corner[4], 2U);
  EXPECT_EQ(per_slot[5][p2k::Role::kTilt], 69U);
  EXPECT_EQ(per_slot[0][p2k::Role::kTilt], 53U);
  EXPECT_EQ(per_slot[0][p2k::Role::kRealAxis], 29U);
  EXPECT_EQ(per_slot[3][p2k::Role::kParked], 17U);
  EXPECT_EQ(peak_rows, 489U);
}

TEST(P2kRole, EveryFactoryRowSitsInsideItsOwnRoleEnvelope) {
  std::size_t rows = 0;
  std::size_t outside = 0;
  for (const auto& body : bank()) {
    for (std::size_t ci = 0; ci < 4; ++ci) {
      const auto corner = p2k::rom_corner_words(body, ci);
      for (std::size_t si = 0; si < 6; ++si) {
        const auto words = section(corner, si);
        const auto role = p2k::role_of(words);
        ++rows;
        outside += p2k::within_envelope(role, words) ? 0U : 1U;
      }
    }
  }
  EXPECT_EQ(rows, 792U);
  EXPECT_LE(outside, rows / 10);
}

TEST(P2kRole, ACancellingPairOnTheCircleIsNotAPeak) {
  trench::core::SectionGeometry g;
  g.pole = trench::core::ConjugatePair{932.0, 0.9979};
  g.zero = trench::core::ConjugatePair{932.0, 0.9998};
  EXPECT_EQ(p2k::role_of(g), p2k::Role::kPeakNotch);
  EXPECT_FALSE(p2k::within_envelope(p2k::Role::kPeak, g));
  EXPECT_FALSE(p2k::within_envelope(p2k::Role::kTilt, g));
}

TEST(P2kRole, ATiltPoleAtNyquistIsOutsideTheTiltEnvelope) {
  trench::core::SectionGeometry g;
  g.pole = trench::core::ConjugatePair{19549.0, 0.9998};
  g.zero = trench::core::ConjugatePair{318.0, 0.9763};
  EXPECT_EQ(p2k::role_of(g), p2k::Role::kTilt);
  EXPECT_FALSE(p2k::within_envelope(p2k::Role::kTilt, g));
  g.pole = trench::core::ConjugatePair{655.0, 0.986};
  g.zero = trench::core::ConjugatePair{12372.0, 0.9};
  EXPECT_TRUE(p2k::within_envelope(p2k::Role::kTilt, g));
}

TEST(P2kRole, ATiltIntentKeepsTheFitInsideTheTiltEnvelope) {
  const auto& body = bank().front();
  const auto rom = p2k::rom_corner_words(body, 0);
  const auto target = p2k::corner_response_db(rom);
  p2k::FitOptions opts;
  opts.max_passes = 3;
  opts.intent[5] = p2k::Role::kTilt;
  const std::array<p2k::Seed, 1> seeds{p2k::SeedPeel{}};
  const auto fit = p2k::fit_corner_watched(target, seeds, opts, nullptr, nullptr, nullptr);
  ASSERT_TRUE(fit.has_value());
  trench::core::PackedSection s6{};
  for (std::size_t wi = 0; wi < 5; ++wi) {
    s6[wi] = fit->packed[5 * p2k::kWordCount + wi];
  }
  const auto geometry = trench::core::geometry_from_words(s6, p2k::kSr);
  EXPECT_TRUE(p2k::within_envelope(p2k::Role::kTilt, geometry));
}

TEST(P2kRole, NoIntentIsByteIdenticalToTheUnconstrainedFit) {
  const auto& body = bank().front();
  const auto rom = p2k::rom_corner_words(body, 1);
  const auto target = p2k::corner_response_db(rom);
  p2k::FitOptions opts;
  opts.max_passes = 2;
  const std::array<p2k::Seed, 1> seeds{p2k::rom_seed(rom)};
  const auto a = p2k::fit_corner_watched(target, seeds, opts, nullptr, nullptr, nullptr);
  p2k::FitOptions with_empty_intent;
  with_empty_intent.max_passes = 2;
  with_empty_intent.intent = p2k::RoleIntent{};
  const auto b = p2k::fit_corner_watched(target, seeds, with_empty_intent, nullptr, nullptr, nullptr);
  ASSERT_TRUE(a && b);
  EXPECT_EQ(a->packed, b->packed);
}

TEST(P2kRole, ASeatedTiltRowStartsInsideTheTiltEnvelope) {
  const auto& body = bank().front();
  const auto rom = p2k::rom_corner_words(body, 0);
  const auto target = p2k::corner_response_db(rom);
  const auto peel = p2k::peel_seed(target);
  for (const std::size_t si : {std::size_t{0}, std::size_t{5}}) {
    const auto seated = p2k::seat_words(p2k::Role::kTilt, peel[si], si);
    const trench::core::PackedSection words{seated[0], seated[1], seated[2], seated[3], 0};
    EXPECT_TRUE(p2k::within_envelope(p2k::Role::kTilt, words)) << "section " << si;
  }
  p2k::FitOptions opts;
  opts.max_passes = 2;
  opts.intent[5] = p2k::Role::kTilt;
  const std::array<p2k::Seed, 1> seeds{p2k::SeedPeel{}};
  const auto fit = p2k::fit_corner_watched(target, seeds, opts, nullptr, nullptr, nullptr);
  ASSERT_TRUE(fit.has_value());
  trench::core::PackedSection s6{};
  for (std::size_t wi = 0; wi < 5; ++wi) {
    s6[wi] = fit->packed[5 * p2k::kWordCount + wi];
  }
  EXPECT_TRUE(p2k::within_envelope(p2k::Role::kTilt, s6));
}
