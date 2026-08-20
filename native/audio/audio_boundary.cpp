#include "trench/audio/audio_boundary.hpp"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace trench::audio {

int juce_major_version() { return JUCE_MAJOR_VERSION; }

int probe_buffer_sample_count() {
  juce::AudioBuffer<float> buffer(2, 64);
  buffer.clear();
  return buffer.getNumSamples();
}

}  // namespace trench::audio
