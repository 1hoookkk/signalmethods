#pragma once
#include "trench/core/audition.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

inline int radius_distortion_tests() {
  int failures = 0;
  const auto check = [&](bool ok, const char* label) {
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
  };
  for (const double rate : {44100.0, 48000.0, 96000.0}) {
    const double r = std::exp(-1000.0 / rate);
    const double angle = 2.0 * 3.141592653589793 * 1500.0 / rate;
    const double cs = std::cos(angle), sn = std::sin(angle);
    const double dc = 1.0 - 2.0 * r * cs + r * r;
    for (const double amplitude : {0.000001, 0.1, 10000.0}) {
      trench::core::Cascade cascade;
      for (auto& row : cascade) row = {1.0, 0.0, 0.0, 0.0, 0.0};
      cascade[0] = {dc, -0.3 * dc, 0.1 * dc, -2.0 * r * cs, r * r};
      trench::core::CascadeRunner runner;
      runner.set_immediate(cascade);
      runner.set_ring_leveller(false);
      runner.set_radius_distortion(0.02);
      std::array<float, 2048> actual{}, expected{};
      for (std::size_t i = 0; i < 32; ++i) actual[i] = static_cast<float>(amplitude * std::cos(angle * i));
      expected = actual;
      double u = 0.0, v = 0.0, p1 = 0.0, p2 = 0.0;
      bool bounded = true;
      for (std::size_t i = 0; i < expected.size(); ++i) {
        const double threshold = i < 256 ? 0.02 : 0.0;
        const double m = threshold > 0.0 && std::abs(p1) > threshold ? 1.0 - threshold / std::abs(p1) : 0.0;
        const double re = r + m * r * (1.0 - r);
        bounded = bounded && re >= r && re < 1.0;
        const double gain = (1.0 - 2.0 * re * cs + re * re) / dc;
        double next_u = expected[i] * gain + re * (cs * u - sn * v);
        double next_v = re * (sn * u + cs * v);
        if (threshold > 0.0) {
          next_u = std::clamp(next_u, -1000.0 * threshold, 1000.0 * threshold);
          next_v = std::clamp(next_v, -1000.0 * threshold, 1000.0 * threshold);
        }
        const double pole_output = next_u + cs / sn * next_v;
        expected[i] = static_cast<float>(dc * (pole_output - 0.3 * p1 + 0.1 * p2));
        p2 = p1;
        p1 = pole_output;
        u = next_u;
        v = next_v;
      }
      runner.process(std::span<float>(actual.data(), 256));
      runner.set_radius_distortion(0.0);
      runner.process(std::span<float>(actual.data() + 256, actual.size() - 256));
      double error = 0.0;
      for (std::size_t i = 0; i < actual.size(); ++i) error = std::max(error, std::abs(static_cast<double>(actual[i]) - expected[i]));
      std::printf("radius reference %.0f Hz amplitude %.6g: max error %.9g\n", rate, amplitude, error);
      check(error < 1.0e-5, "radius lift follows pre-zero state, coupled clamp, and continuous disable");
      check(bounded && runner.coefficients() == cascade, "radius stays below unity without changing authored coefficients");
      runner.reset();
      runner.set_radius_distortion(0.02);
      std::array<float, 32> silence{};
      runner.process(silence);
      check(std::all_of(silence.begin(), silence.end(), [](float x) { return x == 0.0f; }), "reset clears nonlinear state without a residual tail");
    }
  }
  trench::core::Cascade cascade;
  for (auto& row : cascade) row = {1.0, 0.0, 0.0, 0.0, 0.0};
  cascade[0] = {0.1, 0.0, 0.0, -1.8 * std::cos(0.7), 0.81};
  std::array<int, 2> tail{};
  for (int mode = 0; mode < 2; ++mode) {
    trench::core::CascadeRunner runner;
    runner.set_immediate(cascade);
    runner.set_ring_leveller(false);
    runner.set_radius_distortion(mode == 0 ? 0.0 : 0.02);
    std::array<float, 2048> impulse{};
    impulse[0] = 0.2f;
    runner.process(impulse);
    double peak = 0.0;
    for (float x : impulse) peak = std::max(peak, std::abs(static_cast<double>(x)));
    for (std::size_t i = 0; i < impulse.size(); ++i)
      if (std::abs(impulse[i]) > peak * 0.01) tail[mode] = static_cast<int>(i);
  }
  std::printf("radius tail at -40 dB: linear %d samples, level-dependent %d samples\n", tail[0], tail[1]);
  check(tail[1] > tail[0], "level-dependent resonance lengthens the decay at fixed filter geometry");
  return failures;
}
