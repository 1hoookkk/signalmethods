#include "Audio.h"
#include <algorithm>
#include <cmath>

namespace hs
{
namespace
{
constexpr int kNoteOn = 1, kNoteOff = 2, kSustain = 3, kAllOff = 4;
}

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
    for (const auto& input : juce::MidiInput::getAvailableDevices())
    {
        manager.setMidiInputDeviceEnabled (input.identifier, true);
        manager.addMidiInputDeviceCallback (input.identifier, this);
    }
    open = true;
    return true;
}

void Audio::stop()
{
    playing.store (false);
    if (! open) return;
    manager.removeAudioCallback (this);
    for (const auto& input : juce::MidiInput::getAvailableDevices()) manager.removeMidiInputDeviceCallback (input.identifier, this);
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

void Audio::push (Event e)
{
    const int i = eventWrite.load (std::memory_order_relaxed);
    events[(size_t) (i % (int) events.size())] = e;
    eventWrite.store (i + 1, std::memory_order_release);
}

void Audio::noteOn (int midi, float velocity)
{
    note.store (midi);
    push ({ kNoteOn, midi, std::clamp (velocity, 0.05f, 1.0f) });
}

void Audio::noteOff (int midi) { push ({ kNoteOff, midi, 0.0f }); }

void Audio::allNotesOff() { push ({ kAllOff, 0, 0.0f }); }

void Audio::sustain (bool down) { push ({ kSustain, 0, down ? 1.0f : 0.0f }); }

double Audio::voiceHz (int index) const
{
    return index >= 0 && index < kVoices ? voiceHzShadow[(size_t) index].load() : 0.0;
}

void Audio::drain()
{
    const int end = eventWrite.load (std::memory_order_acquire);
    for (; eventRead < end; ++eventRead)
    {
        const auto e = events[(size_t) (eventRead % (int) events.size())];
        if (e.type == kNoteOn)
        {
            Voice* v = nullptr;
            for (auto& candidate : voices) if (candidate.note == e.note) { v = &candidate; break; }
            if (v == nullptr) for (auto& candidate : voices) if (candidate.note < 0) { v = &candidate; break; }
            if (v == nullptr) for (auto& candidate : voices) if (! candidate.held && (v == nullptr || candidate.age < v->age)) v = &candidate;
            if (v == nullptr) for (auto& candidate : voices) if (v == nullptr || candidate.age < v->age) v = &candidate;
            v->note = e.note; v->age = ++clock; v->velocity = e.value; v->held = true; v->sustained = false;
            v->burst = (int) (0.010 * rate);
            if (v->envelope < 1e-4) v->phase = 0.0;
        }
        else if (e.type == kNoteOff)
        {
            for (auto& v : voices)
                if (v.note == e.note && v.held) { v.held = false; v.sustained = pedal; }
        }
        else if (e.type == kSustain)
        {
            pedal = e.value > 0.5f;
            if (! pedal) for (auto& v : voices) v.sustained = false;
        }
        else if (e.type == kAllOff)
        {
            pedal = false;
            for (auto& v : voices) { v.held = false; v.sustained = false; }
        }
    }
}

float Audio::next (Voice& v)
{
    const int src = source.load (std::memory_order_relaxed);
    if (src == 0)
    {
        const double hz = 440.0 * std::pow (2.0, (v.note - 69 + bendSt.load (std::memory_order_relaxed)) / 12.0);
        v.phase += hz / rate;
        v.phase -= std::floor (v.phase);
        return (float) ((2.0 * v.phase - 1.0) * 0.4);
    }
    return 0.0f;
}

void Audio::audioDeviceAboutToStart (juce::AudioIODevice* device) { prepare (device->getCurrentSampleRate()); }

void Audio::prepare (double sampleRate)
{
    rate = sampleRate;
    runner.set_sample_rate (rate);
    runner.reset();
    runner.set_ring_leveller (true);
    runner.set_pole_distortion (0.0);
    trench::core::Cascade identity {};
    for (auto& row : identity) row = trench::core::section_words_to_biquad (trench::core::kIdentitySection);
    runner.set_immediate (identity);
    consumed = 0;
    for (auto& v : voices) v = Voice {};
    drone = 0.0; pedal = false;
    eventRead = eventWrite.load();
}

void Audio::audioDeviceStopped() {}

void Audio::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        const int n = message.getNoteNumber();
        noteOn (n, message.getFloatVelocity());
        if (onNote) juce::MessageManager::callAsync ([this, n] { if (onNote) onNote (n); });
    }
    else if (message.isNoteOff()) noteOff (message.getNoteNumber());
    else if (message.isSustainPedalOn()) sustain (true);
    else if (message.isSustainPedalOff()) sustain (false);
    else if (message.isAllNotesOff() || message.isAllSoundOff()) allNotesOff();
    else if (message.isPitchWheel()) bend ((message.getPitchWheelValue() - 8192) / 8192.0 * 2.0);
    else if (message.isController() && message.getControllerNumber() == 1)
    {
        const double value = message.getControllerValue() / 127.0;
        if (onWheel) juce::MessageManager::callAsync ([this, value] { if (onWheel) onWheel (value); });
    }
}

