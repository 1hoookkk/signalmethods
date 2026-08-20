#include "trench/audio/audio_boundary.hpp"

#include <gtest/gtest.h>

TEST(AudioBoundary, JuceNineAudioModulesBuildAndRun) {
  EXPECT_EQ(trench::audio::juce_major_version(), 9);
  EXPECT_EQ(trench::audio::probe_buffer_sample_count(), 64);
}
