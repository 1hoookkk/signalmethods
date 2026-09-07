#pragma once

#include "Locator.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <trench/core/audition.hpp>
#include <trench/core/native_body.hpp>
#include <array>
#include <atomic>

namespace lens
{
class Processor : public juce::AudioProcessor
{
public:
    static constexpr int kHop = 128, kRanked = 5;

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
    std::atomic<float>* smooth = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* gate = nullptr;

    Locator locator;
    bool loaded = false;
    std::atomic<int> matched { -1 };
    std::array<std::atomic<int>, kRanked> ranked {};
    std::array<std::atomic<float>, kRanked> rankedDistance {};
    std::atomic<bool> quiet { true };
    std::atomic<unsigned int> frames { 0 };
    Descriptor shownDescriptor {};
    std::atomic<unsigned int> descriptorSeq { 0 };
    std::array<std::atomic<int>, 4> slots {};
    void capture (int slot);
    double sampleRate() const { return rate; }
    double analysisRate() const { return rate / decimation; }

private:
    void locate();
    std::array<trench::core::CascadeRunner, 2> runners;
    double rate = 44100.0;
    int decimation = 4, phase = 0, ringWrite = 0, sinceHop = 0, playing = -1;
    std::array<float, 33> fir {};
    std::array<float, 64> firHistory {};
    int firWrite = 0;
    std::array<float, kFrame> ring {}, frame {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};
}
