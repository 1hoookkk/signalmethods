#include <ceres/version.h>
#include <gtest/gtest.h>
#include <pocketfft_hdronly.h>

#include <Eigen/Core>

TEST(Foundation, CurrentNumericsStackIsUsable) {
  const Eigen::Vector2d vector{3.0, 4.0};
  EXPECT_DOUBLE_EQ(vector.norm(), 5.0);
  EXPECT_GE(CERES_VERSION_MAJOR, 2);

  const pocketfft::shape_t shape{8};
  ASSERT_EQ(shape.size(), 1U);
  EXPECT_EQ(shape.front(), 8U);
}
