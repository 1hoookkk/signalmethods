#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include "trench/core/audition.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

namespace p2k = trench::core::p2k;
namespace nb = trench::core::native;
using trench::core::Cascade;

namespace {

constexpr double kSr = 44'100.0;
constexpr double kSeconds = 8.0;
constexpr std::size_t kBlock = 64;
constexpr float kQ = 0.5F;

void write_wav(const std::filesystem::path& path, const std::vector<float>& samples) {
  std::ofstream out(path, std::ios::binary);
  const auto put32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
  const auto put16 = [&](std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
  const std::uint32_t bytes = static_cast<std::uint32_t>(samples.size() * 2);
  out.write("RIFF", 4);
  put32(36 + bytes);
  out.write("WAVEfmt ", 8);
  put32(16);
  put16(1);
  put16(1);
  put32(static_cast<std::uint32_t>(kSr));
  put32(static_cast<std::uint32_t>(kSr) * 2);
  put16(2);
  put16(16);
  out.write("data", 4);
  put32(bytes);
  for (const float s : samples) {
    const double c = std::clamp(static_cast<double>(s), -1.0, 1.0);
    put16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lrint(c * 32767.0))));
  }
}

Cascade packed_cascade(const std::array<p2k::StoredCorner, 4>& corners, float m, float q) {
  const auto words = p2k::interpolate_plane(corners, m, q);
  Cascade out{};
  for (auto& s : out) s = {1.0, 0.0, 0.0, 0.0, 0.0};
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    out[si] = trench::core::section_words_to_biquad(words[si]);
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: audition_morph <body.bin> <out_prefix>\n";
    return 1;
  }
  std::ifstream in(argv[1], std::ios::binary);
  const std::vector<std::uint8_t> body((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
  const auto corners = p2k::body_corners(body);
  const auto floated = nb::import_p2k(body);
  const std::size_t n = static_cast<std::size_t>(kSr * kSeconds);

  for (const bool use_float : {false, true}) {
    trench::core::SawSource saw(49.0, kSr, 0.25F);
    trench::core::CascadeRunner runner;
    std::vector<float> out(n);
    float peak = 0.0F;
    for (std::size_t off = 0; off < n; off += kBlock) {
      const double t = static_cast<double>(off) / kSr / kSeconds;
      const float m = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * t));
      runner.set_target(use_float ? nb::cascade(nb::blend(floated, m, kQ, kSr),
                                                nb::blend_gain_db(floated, m, kQ))
                                  : packed_cascade(corners, m, kQ));
      const std::size_t len = std::min(kBlock, n - off);
      for (std::size_t i = 0; i < len; ++i) out[off + i] = saw.next();
      runner.process(std::span<float>(out).subspan(off, len));
    }
    for (const float s : out) peak = std::max(peak, std::abs(s));
    for (auto& s : out) s = s / peak * 0.5F;
    const std::string path = std::string(argv[2]) + (use_float ? "_2019.wav" : "_1992.wav");
    write_wav(path, out);
    std::cout << path << " peak before normalise " << 20.0 * std::log10(peak) << " dBFS\n";
  }
  return 0;
}
