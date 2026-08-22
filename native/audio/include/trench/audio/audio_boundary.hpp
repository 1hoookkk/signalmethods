#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace trench::audio {

int juce_major_version();
int probe_buffer_sample_count();

struct MonoClip {
  std::vector<float> samples;
  double sample_rate_hz{};
};

std::optional<MonoClip> decode_mono(const std::filesystem::path& path);

}  // namespace trench::audio
