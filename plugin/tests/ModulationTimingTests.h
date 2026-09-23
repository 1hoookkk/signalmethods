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
        movement.render (&sample, 1, 0.0f, t, 5, 0, 3, 2);
        check (sample == 0.0f, "four-bar Long Return starts at its first point");
        t.ppq = 26.0 - 2.0 / rate;
        movement.render (&sample, 1, 0.0f, t, 5, 0, 3, 2);
        check (sample < ending && sample > ending - 0.01f, "one-shot has not reached its ending a sample before four bars");
        t.ppq = 26.0;
        movement.render (&sample, 1, 0.0f, t, 5, 0, 3, 2);
        check (std::abs (sample - ending) < 1.0e-7f, "one-shot reaches its authored ending exactly at four bars");
        t.ppq = 42.0;
        movement.render (&sample, 1, 0.0f, t, 5, 0, 3, 2);
        check (sample == ending, "one-shot holds after completion");
        t.ppq = 10.0;
        movement.render (&sample, 1, 0.0f, t, 5, 0, 0, 0);
        check (sample == 0.0f, "one-shot at its own timing starts at its first point");
        t.ppq = 13.9;
        movement.render (&sample, 1, 0.0f, t, 5, 0, 0, 0);
        check (sample < ending, "Long Return is still short of its ending before its own one-bar cycle");
        t.ppq = 14.0;
        movement.render (&sample, 1, 0.0f, t, 5, 0, 0, 0);
        check (std::abs (sample - ending) < 1.0e-7f, "Long Return completes its own one-bar cycle exactly at the bar");
        t.ppq = 10.0;
        movement.render (&sample, 1, 0.0f, t, 5, 0, 3, 2, 1);
        check (sample == 0.0f, "Restart replays a completed one-shot");
    }
    {
        const auto& loop = trench::kFuncGenPatterns[6];
        trench::Movement movement;
        movement.prepare (64.0);
        trench::MovementTransport t;
        t.bpm = 120.0; t.playing = true; t.ppq = 0.0; t.beatsPerBar = 3.5;
        float sample = 0.0f;
        movement.render (&sample, 1, 0.5f, t, 7, 0, 3, 1);
        t.ppq = 7.0;
        movement.render (&sample, 1, 0.5f, t, 7, 0, 3, 1);
        check (sample == loop.values[4], "four-bar loop lands on its fifth cell at two bars in 7/8");
        t.ppq = 14.0;
        movement.render (&sample, 1, 0.5f, t, 7, 0, 3, 1);
        check (sample == loop.values[0], "four-bar loop restarts after fourteen quarter-note beats in 7/8");
    }
    for (int preset : { 2, 4, 5, 6 })
    {
        trench::Movement whole, sliced;
        whole.prepare (64.0); sliced.prepare (64.0);
        trench::MovementTransport t;
        t.bpm = 120.0; t.playing = true; t.ppq = 0.0;
        std::vector<float> a (600), b (600);
        whole.render (a.data(), 600, 0.25f, t, preset, 0, 3, 2);
        for (int start = 0; start < 600; start += 37)
        {
            t.ppq = start / 32.0;
            sliced.render (b.data() + start, std::min (37, 600 - start), 0.25f, t, preset, 0, 3, 2);
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
        chip.selectPattern (5); chip.selectLength (3); chip.selectPlayback (2);
        check (p.apvts.getRawParameterValue (ParamID::moveLength)->load() == 3.0f
            && p.apvts.getRawParameterValue (ParamID::movePlayback)->load() == 2.0f,
            "modulation menu reaches host timing parameters");
        check (chip.displayText() == juce::String::fromUTF8 ("Long Return \xc2\xb7 4 bars \xc2\xb7 Once"), "modulation status shows the selected duration and playback");
        juce::MemoryBlock state;
        p.getStateInformation (state);
        PluginProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.apvts.getRawParameterValue (ParamID::moveLength)->load() == 3.0f
            && restored.apvts.getRawParameterValue (ParamID::movePlayback)->load() == 2.0f,
            "modulation timing survives project recall");
        auto old = p.apvts.copyState();
        old.removeChild (old.getChildWithProperty ("id", ParamID::moveLength), nullptr);
        old.removeChild (old.getChildWithProperty ("id", ParamID::movePlayback), nullptr);
        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary (*old.createXml(), legacy);
        restored.setStateInformation (legacy.getData(), (int) legacy.getSize());
        check (restored.apvts.getRawParameterValue (ParamID::moveLength)->load() == 0.0f
            && restored.apvts.getRawParameterValue (ParamID::movePlayback)->load() == 0.0f,
            "older projects retain authored preset timing");
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
        set (ParamID::moveLength, 3.0f); set (ParamID::movePlayback, 2.0f);
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
    return failures;
}
