#pragma once
#include "FuncGenPatterns.h"
#include <cmath>
#include <cstdint>

namespace trench
{

struct MovementTransport
{
    double bpm = 120.0;
    double ppq = -1.0;     // block-start PPQ; < 0 = unknown
    bool playing = false;
    double beatsPerBar = 4.0;
};

class Movement
{
public:
    static constexpr int kGrowlIndex = kNumFuncGenPatterns + 1;
    enum Transition { PatternTransition = 0, StepTransition = 1, GlideTransition = 2 };
    // One function-generator step per 16th note; a 16-step pattern is one bar.
    static constexpr double kStepBeats = 0.25;

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

    void render (float* morphBuffer, int numSamples, float baseMorph,
                 const MovementTransport& t, int presetIndex,
                 int transition = PatternTransition, int length = 0, int playback = 0,
                 std::uint64_t restart = 0, const FuncGenPattern* custom = nullptr,
                 int customLoopSteps = 0) noexcept
    {
        const float base = clamp01 (baseMorph);
        // OWNERSHIP (X3_MOVEMENT_SPEC.md): this renderer owns WHAT the
        // trajectory is — base wheel plus function generator, raw and
        // clamped. HOW motion arrives at the filter (the morph one-pole once
        // per 32-sample tick, the kernel ramp, the one-block lag) is the
        // engine's X3 movement path, applied to the complete summed Morph
        // destination. No base ramp, no hand smoother, no output slew here.
        if (custom != nullptr) presetIndex = 1000;
        const bool bank = custom != nullptr || (presetIndex >= 1 && presetIndex <= kNumFuncGenPatterns);
        if (! bank || numSamples <= 0)
        {
            // OFF (and GROWL, which the engine renders): the pattern
            // contribution is exactly zero — the buffer IS the wheel.
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
        const int durationChoice = length < 0 ? 0 : length > 4 ? 4 : length;

        const double bpm = t.bpm > 1.0e-6 ? t.bpm : 120.0;
        const double barBeats = t.beatsPerBar > 0.0 ? t.beatsPerBar : 4.0;
        // A gesture's length is its own cycle: the steps it walks times the
        // time each step takes. A one-shot walks the same cycle once, so its
        // route spans the cycle rather than one interval less than it.
        const int loopSteps = customLoopSteps > 0 ? customLoopSteps
                             : (p.direction == 2 ? 2 * p.steps - 2 : p.steps);
        const int stepCount = intervals > 0 ? intervals : 1;
        const double stepBeats = p.stepBeats > 0.0 ? p.stepBeats : kStepBeats;
        const double cycleBeats = (double) (loopSteps > 0 ? loopSteps : 1) * stepBeats;
        const double patternBeats = durationChoice > 0
            ? (double) (1 << (durationChoice - 1)) * barBeats / (double) stepCount
            : cycleBeats / (double) stepCount;
        const double beatsPerSample = bpm / 60.0 / sr;
        const bool hostLocked = t.playing && t.ppq >= 0.0;
        // Step Rate (the X3's own control): a movement that carries one runs
        // its steps on a seconds clock, free of the host tempo. A chosen bar
        // length is a BPM lock and wins; no rate leaves the beat grid.
        const bool freeRate = p.rateHz > 0.0 && durationChoice == 0;

        // Anchor law: the wheel is the key, so every pattern starts at step 0
        // where you put it. It re-anchors
        // on the transport play edge, on preset selection, and when the
        // operator finishes placing the MORPH wheel. The random/brownian walk
        // re-seeds on selection only.
        const bool playEdge = t.playing && ! prevPlaying;
        const bool selected = presetIndex != prevPreset;
        const double startBeats = hostLocked ? t.ppq : freeBeats;
        if (playEdge || selected || length != prevLength || playback != prevPlayback || restart != prevRestart)
        {
            anchorBeats = startBeats;
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
            morphBuffer[i] = (v + 1.0f) * 0.5f * (1.0f - base);
        }

        updateClock (t, numSamples);
    }

private:
    static float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    /// The original bank's direction grammar.
    /// Deterministic modes map the absolute step counter straight to a table
    /// position; random/brownian advance a seeded walk one step per counter
    /// tick, so a stalled transport holds and a block boundary never skips.
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
            case 0: // forward
                pos = wrap (g, n);
                next = wrap (g + 1, n);
                return;
            case 1: // reverse
                pos = n - 1 - wrap (g, n);
                next = n - 1 - wrap (g + 1, n);
                return;
            case 2: // pendulum
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
            case 5: // one-shot: walk once, hold the end cell
                pos = (int) (m < n ? m : n - 1);
                next = (int) (m + 1 < n ? m + 1 : n - 1);
                return;
            case 3: // random: any step
            case 4: // brownian: adjacent step, bounce off the ends
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
        return (float) (rngState >> 8) * (1.0f / 8388608.0f) - 1.0f; // [-1,1)
    }
    // While host-locked the free clock shadows the grid, so a transport stop
    // free-runs onward from where the song left off instead of jumping.
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
    double freeBeats = 0.0;    // stopped-transport audition clock, in beats
    double freeSeconds = 0.0;  // the free step-rate clock, in seconds
    double anchorBeats = 0.0;  // where the current pattern started
    double anchorSeconds = 0.0;
    bool prevPlaying = false;
    int prevPreset = -1;
    int prevLength = -1, prevPlayback = -1;
    std::uint64_t prevRestart = 0;
    // The random/brownian walk — the bank's one necessary state.
    static constexpr std::int64_t kWalkCatchUpLimit = 64;
    std::int64_t walkCycle = -1;
    int walkPos = 0, walkNext = 0;
    std::uint32_t rngState = 0x54524E43u;   // 'TRNC' — deterministic seed
};

} // namespace trench
