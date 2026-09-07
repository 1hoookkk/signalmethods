#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <trench/core/audition.hpp>
#include <trench/core/native_body.hpp>
#include <trench/core/packed_body.hpp>
#include <atomic>
#include <mutex>
#include <vector>

namespace lens
{
struct Snapshot
{
    juce::String name;
    trench::core::CornerWords words {};
    float x = 0.5f, y = 0.5f;
};

struct Step { double t = 0.0; float x = 0.5f, y = 0.5f; };

class Processor : public juce::AudioProcessor
{
public:
    Processor();
    ~Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "LENS"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;
    std::atomic<float>* x = nullptr;
    std::atomic<float>* y = nullptr;
    std::atomic<float>* bypass = nullptr;

    std::vector<Snapshot> library;
    std::vector<Snapshot> placed;
    std::mutex placedLock;
    void placeNext (size_t dot);
    void moveDot (size_t dot, float nx, float ny);
    trench::core::CornerWords blend (float px, float py) const;

    std::atomic<bool> recording { false }, playing { false };
    std::vector<Step> path;
    std::mutex pathLock;
    void setPuck (float nx, float ny);
    void clearPath();

private:
    void loadLibrary();
    std::array<trench::core::CascadeRunner, 2> runners;
    double rate = 44100.0;
    float lastX = -1.0f, lastY = -1.0f;
    double clock = 0.0, playhead = 0.0;
    size_t playIndex = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};
}
