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
using Words = trench::core::CornerWords;

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
    bool acceptsMidi() const override { return true; }
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
    std::atomic<float>* morph = nullptr;
    std::atomic<float>* bypass = nullptr;

    Words current {}, a {}, b {};
    bool haveA = false, haveB = false;
    mutable std::mutex wordsLock;
    Words words() const;
    void setPole (int row, double hz, double radius);
    void clearRow (int row);
    void seed (const std::vector<std::pair<double, double>>& resonances);
    void capture (int which);
    void setMorph (float t);
    double sampleRate() const { return rate; }

    int pullInput (float* dst, int max);
    std::atomic<int> struck { 0 };

private:
    void publish();
    static void unityDc (Words& w);
    std::array<trench::core::CascadeRunner, 2> runners;
    double rate = 44100.0;
    std::atomic<unsigned long long> generation { 1 };
    unsigned long long consumed = 0;
    float lastMorph = -1.0f;
    int burstLeft = 0;
    float burstGain = 0.0f;
    std::uint32_t random = 22222u;
    static constexpr unsigned int kTap = 65536;
    std::vector<float> tapRing = std::vector<float> (kTap, 0.0f);
    std::atomic<unsigned int> tapWrite { 0 };
    unsigned int tapRead = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};
}
