#pragma once
#include "FuncGenPatterns.h"
#include <cmath>
#include <cstdint>

namespace trench
{

struct MovementTransport
{
    double bpm = 120.0;
    double ppq = -1.0;
    bool playing = false;
    double beatsPerBar = 4.0;
};

class Movement
{
public:
    static constexpr int kGrowlIndex = kNumFuncGenPatterns + 1;
    enum Transition { PatternTransition = 0, StepTransition = 1, GlideTransition = 2 };
    static constexpr int kRateChoices = 5;
    static constexpr int kDefaultRate = 2;

    static double rateBars (int choice) noexcept
    {
        return 0.25 * (double) (1 << (choice < 0 ? 0 : choice >= kRateChoices ? kRateChoices - 1 : choice));
    }

    static double authoredBars (const FuncGenPattern& p, int loopSteps = 0) noexcept
    {
        const int cells = loopSteps > 0 ? loopSteps : (p.direction == 2 ? 2 * p.steps - 2 : p.steps);
        return (double) (cells > 0 ? cells : 1) * (p.stepBeats > 0.0 ? p.stepBeats : 0.25) / 4.0;
    }

    static int authoredLengthChoice (const FuncGenPattern& p, int loopSteps = 0) noexcept
    {
        const double octaves = std::log2 (authoredBars (p, loopSteps) / rateBars (0));
        const int choice = (int) std::floor (octaves + 0.5);
        return choice < 0 ? 0 : choice >= kRateChoices ? kRateChoices - 1 : choice;
    }

    static void travel (const FuncGenPattern& p, float& low, float& high) noexcept
    {
        low = high = p.steps > 0 ? p.values[0] : 0.0f;
        for (int i = 1; i < p.steps; ++i)
        {
            low = p.values[i] < low ? p.values[i] : low;
            high = p.values[i] > high ? p.values[i] : high;
        }
    }

    void setSwing (double amount) noexcept { swing = amount < 0.0 ? 0.0 : amount > 1.0 ? 1.0 : amount; }

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        freeBeats = 0.0;
        freeSeconds = 0.0;
        anchorBeats = 0.0;
        anchorSeconds = 0.0;
        prevPlaying = false;
        prevPreset = -1;
        prevLength = prevPlayback = -1;
        prevRestart = 0;
        walkCycle = -1;
    }

