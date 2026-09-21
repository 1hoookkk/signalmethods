#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TrenchBodyRoster.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
void setParam (PluginProcessor& p, const char* id, float denorm)
{
    if (auto* prm = p.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (denorm));
}
void savePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
}
void writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double rate)
{
    file.deleteFile();
    file.getParentDirectory().createDirectory();
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                   .withSampleRate (rate)
                                                   .withNumChannels (audio.getNumChannels())
                                                   .withBitsPerSample (24));
    writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf (stderr, "usage: TRENCH_MorphClip <body240> <outdir> [seconds=8] [fps=20] [scale=2] [hz=110]\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto bodyFile = cwd.getChildFile (argv[1]);
    const auto outDir = cwd.getChildFile (argv[2]);
    const double seconds = argc > 3 ? std::atof (argv[3]) : 8.0;
    const int fps = argc > 4 ? std::atoi (argv[4]) : 20;
    const float scale = argc > 5 ? (float) std::atof (argv[5]) : 2.0f;
    const double hz = argc > 6 ? std::atof (argv[6]) : 110.0;
    const double rate = 44100.0;
    const int block = 256;

    juce::MemoryBlock body;
    if (! bodyFile.loadFileAsData (body) || body.getSize() != 240)
    {
        std::fprintf (stderr, "not a 240-byte body: %s\n", bodyFile.getFullPathName().toRawUTF8());
        return 2;
    }
    outDir.createDirectory();

    PluginProcessor processor;
    processor.setPlayConfigDetails (2, 2, rate, block);
    processor.prepareToPlay (rate, block);

    trench::rescanBodyRoster();
    setParam (processor, ParamID::body, 1.0f);
    for (int i = 0; i < 200 && ! (processor.getLoadedBodyIndex() == 1 && processor.getLastLoadOk()); ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    if (! processor.installBodyBytes (body.getData(), body.getSize()))
    {
        std::fprintf (stderr, "installBodyBytes refused\n");
        return 1;
    }
    setParam (processor, ParamID::movePreset, 0.0f);
    setParam (processor, ParamID::keySnap, 0.0f);
    setParam (processor, ParamID::q, 0.0f);
    setParam (processor, ParamID::morph, 0.0f);

    auto* editor = processor.createEditorIfNeeded();
    juce::Component holder;
    holder.setSize (editor->getWidth(), editor->getHeight());
    holder.addAndMakeVisible (editor);
    holder.addToDesktop (juce::ComponentPeer::windowIsTemporary);
    holder.setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (600);

    const int frames = (int) std::lround (seconds * rate);
    const int perFrame = (int) std::lround (rate / fps);
    juce::AudioBuffer<float> render (2, frames);
    double phase = 0.0;
    for (int ch = 0; ch < 2; ++ch)
    {
        phase = 0.0;
        float* d = render.getWritePointer (ch);
        for (int n = 0; n < frames; ++n)
        {
            d[n] = (float) (0.3 * (2.0 * phase - 1.0));
            phase += hz / rate;
            if (phase >= 1.0) phase -= 1.0;
        }
    }

    juce::MidiBuffer midi;
    int frameIndex = 0;
    int start = 0;
    while (start < frames)
    {
        const double t = (double) start / rate;
        const float morph = (float) (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * t / seconds));
        setParam (processor, ParamID::morph, morph);
        const int end = juce::jmin (frames, start + perFrame);
        for (int s = start; s < end; s += block)
        {
            const int count = juce::jmin (block, end - s);
            float* chans[2] = { render.getWritePointer (0) + s, render.getWritePointer (1) + s };
            juce::AudioBuffer<float> b (chans, 2, count);
            midi.clear();
            processor.processBlock (b, midi);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        holder.repaint();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        char name[32];
        std::snprintf (name, sizeof name, "frame_%04d.png", frameIndex++);
        savePng (holder.createComponentSnapshot (holder.getLocalBounds(), true, scale), outDir.getChildFile (name));
        start = end;
    }
    processor.releaseResources();

    double peak = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < frames; ++n)
            peak = std::max (peak, (double) std::abs (render.getSample (ch, n)));
    writeWav (outDir.getChildFile ("morph.wav"), render, rate);
    std::printf ("frames %d  wav peak %.2f dBFS  body %s\n", frameIndex, 20.0 * std::log10 (peak), bodyFile.getFileName().toRawUTF8());

    processor.editorBeingDeleted (editor);
    holder.removeChildComponent (editor);
    delete editor;
    return 0;
}
