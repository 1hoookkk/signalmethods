#pragma once
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "TestFixtures.h"
#include "dsp/PreampLaw.h"
#include "dsp/EmuLimiter.h"
#include <cstdio>

inline int driveSlamTests()
{
    int failed = 0;
    const auto check = [&] (bool ok, const char* label)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", label);
        if (! ok) ++failed;
    };
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
        for (const int channels : { 1, 2 })
        {
            TrenchDspBridge bridge;
            bridge.prepare (rate, 128);
            check (bridge.loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size), "gain test body loads");
            juce::AudioBuffer<float> audio (channels, 128);
            trench::EmuLimiter model;
            model.prepare (rate);
            bool exact = true, bounded = true;
            for (int block = 0; block < 32; ++block)
            {
                const float level = block < 16 ? 4.0f : 0.125f;
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < 128; ++i) audio.setSample (c, i, c == 0 ? level : 0.125f);
                bridge.process (audio, {});
                for (int i = 0; i < 128; ++i)
                {
                    float left = level, right = 0.125f;
                    model.process (left, channels > 1 ? &right : nullptr);
                    exact = exact && std::abs (audio.getSample (0, i) - trench::softGuard (left)) < 1.0e-6f;
                    if (channels > 1) exact = exact && std::abs (audio.getSample (1, i) - trench::softGuard (right)) < 1.0e-6f;
                    for (int c = 0; c < channels; ++c) bounded = bounded && std::abs (audio.getSample (c, i)) <= trench::kFinalSafetyCeiling;
                }
            }
            check (exact && bounded, "the crossing limiter then the soft clip shape every sample: hot input is pulled toward -4 dBFS, recovery follows the limiter's release, stereo is linked");
            TrenchDspBridge clean;
            clean.prepare (rate, 128);
            clean.loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
            bridge.process (audio, {});
            clean.setInputDrive (trench::preampGain (1.0f));
            for (int block = 0; block < 12; ++block)
            {
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < 128; ++i) audio.setSample (c, i, 0.01f);
                clean.process (audio, {});
            }
            check (std::abs (audio.getSample (0, 127) - 0.1f) < 1.0e-6f, "INPUT supplies +20 dB clean gain at maximum below the limiter threshold");
            clean.setInputDrive (1.0f);
            check (! clean.inputDriveIsUnity(), "INPUT return keeps processing active while the gain ramp settles");
        }
    for (const double radiusMode : { 0.0, 0.01 })
        for (const double feedbackMode : { 0.0, 1.0 })
        {
            trench::core::Cascade coefficients {};
            for (auto& stage : coefficients) stage = { 1.0, 0.0, 0.0, 0.0, 0.0 };
            coefficients[0] = { 1000.0, 0.0, 0.0, -0.5, 0.25 };
            coefficients[1][0] = 0.001;
            trench::core::CascadeRunner runner;
            runner.set_ring_leveller (false);
            runner.set_immediate (coefficients);
            runner.set_stage_saturation (true, 1.0);
            runner.set_pole_distortion (feedbackMode);
            runner.set_radius_distortion (radiusMode);
            float sample = 0.1f;
            runner.process (std::span<float> (&sample, 1));
            check (std::isfinite (sample) && std::abs (sample) <= 0.002f,
                "first section bounds a +40 dBFS peak before downstream attenuation in every processing branch");
        }
    {
        PluginProcessor plain, nonlinear;
        for (auto* p : { &plain, &nonlinear })
        {
            p->setRateAndBufferSizeDetails (48000, 128);
            p->prepareToPlay (48000, 128);
            p->setEditorOpen (true);
            {
                const auto fixture = fixtureBody ("xml_crisp.body240");
                check (p->installBodyBytes (fixture.getData(), fixture.getSize()), "resonance integration body loads");
            }
        }
        nonlinear.apvts.getParameter (ParamID::output)->setValueNotifyingHost (0.75f);
        juce::AudioBuffer<float> a (2, 128), b (2, 128);
        juce::MidiBuffer midi;
        double difference = 0.0;
        bool bounded = true;
        float outputPeak = 0.0f, sidePeak = 0.0f;
        for (int block = 0; block < 80; ++block)
        {
            for (int i = 0; i < 128; ++i)
            {
                const float x = block < 40 ? 0.9f * std::sin (0.073f * (float) (block * 128 + i)) : 0.0f;
                a.setSample (0, i, x);
                b.setSample (0, i, x);
                a.setSample (1, i, 0.0f);
                b.setSample (1, i, 0.0f);
            }
            plain.processBlock (a, midi);
            nonlinear.processBlock (b, midi);
            for (int i = 0; i < 128; ++i)
            {
                difference += std::abs (a.getSample (0, i) - b.getSample (0, i));
#if TRENCH_DEV_PANEL
                const float bound = 1.5f;
#else
                const float bound = trench::kFinalSafetyCeiling;
#endif
                outputPeak = std::max (outputPeak, std::abs (b.getSample (0, i)));
                sidePeak = std::max (sidePeak, std::abs (b.getSample (1, i)));
                bounded = bounded && std::isfinite (b.getSample (0, i)) && std::abs (b.getSample (0, i)) <= bound
                    && b.getSample (1, i) == 0.0f;
            }
        }
        std::printf ("      OUTPUT 75 peak %g, silent channel peak %g\n", outputPeak, sidePeak);
        check (difference > 0.01, "OUTPUT changes audio with movement disabled");
        check (bounded, "OUTPUT stays under the -0.1 dBFS ceiling and stereo states stay independent");
        check (std::abs (nonlinear.getEffectiveMorphForUi() - 0.5f) < 1.0e-6f,
            "OUTPUT does not move the MORPH wheel");
        juce::MemoryBlock saved;
        nonlinear.getStateInformation (saved);
        plain.setStateInformation (saved.getData(), (int) saved.getSize());
        check (std::abs (plain.apvts.getRawParameterValue (ParamID::output)->load() - 0.75f) < 1.0e-6f,
            "OUTPUT survives project recall");
    }
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails (48000, 128);
    processor.prepareToPlay (48000, 128);
    processor.setEditorOpen (true);
    check (processor.installBodyBytes (BinaryData::identity_body240, BinaryData::identity_body240Size), "soft clip test body loads");
    check (processor.apvts.getParameter (ParamID::slamDrive) == nullptr, "separate SLAM parameter is retired");
    {
        PluginProcessor colour;
        colour.setRateAndBufferSizeDetails (48000, 128);
        colour.prepareToPlay (48000, 128);
        colour.installBodyBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
        juce::AudioBuffer<float> dry (2, 128), wet (2, 128);
        juce::MidiBuffer m;
        auto* out = colour.apvts.getParameter (ParamID::output);
        double difference = 0.0; float peak = 0.0f; bool finite = true;
        for (int block = 0; block < 40; ++block)
        {
            for (int i = 0; i < 128; ++i) { const float s = 0.25f * (float) std::sin (0.1 * (block * 128 + i)); dry.setSample (0, i, s); dry.setSample (1, i, s); }
            wet.makeCopyOf (dry);
            out->setValueNotifyingHost (block < 20 ? 0.0f : 1.0f);
            colour.processBlock (wet, m);
            for (int i = 0; i < 128; ++i)
            {
                const float v = wet.getSample (0, i);
                finite = finite && std::isfinite (v);
                peak = std::max (peak, std::abs (v));
                if (block >= 30) difference += std::abs (v - dry.getSample (0, i));
                if (block >= 10 && block < 20) difference -= std::abs (v - dry.getSample (0, i));
            }
        }
#if TRENCH_DEV_PANEL
        const float colourBound = 1.5f;
#else
        const float colourBound = trench::kFinalSafetyCeiling;
#endif
        check (finite && difference > 1.0 && peak <= colourBound,
            "No Filter is a colour path: OUTPUT saturates the dry signal and stays under the ceiling");
    }
    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    trench::EmuLimiter model;
    model.prepare (48000.0);
    bool clipped = true;
    for (const float level : { 0.1f, 0.6f, 1.0f, 4.0f, 0.125f })
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i) audio.setSample (c, i, c == 0 ? level : -level);
        processor.processBlock (audio, midi);
        for (int i = 0; i < 128; ++i)
        {
            float left = level, right = -level;
            model.process (left, &right);
            clipped = clipped && std::abs (audio.getSample (0, i) - trench::softGuard (left)) < 1.0e-6f
                              && std::abs (audio.getSample (1, i) - trench::softGuard (right)) < 1.0e-6f;
        }
        if (level == 0.6f)
            check (processor.getOutClipForUi() > 0.99f, "clip activity begins at the actual soft knee");
    }
    check (clipped, "filter output passes the crossing limiter then the soft clip; below -4 dBFS neither acts");
    auto* drive = processor.apvts.getParameter (ParamID::preamp);
    drive->setValueNotifyingHost (1.0f);
    const auto settled = [&]
    {
        for (int block = 0; block < 1600; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i) audio.setSample (c, i, 0.1f);
            processor.processBlock (audio, midi);
        }
        return audio.getSample (0, 127);
    };
    const float held = settled();
    check (std::abs (held - trench::softGuard ((float) trench::EmuLimiter::kThreshold)) < 0.01f,
        "INPUT at full is held at the crossing limiter's -4 dBFS threshold, with no output drive stage");
    check (processor.apvts.getParameter ("outputTrim") == nullptr, "no output gain control follows the soft clipper");
    auto savedTree = processor.apvts.copyState();
    juce::ValueTree retired ("PARAM");
    retired.setProperty ("id", "outputTrim", nullptr);
    retired.setProperty ("value", 6.0f, nullptr);
    savedTree.addChild (retired, -1, nullptr);
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary (*savedTree.createXml(), saved);
    processor.setStateInformation (saved.getData(), (int) saved.getSize());
    check (! processor.apvts.copyState().getChildWithProperty ("id", "outputTrim").isValid(),
        "retired output trim is discarded when loading an interim saved project");
    {
        const float after = settled();
        std::printf ("      held %g, after retired trim %g, ripple bound 0.01\n", held, after);
        check (std::abs (after - trench::softGuard ((float) trench::EmuLimiter::kThreshold)) < 0.01f, "saved output gain cannot change the final limited signal");
    }
    check (processor.apvts.getParameter ("amount") == nullptr, "redundant MIX parameter is removed");
    auto oldMixState = processor.apvts.copyState();
    juce::ValueTree oldMix ("PARAM");
    oldMix.setProperty ("id", "amount", nullptr);
    oldMix.setProperty ("value", 0.0f, nullptr);
    oldMixState.addChild (oldMix, -1, nullptr);
    juce::MemoryBlock oldMixBytes;
    juce::AudioProcessor::copyXmlToBinary (*oldMixState.createXml(), oldMixBytes);
    processor.setStateInformation (oldMixBytes.getData(), (int) oldMixBytes.getSize());
    check (! processor.apvts.copyState().getChildWithProperty ("id", "amount").isValid(),
        "old dry MIX settings are discarded on project recall");
    check (std::abs (settled() - trench::softGuard ((float) trench::EmuLimiter::kThreshold)) < 0.01f, "effect stays fully wet after recalling a former dry MIX setting");
    auto legacy = processor.apvts.copyState();
    juce::ValueTree oldSlam ("PARAM");
    oldSlam.setProperty ("id", ParamID::slamDrive, nullptr);
    oldSlam.setProperty ("value", 1.0f, nullptr);
    legacy.addChild (oldSlam, -1, nullptr);
    juce::MemoryBlock legacyBytes;
    juce::AudioProcessor::copyXmlToBinary (*legacy.createXml(), legacyBytes);
    processor.setStateInformation (legacyBytes.getData(), (int) legacyBytes.getSize());
    check (! processor.apvts.copyState().getChildWithProperty ("id", ParamID::slamDrive).isValid(),
        "old SLAM settings cannot silently re-enable output distortion");
    auto withDistortion = processor.apvts.copyState();
    juce::ValueTree oldDistortion ("PARAM");
    oldDistortion.setProperty ("id", ParamID::distortion, nullptr);
    oldDistortion.setProperty ("value", 1.0f, nullptr);
    withDistortion.addChild (oldDistortion, -1, nullptr);
    juce::MemoryBlock distortionBytes;
    juce::AudioProcessor::copyXmlToBinary (*withDistortion.createXml(), distortionBytes);
    processor.setStateInformation (distortionBytes.getData(), (int) distortionBytes.getSize());
    check (! processor.apvts.copyState().getChildWithProperty ("id", ParamID::distortion).isValid(),
        "old Distortion settings are discarded on project recall");
    {
        trench::DeskDrive desk;
        desk.prepare (48000.0);
        desk.setEnabled (true);
        float quiet = 0.0f, hot = 0.0f;
        for (int i = 0; i < 48000; ++i)
            quiet = std::max (quiet, std::abs (desk.process (0.0001f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / 48000.0), 1.0f)));
        desk.reset();
        for (int i = 0; i < 48000; ++i)
            hot = std::max (hot, std::abs (desk.process ((float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / 48000.0), 1.0f)));
        check (std::abs (trench::DeskDrive::inTrim (0.0f) - 1.0) < 1.0e-9 && std::abs (juce::Decibels::gainToDecibels (trench::DeskDrive::inTrim (1.0f)) - 18.0) < 1.0e-6
               && std::abs (juce::Decibels::gainToDecibels (trench::DeskDrive::outPad (1.0f)) + 13.5) < 1.0e-6, "OUTPUT maps to Mackity In Trim up to +18 dB with Out Pad taking back three quarters");
        std::printf ("      Mackity full trim: quiet peak %g, hot peak %g\n", quiet, hot);
        check (quiet > 1.55e-4f && quiet < 1.80e-4f && hot > 0.15f && hot < 0.30f,
            "OUTPUT at full adds only +4.5 dB to quiet material and saturates full scale at Mackity's fifth-order curve, padded");
    }
#if TRENCH_DEV_PANEL
    for (const char* id : trench::calibration::retired)
        check (processor.apvts.getParameter (id) == nullptr, "retired dynamics control is absent from Dev");
#endif
    return failed;
}
