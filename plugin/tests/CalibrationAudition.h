#pragma once
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/PreampLaw.h"

inline void calibrationAudition (const juce::File& directory)
{
    if (! directory.createDirectory()) throw std::runtime_error ("audition directory");
    juce::String report = "case,input,peak_dbfs,rms_dbfs\n";
    for (const auto* bodyFile : { "plugin/presets/p2k/talking_hedz.body240", "plugin/presets/p2k/megasweepz.body240" })
        for (int setting = 0; setting < 3; ++setting)
        {
            const float drive = (float) setting * 0.5f;
            PluginProcessor processor;
            processor.setRateAndBufferSizeDetails (48000, 128);
            processor.prepareToPlay (48000, 128);
            juce::MemoryBlock bytes;
            if (! juce::File (TRENCH_TABLE_STITCH_ROOT).getChildFile (bodyFile).loadFileAsData (bytes)
                || ! processor.installBodyBytes (bytes.getData(), bytes.getSize())) throw std::runtime_error ("audition body");
            const auto set = [&] (const char* id, float v)
            {
                auto* p = processor.apvts.getParameter (id);
                p->setValueNotifyingHost (p->convertTo0to1 (v));
            };
            set ("preamp", drive);  set ("q", 0.8f);
            constexpr int total = 48000 * 4;
            juce::AudioBuffer<float> result (2, total), block (2, 128);
            juce::MidiBuffer midi;
            for (int start = 0; start < total; start += 128)
            {
                set ("morph", (float) start / total);
                for (int i = 0; i < 128; ++i)
                {
                    const double t = (start + i) / 48000.0;
                    const double phase = juce::MathConstants<double>::twoPi * 110.0 * t;
                    const double env = 0.25 + 0.75 * std::exp (-8.0 * std::fmod (t, 0.5));
                    const float x = (float) (0.16 * env * (std::sin (phase) + 0.3 * std::sin (3 * phase)));
                    block.setSample (0, i, x); block.setSample (1, i, x);
                }
                processor.processBlock (block, midi);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 128; ++i)
                    {
                        const float x = block.getSample (c, i);
                        if (! std::isfinite (x) || std::abs (x) > trench::kFinalSafetyCeiling)
                            throw std::runtime_error ("audition ceiling");
                        result.setSample (c, start + i, x);
                    }
            }
            const auto name = juce::File (bodyFile).getFileNameWithoutExtension() + "-" + juce::String (setting)
                + "-input" + juce::String (drive, 1);
            report += name + "," + juce::String (drive) + ","
                + juce::String (juce::Decibels::gainToDecibels (result.getMagnitude (0, total)), 3) + ","
                + juce::String (juce::Decibels::gainToDecibels (result.getRMSLevel (0, 0, total)), 3) + "\n";
            std::unique_ptr<juce::OutputStream> stream = directory.getChildFile (name + ".wav").createOutputStream();
            if (! stream) throw std::runtime_error ("audition WAV open");
            juce::WavAudioFormat format;
            auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions {}.withSampleRate (48000)
                .withNumChannels (2).withBitsPerSample (24));
            if (! writer || ! writer->writeFromAudioSampleBuffer (result, 0, total)) throw std::runtime_error ("audition WAV write");
        }
    if (! directory.getChildFile ("measurements.csv").replaceWithText (report)) throw std::runtime_error ("audition report");
    std::printf ("%s", report.toRawUTF8());
}

