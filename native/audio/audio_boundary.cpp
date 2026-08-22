#include "trench/audio/audio_boundary.hpp"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <memory>

namespace trench::audio {

int juce_major_version() { return JUCE_MAJOR_VERSION; }

int probe_buffer_sample_count() {
  juce::AudioBuffer<float> buffer(2, 64);
  buffer.clear();
  return buffer.getNumSamples();
}

std::optional<MonoClip> decode_mono(const std::filesystem::path& path) {
  juce::AudioFormatManager manager;
  manager.registerBasicFormats();
  const juce::File file(juce::String(path.wstring().c_str()));
  std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(file));
  if (!reader || reader->numChannels == 0 || reader->lengthInSamples <= 0) return std::nullopt;
  const auto length = static_cast<int>(reader->lengthInSamples);
  juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), length);
  if (!reader->read(&buffer, 0, length, 0, true, true)) return std::nullopt;
  MonoClip clip;
  clip.sample_rate_hz = reader->sampleRate;
  clip.samples.resize(static_cast<std::size_t>(length));
  const float scale = 1.0F / static_cast<float>(buffer.getNumChannels());
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
    const float* data = buffer.getReadPointer(channel);
    for (int i = 0; i < length; ++i) clip.samples[static_cast<std::size_t>(i)] += data[i] * scale;
  }
  return clip;
}

}  // namespace trench::audio
