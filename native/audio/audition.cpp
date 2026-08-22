#include "trench/audio/audition.hpp"

#include <juce_audio_devices/juce_audio_devices.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

#include "trench/core/audition.hpp"

namespace trench::audio {

struct Audition::Impl final : public juce::AudioIODeviceCallback {
  juce::AudioDeviceManager manager;
  std::mutex lock;
  trench::core::Cascade pending{};
  bool pending_fresh{};
  std::shared_ptr<const MonoClip> pending_clip;
  bool clip_fresh{};
  std::atomic<double> saw_hz{49.0};
  std::atomic<float> saw_level{0.25F};
  std::atomic<bool> gate{false};
  std::atomic<bool> active{false};
  double sample_rate{44100.0};

  trench::core::CascadeRunner runner;
  std::shared_ptr<const MonoClip> clip;
  std::size_t clip_pos{};
  double saw_phase{};
  double saw_step{};
  float gain{};

  void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
    sample_rate = device->getCurrentSampleRate();
    runner.reset();
    gain = 0.0F;
  }

  void audioDeviceStopped() override {}

  void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* out, int channels,
                                        int frames,
                                        const juce::AudioIODeviceCallbackContext&) override {
    if (lock.try_lock()) {
      if (pending_fresh) {
        runner.set_target(pending);
        pending_fresh = false;
      }
      if (clip_fresh) {
        clip = pending_clip;
        clip_pos = 0;
        clip_fresh = false;
      }
      lock.unlock();
    }
    if (channels <= 0 || frames <= 0) return;
    float* left = out[0];
    const bool open = gate.load();
    const float target_gain = open ? 1.0F : 0.0F;
    const float slew = 1.0F / 256.0F;
    saw_step = saw_hz.load() / sample_rate;
    const float level = saw_level.load();
    for (int i = 0; i < frames; ++i) {
      float x = 0.0F;
      if (clip && !clip->samples.empty()) {
        x = clip->samples[clip_pos];
        clip_pos = (clip_pos + 1) % clip->samples.size();
      } else {
        x = static_cast<float>(2.0 * saw_phase - 1.0) * level;
        saw_phase += saw_step;
        if (saw_phase >= 1.0) saw_phase -= 1.0;
      }
      gain += std::clamp(target_gain - gain, -slew, slew);
      left[i] = x * gain;
    }
    if (gain <= 0.0F && !open) {
      std::fill(left, left + frames, 0.0F);
    } else {
      runner.process({left, static_cast<std::size_t>(frames)});
    }
    for (int c = 1; c < channels; ++c) std::copy(left, left + frames, out[c]);
  }
};

Audition::Audition() : impl_(std::make_unique<Impl>()) {}

Audition::~Audition() { stop(); }

std::string Audition::start() {
  if (impl_->active.load()) return {};
  const auto error = impl_->manager.initialiseWithDefaultDevices(0, 2);
  if (error.isNotEmpty()) return error.toStdString();
  impl_->manager.addAudioCallback(impl_.get());
  impl_->active.store(true);
  return {};
}

void Audition::stop() {
  if (!impl_->active.load()) return;
  impl_->manager.removeAudioCallback(impl_.get());
  impl_->manager.closeAudioDevice();
  impl_->active.store(false);
}

bool Audition::running() const noexcept { return impl_->active.load(); }

double Audition::sampleRateHz() const noexcept { return impl_->sample_rate; }

void Audition::setCascade(const trench::core::Cascade& cascade) {
  const std::scoped_lock guard(impl_->lock);
  impl_->pending = cascade;
  impl_->pending_fresh = true;
}

void Audition::setSaw(double hz, float level) {
  impl_->saw_hz.store(hz);
  impl_->saw_level.store(level);
  const std::scoped_lock guard(impl_->lock);
  impl_->pending_clip.reset();
  impl_->clip_fresh = true;
}

void Audition::setClip(MonoClip clip) {
  const std::scoped_lock guard(impl_->lock);
  impl_->pending_clip = std::make_shared<const MonoClip>(std::move(clip));
  impl_->clip_fresh = true;
}

void Audition::setGate(bool open) { impl_->gate.store(open); }

}  // namespace trench::audio
