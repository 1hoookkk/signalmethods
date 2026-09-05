#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <trench/core/audition.hpp>
#include <trench/core/native_body.hpp>
#include "../model/Frame.h"
#include <atomic>
#include <memory>
#include <vector>

namespace ws
{
class Audio : public juce::AudioIODeviceCallback
{
public:
    Audio();
    ~Audio() override;

    void setClip (std::shared_ptr<const std::vector<float>> samples, double sampleRate);
    void setRegion (double startSeconds, double endSeconds);
    void setWords (const Words& words);
    void setPlaying (bool on);
    void setWet (bool on);
    bool isPlaying() const { return playing.load(); }
    double playhead() const { return position.load(); }
    float peak() const { return peakLevel.load(); }
    bool available() const { return ready; }
    juce::String deviceInfo() const;

    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    juce::AudioDeviceManager manager;
    bool ready = false;
    juce::String initError;
    double sawPhase = 0.0;
    std::shared_ptr<const std::vector<float>> clip;
    std::atomic<double> clipRate { 44100.0 }, deviceRate { 44100.0 };
    std::atomic<double> regionStart { 0.0 }, regionEnd { 0.0 }, position { 0.0 };
    std::atomic<bool> playing { false }, wet { true };
    std::atomic<float> peakLevel { 0.0f };
    double cursor = 0.0;
    std::array<Words, 2> pending {};
    std::atomic<int> pendingIndex { -1 };
    trench::core::CascadeRunner runner;
    juce::SpinLock clipLock;
};
}
