#pragma once
#include "FuncGenPatterns.h"
#include <cmath>
#include <cstdint>

// MOVEMENT — the audio-rate Morph renderer.
//
// THE BANK IS ORIGINAL: the shipping patterns are independently authored in
// plugin/filters/original_patterns.json and baked by
// tools/bake_original_funcgens.py. Direction (forward, reverse, pendulum,
// random, brownian, one-shot), smoothing and step count remain properties of
// the pattern grammar; no factory template data or vendor pattern name enters
// the binary.
//
// The renderer writes ONE Morph position per SAMPLE, raw:
//
//     span     = pattern[n] >= 0 ? 1 - baseMorph : baseMorph
//     morph[n] = clamp (baseMorph + pattern[n] * span, 0, 1)
//
// THE PATTERN REACHES BOTH WALLS FROM WHEREVER THE WHEEL RESTS (Tyson
// 2026-08-25 "if I put it at 90% morph it should go back to the lowest
// point"): each side of the pattern is scaled into the room the wheel
// actually has on that side, so a full step lands exactly on 0 or on 1 at
// any base. At base 0.5 that is the old fixed +-0.5 throw exactly; the old
// law was the same +-0.5 everywhere, which at base 0.9 could only fall to
// 0.4 and flattened every rise against the ceiling.
//
// FOLLOW's contribution is added inside the engine by the one authoritative
// detector — it is not this renderer's business. No smoothing anywhere in
// this renderer: the recovered X3 law smooths the COMPLETE summed Morph
// destination (trajectory + FOLLOW + GROWL) with one one-pole, once per
// 32-sample control tick, inside the engine's X3 movement path
// (X3_MOVEMENT_SPEC.md, FUN_1802c0430). A second smoother here would put
// the pattern through two different laws depending on where it was summed.
// PATTERN uses the authored cell law; STEP holds cells and GLIDE linearly
// interpolates their Morph targets. A one-shot reaches its final authored
// value and stays there. The engine's one authoritative one-pole and kernel ramp
// are what keep that edge from landing as a click.
//
// Timing law: a bank pattern plays at the rate ITS TEMPLATE was authored at —
// steps per beat when the template was tempo-synced, free Hz when it was not.
// The preset defines its own rhythm; no division control feeds it. While the host
// plays, every sample's step and phase derive from the block-start PPQ, the
// BPM and the sample index — the pattern belongs to the grid from where it
// was last anchored, bit-exactly, at any block size. While the host is stopped
// the pattern free-runs at the last known tempo for live-input audition, and
// the next play edge returns it to the host grid. Deterministic directions
// (forward/reverse/pendulum) are stateless functions of that step counter;
// random and brownian are the one necessary walk state, re-seeded on preset
// selection.
//
// A pattern's Morph levels are wheel positions, never pitches. KEY is what
// turns the decoded resonances into scale-constrained movement.

namespace trench
{

struct MovementTransport
{
    double bpm = 120.0;
    double ppq = -1.0;     // block-start PPQ; < 0 = unknown
    bool playing = false;
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
        anchorBeats = 0.0;
        retriggerRequest = false;
        prevPlaying = false;
        prevPreset = -1;
        walkCycle = -1;
    }