inline void sweepAudition (const juce::File& directory, bool emu = false)
{
    if (! directory.createDirectory()) throw std::runtime_error ("sweep directory");
    juce::String report = "file,body,q,output_peak_dbfs,output_rms_dbfs\n";
    const auto renderCase = [&] (const juce::String& name, const char* bodyFile, float q)
    {
        juce::MemoryBlock bytes;
        if (! juce::File (TRENCH_TABLE_STITCH_ROOT).getChildFile (bodyFile).loadFileAsData (bytes)) throw std::runtime_error ("sweep body");
        auto values = trench::calibration::defaults();
        TrenchDspBridge bridge;
        bridge.prepare (44100, 128);
        if (! bridge.loadCartridgeBytes (bytes)) throw std::runtime_error ("sweep body load");
        bridge.applyCalibration (values);
        bridge.setInputDrive (1.0f);
        bridge.setOutputDrive (0.0f);
        TrenchParams params;
        params.q = q;
        params.poleDistortion = 0.0f;
        const int total = 44100 * 12;
        juce::AudioBuffer<float> result (2, total + 128), block (2, 128);
        result.clear();
        float morph[128];
        for (int start = 0; start < total; start += 128)
        {
            for (int i = 0; i < 128; ++i)
            {
                const double t = (start + i) / 44100.0;
                double saw = 0;
                for (int h = 1; h <= 60; ++h) saw += (std::sin (juce::MathConstants<double>::twoPi * 55.0 * h * t) + 0.5 * std::sin (juce::MathConstants<double>::twoPi * 55.3 * h * t)) / h;
                const float x = (float) (0.05 * saw);
                block.setSample (0, i, x);
                block.setSample (1, i, x);
                morph[i] = (float) juce::jlimit (0.0, 1.0, (t - 1.0) / 10.0);
            }
            bridge.processTrajectory (block, morph, params);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i)
                {
                    const float y = block.getSample (c, i);
                    if (! std::isfinite (y)) throw std::runtime_error ("nonfinite sweep audio");
                    result.setSample (c, start + i, trench::calibration::guard (y * 0.35f, 0.8f, trench::kFinalSafetyCeiling));
                }
        }
        report += name + "," + bodyFile + "," + juce::String (q, 2) + "," + juce::String (juce::Decibels::gainToDecibels (result.getMagnitude (0, total)), 1) + ","
            + juce::String (juce::Decibels::gainToDecibels (result.getRMSLevel (0, 0, total)), 1) + "\n";
        std::unique_ptr<juce::OutputStream> stream = directory.getChildFile (name + ".wav").createOutputStream();
        if (! stream) throw std::runtime_error ("sweep WAV open");
        juce::WavAudioFormat format;
        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions {}.withSampleRate (44100).withNumChannels (2).withBitsPerSample (24));
        if (! writer || ! writer->writeFromAudioSampleBuffer (result, 0, total))
            throw std::runtime_error ("sweep WAV write");
    };
    if (emu)
    {
        renderCase ("e1-talking_hedz-q100-spot47", "plugin/presets/p2k/talking_hedz.body240", 1.0f);
        renderCase ("e2-talking_hedz-q50-spot44", "plugin/presets/p2k/talking_hedz.body240", 0.5f);
        renderCase ("e3-megasweepz-q100-spot41", "plugin/presets/p2k/megasweepz.body240", 1.0f);
        renderCase ("e4-ubu_orator-q100-spot36", "plugin/presets/p2k/ubu_orator.body240", 1.0f);
        renderCase ("e5-acid_ravage-q50-spots29-79", "plugin/presets/p2k/acid_ravage.body240", 0.5f);
        renderCase ("e6-early_rizer-q100-spots54-69", "plugin/presets/p2k/early_rizer.body240", 1.0f);
        renderCase ("e7-dead_ringer-q100-spot47", "plugin/presets/p2k/dead_ringer.body240", 1.0f);
        if (! directory.getChildFile ("measurements.csv").replaceWithText (report)) throw std::runtime_error ("sweep report");
        std::printf ("%s", report.toRawUTF8());
        return;
    }
    renderCase ("01-high_rise-q100-spot53", "plugin/presets/bodies/xml_high_rise.body240", 1.0f);
    renderCase ("02-high_rise-q50-spot53", "plugin/presets/bodies/xml_high_rise.body240", 0.5f);
    renderCase ("03-cross_band-q100-spots30-65", "plugin/presets/bodies/xml_cross_band.body240", 1.0f);
    renderCase ("04-opium-q50-spots20-30-48-75", "plugin/presets/bodies/xml_opium.body240", 0.5f);
    renderCase ("05-drift-q100-spots27-41-61", "plugin/presets/bodies/xml_drift.body240", 1.0f);
    renderCase ("06-low_shape-q100-spot57", "plugin/presets/bodies/xml_low_shape.body240", 1.0f);
    renderCase ("07-crisp-q100-spots77-97", "plugin/presets/bodies/xml_crisp.body240", 1.0f);
    renderCase ("08-shine-q100-no-spots", "plugin/presets/bodies/xml_shine.body240", 1.0f);
    if (! directory.getChildFile ("measurements.csv").replaceWithText (report)) throw std::runtime_error ("sweep report");
    std::printf ("%s", report.toRawUTF8());
}
