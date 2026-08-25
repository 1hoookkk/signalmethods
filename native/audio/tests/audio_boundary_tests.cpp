#include "trench/audio/audio_boundary.hpp"
#include "trench/audio/audition.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numbers>
#include <vector>

namespace {

double response_peak_hz(const trench::core::Cascade& cascade,
                        double sample_rate_hz, double low_hz,
                        double high_hz) {
  double best_hz = low_hz;
  double best_db = -std::numeric_limits<double>::infinity();
  for (double hz = low_hz; hz <= high_hz; hz += 0.25) {
    const double db = trench::core::cascade_response_db(cascade, hz,
                                                        sample_rate_hz);
    if (db > best_db) {
      best_db = db;
      best_hz = hz;
    }
  }
  return best_hz;
}

trench::audio::AuditionView pitched_view() {
  namespace native = trench::core::native;
  const native::RealRoots parked{std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity()};
  native::Body body;
  for (auto& corner : body.corners) {
    for (auto& section : corner.sections) {
      section = {parked, parked, true};
    }
    corner.sections[0].pole = native::Resonant{1700.0, 70.0};
  }
  return {body, 0.0F, 0.0F, 0.0};
}

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
  const auto stereo = trench::audio::decode_audio(path);
  ASSERT_TRUE(stereo.has_value());
  EXPECT_DOUBLE_EQ(stereo->sample_rate_hz, 48000.0);
  ASSERT_EQ(stereo->channels.size(), 2U);
  EXPECT_EQ(stereo->channels[0].size(), 4800U);
  EXPECT_NEAR(stereo->channels[0][100], -stereo->channels[1][100], 1.0e-6);
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

TEST(AuditionRate, SameNativeBodyKeepsItsPitchAtTwoDeviceRates) {
  const auto view = pitched_view();
  const auto at_44100 = trench::audio::design_audition(view, 44100.0);
  const auto at_48000 = trench::audio::design_audition(view, 48000.0);

  const double pitch_44100 = response_peak_hz(at_44100, 44100.0, 1200.0, 2300.0);
  const double pitch_48000 = response_peak_hz(at_48000, 48000.0, 1200.0, 2300.0);
  EXPECT_NEAR(pitch_48000, pitch_44100, 1.0);

  const double stale_pitch = response_peak_hz(at_44100, 48000.0, 1200.0, 2300.0);
  EXPECT_NEAR(stale_pitch, pitch_44100 * 48000.0 / 44100.0, 2.0);
  EXPECT_GT(stale_pitch - pitch_48000, 100.0);
}
