#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../tools/adversary_grade.hpp"

namespace {

std::vector<std::pair<std::string, std::vector<std::uint8_t>>> adversary_bank() {
  const std::filesystem::path dir = std::string(TRENCH_SOURCE_ROOT) + "/dev/adversary";
  std::vector<std::filesystem::path> paths;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() == ".bin") paths.push_back(entry.path());
  }
  std::sort(paths.begin(), paths.end());
  std::vector<std::pair<std::string, std::vector<std::uint8_t>>> out;
  for (const auto& path : paths) {
    std::ifstream in(path, std::ios::binary);
    out.emplace_back(path.stem().string(),
                     std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                               std::istreambuf_iterator<char>()));
  }
  return out;
}

}  // namespace

TEST(Adversary, TheLatticeInvariantsHoldOnItsOwnWorstCases) {
  const auto bank = adversary_bank();
  ASSERT_FALSE(bank.empty());
  double worst_motion = -1e9;
  double worst_peak = 0.0;
  std::string worst_name;
  for (const auto& [name, bytes] : bank) {
    ASSERT_EQ(bytes.size(), 240U) << name;
    const auto grade = trench::adversary::grade_bytes(bytes);
    EXPECT_EQ(grade.refused, 0U) << name;
    EXPECT_EQ(grade.non_finite, 0U) << name;
    EXPECT_EQ(grade.pole_over_ceiling, 0U) << name;
    worst_peak = std::max(worst_peak, grade.moving_peak);
    if (grade.motion_db > worst_motion) {
      worst_motion = grade.motion_db;
      worst_name = name;
    }
  }
  std::cout << "adversary bodies " << bank.size() << " worst motion " << worst_motion << " dB ("
            << worst_name << ") worst moving peak " << worst_peak << "\n";
}
