#pragma once
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "TestFixtures.h"
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
            trench::DeskDrive modelL, modelR;
            for (auto* m : { &modelL, &modelR }) { m->prepare (rate); m->setEnabled (true); }
            bool exact = true;
            for (int block = 0; block < 32; ++block)
            {
                const float level = block < 16 ? 4.0f : 0.125f;
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < 128; ++i) audio.setSample (c, i, (c == 0 ? level : 0.125f) * (float) std::sin (0.07 * (block * 128 + i)));
                juce::AudioBuffer<float> dry;
                dry.makeCopyOf (audio);
                bridge.process (audio, {});
                for (int i = 0; i < 128; ++i)
                {
                    exact = exact && audio.getSample (0, i) == modelL.process (dry.getSample (0, i));
                    if (channels > 1) exact = exact && audio.getSample (1, i) == modelR.process (dry.getSample (1, i));
                }
            }
            check (exact, "after the filter every sample passes Mackity at its own unity setting, per channel, with nothing else in the bridge");
            const auto tonePeak = [&] (float inputGain, float outputGain)
            {
                TrenchDspBridge b;
                b.prepare (rate, 128);
                b.loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
                b.setInputDrive (inputGain);
                b.setOutputLevel (outputGain);
                juce::AudioBuffer<float> a (channels, 128);
                float peak = 0.0f;
                for (int block = 0; block < 200; ++block)
                {
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < 128; ++i) a.setSample (c, i, 0.001f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (block * 128 + i) / rate));
                    b.process (a, {});
                    if (block >= 150) peak = std::max (peak, a.getMagnitude (0, 0, 128));
                }
                return peak;
            };
            const float unity = tonePeak (1.0f, 1.0f);
            check (std::abs (juce::Decibels::gainToDecibels (tonePeak (juce::Decibels::decibelsToGain (24.0f), 1.0f) / unity) - 24.0f) < 0.1f
                   && std::abs (juce::Decibels::gainToDecibels (tonePeak (juce::Decibels::decibelsToGain (-24.0f), 1.0f) / unity) + 24.0f) < 0.1f,
                   "INPUT is a clean level from -24 to +24 dB");
            check (std::abs (juce::Decibels::gainToDecibels (tonePeak (1.0f, juce::Decibels::decibelsToGain (-12.0f)) / unity) + 12.0f) < 0.1f,
                   "OUTPUT is a clean level after Mackity");
            {
                TrenchDspBridge slam;
                slam.prepare (rate, 128);
                slam.loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
                slam.setInputSlam (true);
                trench::DeskDrive pre, post;
                for (auto* m : { &pre, &post }) { m->prepare (rate); m->setEnabled (true); }
                pre.setTrims (0.4, 0.5);
                juce::AudioBuffer<float> a (1, 128);
                bool matches = true;
                double moved = 0.0;
                for (int block = 0; block < 16; ++block)
                {
                    for (int i = 0; i < 128; ++i) a.setSample (0, i, 0.9f * (float) std::sin (0.05 * (block * 128 + i)));
                    juce::AudioBuffer<float> dry; dry.makeCopyOf (a);
                    slam.process (a, {});
                    for (int i = 0; i < 128; ++i)
                    {
                        const float want = post.process (pre.process (dry.getSample (0, i)));
                        matches = matches && a.getSample (0, i) == want;
                        moved += std::abs (want - dry.getSample (0, i));
                    }
                }
                check (matches && moved > 1.0, "SLAM puts Mackity before the filter at In Trim 0.4 (+24 dB) and Out Pad 0.5, in series with the one after it");
            }
            TrenchDspBridge clean;
            clean.prepare (rate, 128);
            clean.loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
            clean.setInputDrive (2.0f);
            clean.process (audio, {});
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
        bool independent = true;
        double gainError = 0.0;
        const float requestedGain = juce::Decibels::decibelsToGain (12.0f);
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
                outputPeak = std::max (outputPeak, std::abs (b.getSample (0, i)));
                sidePeak = std::max (sidePeak, std::abs (b.getSample (1, i)));
                independent = independent && std::isfinite (b.getSample (0, i))
                    && b.getSample (1, i) == 0.0f;
                if (block >= 4)
                    gainError = std::max (gainError, (double) std::abs (b.getSample (0, i) - a.getSample (0, i) * requestedGain));
            }
        }
        std::printf ("      OUTPUT 75 peak %g, silent channel peak %g\n", outputPeak, sidePeak);
        check (difference > 0.01, "OUTPUT changes audio with movement disabled");
        check (independent, "OUTPUT remains finite and stereo states stay independent");
        check (outputPeak > 1.0f && gainError < 1.0e-5,
               "OUTPUT +12 dB scales the entire resonant waveform exactly, including peaks above full scale");
        check (std::abs (nonlinear.getEffectiveMorphForUi() - 0.5f) < 1.0e-6f,
            "OUTPUT does not move the MORPH wheel");
        juce::MemoryBlock saved;
        nonlinear.getStateInformation (saved);
        plain.setStateInformation (saved.getData(), (int) saved.getSize());
        check (std::abs (plain.apvts.getParameter (ParamID::output)->getValue() - 0.75f) < 1.0e-3f,
            "OUTPUT survives project recall");
    }
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails (48000, 128);
    processor.prepareToPlay (48000, 128);
    processor.setEditorOpen (true);
    check (processor.installBodyBytes (BinaryData::identity_body240, BinaryData::identity_body240Size), "output gain test body loads");
    check (processor.apvts.getParameter (ParamID::slamDrive) == nullptr, "separate SLAM parameter is retired");
    {
        PluginProcessor colour;
        colour.setRateAndBufferSizeDetails (48000, 128);
        colour.prepareToPlay (48000, 128);
        colour.installBodyBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
        juce::AudioBuffer<float> wet (2, 128);
        juce::MidiBuffer m;
        auto* in = colour.apvts.getParameter (ParamID::preamp);
        const auto harmonics = [&] (float db)
        {
            in->setValueNotifyingHost (in->convertTo0to1 (db));
            std::vector<float> tail;
            float peak = 0.0f;
            for (int block = 0; block < 64; ++block)
            {
                for (int i = 0; i < 128; ++i) { const float v = 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 375.0 * (block * 128 + i) / 48000.0); wet.setSample (0, i, v); wet.setSample (1, i, v); }
                colour.processBlock (wet, m);
                if (block >= 32) { tail.insert (tail.end(), wet.getReadPointer (0), wet.getReadPointer (0) + 128); peak = std::max (peak, wet.getMagnitude (0, 0, 128)); }
            }
            double f = 0.0, h = 0.0;
            for (int k = 1; k <= 7; ++k)
            {
                double re = 0.0, im = 0.0;
                for (size_t i = 0; i < tail.size(); ++i)
                {
                    const double a = 2.0 * juce::MathConstants<double>::pi * 375.0 * k * (double) i / 48000.0;
                    re += tail[i] * std::cos (a); im += tail[i] * std::sin (a);
                }
                (k == 1 ? f : h) += re * re + im * im;
            }
            return std::make_pair (std::sqrt (h / f), peak);
        };
        const auto soft = harmonics (0.0f), hard = harmonics (18.0f);
        std::printf ("      No Filter: harmonic ratio %.4f at INPUT 0 dB, %.4f at +18 dB, peak %.3f\n", soft.first, hard.first, hard.second);
        check (soft.first < 0.005 && hard.first > 0.05 && std::isfinite (hard.second),
            "No Filter is a colour path: INPUT drives Mackity into saturation");
    }
    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    auto* outLevel = processor.apvts.getParameter (ParamID::output);
    outLevel->setValueNotifyingHost (outLevel->convertTo0to1 (24.0f));
    const auto settled = [&]
    {
        float peak = 0.0f;
        for (int block = 0; block < 400; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i) audio.setSample (c, i, 0.9f * (float) std::sin (0.05 * (block * 128 + i)));
            processor.processBlock (audio, midi);
            if (block >= 300) peak = std::max (peak, audio.getMagnitude (0, 0, 128));
        }
        return peak;
    };
    const float held = settled();
    auto* output = processor.apvts.getParameter (ParamID::output);
    output->setValueNotifyingHost (output->convertTo0to1 (0.0f));
    const float unity = settled();
    check (held > 1.0f && std::abs (juce::Decibels::gainToDecibels (held / unity) - 24.0f) < 0.001f,
        "OUTPUT +24 dB delivers the requested gain with no final ceiling or compensation");
    output->setValueNotifyingHost (output->convertTo0to1 (24.0f));
    settled();
    check (processor.getOutClipForUi() > 0.0f, "the output meter reports overs without changing the audio");
    check (processor.apvts.getParameter ("outputTrim") == nullptr, "no secondary output trim control exists");
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
        check (after == held, "retired output trim cannot change the requested output gain");
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
    check (settled() == held, "effect stays fully wet after recalling a former dry MIX setting");
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
        {
            const float v = desk.process (0.01f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / 48000.0));
            if (i > 24000) quiet = std::max (quiet, std::abs (v));
        }
        desk.reset();
        for (int i = 0; i < 48000; ++i)
            hot = std::max (hot, std::abs (desk.process (4.0f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / 48000.0))));
        check (std::abs (quiet - 0.01f) < 1.0e-4f && hot > 0.80f && hot < 1.25f,
            "Mackity at its own unity setting passes quiet material and holds a hot one at its fifth-order rail");
    }
#if TRENCH_DEV_PANEL
    for (const char* id : trench::calibration::retired)
        check (processor.apvts.getParameter (id) == nullptr, "retired dynamics control is absent from Dev");
#endif
    return failed;
}
