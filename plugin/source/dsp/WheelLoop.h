#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace trench
{
class WheelLoop
{
public:
    enum class Mode { Idle, Armed, Recording, Playing };
    static constexpr int kTicksPerBeat = 96;
    static constexpr int kMaxBeats = 64;
    static constexpr double kBeatsPerBar = 4.0;

    WheelLoop() : values ((size_t) (kTicksPerBeat * kMaxBeats), 0.5f) {}

    void arm (int bars) noexcept
    {
        lengthBeats.store (juce::jlimit (1, kMaxBeats, (int) (bars * kBeatsPerBar)), std::memory_order_relaxed);
        mode.store (Mode::Armed, std::memory_order_release);
    }
    void play() noexcept
    {
        if (recordedBeats.load (std::memory_order_relaxed) > 0)
            mode.store (Mode::Playing, std::memory_order_release);
    }
    void stop() noexcept { mode.store (Mode::Idle, std::memory_order_release); }
    Mode currentMode() const noexcept { return mode.load (std::memory_order_acquire); }
    int beatsRecorded() const noexcept { return recordedBeats.load (std::memory_order_relaxed); }
    double phaseBeats() const noexcept { return uiPhase.load (std::memory_order_relaxed); }

    void process (float* morph, int numSamples, double ppq, bool playing, double bpm, double sampleRate) noexcept
    {
        const Mode m = mode.load (std::memory_order_acquire);
        if (m == Mode::Idle || ! playing || ppq < 0.0 || bpm <= 0.0 || sampleRate <= 0.0)
            return;
        const double beatsPerSample = bpm / 60.0 / sampleRate;
        if (m == Mode::Armed)
        {
            const double nextBar = std::ceil (ppq / kBeatsPerBar) * kBeatsPerBar;
            const double barIn = nextBar - ppq;
            if (barIn > (double) numSamples * beatsPerSample)
                return;
            startBeat = nextBar;
            const int startIndex = (int) std::ceil (barIn / beatsPerSample);
            mode.store (Mode::Recording, std::memory_order_release);
            record (morph, startIndex, numSamples, ppq, beatsPerSample);
            return;
        }
        if (m == Mode::Recording)
        {
            record (morph, 0, numSamples, ppq, beatsPerSample);
            return;
        }
        const int len = recordedBeats.load (std::memory_order_relaxed);
        if (len <= 0)
            return;
        const double loopLen = (double) len;
        for (int i = 0; i < numSamples; ++i)
        {
            double phase = std::fmod (ppq + (double) i * beatsPerSample - startBeat, loopLen);
            if (phase < 0.0) phase += loopLen;
            const double t = phase * kTicksPerBeat;
            const int i0 = (int) t;
            const int i1 = (i0 + 1) % (len * kTicksPerBeat);
            const float frac = (float) (t - (double) i0);
            const float target = values[(size_t) i0] + (values[(size_t) i1] - values[(size_t) i0]) * frac;
            smoothed += (target - smoothed) * kSmooth;
            morph[i] = smoothed;
            if (i == numSamples - 1)
                uiPhase.store (phase, std::memory_order_relaxed);
        }
    }

    void quantize (int stepsPerBeat, bool glide)
    {
        const int beats = recordedBeats.load (std::memory_order_relaxed);
        if (beats <= 0)
            return;
        if (raw.size() < values.size())
            raw = values;
        const int ticks = beats * kTicksPerBeat;
        if (stepsPerBeat <= 0)
        {
            std::copy (raw.begin(), raw.begin() + ticks, values.begin());
            return;
        }
        const int stepTicks = juce::jmax (1, kTicksPerBeat / stepsPerBeat);
        const int steps = ticks / stepTicks;
        std::vector<float> level ((size_t) steps, 0.0f);
        for (int st = 0; st < steps; ++st)
        {
            double sum = 0.0;
            for (int t = 0; t < stepTicks; ++t)
                sum += raw[(size_t) (st * stepTicks + t)];
            level[(size_t) st] = (float) (sum / stepTicks);
        }
        for (int st = 0; st < steps; ++st)
        {
            const float a = level[(size_t) st];
            const float b = level[(size_t) ((st + 1) % steps)];
            for (int t = 0; t < stepTicks; ++t)
                values[(size_t) (st * stepTicks + t)] = glide ? a + (b - a) * (float) t / (float) stepTicks : a;
        }
    }
    void keepRaw() { raw = values; }
    std::vector<float> snapshot() const { return std::vector<float> (values.begin(), values.begin() + (size_t) (juce::jmax (1, beatsRecorded()) * kTicksPerBeat)); }
    bool load (const std::vector<float>& ticks, int beats)
    {
        if (beats <= 0 || beats > kMaxBeats || ticks.size() < (size_t) (beats * kTicksPerBeat))
            return false;
        stop();
        std::copy (ticks.begin(), ticks.begin() + (size_t) (beats * kTicksPerBeat), values.begin());
        raw = values;
        recordedBeats.store (beats, std::memory_order_relaxed);
        startBeat = 0.0;
        return true;
    }

private:
    void record (const float* morph, int from, int numSamples, double ppq, double beatsPerSample) noexcept
    {
        const int len = lengthBeats.load (std::memory_order_relaxed);
        for (int i = from; i < numSamples; ++i)
        {
            const double beat = ppq + (double) i * beatsPerSample - startBeat;
            if (beat < 0.0) continue;
            if (beat >= (double) len)
            {
                recordedBeats.store (len, std::memory_order_relaxed);
                rawDirty.store (true, std::memory_order_release);
                mode.store (Mode::Playing, std::memory_order_release);
                return;
            }
            const int tick = (int) (beat * kTicksPerBeat);
            values[(size_t) tick] = morph[i];
            uiPhase.store (beat, std::memory_order_relaxed);
        }
    }
    static constexpr float kSmooth = 1.0f - 0.9692332344763441f;
    std::vector<float> values;
    std::vector<float> raw;
    float smoothed = 0.5f;
public:
    bool takeRawDirty() noexcept { return rawDirty.exchange (false, std::memory_order_acq_rel); }
private:
    std::atomic<bool> rawDirty { false };
    std::atomic<Mode> mode { Mode::Idle };
    std::atomic<int> lengthBeats { 16 };
    std::atomic<int> recordedBeats { 0 };
    std::atomic<double> uiPhase { 0.0 };
    double startBeat = 0.0;
};
}
