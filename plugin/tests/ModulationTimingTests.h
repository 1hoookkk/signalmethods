#pragma once
#include "PluginProcessor.h"
#include "ui/ModulationChip.h"
#include <algorithm>
#include <vector>

inline int modulationTimingTests()
{
    int failures = 0;
    const auto check = [&] (bool ok, const char* name)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", name);
        if (! ok) ++failures;
    };
    const auto& oneShot = trench::kFuncGenPatterns[4];
    const float ending = oneShot.values[oneShot.steps - 1];
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        trench::Movement movement;
        movement.prepare (rate);
        trench::MovementTransport t;
        t.bpm = 120.0; t.playing = true; t.ppq = 10.0;
        float sample = 0.0f;
        movement.render (&sample, 1, t, 5, 0, 4, 2);
        check (sample == 0.0f, "four-bar Long Return starts at its first point");
        t.ppq = 26.0 - 2.0 / rate;
        movement.render (&sample, 1, t, 5, 0, 4, 2);
        check (sample < ending && sample > ending - 0.01f, "one-shot has not reached its ending a sample before four bars");
        t.ppq = 26.0;
        movement.render (&sample, 1, t, 5, 0, 4, 2);
        check (std::abs (sample - ending) < 1.0e-7f, "one-shot reaches its authored ending exactly at four bars");
        t.ppq = 42.0;
        movement.render (&sample, 1, t, 5, 0, 4, 2);
        check (sample == ending, "one-shot holds after completion");
        t.ppq = 10.0;
        movement.render (&sample, 1, t, 5, 0, 2, 0);
        check (sample == 0.0f, "one-shot at a 1-bar rate starts at its first point");
        t.ppq = 13.9;
        movement.render (&sample, 1, t, 5, 0, 2, 0);
        check (sample < ending, "Long Return at 1 bar is still short of its ending before the bar");
        t.ppq = 14.0;
        movement.render (&sample, 1, t, 5, 0, 2, 0);
        check (std::abs (sample - ending) < 1.0e-7f, "Long Return at 1 bar completes exactly at the bar");
        t.ppq = 10.0;
        movement.render (&sample, 1, t, 5, 0, 4, 2, 1);
        check (sample == 0.0f, "Restart replays a completed one-shot");
    }
    {
        const auto& loop = trench::kFuncGenPatterns[6];
        trench::Movement movement;
        movement.prepare (64.0);
        trench::MovementTransport t;
        t.bpm = 120.0; t.playing = true; t.ppq = 0.0; t.beatsPerBar = 3.5;
        float sample = 0.0f;
        movement.render (&sample, 1, t, 7, 0, 4, 1);
        t.ppq = 7.0;
        movement.render (&sample, 1, t, 7, 0, 4, 1);
        check (sample == loop.values[4], "four-bar loop lands on its fifth cell at two bars in 7/8");
        t.ppq = 14.0;
        movement.render (&sample, 1, t, 7, 0, 4, 1);
        check (sample == loop.values[0], "four-bar loop restarts after fourteen quarter-note beats in 7/8");
    }
    {
        const auto& arc = trench::kFuncGenPatterns[6];
        trench::Movement movement;
        movement.prepare (64.0);
        trench::MovementTransport t;
        t.bpm = 120.0; t.playing = true; t.ppq = 5.0;
        float sample = 0.0f;
        movement.render (&sample, 1, t, 7, 0, 4, 1);
        const float midway = arc.values[2] + 0.5f * (arc.values[3] - arc.values[2]);
        const bool onGrid = std::abs (sample - midway) < 1.0e-6f;
        movement.render (&sample, 1, t, 7, 0, 4, 1, 1);
        check (onGrid && sample == arc.values[0], "a loop started mid-song plays from the song's bar grid; Restart replays it from here");
        check (trench::Movement::authoredLengthChoice (trench::kFuncGenPatterns[19]) == 4
                   && trench::Movement::authoredLengthChoice (trench::kFuncGenPatterns[4]) == 2
                   && trench::Movement::authoredLengthChoice (trench::kFuncGenPatterns[0]) == 1,
               "Slow Tide is authored as 4 bars, Long Return as 1 bar, Triplet Relay as 1/2 bar");
        movement.prepare (64.0);
        t.ppq = 0.0;
        movement.render (&sample, 1, t, 7, 0, 0, 1);
        t.ppq = 0.5;
        movement.render (&sample, 1, t, 7, 0, 0, 1);
        const bool halfway = sample == arc.values[4];
        t.ppq = 1.0;
        movement.render (&sample, 1, t, 7, 0, 0, 1);
        check (halfway && sample == arc.values[0], "a 1/4-bar rate plays one whole pass per beat");
    }
    for (int preset : { 2, 4, 5, 6 })
    {
        trench::Movement whole, sliced;
        whole.prepare (64.0); sliced.prepare (64.0);
        trench::MovementTransport t;
        t.bpm = 120.0; t.playing = true; t.ppq = 0.0;
        std::vector<float> a (600), b (600);
        whole.render (a.data(), 600, t, preset, 0, 4, 2);
        for (int start = 0; start < 600; start += 37)
        {
            t.ppq = start / 32.0;
            sliced.render (b.data() + start, std::min (37, 600 - start), t, preset, 0, 4, 2);
        }
        check (std::equal (a.begin(), a.end(), b.begin(), [] (float x, float y) { return std::abs (x-y) < 1.0e-6f; }),
               "timed gesture is independent of block boundaries");
        check (a[512] == a.back(), "forward, reverse, pendulum and return gestures hold after four bars");
    }
    {
        PluginProcessor p;
        const auto layout = trench::UiLayout::defaults();
        const trench::ui::Theme theme { layout };
        trench::ui::ModulationChip chip (p.apvts, theme);
        chip.selectPattern (5); chip.selectLength (4); chip.selectPlayback (2);
        check (p.apvts.getRawParameterValue (ParamID::moveLength)->load() == 4.0f
            && p.apvts.getRawParameterValue (ParamID::movePlayback)->load() == 2.0f,
            "modulation menu reaches host timing parameters");
        check (chip.displayText() == juce::String::fromUTF8 ("Long Return \xc2\xb7 4 bars \xc2\xb7 lands"), "modulation status shows the selected pattern, its length and whether it lands");
        juce::MemoryBlock state;
        p.getStateInformation (state);
        PluginProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.apvts.getRawParameterValue (ParamID::moveLength)->load() == 4.0f
            && restored.apvts.getRawParameterValue (ParamID::movePlayback)->load() == 2.0f,
            "modulation timing survives project recall");
        auto old = p.apvts.copyState();
        old.removeChild (old.getChildWithProperty ("id", ParamID::moveLength), nullptr);
        old.removeChild (old.getChildWithProperty ("id", ParamID::movePlayback), nullptr);
        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary (*old.createXml(), legacy);
        restored.setStateInformation (legacy.getData(), (int) legacy.getSize());
        check (restored.apvts.getRawParameterValue (ParamID::moveLength)->load() == (float) trench::Movement::kDefaultRate
            && restored.apvts.getRawParameterValue (ParamID::movePlayback)->load() == 0.0f,
            "older projects open at the default 1-bar rate");
    }
    {
        struct Clock final : juce::AudioPlayHead
        {
            double ppq = 0.0;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setIsPlaying (true); info.setBpm (120.0); info.setPpqPosition (ppq);
                info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 7, 8 });
                return info;
            }
        } clock;
        PluginProcessor p;
        p.setPlayHead (&clock);
        p.setPlayConfigDetails (2, 2, 48000.0, 1);
        p.prepareToPlay (48000.0, 1);
        p.setEditorOpen (true);
        const auto set = [&] (const char* id, float value)
        {
            auto* parameter = p.apvts.getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set (ParamID::morph, 0.0f); set (ParamID::movePreset, 5.0f);
        set (ParamID::moveLength, 4.0f); set (ParamID::movePlayback, 2.0f);
        juce::AudioBuffer<float> buffer (2, 1);
        juce::MidiBuffer midi;
        buffer.clear(); p.processBlock (buffer, midi);
        clock.ppq = 14.0;
        buffer.clear(); p.processBlock (buffer, midi);
        check (std::abs (p.getEffectiveMorphForUi() - ending) < 1.0e-6f,
               "processor connects host meter and menu timing to the actual Morph trajectory");
        p.restartMovement();
        buffer.clear(); p.processBlock (buffer, midi);
        check (p.getEffectiveMorphForUi() == 0.0f, "processor Restart reaches the gesture renderer");
        p.setPlayHead (nullptr);
    }
    {
        struct Clock final : juce::AudioPlayHead
        {
            double ppq = 1234.5; bool playing = true;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setIsPlaying (playing); info.setBpm (140.0); info.setPpqPosition (ppq);
                info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
                return info;
            }
        } clock;
        PluginProcessor p;
        p.setPlayHead (&clock);
        p.setPlayConfigDetails (2, 2, 44100.0, 512);
        p.prepareToPlay (44100.0, 512);
        p.setEditorOpen (true);
        const auto set = [&] (const char* id, float value)
        {
            auto* parameter = p.apvts.getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set (ParamID::morph, 0.108f); set (ParamID::movePreset, 9.0f); set (ParamID::moveLength, 3.0f); set (ParamID::movePlayback, 0.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        float low = 1.0f, high = 0.0f;
        for (int block = 0; block < 200; ++block)
        {
            for (int i = 0; i < 512; ++i) { const float s = 0.1f * (float) std::sin (0.05 * (block * 512 + i)); buffer.setSample (0, i, s); buffer.setSample (1, i, s); }
            p.processBlock (buffer, midi);
            low = std::min (low, p.getEffectiveMorphForUi()); high = std::max (high, p.getEffectiveMorphForUi());
            clock.ppq += 512.0 * 140.0 / 60.0 / 44100.0;
        }
        std::printf ("      Relay Teeth 2 bars from bar 309 at 140 BPM: effective Morph %g .. %g, modulated=%d\n", low, high, (int) p.isMorphModulatedForUi());
        check (high - low > 0.5f && p.isMorphModulatedForUi(), "a movement runs from a mid-song transport with 512-sample blocks");
        clock.playing = false;
        low = 1.0f; high = 0.0f;
        for (int block = 0; block < 200; ++block)
        {
            buffer.clear(); p.processBlock (buffer, midi);
            low = std::min (low, p.getEffectiveMorphForUi()); high = std::max (high, p.getEffectiveMorphForUi());
        }
        check (high - low > 0.5f, "a movement keeps running on its own clock when the transport stops");
        p.setPlayHead (nullptr);
    }
    return failures;
}