    /// Restart the pattern from step 0 at the next block — the MORPH wheel is
    /// the key. Audio thread only; the processor relays the gesture through an
    /// atomic.
    void retrigger() noexcept { retriggerRequest = true; }
    void render (float* morphBuffer, int numSamples, float baseMorph,
                 const MovementTransport& t, int presetIndex,
                 int transition = PatternTransition) noexcept
    {
        const float base = clamp01 (baseMorph);
        // OWNERSHIP (X3_MOVEMENT_SPEC.md): this renderer owns WHAT the
        // trajectory is — base wheel plus function generator, raw and
        // clamped. HOW motion arrives at the filter (the morph one-pole once
        // per 32-sample tick, the kernel ramp, the one-block lag) is the
        // engine's X3 movement path, applied to the complete summed Morph
        // destination. No base ramp, no hand smoother, no output slew here.
        const bool bank = presetIndex >= 1 && presetIndex <= kNumFuncGenPatterns;
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

        const FuncGenPattern& p = kFuncGenPatterns[presetIndex - 1];

        const double bpm = t.bpm > 1.0e-6 ? t.bpm : 120.0;
        const double patternBeats = bankStepBeats (p, bpm, kStepBeats);
        const double beatsPerSample = bpm / 60.0 / sr;
        const bool hostLocked = t.playing && t.ppq >= 0.0;

        // Anchor law: the wheel is the key, so every pattern starts at step 0
        // where you put it. It re-anchors
        // on the transport play edge, on preset selection, and when the
        // operator finishes placing the MORPH wheel. The random/brownian walk
        // re-seeds on selection only.
        const bool playEdge = t.playing && ! prevPlaying;
        const bool selected = presetIndex != prevPreset;
        const double startBeats = hostLocked ? t.ppq : freeBeats;
        if (playEdge || selected || retriggerRequest)
            anchorBeats = startBeats;
        retriggerRequest = false;
        if (selected)
            walkCycle = -1;
        prevPreset = presetIndex;
        prevPlaying = t.playing;

        for (int i = 0; i < numSamples; ++i)
        {
            const double beats = startBeats + (double) i * beatsPerSample;
            const double stepPos = (beats - anchorBeats) / patternBeats;
            const std::int64_t g = (std::int64_t) std::floor (stepPos);
            const float frac = (float) (stepPos - (double) g);
            int pos, next;
            stepPositions (p, g, pos, next);
            float v = p.values[pos];
            const bool glide = transition == GlideTransition
                            || (transition == PatternTransition && p.smooth);
            if (glide)
                v += (p.values[next] - v) * frac;
            const float span = v >= 0.0f ? 1.0f - base : base;
            morphBuffer[i] = v * span;
        }

        updateClock (t, numSamples);
    }

private:
    static float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    static double bankStepBeats (const FuncGenPattern& p, double bpm, double fallback) noexcept
    {
        if (p.rateHz > 0.0)
        {
            const double b = bpm / (60.0 * p.rateHz);
            return b < 1.0 / 64.0 ? 1.0 / 64.0 : (b > 4.0 ? 4.0 : b);
        }
        return p.stepBeats > 0.0 ? p.stepBeats : fallback;
    }

    /// The original bank's direction grammar.
    /// Deterministic modes map the absolute step counter straight to a table
    /// position; random/brownian advance a seeded walk one step per counter
    /// tick, so a stalled transport holds and a block boundary never skips.
    void stepPositions (const FuncGenPattern& p, std::int64_t g,
                        int& pos, int& next) noexcept
    {
        const int n = p.steps > 1 ? p.steps : 1;
        const std::int64_t m = g < 0 ? 0 : g;
        switch (p.direction)
        {
            default:
            case 0: // forward
                pos = (int) (m % n);
                next = (int) ((m + 1) % n);
                return;
            case 1: // reverse
                pos = n - 1 - (int) (m % n);
                next = n - 1 - (int) ((m + 1) % n);
                return;
            case 2: // pendulum
            {
                const auto pend = [n] (std::int64_t k)
                {
                    const int period = 2 * n - 2 > 0 ? 2 * n - 2 : 1;
                    const int q = (int) (k % period);
                    return q < n ? q : period - q;
                };
                pos = pend (m);
                next = pend (m + 1);
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
    }
    double sr = 48000.0;
    double freeBeats = 0.0;    // stopped-transport audition clock, in beats
    double anchorBeats = 0.0;  // where the current pattern started
    bool retriggerRequest = false;
    bool prevPlaying = false;
    int prevPreset = -1;
    // The random/brownian walk — the bank's one necessary state.
    static constexpr std::int64_t kWalkCatchUpLimit = 64;
    std::int64_t walkCycle = -1;
    int walkPos = 0, walkNext = 0;
    std::uint32_t rngState = 0x54524E43u;   // 'TRNC' — deterministic seed
};

} // namespace trench
