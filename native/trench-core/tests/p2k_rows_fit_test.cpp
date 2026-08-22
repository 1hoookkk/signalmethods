#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "trench/core/formants.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/rows_fit.hpp"
#include "trench/core/section_param.hpp"

namespace p2k = trench::core::p2k;

namespace {

std::vector<std::uint8_t> hedz() {
  const std::filesystem::path path =
      std::string(TRENCH_SOURCE_ROOT) + "/ref/presets/P2k_013_talking_hedz.bin";
  std::ifstream in(path, std::ios::binary);
  return {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
}

std::vector<double> response_of(const p2k::Rows& rows) {
  const auto words = p2k::words_from_rows(rows);
  p2k::StoredCorner corner{};
  for (std::size_t si = 0; si < 6; ++si) {
    for (std::size_t wi = 0; wi < 4; ++wi) corner[si][wi] = words[si][wi];
    corner[si][4] = p2k::nearest_gain_word(0.25);
  }
  return p2k::corner_response_db(corner);
}

}  // namespace

TEST(P2kRowsFit, AVowelMadeFromRowsIsRecoveredFromItsOwnResponseWithDerivedZeros) {
  const auto* aa = p2k::klatt_vowel("aa");
  ASSERT_NE(aa, nullptr);
  const auto truth = p2k::rows_from_formants(aa->f).rows;
  const auto target = response_of(truth);
  const auto seed = p2k::seed_rows_from_target(target);
  const auto fit = p2k::fit_rows_watched(target, seed, p2k::RowsFitOptions{}, p2k::grid(),
                                         nullptr, nullptr);
  ASSERT_TRUE(fit.has_value());
  EXPECT_LT(fit->rms_db, 1.5);
  for (std::size_t si = 0; si < 6; ++si) {
    if (fit->rows[si].type != p2k::SectionType::kEq) continue;
    const trench::core::PackedSection words{fit->words[si][0], fit->words[si][1],
                                            fit->words[si][2], fit->words[si][3], 0};
    const auto geometry = trench::core::geometry_from_words(words, p2k::kSr);
    const auto* pole = std::get_if<trench::core::ConjugatePair>(&geometry.pole);
    const auto* zero = std::get_if<trench::core::ConjugatePair>(&geometry.zero);
    ASSERT_NE(pole, nullptr);
    ASSERT_NE(zero, nullptr);
    EXPECT_LT(std::abs(std::log2(zero->hz / pole->hz)), 0.1) << "row " << si;
    EXPECT_LT(zero->radius, 0.999) << "row " << si;
  }
}

TEST(P2kRowsFit, TalkingHedzCornerZeroIsMatchedByTypedRows) {
  const auto body = hedz();
  const auto target = p2k::corner_response_db(p2k::rom_corner_words(body, 0));
  const auto seed = p2k::seed_rows_from_target(target);
  const auto fit = p2k::fit_rows_watched(target, seed, p2k::RowsFitOptions{}, p2k::grid(),
                                         nullptr, nullptr);
  ASSERT_TRUE(fit.has_value());
  EXPECT_LT(fit->rms_db, 4.0);
  std::size_t eq = 0;
  for (const auto& r : fit->rows) eq += r.type == p2k::SectionType::kEq ? 1U : 0U;
  EXPECT_GE(eq, 3U);
}

TEST(P2kRowsFit, StopRequestedReturnsTheLastAcceptedRows) {
  const auto body = hedz();
  const auto target = p2k::corner_response_db(p2k::rom_corner_words(body, 1));
  const auto seed = p2k::seed_rows_from_target(target);
  int calls = 0;
  const auto fit = p2k::fit_rows_watched(
      target, seed, p2k::RowsFitOptions{}, p2k::grid(), [&calls] { return ++calls > 2; },
      nullptr);
  ASSERT_TRUE(fit.has_value());
  EXPECT_TRUE(fit->stopped);
  EXPECT_LE(fit->rms_db, p2k::rows_rms_db(seed, target) + 1e-9);
}
