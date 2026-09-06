#include "Audio.h"
#include <algorithm>
#include <cmath>

namespace hs
{
Audio::~Audio() { stop(); }

bool Audio::start()
{
    if (open) return true;
    error = manager.initialiseWithDefaultDevices (0, 2);
    if (error.isNotEmpty()) return false;
    auto* device = manager.getCurrentAudioDevice();
    if (device == nullptr) { error = "no audio device"; return false; }
    deviceInfo = device->getName() + " " + juce::String (device->getCurrentSampleRate(), 0) + " Hz";
    manager.addAudioCallback (this);
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        manager.setMidiInputDeviceEnabled (device.identifier, true);
        manager.addMidiInputDeviceCallback (device.identifier, this);
    }
    open = true;
    return true;
}

void Audio::stop()
{
    playing.store (false);
    if (! open) return;
    manager.removeAudioCallback (this);
    for (const auto& device : juce::MidiInput::getAvailableDevices()) manager.removeMidiInputDeviceCallback (device.identifier, this);
    manager.closeAudioDevice();
    open = false;
}

void Audio::publish (const std::array<std::uint16_t, 30>& w)
{
    const auto generation = published.load() + 1;
    auto& slot = slots[generation % 2];
    slot.sequence.store (generation * 2 - 1, std::memory_order_seq_cst);
    for (size_t i = 0; i < 30; ++i) slot.words[i].store (w[i], std::memory_order_seq_cst);
    slot.sequence.store (generation * 2, std::memory_order_seq_cst);
    published.store (generation, std::memory_order_release);
}

void Audio::consume()
{
    const auto generation = published.load (std::memory_order_acquire);
    if (generation == 0 || generation == consumed) return;
    auto& slot = slots[generation % 2];
    if (slot.sequence.load() != generation * 2) return;
    trench::core::CornerWords w;
    w.fill (trench::core::kIdentitySection);
    for (size_t s = 0; s < 6; ++s) for (size_t k = 0; k < 5; ++k) w[s][k] = slot.words[s * 5 + k].load();
    if (slot.sequence.load() != generation * 2) return;
    runner.set_glide (trench::core::native::rewarp_cascade (w, trench::core::kP2kDatumHz, rate), 256);
    consumed = generation;
}

float Audio::next()
{
    const int src = source.load (std::memory_order_relaxed);
    if (src == 0)
    {
        phase += 440.0 * std::pow (2.0, (note.load (std::memory_order_relaxed) - 69) / 12.0) / rate;
        phase -= std::floor (phase);
        return (float) ((2.0 * phase - 1.0) * 0.4);
    }
    random ^= random << 13; random ^= random >> 17; random ^= random << 5;
    const double white = double (random) / 4294967295.0 * 2.0 - 1.0;
    pink0 = 0.99765 * pink0 + white * 0.0990460;
    pink1 = 0.96300 * pink1 + white * 0.2965164;
    pink2 = 0.57000 * pink2 + white * 1.0526913;
    return (float) (0.18 * (pink0 + pink1 + pink2 + white * 0.1848));
}

void Audio::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    rate = device->getCurrentSampleRate();
    runner.set_sample_rate (rate);
    runner.reset();
    runner.set_ring_leveller (true);
    runner.set_pole_distortion (0.0);
    trench::core::Cascade identity {};
    for (auto& row : identity) row = trench::core::section_words_to_biquad (trench::core::kIdentitySection);
    runner.set_immediate (identity);
    consumed = 0;
}

void Audio::audioDeviceStopped() {}

void Audio::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    if (! message.isNoteOn()) return;
    const int n = message.getNoteNumber();
    note.store (n);
    if (onNote) juce::MessageManager::callAsync ([this, n] { if (onNote) onNote (n); });
}

void Audio::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    consume();
    const bool play = playing.load();
    for (int offset = 0; offset < numSamples; offset += (int) block.size())
    {
        const int n = std::min ((int) block.size(), numSamples - offset);
        for (int i = 0; i < n; ++i) block[(size_t) i] = play ? next() : 0.0f;
        if (play) runner.process (std::span<float> (block.data(), (size_t) n));
        for (int i = 0; i < n; ++i)
        {
            const float y = std::isfinite (block[(size_t) i]) ? block[(size_t) i] * 0.5f : 0.0f;
            for (int ch = 0; ch < numOut; ++ch) if (out[ch] != nullptr) out[ch][offset + i] = y;
        }
    }
}
}