void Audio::audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    consume();
    drain();
    const bool droning = playing.load();
    const int src = source.load (std::memory_order_relaxed);
    const double attack = 1.0 - std::exp (-1.0 / (0.002 * rate)), release = 1.0 - std::exp (-1.0 / (0.12 * rate));
    const int burstLength = std::max (1, (int) (0.010 * rate));
    int sounding = 0;
    for (auto& v : voices) if (v.note >= 0 && (v.held || v.sustained || v.envelope >= 1e-4)) ++sounding;
    active.store (sounding);
    for (int i = 0; i < kVoices; ++i)
        voiceHzShadow[(size_t) i].store (voices[(size_t) i].note >= 0 && voices[(size_t) i].envelope > 1e-4 ? 440.0 * std::pow (2.0, (voices[(size_t) i].note - 69 + bendSt.load()) / 12.0) : 0.0);
    const float spread = 1.0f / std::sqrt ((float) std::max (1, sounding));
    for (int offset = 0; offset < numSamples; offset += (int) block.size())
    {
        const int n = std::min ((int) block.size(), numSamples - offset);
        for (int i = 0; i < n; ++i)
        {
            drone += ((droning ? 1.0 : 0.0) - drone) * (droning ? attack : release);
            float x = 0.0f;
            float gate = (float) drone;
            for (auto& v : voices)
            {
                if (v.note < 0) continue;
                const double target = v.held || v.sustained ? 1.0 : 0.0;
                v.envelope += (target - v.envelope) * (target > v.envelope ? attack : release);
                if (v.envelope < 1e-4 && target == 0.0) { v.note = -1; continue; }
                gate = std::max (gate, (float) v.envelope * v.velocity);
                if (src == 0) x += next (v) * (float) v.envelope * v.velocity * spread;
                if (v.burst > 0)
                {
                    random ^= random << 13; random ^= random >> 17; random ^= random << 5;
                    const double window = 0.5 - 0.5 * std::cos (2.0 * 3.141592653589793 * (burstLength - v.burst) / (double) burstLength);
                    x += (float) ((double (random) / 4294967295.0 * 2.0 - 1.0) * 0.6 * window) * v.velocity;
                    --v.burst;
                }
            }
            if (droning && src == 0)
            {
                phase += 440.0 * std::pow (2.0, (note.load (std::memory_order_relaxed) - 69 + bendSt.load (std::memory_order_relaxed)) / 12.0) / rate;
                phase -= std::floor (phase);
                x += (float) ((2.0 * phase - 1.0) * 0.4 * drone);
            }
            if (src == 1 && gate > 1e-5)
            {
                random ^= random << 13; random ^= random >> 17; random ^= random << 5;
                const double white = double (random) / 4294967295.0 * 2.0 - 1.0;
                pink0 = 0.99765 * pink0 + white * 0.0990460;
                pink1 = 0.96300 * pink1 + white * 0.2965164;
                pink2 = 0.57000 * pink2 + white * 1.0526913;
                x += (float) (0.18 * (pink0 + pink1 + pink2 + white * 0.1848)) * gate;
            }
            if (src == 2 && gate > 1e-5)
            {
                if (auto fresh = loop.load(); fresh != playingLoop) { playingLoop = fresh; loopPos = 0.0; }
                if (playingLoop != nullptr && playingLoop->samples.size() >= 2)
                {
                    const auto& s = playingLoop->samples;
                    const size_t k = (size_t) loopPos;
                    const double frac = loopPos - (double) k;
                    x += (float) ((1.0 - frac) * s[k] + frac * s[(k + 1) % s.size()]) * 0.8f * gate;
                    loopPos += playingLoop->rate / rate;
                    if (loopPos >= (double) s.size()) loopPos -= (double) s.size();
                }
            }
            block[(size_t) i] = x;
        }
        runner.process (std::span<float> (block.data(), (size_t) n));
        for (int i = 0; i < n; ++i)
        {
            const float y = std::isfinite (block[(size_t) i]) ? block[(size_t) i] * 0.5f : 0.0f;
            for (int ch = 0; ch < numOut; ++ch) if (out[ch] != nullptr) out[ch][offset + i] = y;
        }
    }
}
}
