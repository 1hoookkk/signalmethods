#include "trench/core/fit_target.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>

namespace {

TEST(FitTarget, ExplicitCurveCarriesHzMagnitudePhaseAndWeight) {
  const auto path = std::filesystem::temp_directory_path() / "trench_fit_target.csv";
  {
    std::ofstream out(path);
    out << "frequency_hz,magnitude_db,phase_deg,weight\n"
           "1000,-5.7,,1\n"
           "20,-18.2,-90,0.2\n"
           "4000,-28.4,45,0.8\n";
  }
  const auto target = trench::core::read_fit_target(path);
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->kind, trench::core::TargetKind::kTransferFunction);
  EXPECT_TRUE(target->absolute_level);
  EXPECT_EQ(target->frequency_hz, (std::vector<double>{20.0, 1000.0, 4000.0}));
  EXPECT_EQ(target->magnitude_db, (std::vector<double>{-18.2, -5.7, -28.4}));
  ASSERT_EQ(target->phase_rad.size(), 3U);
  EXPECT_NEAR(target->phase_rad[0], -std::numbers::pi / 2.0, 1.0e-12);
  EXPECT_TRUE(std::isnan(target->phase_rad[1]));
  EXPECT_NEAR(target->phase_rad[2], std::numbers::pi / 4.0, 1.0e-12);
  EXPECT_EQ(target->weight, (std::vector<double>{0.2, 1.0, 0.8}));
  std::filesystem::remove(path);
}

TEST(FitTarget, OnlyHzAndMagnitudeAreRequired) {
  const auto path = std::filesystem::temp_directory_path() / "trench_fit_target.txt";
  {
    std::ofstream out(path);
    out << "frequency_hz magnitude_db\n20 -18.2\n100 0\n1000 -5.7\n";
  }
  const auto target = trench::core::read_fit_target(path);
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->kind, trench::core::TargetKind::kEnvelope);
  EXPECT_FALSE(target->absolute_level);
  EXPECT_TRUE(target->phase_rad.empty());
  EXPECT_EQ(target->weight, (std::vector<double>{1.0, 1.0, 1.0}));
  const auto grid = trench::core::magnitude_on_grid(
      *target, std::array<double, 3>{20.0, 100.0, 1000.0});
  EXPECT_EQ(grid, target->magnitude_db);
  std::filesystem::remove(path);
}

TEST(FitTarget, NakedImplicitMagnitudeListsAreRejected) {
  const auto path = std::filesystem::temp_directory_path() / "trench_bad_target.txt";
  {
    std::ofstream out(path);
    out << "-18.2\n-3.1\n0\n";
  }
  EXPECT_FALSE(trench::core::read_fit_target(path).has_value());
  std::filesystem::remove(path);
}

}  // namespace
