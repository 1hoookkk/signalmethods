#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "trench/core/p2k.hpp"
#include "trench/core/section_param.hpp"

namespace p2k = trench::core::p2k;

namespace {

constexpr std::uint16_t kUnityScaleWord = 0xDFFF;

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

TEST(P2kSectionParam, AnEqParamSurvivesTheRoundTripThroughWords) {
  p2k::Rng rng(20'260'823);
  for (int trial = 0; trial < 20; ++trial) {
    p2k::SectionParam want;
    want.type = p2k::SectionType::kEq;
    want.fc_hz = rng.uniform(120.0, 5000.0);
    want.bw_oct = rng.uniform(0.15, 1.5);
    want.gain_db = rng.uniform(-12.0, 12.0);
    const std::array<std::uint16_t, 4> current{0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF};
    const auto words = p2k::words_from_param(want, current, 0);
    const trench::core::PackedSection packed{words[0], words[1], words[2], words[3],
                                             kUnityScaleWord};
    const auto got = p2k::param_of(packed);
    EXPECT_EQ(got.type, p2k::SectionType::kEq) << "trial " << trial;
    EXPECT_NEAR(got.fc_hz, want.fc_hz, want.fc_hz * 0.02) << "trial " << trial;
    EXPECT_NEAR(got.bw_oct, want.bw_oct, want.bw_oct * 0.10) << "trial " << trial;
    EXPECT_NEAR(got.gain_db, want.gain_db, 1.5) << "trial " << trial;
  }
}

TEST(P2kSectionParam, EveryFactoryRowReadsAsAFiniteParamAndAllTypesAreUsed) {
  ASSERT_EQ(bank().size(), 33U);
  std::map<p2k::SectionType, std::size_t> histogram;
  std::size_t rows = 0;
  std::size_t zero_far_below = 0;
  std::size_t real_axis_zero = 0;
  for (const auto& body : bank()) {
    for (std::size_t ci = 0; ci < 4; ++ci) {
      const auto corner = p2k::rom_corner_words(body, ci);
      for (std::size_t si = 0; si < 6; ++si) {
        const auto words = section(corner, si);
        const auto param = p2k::param_of(words);
        ASSERT_TRUE(std::isfinite(param.fc_hz));
        ASSERT_TRUE(std::isfinite(param.bw_oct));
        ASSERT_TRUE(std::isfinite(param.gain_db));
        ++histogram[param.type];
        ++rows;
        if (param.type != p2k::SectionType::kEq) continue;
        const auto geometry = trench::core::geometry_from_words(words);
        const auto* p = std::get_if<trench::core::ConjugatePair>(&geometry.pole);
        const auto* z = std::get_if<trench::core::ConjugatePair>(&geometry.zero);
        if (p != nullptr && z != nullptr && std::log2(z->hz / p->hz) <= -2.0) ++zero_far_below;
        if (z == nullptr) ++real_axis_zero;
      }
    }
  }
  EXPECT_EQ(rows, 792U);
  EXPECT_GT(histogram[p2k::SectionType::kLowPass], 0U);
  EXPECT_EQ(histogram[p2k::SectionType::kEq], 562U);
  EXPECT_EQ(zero_far_below, 62U);
  EXPECT_EQ(real_axis_zero, 35U);
}

TEST(P2kSectionParam, AFactoryEqRowReEncodedFromItsOwnParamKeepsTheCornerResponse) {
  const std::filesystem::path dir = std::string(TRENCH_SOURCE_ROOT) + "/ref/presets";
  std::ifstream in(dir / "P2k_014_zoom_peaks.bin", std::ios::binary);
  const std::vector<std::uint8_t> body((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
  ASSERT_EQ(body.size(), 240U);
  const auto corner = p2k::rom_corner_words(body, 1);
  const auto words = section(corner, 2);
  const auto param = p2k::param_of(words);
  ASSERT_EQ(param.type, p2k::SectionType::kEq);

  const std::array<std::uint16_t, 4> current{words[0], words[1], words[2], words[3]};
  const auto rewritten = p2k::words_from_param(param, current, 2);
  auto candidate = corner;
  for (std::size_t wi = 0; wi < 4; ++wi) {
    candidate[2][wi] = rewritten[wi];
  }
  const auto reference = p2k::corner_response_db(corner);
  const auto response = p2k::corner_response_db(candidate);
  double worst = 0.0;
  for (std::size_t i = 0; i < response.size(); ++i) {
    worst = std::max(worst, std::abs(response[i] - reference[i]));
  }
  EXPECT_LT(worst, 3.0);
}

TEST(P2kSectionParam, AnFcEditSlidesTheWholeBandAndReturnsToItsOwnBytes) {
  const double semitone = std::pow(2.0, 1.0 / 12.0);
  std::size_t rows = 0;
  std::size_t identical = 0;
  for (const auto& body : bank()) {
    for (std::size_t ci = 0; ci < 4; ++ci) {
      const auto corner = p2k::rom_corner_words(body, ci);
      for (std::size_t si = 0; si < 6; ++si) {
        const auto words = section(corner, si);
        const auto param = p2k::param_of(words);
        if (param.type != p2k::SectionType::kEq) continue;
        if (param.fc_hz * semitone > 15'000.0) continue;
        ++rows;
        const std::array<std::uint16_t, 4> current{words[0], words[1], words[2], words[3]};
        const auto read = [&](const std::array<std::uint16_t, 4>& w) {
          return p2k::param_of(trench::core::PackedSection{w[0], w[1], w[2], w[3], words[4]});
        };
        auto here = current;
        for (int cycle = 0; cycle < 32; ++cycle) {
          auto up = read(here);
          up.fc_hz *= semitone;
          const auto moved =
              p2k::words_from_param_keeping_offset(up, p2k::SectionEdit::kFc, here, si);
          auto back = read(moved);
          back.fc_hz = param.fc_hz;
          here = p2k::words_from_param_keeping_offset(back, p2k::SectionEdit::kFc, moved, si);
          ASSERT_EQ(here[1], current[1]);
          ASSERT_EQ(here[2], current[2]);
          ASSERT_EQ(here[3], current[3]);
          ASSERT_LE(std::abs(static_cast<int>(p2k::nearest_lattice_word(here[0])) -
                             static_cast<int>(p2k::nearest_lattice_word(current[0]))),
                    2)
              << "cycle " << cycle;
          if (cycle == 0) identical += here == current ? 1U : 0U;
        }
      }
    }
  }
  EXPECT_EQ(rows, 487U);
  EXPECT_EQ(identical, 445U);
}

TEST(P2kSectionParam, AGainEditKeepsThePoleAndTheAuthoredZeroOffset) {
  std::size_t rows = 0;
  std::size_t offset_rows = 0;
  std::size_t kept_offset = 0;
  std::size_t snapped_offset = 0;
  double worst_resolved = 0.0;
  for (const auto& body : bank()) {
    for (std::size_t ci = 0; ci < 4; ++ci) {
      const auto corner = p2k::rom_corner_words(body, ci);
      for (std::size_t si = 0; si < 6; ++si) {
        const auto words = section(corner, si);
        const auto param = p2k::param_of(words);
        if (param.type != p2k::SectionType::kEq) continue;
        ++rows;
        const std::array<std::uint16_t, 4> current{words[0], words[1], words[2], words[3]};
        auto louder = param;
        louder.gain_db = param.gain_db + 6.0;
        const auto kept =
            p2k::words_from_param_keeping_offset(louder, p2k::SectionEdit::kGain, current, si);
        ASSERT_EQ(kept[2], current[2]);
        ASSERT_EQ(kept[3], current[3]);
        const auto snapped = p2k::words_from_param(louder, current, si);
        const auto geometry = [&](const std::array<std::uint16_t, 4>& w) {
          return trench::core::geometry_from_words(
              trench::core::PackedSection{w[0], w[1], w[2], w[3], words[4]});
        };
        const auto offset = [](const trench::core::SectionGeometry& g) {
          const auto* p = std::get_if<trench::core::ConjugatePair>(&g.pole);
          const auto* z = std::get_if<trench::core::ConjugatePair>(&g.zero);
          return p != nullptr && z != nullptr ? std::log2(z->hz / p->hz) : 0.0;
        };
        const auto edited = geometry(kept);
        const double authored = offset(geometry(current));
        const auto* zero = std::get_if<trench::core::ConjugatePair>(&edited.zero);
        if (zero == nullptr) continue;
        if (zero->radius >= 0.9) {
          worst_resolved = std::max(worst_resolved, std::abs(offset(edited) - authored));
        }
        if (std::abs(authored) <= 0.25) continue;
        ++offset_rows;
        kept_offset += std::abs(offset(edited)) > 0.25 ? 1U : 0U;
        snapped_offset += std::abs(offset(geometry(snapped))) > 0.25 ? 1U : 0U;
      }
    }
  }
  EXPECT_EQ(rows, 562U);
  EXPECT_EQ(offset_rows, 376U);
  EXPECT_EQ(kept_offset, 370U);
  EXPECT_EQ(snapped_offset, 18U);
  EXPECT_LT(worst_resolved, 0.05);
}

TEST(P2kSectionParam, TheFourControlsCarryAFifthOfTheBankSEqRowsWithinThreeDecibels) {
  std::size_t eq_rows = 0;
  std::size_t within = 0;
  for (const auto& body : bank()) {
    for (std::size_t ci = 0; ci < 4; ++ci) {
      const auto corner = p2k::rom_corner_words(body, ci);
      const auto reference = p2k::corner_response_db(corner);
      for (std::size_t si = 0; si < 6; ++si) {
        const auto words = section(corner, si);
        const auto param = p2k::param_of(words);
        if (param.type != p2k::SectionType::kEq) continue;
        ++eq_rows;
        const std::array<std::uint16_t, 4> current{words[0], words[1], words[2], words[3]};
        const auto rewritten = p2k::words_from_param(param, current, si);
        auto candidate = corner;
        for (std::size_t wi = 0; wi < 4; ++wi) {
          candidate[si][wi] = rewritten[wi];
        }
        const auto response = p2k::corner_response_db(candidate);
        double worst = 0.0;
        for (std::size_t i = 0; i < response.size(); ++i) {
          worst = std::max(worst, std::abs(response[i] - reference[i]));
        }
        within += worst < 3.0 ? 1U : 0U;
      }
    }
  }
  EXPECT_EQ(eq_rows, 562U);
  EXPECT_EQ(within, 95U);
}

TEST(P2kSectionParam, TheLowSectionsThirdControlIsTheTrenchAtTheFloorDepth) {
  p2k::SectionParam low{p2k::SectionType::kLowPass, 225.0, 0.8, 0.0, 7200.0};
  const auto words = p2k::words_from_param(low, p2k::identity_words()[2], 2, trench::core::kP2kDatumHz);
  const auto geometry = trench::core::geometry_from_words({words[0], words[1], words[2], words[3], 0},
                                                          trench::core::kP2kDatumHz);
  const auto* zero = std::get_if<trench::core::ConjugatePair>(&geometry.zero);
  ASSERT_NE(zero, nullptr);
  EXPECT_NEAR(std::log2(zero->hz / 7200.0), 0.0, 0.05);
  EXPECT_NEAR(zero->radius, p2k::trench_floor_radius(), 1e-4);
  const auto back = p2k::param_of({words[0], words[1], words[2], words[3], 0}, trench::core::kP2kDatumHz);
  EXPECT_EQ(back.type, p2k::SectionType::kLowPass);
  EXPECT_NEAR(std::log2(back.trench_hz / 7200.0), 0.0, 0.05);
  low.trench_hz = 1800.0;
  const auto moved = p2k::words_from_param_keeping_offset(low, p2k::SectionEdit::kGain, words, 2,
                                                          trench::core::kP2kDatumHz);
  EXPECT_EQ(moved[2], words[2]);
  EXPECT_NE(moved[0], words[0]);
}

TEST(P2kSectionParam, EveryFactoryLowSectionReadsItsTrenchFromItsZero) {
  const std::filesystem::path dir = std::string(TRENCH_SOURCE_ROOT) + "/ref/presets";
  std::size_t low = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() != ".bin") continue;
    std::ifstream in(entry.path(), std::ios::binary);
    const std::vector<std::uint8_t> body((std::istreambuf_iterator<char>(in)),
                                         std::istreambuf_iterator<char>());
    if (body.size() != trench::core::kLegacyBodyBytes) continue;
    for (std::size_t corner = 0; corner < 4; ++corner) {
      const auto words = p2k::rom_corner_words(body, corner);
      for (std::size_t si = 0; si < 6; ++si) {
        const auto param = p2k::param_of({words[si][0], words[si][1], words[si][2], words[si][3], 0},
                                         trench::core::kP2kDatumHz);
        if (param.type != p2k::SectionType::kLowPass) continue;
        ++low;
        EXPECT_GE(std::log2(param.trench_hz / param.fc_hz), p2k::kTrenchMinOct - 1e-9);
      }
    }
  }
  EXPECT_GT(low, 100U);
}
