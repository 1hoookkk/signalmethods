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
                    exact = exact && audio.getSample (0, i) == dry.getSample (0, i);
                    if (channels > 1) exact = exact && audio.getSample (1, i) == dry.getSample (1, i);
                }
            }
            check (exact, "with SLAM off nothing but the filter touches the signal: no Mackity after it, per channel");
            const auto tonePeak = [&] (float inputGain, float outputGain, bool slamOn = false, float amplitude = 0.001f)
            {
                TrenchDspBridge b;
                b.prepare (rate, 128);
                b.loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
                b.setInputDrive (inputGain);
                b.setInputSlam (slamOn);
                b.setOutputLevel (outputGain);
                juce::AudioBuffer<float> a (channels, 128);
                float peak = 0.0f;
                for (int block = 0; block < 200; ++block)
                {
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < 128; ++i) a.setSample (c, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (block * 128 + i) / rate));
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
                   "OUTPUT is a clean level after the filter");
            const float slamQuiet = tonePeak (1.0f, 1.0f, true);
            check (std::abs (juce::Decibels::gainToDecibels (slamQuiet / unity) - 18.06f) < 0.2f,
                   "SLAM uses fixed gain without automatic RMS compensation");
            const float clipped = tonePeak (1.0f, 1.0f, true, 0.125f);
            const float pushed = tonePeak (4.0f, 1.0f, true, 0.125f);
            std::printf ("      SLAM clipped / pushed peaks %.6f / %.6f at %.0f Hz\n", clipped, pushed, rate);
            check (clipped > 0.35f && pushed < 0.65f,
                   "SLAM remains bounded while INPUT drives further into clipping");
            check (std::abs (tonePeak (4.0f, 0.25f, true, 0.125f) / pushed - 0.25f) < 1.0e-5f,
                   "OUTPUT scales SLAM after clipping without changing its drive");
            TrenchDspBridge clean;
            clean.prepare (rate, 128);
            clean.loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
            clean.setInputDrive (2.0f);
            clean.process (audio, {});
            clean.setInputDrive (1.0f);
            check (! clean.inputDriveIsUnity(), "INPUT return keeps processing active while the gain ramp settles");
        }
    {
        const auto burstCrest = [] (float drive)
        {
            TrenchDspBridge bridge;
            bridge.prepare (48000.0, 128);
            bridge.loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
            bridge.setInputSlam (true);
            bridge.setInputDrive (drive);
            juce::AudioBuffer<float> audio (1, 128);
            const std::array<double, 4> starts { 0.25, 0.85, 1.45, 2.05 };
            const std::array<double, 4> amplitudes { 0.02, 0.08, 0.32, 0.8 };
            double power = 0.0;
            float peak = 0.0f;
            for (int block = 0; block < 1125; ++block)
            {
                for (int i = 0; i < 128; ++i)
                {
                    const double time = (block * 128 + i) / 48000.0;
                    double x = 0.0;
                    for (size_t hit = 0; hit < starts.size(); ++hit)
                    {
                        const double age = time - starts[hit];
                        if (age >= 0.0 && age < 0.12)
                            x += amplitudes[hit] * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * age) * std::exp (-age / 0.02);
                    }
                    audio.setSample (0, i, (float) x);
                }
                bridge.process (audio, {});
                for (int i = 0; i < 128; ++i)
                {
                    const auto x = audio.getSample (0, i);
                    power += (double) x * x;
                    peak = std::max (peak, std::abs (x));
                }
            }
            return 20.0 * std::log10 (peak / std::sqrt (power / 144000.0));
        };
        const double normal = burstCrest (1.0f), pushed = burstCrest (juce::Decibels::decibelsToGain (12.0f));
        std::printf ("      Transient crest at INPUT 0 / +12 dB: %.3f / %.3f dB\n", normal, pushed);
        check (normal - pushed > 1.5,
               "INPUT +12 dB with SLAM reduces transient crest instead of only making the same waveform louder");
    }
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
    {
        TrenchDspBridge quietRight, hotRight;
        for (auto* bridge : { &quietRight, &hotRight })
        {
            bridge->prepare (rate, 128);
            bridge->loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
            bridge->setInputSlam (true);
        }
        juce::AudioBuffer<float> a (2, 128), b (2, 128);
        bool independent = true;
        for (int block = 0; block < 160; ++block)
        {
            for (int i = 0; i < 128; ++i)
            {
                const auto phase = 2.0 * juce::MathConstants<double>::pi * 1000.0 * (block * 128 + i) / rate;
                const auto left = 0.002f * (float) std::sin (phase);
                a.setSample (0, i, left); b.setSample (0, i, left);
                a.setSample (1, i, 0.0f);
                b.setSample (1, i, block >= 64 && block < 128 ? 0.9f * (float) std::sin (phase) : 0.0f);
            }
            quietRight.process (a, {});
            hotRight.process (b, {});
            for (int i = 0; i < 128; ++i)
                independent = independent && a.getSample (0, i) == b.getSample (0, i);
        }
        check (independent, "a transient on one channel cannot ride the other channel's SLAM gain");
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
        const auto harmonics = [&] (float db, bool slamOn)
        {
            colour.apvts.getParameter (ParamID::inputSlam)->setValueNotifyingHost (slamOn ? 1.0f : 0.0f);
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
        const auto soft = harmonics (18.0f, false), hard = harmonics (0.0f, true);
        std::printf ("      No Filter: harmonic ratio %.4f at INPUT +18 dB, %.4f with SLAM, peak %.3f\n", soft.first, hard.first, hard.second);
        check (soft.first < 0.005 && hard.first > 0.05 && std::isfinite (hard.second),
            "No Filter: INPUT alone stays clean, SLAM drives Mackity into saturation");
        const auto first = harmonics (-24.0f, true);
        check (first.first < 0.005 && hard.first > first.first * 10.0,
               "INPUT precedes SLAM: backing it down cleans up the clipping");
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
