#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

#include "trench/core/audition.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

namespace {

std::vector<std::uint8_t> hedz() {
  const std::filesystem::path path =
      std::string(TRENCH_SOURCE_ROOT) + "/ref/presets/P2k_013_talking_hedz.bin";
  std::ifstream in(path, std::ios::binary);
  return {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
}

double rms_of(const std::vector<float>& v, std::size_t from) {
  double acc = 0.0;
  for (std::size_t i = from; i < v.size(); ++i) acc += double(v[i]) * v[i];
  return std::sqrt(acc / static_cast<double>(v.size() - from));
}

}  // namespace

TEST(Audition, IdentityCascadeIsBitExactPassThrough) {
  trench::core::CascadeRunner runner;
  trench::core::Cascade identity{};
  for (auto& s : identity) s = {1.0, 0.0, 0.0, 0.0, 0.0};
  runner.set_target(identity);
  std::vector<float> block(1000);
  for (std::size_t i = 0; i < block.size(); ++i) block[i] = std::sin(0.01f * float(i));
  const auto in = block;
  runner.process(block);
  for (std::size_t i = 0; i < block.size(); ++i) EXPECT_EQ(block[i], in[i]);
}

TEST(Audition, HedzCornerZeroLevelsASineLikeItsResponse) {
  const auto body = trench::core::PackedBody::from_body_bytes(hedz());
  const auto cascade = body.interpolate_biquads(0.0F, 0.0F, 0.0F);
  constexpr double kSr = trench::core::kP2kDatumHz;
  for (const double hz : {700.0, 2000.0, 5000.0}) {
    trench::core::CascadeRunner runner;
    runner.set_target(cascade);
    std::vector<float> block(44100);
    for (std::size_t i = 0; i < block.size(); ++i) {
      block[i] = static_cast<float>(0.25 * std::sin(2.0 * std::numbers::pi * hz * double(i) / kSr));
    }
    const double in_rms = rms_of(block, 22050);
    runner.process(block);
    const double gain_db = 20.0 * std::log10(rms_of(block, 22050) / in_rms);
    const double expected = trench::core::cascade_response_db(cascade, hz, kSr);
    EXPECT_NEAR(gain_db, expected, 0.5) << hz << " Hz";
  }
}

TEST(Audition, TargetChangesRampWithinOneTick) {
  trench::core::CascadeRunner runner;
  trench::core::Cascade a{};
  for (auto& s : a) s = {1.0, 0.0, 0.0, 0.0, 0.0};
  auto b = a;
  b[0] = {0.5, 0.0, 0.0, 0.0, 0.0};
  runner.set_target(a);
  std::vector<float> block(trench::core::kAuditionTick * 2, 1.0F);
  runner.set_target(b);
  runner.process(block);
  EXPECT_GT(block[0], 0.9F);
  EXPECT_NEAR(block[trench::core::kAuditionTick - 1], 0.5F, 1e-5F);
  EXPECT_NEAR(block.back(), 0.5F, 1e-6F);
}

TEST(Audition, SawIsBoundedAndPeriodic) {
  trench::core::SawSource saw(49.0, 44100.0, 0.25F);
  float lo = 1.0F;
  float hi = -1.0F;
  for (int i = 0; i < 44100; ++i) {
    const float v = saw.next();
    lo = std::min(lo, v);
    hi = std::max(hi, v);
  }
  EXPECT_NEAR(lo, -0.25F, 1e-3F);
  EXPECT_NEAR(hi, 0.25F, 1e-3F);
}
