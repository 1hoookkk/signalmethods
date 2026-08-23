#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "trench/core/p2k.hpp"

namespace p2k = trench::core::p2k;

namespace {

std::vector<std::pair<std::string, std::vector<std::uint8_t>>> bank() {
  const std::filesystem::path dir = std::string(TRENCH_SOURCE_ROOT) + "/ref/presets";
  std::vector<std::string> names;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() == ".bin" && entry.path().stem().string() <= "P2k_032z") {
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
}

}  // namespace

TEST(P2kRefitBank, EveryCornerSeededFromItsOwnWordsRefitsExactly) {
  double worst = 0.0;
  for (const auto& [name, body] : bank()) {
    for (std::size_t ci = 0; ci < 4; ++ci) {
      const auto rom = p2k::rom_corner_words(body, ci);
      const auto target = p2k::corner_response_db(rom);
      if (!std::all_of(target.begin(), target.end(),
                       [](double v) { return std::isfinite(v); })) {
        continue;
      }
      const std::vector<p2k::Seed> seeds{p2k::Seed{p2k::rom_seed(rom)}};
      const auto fit = p2k::fit_corner(target, seeds, p2k::FitOptions{});
      ASSERT_TRUE(fit.has_value()) << name << " C" << ci;
      worst = std::max(worst, fit->shape_rms_db);
      ASSERT_LT(fit->shape_rms_db, 1e-9)
          << name << " C" << ci << " no longer refits exactly from its own words";
      ASSERT_LT(std::abs(p2k::dc_gain_db(fit->packed)), 0.1) << name << " C" << ci;
    }
  }
  RecordProperty("worst_shape_rms_db", worst);
}
