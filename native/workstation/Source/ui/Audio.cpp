#include "Audio.h"

namespace ws
{
Audio::Audio()
{
    const auto err = manager.initialiseWithDefaultDevices (0, 2);
    ready = err.isEmpty() && manager.getCurrentAudioDevice() != nullptr;
    if (ready) manager.addAudioCallback (this);
}

Audio::~Audio()
{
    if (ready) manager.removeAudioCallback (this);
    manager.closeAudioDevice();
}

void Audio::setClip (std::shared_ptr<const std::vector<float>> samples, double sampleRate)
{
    const juce::SpinLock::ScopedLockType lock (clipLock);
    clip = std::move (samples);
    clipRate.store (sampleRate);
    cursor = 0.0;
    regionStart.store (0.0);
    regionEnd.store (clip ? (double) clip->size() / sampleRate : 0.0);
}

void Audio::setRegion (double a, double b)
{
    if (b < a) std::swap (a, b);
    regionStart.store (a);
    regionEnd.store (b);
}

void Audio::setWords (const Words& words)
{
    const int slot = (pendingIndex.load() + 1) & 1;
    pending[(size_t) slot] = words;
    pendingIndex.store (slot);
}

void Audio::setPlaying (bool on) { playing.store (on); }
void Audio::setWet (bool on) { wet.store (on); }

void Audio::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    deviceRate.store (device->getCurrentSampleRate());
    runner.set_sample_rate (device->getCurrentSampleRate());
    runner.reset();
}

void Audio::audioDeviceStopped() {}

void Audio::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c) juce::FloatVectorOperations::clear (out[c], numSamples);
    const int slot = pendingIndex.exchange (-1);
    if (slot >= 0)
    {
        trench::core::CornerWords cw {};
        for (int s = 0; s < kRows; ++s)
            for (int k = 0; k < kWords; ++k)
                cw[(size_t) s][(size_t) k] = pending[(size_t) slot][(size_t) s][(size_t) k];
        cw[6] = trench::core::kIdentitySection;
        runner.set_glide (trench::core::native::rewarp_cascade (cw, kDatumHz, deviceRate.load()), 256);
    }
    if (! playing.load() || numOut == 0) return;
    const juce::SpinLock::ScopedTryLockType lock (clipLock);
    if (! lock.isLocked() || clip == nullptr || clip->empty()) return;
    const double rate = clipRate.load(), dev = deviceRate.load();
    const double a = regionStart.load() * rate, b = std::min ((double) clip->size() - 1.0, regionEnd.load() * rate);
    if (b - a < 2.0) return;
    if (cursor < a || cursor >= b) cursor = a;
    const double step = rate / dev;
    std::vector<float> block ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        const int i0 = (int) cursor;
        const float frac = (float) (cursor - i0);
        const float s0 = (*clip)[(size_t) i0], s1 = (*clip)[(size_t) std::min (i0 + 1, (int) clip->size() - 1)];
        block[(size_t) i] = s0 + (s1 - s0) * frac;
        cursor += step;
        if (cursor >= b) cursor = a;
    }
    if (wet.load()) runner.process (std::span<float> (block.data(), block.size()));
    for (int c = 0; c < numOut; ++c)
        for (int i = 0; i < numSamples; ++i) out[c][i] = block[(size_t) i] * 0.5f;
    position.store (cursor / rate);
}
}
