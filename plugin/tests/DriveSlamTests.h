#pragma once
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "dsp/PreampLaw.h"
#include <cstdio>

inline float identitySaturationReference (float input)
{
    double value = input;
    for (int stage = 0; stage < trench::kUiStageCount; ++stage)
        if (std::abs (value) > 1.0)
            value = std::copysign (1.0 + 0.5 * std::tanh (2.0 * (std::abs (value) - 1.0)), value);
    return (float) value;
}

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
                    for (int i = 0; i < 128; ++i) audio.setSample (c, i, c == 0 ? level : 0.125f);
                bridge.process (audio, {});
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < 128; ++i)
                        exact = exact && std::abs (audio.getSample (c, i) - identitySaturationReference (c == 0 ? level : 0.125f)) < 1.0e-6f;
            }
            check (exact, "fixed stage saturation bounds hot signals; quiet recovery is immediate and channels are independent");
            bridge.setInputDrive (trench::preampGain (1.0f));
            for (int block = 0; block < 12; ++block)
            {
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < 128; ++i) audio.setSample (c, i, 0.01f);
                bridge.process (audio, {});
            }
            check (std::abs (audio.getSample (0, 127) - 0.1f) < 1.0e-6f, "INPUT supplies +20 dB clean gain at maximum");
            bridge.setInputDrive (1.0f);
            check (! bridge.inputDriveIsUnity(), "INPUT return keeps processing active while the gain ramp settles");
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
            check (std::abs (sample - 0.0015f) < 1.0e-6f,
                "first section clips a +40 dBFS peak before downstream attenuation in every processing branch");
        }
    {
        PluginProcessor plain, nonlinear;
        for (auto* p : { &plain, &nonlinear })
        {
            p->setRateAndBufferSizeDetails (48000, 128);
            p->prepareToPlay (48000, 128);
            p->setEditorOpen (true);
            for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
                if (juce::String (BinaryData::originalFilenames[r]) == "xml_crisp.body240")
                {
                    int size = 0;
                    const auto* bytes = BinaryData::getNamedResource (BinaryData::namedResourceList[r], size);
                    check (p->installBodyBytes (bytes, (size_t) size), "resonance integration body loads");
                }
        }
        nonlinear.apvts.getParameter (ParamID::distortion)->setValueNotifyingHost (0.75f);
        juce::AudioBuffer<float> a (2, 128), b (2, 128);
        juce::MidiBuffer midi;
        double difference = 0.0;
        bool bounded = true;
        for (int block = 0; block < 80; ++block)
        {
            for (int i = 0; i < 128; ++i)
            {
                const float x = block < 40 ? 0.2f * std::sin (0.073f * (float) (block * 128 + i)) : 0.0f;
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
                bounded = bounded && std::isfinite (b.getSample (0, i)) && std::abs (b.getSample (0, i)) <= 1.0f
                    && b.getSample (1, i) == 0.0f;
            }
        }
        check (difference > 0.01, "host Distortion parameter changes audio with movement disabled");
        check (bounded, "nonlinear resonance keeps the output guarded and stereo states independent");
        check (std::abs (nonlinear.getEffectiveMorphForUi() - 0.5f) < 1.0e-6f,
            "signal-dependent resonance does not move the MORPH wheel");
        juce::MemoryBlock saved;
        nonlinear.getStateInformation (saved);
        plain.setStateInformation (saved.getData(), (int) saved.getSize());
        check (std::abs (plain.apvts.getRawParameterValue (ParamID::distortion)->load() - 0.75f) < 1.0e-6f,
            "resonance amount survives project recall");
    }
    PluginProcessor processor;
    processor.setRateAndBufferSizeDetails (48000, 128);
    processor.prepareToPlay (48000, 128);
    processor.setEditorOpen (true);
    check (processor.installBodyBytes (BinaryData::identity_body240, BinaryData::identity_body240Size), "soft clip test body loads");
    check (processor.apvts.getParameter (ParamID::slamDrive) == nullptr, "separate SLAM parameter is retired");
    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    bool clipped = true;
    for (const float level : { 0.1f, 0.6f, 1.0f, 4.0f, 0.125f })
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i) audio.setSample (c, i, c == 0 ? level : -level);
        processor.processBlock (audio, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i)
                clipped = clipped && std::abs (audio.getSample (c, i) - trench::softGuard (identitySaturationReference (c == 0 ? level : -level))) < 1.0e-6f;
        if (level == 0.6f)
            check (processor.getOutClipForUi() > 0.99f, "clip activity begins at the actual soft knee");
    }
    check (clipped, "filter output soft clips without gain recovery or attenuation below the knee");
    auto* drive = processor.apvts.getParameter (ParamID::preamp);
    drive->setValueNotifyingHost (1.0f);
    for (int block = 0; block < 12; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i) audio.setSample (c, i, 0.1f);
        processor.processBlock (audio, midi);
    }
    check (std::abs (audio.getSample (0, 127) - trench::softGuard (1.0f)) < 1.0e-6f, "INPUT reaches filter soft clipping without an output drive stage");
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
    for (int block = 0; block < 12; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i) audio.setSample (c, i, 0.1f);
        processor.processBlock (audio, midi);
    }
    check (std::abs (audio.getSample (0, 127) - trench::softGuard (1.0f)) < 1.0e-6f,
        "saved output gain cannot change the final soft-clipped signal");
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
    for (int block = 0; block < 12; ++block)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i) audio.setSample (c, i, 0.1f);
        processor.processBlock (audio, midi);
    }
    check (std::abs (audio.getSample (0, 127) - trench::softGuard (1.0f)) < 1.0e-6f,
        "effect stays fully wet after recalling a former dry MIX setting");
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
#if TRENCH_DEV_PANEL
    for (const char* id : trench::calibration::retired)
        check (processor.apvts.getParameter (id) == nullptr, "retired dynamics control is absent from Dev");
#endif
    return failed;
}
