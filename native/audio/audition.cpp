#include "trench/audio/audition.hpp"

#include <juce_audio_devices/juce_audio_devices.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <utility>

#include "trench/core/audition.hpp"
#include "trench/core/transpose.hpp"

namespace trench::audio {

trench::core::Cascade design_audition(const AuditionView& view,
                                      double device_sample_rate_hz) {
  const bool at_corner = (view.morph == 0.0F || view.morph == 1.0F) &&
                         (view.q == 0.0F || view.q == 1.0F);
  const auto interior =
      at_corner ? trench::core::native::Corner{}
                : trench::core::native::packed_interior_corner(view.packed, view.morph,
                                                               view.q);
  const auto gain_db =
      at_corner
          ? trench::core::native::blend_gain_db(view.body, view.morph, view.q)
          : interior.gain_db;
  const auto designed =
      at_corner
          ? trench::core::native::cascade(
                trench::core::native::blend(view.body, view.morph, view.q,
                                            device_sample_rate_hz),
                gain_db)
          : trench::core::native::cascade(
                trench::core::native::design(interior, device_sample_rate_hz), gain_db);
  if (view.semitones == 0.0 && at_corner) return designed;

  auto transposed = trench::core::unity_dc(trench::core::transpose_cascade(
      designed, trench::core::ratio_of_semitones(view.semitones),
      device_sample_rate_hz));
  const double gain = std::pow(10.0, gain_db / 20.0);
  for (std::size_t coefficient = 0; coefficient < 3; ++coefficient) {
    transposed[0][coefficient] *= gain;
  }
  return transposed;
}

struct Audition::Impl final : public juce::AudioIODeviceCallback {
  juce::AudioDeviceManager manager;
  std::mutex lock;
  AuditionView view;
  trench::core::Cascade pending{};
  bool pending_fresh{};
  std::shared_ptr<const MonoClip> pending_clip;
  bool clip_fresh{};
  std::atomic<double> saw_hz{49.0};
  std::atomic<float> saw_level{0.25F};
  std::atomic<bool> gate{false};
  std::atomic<bool> active{false};
  std::atomic<double> sample_rate{44100.0};

  trench::core::CascadeRunner runner;
  std::shared_ptr<const MonoClip> clip;
  std::size_t clip_pos{};
  double saw_phase{};
  double saw_step{};
  float gain{};

  void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
    const double actual_rate = device->getCurrentSampleRate();
    sample_rate.store(actual_rate);
    {
      const std::scoped_lock guard(lock);
      pending = design_audition(view, actual_rate);
      pending_fresh = true;
    }
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
    saw_step = saw_hz.load() / sample_rate.load();
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
      bool broken = false;
      for (int i = 0; i < frames; ++i) {
        if (!std::isfinite(left[i])) {
          broken = true;
          break;
        }
      }
      if (broken) {
        runner.reset();
        std::fill(left, left + frames, 0.0F);
      } else {
        for (int i = 0; i < frames; ++i) {
          left[i] = std::clamp(left[i], -1.0F, 1.0F);
        }
      }
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

double Audition::sampleRateHz() const noexcept { return impl_->sample_rate.load(); }

void Audition::setView(AuditionView view) {
  const std::scoped_lock guard(impl_->lock);
  impl_->view = std::move(view);
  impl_->pending = design_audition(impl_->view, impl_->sample_rate.load());
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
