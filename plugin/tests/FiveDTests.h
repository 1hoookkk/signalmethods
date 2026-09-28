#pragma once
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <cstdio>

inline int fiveDTests()
{
    int failed = 0;
    const auto check = [&] (bool ok, const char* label)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", label);
        if (! ok) ++failed;
    };
    const auto warm = [] (trench::QSoundStage& stage, double rate)
    {
        stage.setEnabled (true);
        for (int i = 0; i < (int) rate / 20; ++i)
        {
            float l = 0.0f, r = 0.0f;
            stage.process (l, r);
        }
    };
    {
        juce::WavAudioFormat wav;
        const auto file = juce::File (TRENCH_TABLE_STITCH_ROOT).getChildFile (
            "evidence/authoring/captures/qsound/qcreator_qright90_impulse_11025.wav");
        auto stream = file.createInputStream();
        std::unique_ptr<juce::AudioFormatReader> reader (
            stream != nullptr ? wav.createReaderFor (stream.release(), true) : nullptr);
        check (reader != nullptr && reader->sampleRate == 11025.0 && reader->numChannels == 2,
               "5D reference is the recovered stereo 11025 Hz QCreator capture");
        if (reader != nullptr)
        {
            juce::AudioBuffer<float> reference (2, 128);
            const bool read = reader->read (&reference, 0, 128, 30000, true, true);
            trench::QSoundStage stage;
            stage.prepare (11025.0);
            warm (stage, 11025.0);
            float error = 0.0f;
            for (int i = 0; i < 128; ++i)
            {
                float l = 0.0f, r = i == 0 ? 0.5f : 0.0f;
                stage.process (l, r);
                error = std::max ({ error, std::abs (l - reference.getSample (0, i)),
                                           std::abs (r - reference.getSample (1, i)) });
            }
            std::printf ("      5D native capture maximum error %.9g\n", error);
            check (read && error < 1.0e-7f, "5D right input reproduces the measured +90 degree impulse");
        }
    }
    bool bypass = true, mirror = true, linear = true, finite = true;
    for (const double rate : { 11025.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        trench::QSoundStage dry, normal, swapped, loud;
        for (auto* stage : { &dry, &normal, &swapped, &loud }) stage->prepare (rate);
        for (auto* stage : { &normal, &swapped, &loud }) warm (*stage, rate);
        for (int i = 0; i < 4096; ++i)
        {
            const float a = 0.7f * (float) std::sin ((double) i * 0.031);
            const float b = 0.3f * (float) std::cos ((double) i * 0.071);
            float dl = a, dr = b, nl = a, nr = b, sl = b, sr = a, ll = 4.0f * a, lr = 4.0f * b;
            dry.process (dl, dr);
            normal.process (nl, nr);
            swapped.process (sl, sr);
            loud.process (ll, lr);
            bypass = bypass && dl == a && dr == b;
            mirror = mirror && nl == sr && nr == sl;
            linear = linear && std::abs (ll - 4.0f * nl) < 1.0e-6f && std::abs (lr - 4.0f * nr) < 1.0e-6f;
            finite = finite && std::isfinite (ll) && std::isfinite (lr);
        }
    }
    check (bypass, "5D off is bit-identical stereo bypass at all tested rates");
    check (mirror, "5D mirrors left and right exactly from 11025 to 192000 Hz");
    check (linear && finite, "5D preserves fourfold input scaling with no limiter or automatic gain");
    {
        trench::QSoundStage switched, wet;
        switched.prepare (48000.0);
        wet.prepare (48000.0);
        wet.setEnabled (true);
        float wetL = 0.0f, wetR = 0.0f;
        for (int i = 0; i < 2000; ++i)
        {
            float l = 0.2f, r = -0.125f;
            switched.process (l, r);
            wetL = 0.2f; wetR = -0.125f;
            wet.process (wetL, wetR);
        }
        switched.setEnabled (true);
        float firstL = 0.2f, firstR = -0.125f;
        switched.process (firstL, firstR);
        const bool firstStep = std::abs (firstL - 0.2f) <= std::abs (wetL - 0.2f) / 960.0f + 1.0e-6f
                           && std::abs (firstR + 0.125f) <= std::abs (wetR + 0.125f) / 960.0f + 1.0e-6f;
        float l = 0.0f, r = 0.0f;
        for (int i = 1; i < 960; ++i)
        {
            l = 0.2f; r = -0.125f;
            switched.process (l, r);
        }
        check (firstStep && std::abs (l - wetL) < 1.0e-6f && std::abs (r - wetR) < 1.0e-6f,
               "5D switching reaches the wet response through a 20 ms ramp with current signal history");
        switched.setEnabled (false);
        for (int i = 0; i < 960; ++i)
        {
            l = 0.2f; r = -0.125f;
            switched.process (l, r);
        }
        check (l == 0.2f && r == -0.125f, "5D returns to exact bypass after the off ramp");
    }
    {
        PluginProcessor processor;
        auto* parameter = processor.apvts.getParameter (ParamID::fiveD);
        check (parameter != nullptr && parameter->getDefaultValue() == 0.0f
               && parameter->getValue() == 0.0f && parameter->isAutomatable(),
               "5D is an automatable parameter that defaults off");
        if (parameter != nullptr)
        {
            parameter->setValueNotifyingHost (1.0f);
            juce::MemoryBlock saved;
            processor.getStateInformation (saved);
            parameter->setValueNotifyingHost (0.0f);
            processor.setStateInformation (saved.getData(), (int) saved.getSize());
            check (parameter->getValue() == 1.0f, "5D on survives project-state recall");
            auto legacy = processor.apvts.copyState();
            legacy.removeChild (legacy.getChildWithProperty ("id", ParamID::fiveD), nullptr);
            juce::MemoryBlock oldState;
            juce::AudioProcessor::copyXmlToBinary (*legacy.createXml(), oldState);
            processor.setStateInformation (oldState.getData(), (int) oldState.getSize());
            check (parameter->getValue() == 0.0f, "loading an older project resets 5D to off");
        }
    }
    {
        PluginProcessor dry, wet, gain;
        const auto set = [] (PluginProcessor& processor, const char* id, float value)
        {
            auto* p = processor.apvts.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (value));
        };
        for (auto* processor : { &dry, &wet, &gain })
        {
            set (*processor, ParamID::body, (float) trench::kNoFilterIndex);
            processor->setPlayConfigDetails (2, 2, 48000.0, 128);
            processor->prepareToPlay (48000.0, 128);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
        set (wet, ParamID::fiveD, 1.0f);
        set (gain, ParamID::fiveD, 1.0f);
        set (gain, ParamID::output, 12.0f);
        trench::QSoundStage reference;
        reference.prepare (48000.0);
        reference.setEnabled (true);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> d (2, 128), w (2, 128), g (2, 128);
        float error = 0.0f, gainError = 0.0f, difference = 0.0f;
        const float expectedGain = juce::Decibels::decibelsToGain (12.0f);
        for (int block = 0; block < 48; ++block)
        {
            for (int i = 0; i < 128; ++i)
            {
                const double sample = (double) (block * 128 + i);
                d.setSample (0, i, 0.4f * (float) std::sin (sample * 0.047));
                d.setSample (1, i, 0.25f * (float) std::cos (sample * 0.023));
            }
            w.makeCopyOf (d); g.makeCopyOf (d);
            dry.processBlock (d, midi); wet.processBlock (w, midi); gain.processBlock (g, midi);
            for (int i = 0; i < 128; ++i)
            {
                float l = d.getSample (0, i), r = d.getSample (1, i);
                reference.process (l, r);
                error = std::max ({ error, std::abs (l - w.getSample (0, i)), std::abs (r - w.getSample (1, i)) });
                if (block > 16)
                    for (int c = 0; c < 2; ++c)
                    {
                        gainError = std::max (gainError, std::abs (g.getSample (c, i) - expectedGain * w.getSample (c, i)));
                        difference = std::max (difference, std::abs (w.getSample (c, i) - d.getSample (c, i)));
                    }
            }
        }
        check (error < 2.0e-6f && difference > 0.01f, "5D parameter drives the actual processor spatial stage");
        check (gainError < 2.0e-6f, "manual OUTPUT remains exactly +12 dB with 5D enabled");
        for (auto* processor : { &dry, &wet })
        {
            processor->releaseResources();
            processor->setPlayConfigDetails (1, 1, 48000.0, 128);
            processor->prepareToPlay (48000.0, 128);
        }
        juce::AudioBuffer<float> monoDry (1, 128), monoWet (1, 128);
        bool mono = true;
        for (int block = 0; block < 20; ++block)
        {
            for (int i = 0; i < 128; ++i) monoDry.setSample (0, i, 0.2f * (float) std::sin ((block * 128 + i) * 0.037));
            monoWet.makeCopyOf (monoDry);
            dry.processBlock (monoDry, midi); wet.processBlock (monoWet, midi);
            for (int i = 0; i < 128; ++i) mono = mono && monoDry.getSample (0, i) == monoWet.getSample (0, i);
        }
        check (mono, "5D leaves a mono instance unchanged");
    }
    return failed;
}
