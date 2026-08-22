#include "PluginProcessor.h"
#include "parameters/TrenchParameters.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdio>
#include <memory>

namespace
{
struct Arg { const char* id; float value; };
}

int main (int argc, char** argv)
{
    if (argc < 4)
    {
        std::printf ("usage: RenderNull in.wav body240 out.wav [id=value ...]  (ids: morph q chew amount slamDrive preamp envAmount track movePreset keySnap; morphRamp=<seconds> drives morph 0->1 linearly per block)\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (argv[1])));
    if (reader == nullptr) { std::printf ("cannot read %s\n", argv[1]); return 1; }
    const auto rate = reader->sampleRate;
    const auto frames = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> in ((int) reader->numChannels, frames);
    reader->read (&in, 0, frames, 0, true, true);

    juce::MemoryBlock body;
    if (! juce::File (argv[2]).loadFileAsData (body) || body.getSize() != 240) { std::printf ("bad body %s\n", argv[2]); return 1; }

    PluginProcessor p;
    if (! p.installBodyBytes (body.getData(), body.getSize(), 44100.0)) { std::printf ("installBodyBytes failed\n"); return 1; }
    const Arg defaults[] = {
        { ParamID::morph, 0.0f }, { ParamID::q, 0.5f }, { ParamID::chew, 0.0f }, { ParamID::amount, 1.0f },
        { ParamID::slamDrive, 0.0f }, { ParamID::preamp, 0.0f }, { ParamID::envAmount, 0.0f }, { ParamID::track, 0.0f },
        { ParamID::movePreset, 0.0f }, { ParamID::keySnap, 0.0f } };
    auto set = [&p] (const juce::String& id, float v)
    {
        if (auto* param = p.apvts.getParameter (id)) { param->setValueNotifyingHost (v); return true; }
        std::printf ("no parameter %s\n", id.toRawUTF8()); return false;
    };
    for (const auto& a : defaults) set (a.id, a.value);
    double morphRampSeconds = 0.0;
    for (int i = 4; i < argc; ++i)
    {
        const juce::String s (argv[i]);
        const auto eq = s.indexOfChar ('=');
        if (eq <= 0) continue;
        if (s.substring (0, eq) == "morphRamp") { morphRampSeconds = s.substring (eq + 1).getDoubleValue(); continue; }
        set (s.substring (0, eq), s.substring (eq + 1).getFloatValue());
    }
    constexpr int block = 512;
    p.setPlayConfigDetails (2, 2, rate, block);
    p.prepareToPlay (rate, block);
    juce::AudioBuffer<float> out (2, frames);
    for (int ch = 0; ch < 2; ++ch)
        out.copyFrom (ch, 0, in, juce::jmin (ch, in.getNumChannels() - 1), 0, frames);
    juce::MidiBuffer midi;
    for (int start = 0; start < frames; start += block)
    {
        const int n = juce::jmin (block, frames - start);
        if (morphRampSeconds > 0.0)
            set (ParamID::morph, (float) juce::jmin (1.0, (double) start / rate / morphRampSeconds));
        float* chans[2] = { out.getWritePointer (0) + start, out.getWritePointer (1) + start };
        juce::AudioBuffer<float> slice (chans, 2, n);
        p.processBlock (slice, midi);
    }
    p.releaseResources();

    juce::File outFile (argv[3]);
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (outFile), rate, 2, 32, {}, 0));
    if (writer == nullptr) { std::printf ("cannot write %s\n", argv[3]); return 1; }
    writer->writeFromAudioSampleBuffer (out, 0, frames);
    writer.reset();
    std::printf ("rendered %d frames at %.0f Hz -> %s (load ok %d)\n", frames, rate, argv[3], (int) p.getLastLoadOk());
    return 0;
}
