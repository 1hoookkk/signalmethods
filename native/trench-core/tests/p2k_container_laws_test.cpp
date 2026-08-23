#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

namespace p2k = trench::core::p2k;

namespace {

const std::vector<std::pair<std::string, std::vector<std::uint8_t>>>& bank() {
  static const auto presets = [] {
    const std::filesystem::path dir = std::string(TRENCH_SOURCE_ROOT) + "/ref/presets";
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      if (entry.path().extension() == ".bin") {
        names.push_back(entry.path().filename().string());
      }
    }
    std::sort(names.begin(), names.end());
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> out;
    for (const auto& name : names) {
      std::ifstream in(dir / name, std::ios::binary);
      std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                      std::istreambuf_iterator<char>());
      out.emplace_back(name, std::move(bytes));
    }
    return out;
  }();
  return presets;
}

struct BankCorner {
  std::string name;
  std::size_t ci{};
  p2k::StoredCorner words{};
};

const std::vector<BankCorner>& corners() {
  static const auto all = [] {
    std::vector<BankCorner> out;
    for (const auto& [name, body] : bank()) {
      for (std::size_t ci = 0; ci < 4; ++ci) {
        out.push_back({name, ci, p2k::rom_corner_words(body, ci)});
      }
    }
    return out;
  }();
  return all;
}

std::optional<std::pair<double, double>> root(std::uint16_t w_mag, std::uint16_t w_rsq) {
  const auto [p, q] = p2k::pq(w_mag, w_rsq);
  if (q <= 0.0) {
    return std::nullopt;
  }
  const double r = std::sqrt(q);
  const double c = -p / (2.0 * r);
  if (std::abs(c) > 1.0) {
    return std::nullopt;
  }
  return std::pair{std::acos(c) / std::numbers::pi * (p2k::kSr / 2.0), r};
}

}  // namespace

TEST(P2kContainerLaws, TheDatumIsFortyFourThousandOneHundred) {
  ASSERT_EQ(p2k::kSr, 44'100.0);
}

TEST(P2kContainerLaws, TheMagnitudeByteCeilingIsTheBanksOwnPoleMaximum) {
  std::size_t top = 0;
  for (const auto& c : corners()) {
    for (const auto& st : c.words) {
      if (root(st[2], st[3])) {
        top = std::max(top, static_cast<std::size_t>(st[2] >> 8U));
      }
    }
  }
  ASSERT_EQ(top, p2k::kMaxMagByte);
}

TEST(P2kContainerLaws, OnlyTheSixthStageZeroReachesByte255) {
  std::array<std::size_t, p2k::kStageCount> by_stage{};
  std::size_t zero_top = 0;
  for (const auto& c : corners()) {
    for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
      const auto& st = c.words[si];
      if (root(st[0], st[1])) {
        const auto b = static_cast<std::size_t>(st[0] >> 8U);
        zero_top = std::max(zero_top, b);
        if (b == 255) {
          ++by_stage[si];
        }
      }
    }
  }
  ASSERT_EQ(zero_top, 255U);
  for (std::size_t si = 0; si < 5; ++si) {
    ASSERT_EQ(by_stage[si], 0U) << "byte 255 must appear at no stage but the sixth";
  }
  ASSERT_EQ(by_stage[5], 48U);
}

TEST(P2kContainerLaws, TheMagnitudeCapBindsPolesOnly) {
  ASSERT_EQ(p2k::kMaxMagByte, 254U);
  const auto& words = p2k::lattice_words();
  const auto top = std::count_if(words.begin(), words.end(),
                                 [](std::uint16_t w) { return (w >> 8U) == 255; });
  ASSERT_GT(top, 0);
  ASSERT_TRUE(std::find(words.begin(), words.end(), 0xFF7D) != words.end());
}

TEST(P2kContainerLaws, Byte255IsExcludedBecauseItsDiscriminantIsOneStepFromCollapsing) {
  const std::uint16_t rsq = p2k::word_of(0);
  const auto disc = [rsq](std::size_t byte) {
    const auto [p, q] = p2k::pq(p2k::word_of(byte), rsq);
    return p * p - 4.0 * q;
  };
  const double at254 = disc(p2k::kMaxMagByte);
  const double at255 = disc(255);
  ASSERT_TRUE(at255 < 0.0 && at255 > -0.01) << at255;
  ASSERT_LT(at254, -0.1) << at254;
  ASSERT_LT(at254, at255);
}

TEST(P2kContainerLaws, TheRadiiAboveThePoleCeilingAreCounted) {
  const double ceiling = p2k::pole_radius_ceiling();
  std::size_t canonical = 0;
  for (std::size_t b = 0; b < 256; ++b) {
    const double q = 1.0 - trench::core::decode_word(p2k::word_of(b));
    if (q > 0.0 && std::sqrt(q) > ceiling) {
      ++canonical;
    }
  }
  ASSERT_EQ(canonical, 75U);
  std::size_t all = 0;
  for (const auto w : p2k::lattice_words()) {
    const double q = 1.0 - trench::core::decode_word(w);
    if (q > 0.0 && std::sqrt(q) > ceiling) {
      ++all;
    }
  }
  ASSERT_EQ(all, 75U);
}

TEST(P2kContainerLaws, TheS6WordIsAChosenMarginNotTheFormatFloor) {
  std::set<std::uint64_t> distinct;
  for (std::uint16_t w = 1; w < p2k::kS6ZeroRsqWord; ++w) {
    const double v = trench::core::decode_word(w);
    if (v > 0.0 && 1.0 - v > 0.0) {
      std::uint64_t bits = 0;
      std::memcpy(&bits, &v, sizeof(bits));
      distinct.insert(bits);
    }
  }
  ASSERT_EQ(distinct.size(), static_cast<std::size_t>(p2k::kS6ZeroRsqWord - 1));
}

