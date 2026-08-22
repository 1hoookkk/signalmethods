#include "trench/audio/audio_boundary.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <vector>

namespace {

void put32(std::ofstream& out, std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); }
void put16(std::ofstream& out, std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); }

std::filesystem::path write_stereo_wav(double hz, std::uint32_t rate, std::uint32_t frames) {
  const auto path = std::filesystem::temp_directory_path() / "trench_audio_boundary_test.wav";
  std::ofstream out(path, std::ios::binary);
  const std::uint32_t data_bytes = frames * 4;
  out.write("RIFF", 4);
  put32(out, 36 + data_bytes);
  out.write("WAVEfmt ", 8);
  put32(out, 16);
  put16(out, 1);
  put16(out, 2);
  put32(out, rate);
  put32(out, rate * 4);
  put16(out, 4);
  put16(out, 16);
  out.write("data", 4);
  put32(out, data_bytes);
  for (std::uint32_t i = 0; i < frames; ++i) {
    const double t = static_cast<double>(i) / rate;
    const auto left = static_cast<std::int16_t>(std::lround(16000.0 * std::sin(2.0 * std::numbers::pi * hz * t)));
    const auto right = static_cast<std::int16_t>(std::lround(-16000.0 * std::sin(2.0 * std::numbers::pi * hz * t)));
    put16(out, static_cast<std::uint16_t>(left));
    put16(out, static_cast<std::uint16_t>(right));
  }
  return path;
}

}  // namespace

TEST(AudioBoundary, JuceNineAudioModulesBuildAndRun) {
  EXPECT_EQ(trench::audio::juce_major_version(), 9);
  EXPECT_EQ(trench::audio::probe_buffer_sample_count(), 64);
}

TEST(AudioBoundary, DecodeMonoAveragesChannelsAndReportsTheRate) {
  const auto path = write_stereo_wav(440.0, 48000, 4800);
  const auto clip = trench::audio::decode_mono(path);
  ASSERT_TRUE(clip.has_value());
  EXPECT_DOUBLE_EQ(clip->sample_rate_hz, 48000.0);
  EXPECT_EQ(clip->samples.size(), 4800U);
  double peak = 0.0;
  for (const auto s : clip->samples) peak = std::max(peak, static_cast<double>(std::abs(s)));
  EXPECT_LT(peak, 1e-4) << "opposite-phase channels must cancel in the mono mix";
  std::filesystem::remove(path);
}

TEST(AudioBoundary, DecodeMonoRefusesAMissingFile) {
  EXPECT_FALSE(trench::audio::decode_mono(std::filesystem::temp_directory_path() / "no_such_file.wav").has_value());
}
