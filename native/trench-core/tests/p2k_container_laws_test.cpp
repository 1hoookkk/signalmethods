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

constexpr double kLiveR = 0.45;

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

TEST(P2kContainerLaws, TheBankIsThirtyThreePresetsOfFourCorners) {
  ASSERT_EQ(bank().size(), 33U);
  ASSERT_EQ(corners().size(), 132U);
  for (const auto& [name, body] : bank()) {
    ASSERT_EQ(body.size(), 240U) << name;
  }
}

TEST(P2kContainerLaws, TheDatumIsFortyFourThousandOneHundred) {
  ASSERT_EQ(p2k::kSr, 44'100.0);
}

TEST(P2kContainerLaws, TheBankHoldsThePublishedCountOfLiveRoots) {
  std::size_t live = 0;
  for (const auto& c : corners()) {
    for (const auto& st : c.words) {
      for (const auto [wm, wr] : {std::pair{st[0], st[1]}, std::pair{st[2], st[3]}}) {
        if (const auto r = root(wm, wr); r && r->second > kLiveR) {
          ++live;
        }
      }
    }
  }
  ASSERT_EQ(live, 1'475U);
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

TEST(P2kContainerLaws, ThePoleRadiusCeilingIsTheBanksOwnMaximum) {
  double top = 0.0;
  std::size_t live = 0;
  for (const auto& c : corners()) {
    for (const auto& st : c.words) {
      if (const auto r = root(st[2], st[3])) {
        ++live;
        top = std::max(top, r->second);
      }
    }
  }
  ASSERT_EQ(live, 777U);
  ASSERT_LT(std::abs(top - p2k::kPoleRMax), 1e-9) << top;
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

TEST(P2kContainerLaws, TheSixthStageZeroRadiusWordIsConstantInEveryCorner) {
  for (const auto& c : corners()) {
    ASSERT_EQ(c.words[5][1], p2k::kS6ZeroRsqWord) << c.name << " C" << c.ci;
  }
  const double r = std::sqrt(1.0 - trench::core::decode_word(p2k::kS6ZeroRsqWord));
  ASSERT_GT(r, p2k::kPoleRMax);
  ASSERT_LT(std::abs(r - 0.999'998'148'529'007'7), 1e-15) << r;
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

TEST(P2kContainerLaws, EveryStageOfEveryCornerIsLive) {
  for (const auto& c : corners()) {
    for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
      const auto [zp, zq] = p2k::pq(c.words[si][0], c.words[si][1]);
      const auto [pp, ppq] = p2k::pq(c.words[si][2], c.words[si][3]);
      ASSERT_FALSE(zp == pp && zq == ppq)
          << c.name << " C" << c.ci << " S" << si + 1 << " cancels itself";
    }
  }
}

TEST(P2kContainerLaws, EveryFactoryGeometryWordIsOnTheExponentIndexedLattice) {
  const auto& lat = p2k::lattice_words();
  std::size_t total = 0;
  std::size_t on = 0;
  std::size_t flat = 0;
  for (const auto& c : corners()) {
    for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
      for (std::size_t wi = 0; wi < 4; ++wi) {
        if (si == 5 && wi == 1) {
          continue;
        }
        const std::uint16_t w = c.words[si][wi];
        ++total;
        ASSERT_TRUE(std::binary_search(lat.begin(), lat.end(), w))
            << c.name << " C" << c.ci << " S" << si + 1 << " w" << wi << " off the lattice";
        ++on;
        if ((w & 0xFFU) == p2k::kFiller) {
          ++flat;
        }
      }
    }
  }
  ASSERT_EQ(on, total);
  ASSERT_EQ(total, 3'036U);
  const double flat_pct = 100.0 * static_cast<double>(flat) / static_cast<double>(total);
  ASSERT_TRUE(flat_pct >= 79.0 && flat_pct < 82.0) << flat_pct;
}

TEST(P2kContainerLaws, TheLowByteIsAFunctionOfTheExponent) {
  for (const auto& c : corners()) {
    for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
      for (std::size_t wi = 0; wi < 4; ++wi) {
        if (si == 5 && wi == 1) {
          continue;
        }
        const std::uint16_t w = c.words[si][wi];
        const auto e = static_cast<std::size_t>(((static_cast<std::uint32_t>(w) + 1U) >> 12U) & 0xFU);
        const std::uint16_t canonical = p2k::word_of(w >> 8U);
        const auto alt = static_cast<std::uint16_t>(((w >> 8U) << 8U) | 0x7DU);
        ASSERT_TRUE(w == canonical || (e == 15 && w == alt))
            << c.name << " C" << c.ci << " S" << si + 1 << " w" << wi;
      }
    }
  }
}

TEST(P2kContainerLaws, TheGeometryLowByteIsFarLessVariousThanTheGainLowByte) {
  std::array<std::set<std::uint8_t>, 4> geo;
  std::set<std::uint8_t> gain;
  for (const auto& c : corners()) {
    for (const auto& st : c.words) {
      for (std::size_t wi = 0; wi < 4; ++wi) {
        geo[wi].insert(static_cast<std::uint8_t>(st[wi] & 0xFFU));
      }
      gain.insert(static_cast<std::uint8_t>(st[4] & 0xFFU));
    }
  }
  for (const auto& set : geo) {
    ASSERT_TRUE(set.size() >= 4 && set.size() <= 8) << set.size();
  }
  ASSERT_GE(gain.size(), 90U);
}

TEST(P2kContainerLaws, RealAxisPairsAreLegalAndSurviveSeating) {
  std::size_t real_pair_corners = 0;
  std::size_t modified = 0;
  for (const auto& c : corners()) {
    p2k::CornerWords geometry{};
    bool has_real = false;
    for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
      for (std::size_t wi = 0; wi < 4; ++wi) {
        geometry[si][wi] = c.words[si][wi];
      }
      for (const auto [wm, wr] :
           {std::pair{c.words[si][0], c.words[si][1]}, std::pair{c.words[si][2], c.words[si][3]}}) {
        const auto [p, q] = p2k::pq(wm, wr);
        if (p * p - 4.0 * q >= 0.0) {
          has_real = true;
        }
      }
    }
    if (has_real) {
      ++real_pair_corners;
    }
    if (p2k::enter(geometry) != geometry) {
      ++modified;
    }
  }
  ASSERT_EQ(real_pair_corners, 46U);
  ASSERT_EQ(modified, 0U) << "enter must not repair a factory corner";
}