    void render (float* morphBuffer, int numSamples,
                 const MovementTransport& t, int presetIndex,
                 int transition = PatternTransition, int length = kDefaultRate, int playback = 0,
                 std::uint64_t restart = 0, const FuncGenPattern* custom = nullptr,
                 int customLoopSteps = 0) noexcept
    {
        if (custom != nullptr) presetIndex = 1000;
        const bool bank = custom != nullptr || (presetIndex >= 1 && presetIndex <= kNumFuncGenPatterns);
        if (! bank || numSamples <= 0)
        {
            for (int i = 0; i < numSamples; ++i)
                morphBuffer[i] = 0.0f;
            prevPreset = presetIndex;
            prevPlaying = t.playing;
            updateClock (t, numSamples);
            return;
        }

        FuncGenPattern p = custom != nullptr ? *custom : kFuncGenPatterns[presetIndex - 1];
        const bool once = playback == 2 || (playback == 0 && p.direction == 5);
        if (p.direction == 5 && ! once) p.direction = 0;
        const int intervals = p.direction == 2 ? 2 * p.steps - 2
                            : once ? p.steps - 1 : p.steps;
        const double bpm = t.bpm > 1.0e-6 ? t.bpm : 120.0;
        const double barBeats = t.beatsPerBar > 0.0 ? t.beatsPerBar : 4.0;
        const int loopSteps = customLoopSteps > 0 ? customLoopSteps
                             : (p.direction == 2 ? 2 * p.steps - 2 : p.steps);
        const int stepCount = intervals > 0 ? intervals : 1;
        const double patternBeats = rateBars (length) * barBeats / (double) stepCount;
        const double beatsPerSample = bpm / 60.0 / sr;
        const bool hostLocked = t.playing && t.ppq >= 0.0;
        const bool freeRate = p.rateHz > 0.0;

        const bool playEdge = t.playing && ! prevPlaying;
        const bool selected = presetIndex != prevPreset;
        const double startBeats = hostLocked ? t.ppq : freeBeats;
        const double gestureBeats = rateBars (length) * barBeats;
        if (playEdge || selected || length != prevLength || playback != prevPlayback || restart != prevRestart)
        {
            const bool onGrid = hostLocked && ! once && ! freeRate && restart == prevRestart && gestureBeats > 0.0;
            anchorBeats = onGrid ? std::floor (startBeats / gestureBeats) * gestureBeats : startBeats;
            anchorSeconds = freeSeconds;
        }
        if (selected)
            walkCycle = -1;
        prevPreset = presetIndex;
        prevPlaying = t.playing;
        prevLength = length;
        prevPlayback = playback;
        prevRestart = restart;

        const double clockStart = freeRate ? freeSeconds : startBeats;
        const double clockAnchor = freeRate ? anchorSeconds : anchorBeats;
        const double clockPeriod = freeRate
            ? (double) (loopSteps > 0 ? loopSteps : 1) / p.rateHz / (double) stepCount
            : patternBeats;
        const double clockPerSample = freeRate ? 1.0 / sr : beatsPerSample;

        for (int i = 0; i < numSamples; ++i)
        {
            const double clock = clockStart + (double) i * clockPerSample;
            double stepPos = (clock - clockAnchor) / clockPeriod;
            if (swing > 0.0)
            {
                const double late = 1.0 + 0.5 * swing;
                const double pair = std::floor (stepPos * 0.5);
                const double f = stepPos - 2.0 * pair;
                stepPos = 2.0 * pair + (f < late ? f / late : 1.0 + (f - late) / (2.0 - late));
            }
            if (once) stepPos = std::fmax (0.0, std::fmin ((double) intervals, stepPos));
            const std::int64_t g = (std::int64_t) std::floor (stepPos);
            const float frac = (float) (stepPos - (double) g);
            int pos, next;
            if (once && (p.direction == 0 || p.direction == 1 || p.direction == 5))
            {
                const int last = p.steps - 1;
                pos = (int) g < last ? (int) g : last;
                next = pos < last ? pos + 1 : last;
                if (p.direction == 1) { pos = last - pos; next = last - next; }
            }
            else stepPositions (p, g, pos, next);
            float v = p.values[pos];
            const bool glide = transition == GlideTransition
                            || (transition == PatternTransition && p.smooth);
            if (glide)
                v += (p.values[next] - v) * frac;
            morphBuffer[i] = v;
        }

        updateClock (t, numSamples);
    }

private:
    double swing = 0.0;
    void stepPositions (const FuncGenPattern& p, std::int64_t g,
                        int& pos, int& next) noexcept
    {
        const int n = p.steps > 1 ? p.steps : 1;
        const std::int64_t m = g < 0 ? 0 : g;
        const auto wrap = [] (std::int64_t k, int period)
        {
            const std::int64_t r = k % period;
            return (int) (r < 0 ? r + period : r);
        };
        switch (p.direction)
        {
            default:
            case 0:
                pos = wrap (g, n);
                next = wrap (g + 1, n);
                return;
            case 1:
                pos = n - 1 - wrap (g, n);
                next = n - 1 - wrap (g + 1, n);
                return;
            case 2:
            {
                const auto pend = [n, &wrap] (std::int64_t k)
                {
                    const int period = 2 * n - 2 > 0 ? 2 * n - 2 : 1;
                    const int q = wrap (k, period);
                    return q < n ? q : period - q;
                };
                pos = pend (g);
                next = pend (g + 1);
                return;
            }
            case 5:
                pos = (int) (m < n ? m : n - 1);
                next = (int) (m + 1 < n ? m + 1 : n - 1);
                return;
            case 3:
            case 4:
            {
                if (walkCycle < 0 || m - walkCycle > kWalkCatchUpLimit)
                {
                    walkCycle = m;
                    walkPos = (int) (m % n);
                    walkNext = walkStep (p, walkPos);
                }
                while (walkCycle < m)
                {
                    walkPos = walkNext;
                    walkNext = walkStep (p, walkPos);
                    ++walkCycle;
                }
                pos = walkPos;
                next = walkNext;
                return;
            }
        }
    }
    int walkStep (const FuncGenPattern& p, int pos) noexcept
    {
        const int n = p.steps > 1 ? p.steps : 1;
        if (p.direction == 3)
            return (int) ((whiteBip() * 0.5f + 0.5f) * (float) n) % n;
        const bool up = whiteBip() >= 0.0f;
        if (pos <= 0) return n > 1 ? 1 : 0;
        if (pos >= n - 1) return n > 1 ? n - 2 : 0;
        return up ? pos + 1 : pos - 1;
    }
    float whiteBip() noexcept
    {
        rngState = rngState * 1664525u + 1013904223u;
        return (float) (rngState >> 8) * (1.0f / 8388608.0f) - 1.0f;
    }
    void updateClock (const MovementTransport& t, int numSamples) noexcept
    {
        const double bpm = t.bpm > 1.0e-6 ? t.bpm : 120.0;
        const double advance = (double) numSamples * bpm / 60.0 / sr;
        if (t.playing && t.ppq >= 0.0)
            freeBeats = t.ppq + advance;
        else
            freeBeats += advance;
        freeSeconds += (double) numSamples / sr;
    }
    double sr = 48000.0;
    double freeBeats = 0.0;
    double freeSeconds = 0.0;
    double anchorBeats = 0.0;
    double anchorSeconds = 0.0;
    bool prevPlaying = false;
    int prevPreset = -1;
    int prevLength = -1, prevPlayback = -1;
    std::uint64_t prevRestart = 0;
    static constexpr std::int64_t kWalkCatchUpLimit = 64;
    std::int64_t walkCycle = -1;
    int walkPos = 0, walkNext = 0;
    std::uint32_t rngState = 0x54524E43u;
};

}
