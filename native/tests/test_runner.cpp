#include "harness.hpp"

#include "trench/core/audition.hpp"
#include "trench/core/native_body.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

namespace native = trench::core::native;
using trench::core::Biquad;
using trench::core::Cascade;

Cascade cascadeOf(double hz, double bw_hz, double zero_hz, double zero_bw_hz) {
  native::Corner corner{};
  for (auto& section : corner.sections) {
    section = {native::RealRoots{INFINITY, INFINITY}, native::RealRoots{INFINITY, INFINITY}, true};
  }
  corner.sections[0] = {native::Resonant{hz, bw_hz}, native::Resonant{zero_hz, zero_bw_hz}, true};
  corner.sections[1] = {native::Resonant{2.0 * hz, 2.0 * bw_hz}, native::RealRoots{INFINITY, INFINITY}, true};
  return native::cascade(native::design(corner, 44'100.0), 0.0);
}

std::vector<Cascade> trajectory(trench::core::CascadeRunner& runner, const Cascade& target, std::size_t block) {
  runner.set_target(trench::core::encode_cascade(target));
  std::vector<Cascade> out;
  std::vector<float> zeros(block, 0.0F);
  std::size_t done = 0;
  while (done < 3 * trench::core::kApproachSamples) {
    const std::size_t count = std::min(block, 3 * trench::core::kApproachSamples - done);
    for (std::size_t n = 0; n < count; ++n) {
      runner.process({zeros.data(), 1});
      out.push_back(runner.coefficients());
    }
    done += count;
  }
  return out;
}

}  // namespace

TRENCH_TEST(encoding_round_trips_a_cascade) {
  const Cascade cascade = cascadeOf(700.0, 80.0, 1'400.0, 200.0);
  for (std::size_t si = 0; si < trench::core::kSectionCount; ++si) {
    const Biquad back = trench::core::decode_section(trench::core::encode_section(cascade[si]));
    for (std::size_t ci = 0; ci < trench::core::kCoefficientCount; ++ci) {
      CHECK_NEAR(back[ci], cascade[si][ci], 1.0e-9);
    }
  }
}

TRENCH_TEST(runner_ramps_in_the_encoded_domain_and_ignores_block_size) {
  const Cascade a = cascadeOf(300.0, 40.0, 500.0, 60.0);
  const Cascade b = cascadeOf(1'200.0, 160.0, 2'000.0, 240.0);
  trench::core::CascadeRunner small;
  small.set_target(trench::core::encode_cascade(a));
  trench::core::CascadeRunner large;
  large.set_target(trench::core::encode_cascade(a));
  const auto fine = trajectory(small, b, 7);
  const auto coarse = trajectory(large, b, 64);
  CHECK(fine.size() == coarse.size());
  for (std::size_t n = 0; n < fine.size(); ++n) {
    for (std::size_t si = 0; si < trench::core::kSectionCount; ++si) {
      for (std::size_t ci = 0; ci < trench::core::kCoefficientCount; ++ci) {
        CHECK(fine[n][si][ci] == coarse[n][si][ci]);
      }
    }
  }
  const std::size_t half = trench::core::kApproachSamples / 2 - 1;
  const auto ea = trench::core::encode_section(a[0]);
  const auto eb = trench::core::encode_section(b[0]);
  const Biquad midway = fine[half][0];
  const auto em = trench::core::encode_section(midway);
  for (std::size_t ci = 0; ci < trench::core::kCoefficientCount; ++ci) {
    CHECK_NEAR(em[ci], 0.5 * (ea[ci] + eb[ci]), 1.0e-6);
  }
  const auto pole = native::roots_from_coefficients(midway[3], midway[4], 44'100.0);
  const auto* resonant = std::get_if<native::Resonant>(&pole);
  CHECK(resonant != nullptr);
  std::printf("halfway pole %.2f Hz / %.2f Hz bw; coefficient-average would be a1 %.5f, encoded gives %.5f\n",
              resonant->hz, resonant->bw_hz, 0.5 * (a[0][3] + b[0][3]), midway[3]);
  CHECK(std::abs(midway[3] - 0.5 * (a[0][3] + b[0][3])) > 1.0e-4);
  const Biquad settled = fine[trench::core::kApproachSamples][0];
  for (std::size_t ci = 0; ci < trench::core::kCoefficientCount; ++ci) {
    CHECK_NEAR(settled[ci], b[0][ci], 1.0e-9);
  }
  CHECK(small.remaining() == 0);
}
