#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <trench/core/audition.hpp>
#include <trench/core/native_body.hpp>
#include <array>
#include <atomic>
#include <cstdint>

namespace hs
{
class Audio : public juce::AudioIODeviceCallback
{
public:
    ~Audio() override;
    bool start();
    void stop();
    void publish (const std::array<std::uint16_t, 30>& words);
    void setPlaying (bool on) { playing.store (on); }
    void setSource (int s) { source.store (s); }
    bool isOpen() const { return open; }
    juce::String error, deviceInfo;

    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    struct Slot
    {
        std::atomic<unsigned long long> sequence { 0 };
        std::array<std::atomic<std::uint16_t>, 30> words {};
    };
    void consume();
    float next();
    juce::AudioDeviceManager manager;
    bool open = false;
    std::atomic<bool> playing { false };
    std::atomic<int> source { 1 };
    std::array<Slot, 2> slots;
    std::atomic<unsigned long long> published { 0 };
    unsigned long long consumed = 0;
    trench::core::CascadeRunner runner;
    double rate = 44100.0, phase = 0.0, pink0 = 0.0, pink1 = 0.0, pink2 = 0.0;
    std::uint32_t random = 123456789u;
    std::array<float, 4096> block {};
};
}
