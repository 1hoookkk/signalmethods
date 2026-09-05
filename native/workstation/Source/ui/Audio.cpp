#include "Audio.h"

namespace ws
{
Audio::Audio()
{
    const auto err = manager.initialiseWithDefaultDevices (0, 2);
    initError = err;
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

std::vector<float> Audio::snapshot() const
{
    std::vector<float> out (ring.size());
    const int rp = ringPos.load();
    for (size_t i = 0; i < ring.size(); ++i) out[i] = ring[(size_t) ((rp + (int) i) % (int) ring.size())];
    return out;
}

juce::String Audio::deviceInfo() const
{
    if (! ready) return "no audio device" + (initError.isEmpty() ? juce::String() : ": " + initError);
    auto* d = manager.getCurrentAudioDevice();
    return "listening on " + d->getName() + " at " + juce::String ((int) d->getCurrentSampleRate()) + " Hz";
}
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
    peakLevel.store (0.0f);
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
    if (! lock.isLocked()) return;
    const double rate = clipRate.load(), dev = deviceRate.load();
    if (clip == nullptr || clip->empty() || ! clipOn.load())
    {
        std::vector<float> saw ((size_t) numSamples);
        const double inc = 110.0 / dev;
        const bool white = noiseOn.load();
        for (int i = 0; i < numSamples; ++i)
        {
            saw[(size_t) i] = white ? noise.nextFloat() * 0.8f - 0.4f : (float) (2.0 * sawPhase - 1.0) * 0.4f;
            sawPhase += inc;
            if (sawPhase >= 1.0) sawPhase -= 1.0;
        }
        if (wet.load()) runner.process (std::span<float> (saw.data(), saw.size()));
        float pk = 0.0f;
        int rp = ringPos.load();
        for (int i = 0; i < numSamples; ++i) { ring[(size_t) rp] = saw[(size_t) i]; rp = (rp + 1) % (int) ring.size(); }
        ringPos.store (rp);
        for (int c = 0; c < numOut; ++c)
            for (int i = 0; i < numSamples; ++i) { out[c][i] = saw[(size_t) i] * 0.5f; pk = std::max (pk, std::abs (out[c][i])); }
        peakLevel.store (pk);
        return;
    }
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
    {
        int rp = ringPos.load();
        for (int i = 0; i < numSamples; ++i) { ring[(size_t) rp] = block[(size_t) i]; rp = (rp + 1) % (int) ring.size(); }
        ringPos.store (rp);
    }
    float pk = 0.0f;
    for (int c = 0; c < numOut; ++c)
        for (int i = 0; i < numSamples; ++i) { out[c][i] = block[(size_t) i] * 0.5f; pk = std::max (pk, std::abs (out[c][i])); }
    peakLevel.store (pk);
    position.store (cursor / rate);
}
}
