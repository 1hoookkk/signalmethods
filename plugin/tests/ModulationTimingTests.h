#pragma once
#include "PluginProcessor.h"
#include "ui/ModulationChip.h"
#include "parameters/CurveMap.h"
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
        check (chip.displayText() == "Long Return", "modulation status shows the selected movement by name");
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
            double ppq = 0.0;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setIsPlaying (true); info.setBpm (120.0); info.setPpqPosition (ppq);
                info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
                return info;
            }
        } clock;
        PluginProcessor p;
        p.setPlayHead (&clock);
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        p.setEditorOpen (true);
        const auto set = [&] (const char* id, float value)
        {
            auto* parameter = p.apvts.getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        set (ParamID::morph, 0.0f); set (ParamID::movePreset, 5.0f);
        set (ParamID::moveLength, 4.0f); set (ParamID::movePlayback, 2.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        for (int block = 0; block < 1200; ++block)
        {
            buffer.clear(); p.processBlock (buffer, midi);
            clock.ppq += 512.0 * 2.0 / 48000.0;
        }
        const float ended = p.getEffectiveMorphForUi();
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 256);
        buffer.clear(); p.processBlock (buffer, midi);
        const float restarted = p.getEffectiveMorphForUi();
        const float expected = trench::kFuncGenPatterns[4].values[0]
            + (trench::kFuncGenPatterns[4].values[1] - trench::kFuncGenPatterns[4].values[0]) * (float) (256.0 * 2.0 / 48000.0 / (16.0 / 15.0));
        std::printf ("      note-on restart: held %g, after a note at sample 256 %g (expected %g)\n", ended, restarted, expected);
        check (p.acceptsMidi() && std::abs (ended - ending) < 1.0e-6f && std::abs (restarted - expected) < 2.0e-3f,
               "a MIDI note replays a landed movement from the note's own sample");
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
    {
        struct Clock final : juce::AudioPlayHead
        {
            double ppq = 0.0;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setIsPlaying (true); info.setBpm (120.0); info.setPpqPosition (ppq);
                info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
                return info;
            }
        } clock;
        PluginProcessor p;
        p.setPlayHead (&clock);
        p.setPlayConfigDetails (2, 2, 48000.0, 500);
        p.prepareToPlay (48000.0, 500);
        p.setEditorOpen (true);
        const auto set = [&] (const char* id, float value)
        {
            auto* parameter = p.apvts.getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        juce::AudioBuffer<float> buffer (2, 500);
        juce::MidiBuffer midi;
        const auto run = [&] (int blocks, std::vector<float>* morph = nullptr, std::vector<float>* q = nullptr)
        {
            for (int b = 0; b < blocks; ++b)
            {
                buffer.clear();
                p.processBlock (buffer, midi);
                clock.ppq += 500.0 * 120.0 / 60.0 / 48000.0;
                if (morph != nullptr) morph->push_back (p.getEffectiveMorphForUi());
                if (q != nullptr) q->push_back (p.getEffectiveQForUi());
            }
        };
        set (ParamID::morph, 0.5f);
        set (ParamID::q, 0.5f);
        set (ParamID::movePreset, (float) trench::kOrbitPatternIndex);
        set (ParamID::moveLength, 3.0f);
        run (10);
        std::vector<float> m, qv;
        run (384, &m, &qv);
        const auto span = [] (const std::vector<float>& v) { return *std::max_element (v.begin(), v.end()) - *std::min_element (v.begin(), v.end()); };
        const auto mean = [] (const std::vector<float>& v) { double s = 0.0; for (float x : v) s += x; return (float) (s / (double) v.size()); };
        const float mMean = mean (m), qMean = mean (qv);
        bool quadrature = true;
        for (size_t i = 0; i + 96 < m.size(); ++i)
            quadrature = quadrature && std::abs ((qv[i] - qMean) - (m[i + 96] - mMean)) < 0.02f;
        std::printf ("      Orbit over two bars: Morph span %g, Q span %g, quadrature=%d\n", span (m), span (qv), (int) quadrature);
        check (span (m) > 0.4f && span (qv) > 0.4f && quadrature && p.isQModulatedForUi(),
               "Orbit turns both wheels, Q a quarter turn ahead of Morph");
        set (ParamID::movePreset, 0.0f);
        run (5);
        check (! p.isQModulatedForUi() && std::abs (p.getEffectiveQForUi() - trench::curves::curveMap (trench::curves::Axis::q, 0.5f)) < 1.0e-5f,
               "leaving Orbit returns Q to the wheel");
        p.setEchoArmed (true);
        run (5);
        p.holdMorph (true);
        for (int b = 0; b < 192; ++b)
        {
            set (ParamID::morph, 0.2f + 0.4f * (float) b / 191.0f);
            run (1);
        }
        p.holdMorph (false);
        run (1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        const auto echo = p.userMotion.get();
        std::vector<float> replay;
        run (192, &replay);
        const auto expected = [] (size_t block) { return trench::curves::curveMap (trench::curves::Axis::morph, 0.2f + 0.4f * ((float) block / 3.0f - 4.0f) / 60.0f); };
        bool traced = true;
        for (size_t i : { (size_t) 60, (size_t) 120, (size_t) 185 })
            traced = traced && std::abs (replay[i] - expected (i)) < 0.06f;
        std::printf ("      Echo: name %s, length choice %g, replay at 60/120/185 blocks %g %g %g (gesture there %g %g %g)\n", echo.name.toRawUTF8(),
                     p.apvts.getRawParameterValue (ParamID::moveLength)->load(), replay[60], replay[120], replay[185], expected (60), expected (120), expected (185));
        check (p.usingUserMotion() && echo.name == PluginProcessor::kEchoName && echo.steps == 64
               && p.apvts.getRawParameterValue (ParamID::moveLength)->load() == 2.0f && traced && p.isEchoArmed(),
               "Echo replays a one-bar sweep of the wheel every bar, from where it started to where it let go");
        const float released = trench::curves::curveMap (trench::curves::Axis::morph, 0.6f);
        std::printf ("      Echo: wheel after release %g (let go at 0.6), first replay block %g (let go at %g), last %g\n",
                     p.apvts.getParameter (ParamID::morph)->getValue(), replay[0], released, replay[191]);
        check (std::abs (p.apvts.getParameter (ParamID::morph)->getValue() - 0.6f) < 1.0e-3f,
               "Echo leaves the MORPH wheel where the hand let go");
        check (std::abs (replay[0] - released) < 0.02f && std::abs (replay[191] - released) < 0.03f,
               "Echo starts from where the hand let go and ends each loop there: no rest and no snap at the seam");
        float seam = 0.0f;
        for (size_t i = 1; i < replay.size(); ++i) seam = std::max (seam, std::abs (replay[i] - replay[i - 1]));
        std::vector<float> second;
        run (16, &second);
        seam = std::max (seam, std::abs (second[0] - replay[191]));
        for (size_t i = 1; i < second.size(); ++i) seam = std::max (seam, std::abs (second[i] - second[i - 1]));
        std::printf ("      Echo: largest block-to-block move across the loop and its seam %g\n", seam);
        check (seam < 0.08f, "Echo glides back to the start of the gesture instead of jumping");
        p.holdMorph (true);
        run (3);
        p.holdMorph (false);
        run (1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        const float parked = p.apvts.getParameter (ParamID::morph)->getValue();
        std::vector<float> still;
        run (40, &still);
        bool stopped = ! p.usingUserMotion() && p.isEchoArmed();
        for (float v : still)
            stopped = stopped && std::abs (v - trench::curves::curveMap (trench::curves::Axis::morph, parked)) < 1.0e-4f;
        check (stopped, "a tap on the wheel stops the Echo and leaves the wheel where it was touched, still armed");
        p.setPlayHead (nullptr);
    }
    return failures;
}
