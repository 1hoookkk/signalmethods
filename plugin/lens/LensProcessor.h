#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <trench/core/audition.hpp>
#include <trench/core/native_body.hpp>
#include <trench/core/packed_body.hpp>
#include <array>
#include <atomic>
#include <complex>

namespace lens
{
using Words = trench::core::CornerWords;

struct Pole { double hz = 0.0, bandwidth = 0.0; };

class Processor : public juce::AudioProcessor
{
public:
    static constexpr int kOrder = 12, kWindow = 512, kHop = 128;

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
    std::atomic<float>* hold = nullptr;
    std::atomic<float>* gate = nullptr;
    std::atomic<bool> quiet { true };
    juce::File keep();

    std::array<std::atomic<double>, 12> shown {};
    std::atomic<unsigned int> frames { 0 };
    std::array<float, kWindow> frameCopy {};
    std::array<double, kOrder + 1> coefficientCopy {};
    double errorCopy = 0.0;
    std::atomic<unsigned int> frameSeq { 0 };
    double sampleRate() const { return rate; }
    double lpcRate() const { return rate / (double) decimation; }

private:
    void analyse();
    void follow();
    std::array<trench::core::CascadeRunner, 2> runners;
    double rate = 44100.0;
    int decimation = 4;
    std::array<float, 4> decimator {};
    int phase = 0;
    std::array<float, kWindow> ring {};
    int ringWrite = 0, sinceHop = 0;
    std::array<float, kWindow> window {};
    std::array<double, kOrder + 1> autocorrelation {}, coefficients {}, scratch {};
    std::array<std::complex<double>, kOrder> roots {};
    std::array<Pole, 6> poles {};
    Words words {};
    std::atomic<int> keepRequest { 0 };
    int keepDone = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Processor)
};
}
