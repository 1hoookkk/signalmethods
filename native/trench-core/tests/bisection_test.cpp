#include "trench/core/bisection.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <array>
#include <vector>

namespace {

namespace bisect = trench::core::bisect;

TEST(Bisection, PlanIsSevenPointsOfThreeRepeatsInKnotOrderByLevel) {
  bisect::Session session(7);
  std::vector<std::size_t> order;
  while (!session.finished()) {
    const auto trial = session.current();
    order.push_back(trial.point);
    session.accept(session.midpoint());
  }
  ASSERT_EQ(order.size(), bisect::kPointCount * bisect::kRepeats);
  const auto& repeats = session.repeats();
  for (const auto& point : repeats) EXPECT_EQ(point.size(), bisect::kRepeats);
  for (std::size_t i = 0; i < 3; ++i) EXPECT_EQ(order[i], 3U);
  for (std::size_t i = 3; i < 9; ++i) EXPECT_TRUE(order[i] == 1U || order[i] == 5U);
  for (std::size_t i = 9; i < 21; ++i) {
    EXPECT_TRUE(order[i] == 0U || order[i] == 2U || order[i] == 4U || order[i] == 6U);
  }
}

TEST(Bisection, IntervalsFollowTheAcceptedMedians) {
  bisect::Session session(3);
  while (!session.finished()) {
    const auto trial = session.current();
    if (trial.point == 3) {
      EXPECT_DOUBLE_EQ(trial.lo, 0.0);
      EXPECT_DOUBLE_EQ(trial.hi, 1.0);
      session.accept(0.2);
      continue;
    }
    if (trial.point == 1) {
      EXPECT_DOUBLE_EQ(trial.lo, 0.0);
      EXPECT_DOUBLE_EQ(trial.hi, 0.2);
    } else if (trial.point == 5) {
      EXPECT_DOUBLE_EQ(trial.lo, 0.2);
      EXPECT_DOUBLE_EQ(trial.hi, 1.0);
    }
    session.accept(session.midpoint());
  }
  EXPECT_DOUBLE_EQ(session.medians()[3], 0.2);
  EXPECT_DOUBLE_EQ(session.medians()[1], 0.1);
  EXPECT_DOUBLE_EQ(session.medians()[5], 0.6);
}

TEST(Bisection, MedianOfThreeIgnoresTheOutlier) {
  EXPECT_DOUBLE_EQ(bisect::median_of({0.1, 0.9, 0.2}), 0.2);
  EXPECT_DOUBLE_EQ(bisect::median_of({0.5, 0.5, 0.5}), 0.5);
}

TEST(Bisection, CurveJsonCarriesKnotsAndRawRepeats) {
  std::array<double, bisect::kPointCount> points{0.05, 0.1, 0.2, 0.35, 0.5, 0.7, 0.9};
  std::array<std::vector<double>, bisect::kPointCount> repeats{};
  for (std::size_t i = 0; i < bisect::kPointCount; ++i) {
    repeats[i] = {points[i] - 0.01, points[i], points[i] + 0.01};
  }
  const auto text = bisect::curve_json("morph", "P2k_006_bassbox_303", points, repeats, true);
  const auto json = nlohmann::json::parse(text);
  EXPECT_EQ(json["axis"], "morph");
  EXPECT_EQ(json["body"], "P2k_006_bassbox_303");
  ASSERT_EQ(json["points"].size(), bisect::kPointCount);
  for (std::size_t i = 0; i < bisect::kPointCount; ++i) {
    EXPECT_DOUBLE_EQ(json["points"][i]["knob"].get<double>(),
                     static_cast<double>(i + 1) / 8.0);
    EXPECT_DOUBLE_EQ(json["points"][i]["internal"].get<double>(), points[i]);
  }
  EXPECT_EQ(json["repeats"]["0.375"].size(), 3U);
  EXPECT_DOUBLE_EQ(json["repeats"]["0.375"][1].get<double>(), 0.2);
  EXPECT_FALSE(json.contains("monotone"));
}

TEST(Bisection, NonMonotoneMediansAreFlaggedAndStillWritten) {
  bisect::Session session(11);
  double value = 0.5;
  while (!session.finished()) {
    session.accept(value);
    value = 0.5;
  }
  EXPECT_FALSE(session.monotone());
  const auto text = bisect::curve_json("q", "flat", session.medians(), session.repeats(),
                                       session.monotone());
  const auto json = nlohmann::json::parse(text);
  EXPECT_FALSE(json["monotone"].get<bool>());
  EXPECT_EQ(json["points"].size(), bisect::kPointCount);
}

}  // namespace
