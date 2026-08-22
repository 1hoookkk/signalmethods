#pragma once
#include "FuncGenPatterns.h"
#include <cmath>
#include <cstdint>

// MOVEMENT — the audio-rate Morph renderer.
//
// THE BANK IS THE MACHINE'S OWN (Tyson 2026-08-15 "utilise the func gens not
// what we have currently"): all 56 E-mu Function Generator patterns, played
// with the function generator's own attributes — direction (forward, reverse,
// pendulum, random, brownian, one-shot), smooth, step count — exactly as
// baked in FuncGenPatterns.h. This supersedes the nine hand-curated phrases
// of 2026-08-10 (EIGHTHS … CORNERS, git history holds them); the six
// earlier-curated picks lead the bank under their probation names.
//
// The renderer writes ONE Morph position per SAMPLE, raw:
//
//     morph[n] = clamp (baseMorph + pattern[n] * kTravel, 0, 1)
//
// FOLLOW's contribution is added inside the engine by the one authoritative
// detector — it is not this renderer's business. No smoothing anywhere in
// this renderer: the recovered X3 law smooths the COMPLETE summed Morph
// destination (trajectory + FOLLOW + GROWL) with one one-pole, once per
// 32-sample control tick, inside the engine's X3 movement path
// (X3_MOVEMENT_SPEC.md, FUN_1802c0430). A second smoother here would put
// the pattern through two different laws depending on where it was summed.
// A smooth pattern interpolates its authored cells, a one-shot reaches its
// final authored value and stays there, and a step pattern changes on the
// exact sample boundary; the engine's block-rate one-pole and kernel ramp
// are what keep that edge from landing as a click.
//
// Timing law: ONE PATTERN STEP PER 16TH NOTE (kStepBeats). While the host
// plays, every sample's step and phase derive from the block-start PPQ, the
// BPM and the sample index — the pattern belongs to the grid, bit-exactly,
// at any block size. While the host is stopped the pattern free-runs at the
// last known tempo for live-input audition, and the next play edge returns
// it to the host grid. Deterministic directions (forward/reverse/pendulum)
// are stateless functions of the absolute step counter; random and brownian
// are the one necessary walk state, re-seeded on preset selection.
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
    // One function-generator step per 16th note; a 16-step pattern is one bar.
    static constexpr double kStepBeats = 0.25;
    // The verdicted default throw (the old DEPTH default) — part of the
    // record, no knob (Tyson 2026-08-10 "it's confusing").
    static constexpr float kTravel = 0.5f;

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        freeBeats = 0.0;
        anchorBeats = 0.0;
        prevPlaying = false;
        prevPreset = -1;
        walkCycle = -1;
    }

    // presetIndex: 0 = OFF, 1..kNumFuncGenPatterns = the E-mu bank,
    // kNumFuncGenPatterns + 1 = GROWL (rendered inside the ENGINE, where the
    // pitch lives — this renderer passes the base wheel through untouched),
    // kNumFuncGenPatterns + 2 = LIVE (the workstation phrase, when fed).
    static constexpr int kGrowlIndex = kNumFuncGenPatterns + 1;
    static constexpr double kDivisionBeats[7] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0 };
    static double stepBeatsFor (int division) noexcept
    {
        return division >= 0 && division < 7 ? kDivisionBeats[division] : kStepBeats;
    }
    void render (float* morphBuffer, int numSamples, float baseMorph,
                 const MovementTransport& t, int presetIndex,
                 const FuncGenPattern* livePhrase, double stepBeats = kStepBeats) noexcept
    {
        const float base = clamp01 (baseMorph);
        // OWNERSHIP (X3_MOVEMENT_SPEC.md): this renderer owns WHAT the
        // trajectory is — base wheel plus function generator, raw and
        // clamped. HOW motion arrives at the filter (the morph one-pole once
        // per 32-sample tick, the kernel ramp, the one-block lag) is the
        // engine's X3 movement path, applied to the complete summed Morph
        // destination. No base ramp, no hand smoother, no output slew here.
        const bool live = presetIndex == kNumFuncGenPatterns + 2 && livePhrase != nullptr;
        const bool bank = presetIndex >= 1 && presetIndex <= kNumFuncGenPatterns;
        if ((! bank && ! live) || numSamples <= 0)
        {
            // OFF (and GROWL, which the engine renders): the pattern
            // contribution is exactly zero — the buffer IS the wheel.
            for (int i = 0; i < numSamples; ++i)
                morphBuffer[i] = base;
            prevPreset = presetIndex;
            prevPlaying = t.playing;
            updateClock (t, numSamples);
            return;
        }

        const FuncGenPattern& p = live ? *livePhrase
                                       : kFuncGenPatterns[presetIndex - 1];
        const bool oneShot = p.direction == 5;

        const double bpm = t.bpm > 1.0e-6 ? t.bpm : 120.0;
        const double beatsPerSample = bpm / 60.0 / sr;
        const bool hostLocked = t.playing && t.ppq >= 0.0;

        // Reset behaviour: a one-shot re-arms on the transport play edge and
        // on preset selection; cyclic deterministic patterns belong to the
        // absolute grid; the random/brownian walk re-seeds on selection.
        const bool playEdge = t.playing && ! prevPlaying;
        const bool selected = presetIndex != prevPreset;
        const double startBeats = hostLocked ? t.ppq : freeBeats;
        if (oneShot && (playEdge || selected))
            anchorBeats = startBeats;
        if (selected)
            walkCycle = -1;
        prevPreset = presetIndex;
        prevPlaying = t.playing;

        for (int i = 0; i < numSamples; ++i)
        {
            const double beats = startBeats + (double) i * beatsPerSample;
            const double stepPos = (oneShot ? beats - anchorBeats : beats) / stepBeats;
            const std::int64_t g = (std::int64_t) std::floor (stepPos);
            const float frac = (float) (stepPos - (double) g);
            int pos, next;
            stepPositions (p, g, pos, next);
            float v = p.values[pos];
            if (p.smooth)
                v += (p.values[next] - v) * frac;
            morphBuffer[i] = clamp01 (base + v * kTravel);
        }

        updateClock (t, numSamples);
    }

private:
    static float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    /// The function generator's own directions (interpreter lifted from the
    /// retired MorphMod, the reference implementation of the 56-pattern law).
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
                if (walkCycle < 0)
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
    double anchorBeats = 0.0;  // where the current one-shot began
    bool prevPlaying = false;
    int prevPreset = -1;
    // The random/brownian walk — the bank's one necessary state.
    std::int64_t walkCycle = -1;
    int walkPos = 0, walkNext = 0;
    std::uint32_t rngState = 0x54524E43u;   // 'TRNC' — deterministic seed
};

} // namespace trench
