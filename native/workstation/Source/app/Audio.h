#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <trench/core/audition.hpp>
#include <trench/core/native_body.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace hs
{
class Audio : public juce::AudioIODeviceCallback, public juce::MidiInputCallback
{
public:
    std::function<void (int)> onNote;
    std::function<void (int, float)> onNoteOn;
    std::function<void (double)> onWheel;
    ~Audio() override;
    bool start();
    void stop();
    void prepare (double sampleRate);
    void publish (const std::array<std::uint16_t, 30>& words);
    void setPlaying (bool on) { playing.store (on); }
    void setSource (int s) { source.store (s); }
    void setNote (int midi) { note.store (midi); }
    void noteOn (int midi, float velocity = 0.8f);
    void noteOff (int midi);
    void allNotesOff();
    void sustain (bool down);
    void bend (double semitones) { bendSt.store (semitones); }
    struct Clip { std::vector<float> samples; double rate = 44100.0; };
    void setLoop (std::shared_ptr<const Clip> clip) { loop.store (std::move (clip)); }
    struct Sample { juce::String name; int midi = 60; std::vector<float> samples; double rate = 44100.0; };
    using Bank = std::vector<Sample>;
    void setBank (std::shared_ptr<const Bank> fresh) { bank.store (std::move (fresh)); }
    std::shared_ptr<const Bank> bankNow() const { return bank.load(); }
    void setSamplerRoute (bool on) { samplerRoute.store (on); }
    void samplerNoteOn (int midi, float velocity = 0.8f);
    void samplerNoteOff (int midi);
    int pullSampler (float* dst, int max);
    bool isOpen() const { return open; }
    int pull (float* dst, int max);
    int pullInput (float* dst, int max);
    int activeVoices() const { return active.load(); }
    double voiceHz (int index) const;
    juce::String error, deviceInfo;

    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    static constexpr int kVoices = 8;

private:
    struct Slot
    {
        std::atomic<unsigned long long> sequence { 0 };
        std::array<std::atomic<std::uint16_t>, 30> words {};
    };
    struct Event { int type = 0, note = 0; float value = 0.0f; };
    struct Voice
    {
        int note = -1;
        unsigned long long age = 0;
        double phase = 0.0, envelope = 0.0;
        float velocity = 0.0f;
        int burst = 0;
        bool held = false, sustained = false;
    };
    void push (Event e);
    void drain();
    void consume();
    float next (Voice& v);
    juce::AudioDeviceManager manager;
    bool open = false;
    std::atomic<bool> playing { false };
    std::atomic<int> source { 1 };
    std::atomic<int> note { 45 };
    std::atomic<int> active { 0 };
    std::atomic<double> bendSt { 0.0 };
    std::array<Event, 128> events {};
    std::atomic<int> eventWrite { 0 };
    int eventRead = 0;
    std::array<Voice, kVoices> voices {};
    std::array<std::atomic<double>, kVoices> voiceHzShadow {};
    unsigned long long clock = 0;
    bool pedal = false;
    double drone = 0.0;
    struct SamplerVoice
    {
        int note = -1, index = -1;
        double position = 0.0, step = 1.0;
        float velocity = 0.0f, envelope = 0.0f;
        bool held = false;
    };
    std::atomic<std::shared_ptr<const Clip>> loop;
    std::shared_ptr<const Clip> playingLoop;
    double loopPos = 0.0;
    std::atomic<std::shared_ptr<const Bank>> bank;
    std::shared_ptr<const Bank> playingBank;
    std::atomic<bool> samplerRoute { false };
    std::array<SamplerVoice, kVoices> samplerVoices {};
    std::array<Slot, 2> slots;
    std::atomic<unsigned long long> published { 0 };
    unsigned long long consumed = 0;
    trench::core::CascadeRunner runner;
    double rate = 44100.0, phase = 0.0, pink0 = 0.0, pink1 = 0.0, pink2 = 0.0;
    std::uint32_t random = 123456789u;
    int trainPos = 0, trainBurst = 0;
    std::array<float, 4096> block {};
    static constexpr unsigned int kTap = 65536;
    std::vector<float> tapRing = std::vector<float> (kTap, 0.0f);
    std::atomic<unsigned int> tapWrite { 0 };
    unsigned int tapRead = 0;
    std::vector<float> inputRing = std::vector<float> (kTap, 0.0f);
    std::atomic<unsigned int> inputWrite { 0 };
    unsigned int inputRead = 0;
    std::vector<float> samplerRing = std::vector<float> (kTap, 0.0f);
    std::atomic<unsigned int> samplerWrite { 0 };
    unsigned int samplerRead = 0;
};
}
