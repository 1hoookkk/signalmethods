#include "PluginProcessor.h"
#include "TrenchBodyRoster.h"
#include "parameters/CurveMap.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>

namespace
{
constexpr int kBlockSize = 512;
constexpr int kLevelCount = 5;

struct Options
{
    juce::String input;
    juce::String axis;
    juce::String body;
    juce::String outputDir;
    double bpm = 0.0;
    float low = 0.0f;
    float high = 1.0f;
};

struct AxisSpec
{
    const char* name;
    const char* parameter;
    trench::curves::Axis curveAxis;
};

struct Metrics
{
    double rms = 0.0;
    double peak = 0.0;
};

struct Render
{
    Render (int channels, int frames) : audio (channels, frames) {}

    juce::AudioBuffer<float> audio;
    float knob = 0.0f;
    float internal = 0.0f;
    double rmsBeforeMatch = 0.0;
    double levelMatchGain = 1.0;
    double commonScale = 1.0;
    double finalRms = 0.0;
    double finalPeak = 0.0;
    juce::File outputFile;
};

void usage()
{
    std::fprintf (stderr,
                  "usage: TRENCH_Stimuli --input <wav> --bpm <positive> "
                  "--axis <output|input|z|morph|q|follow> --body <name|index> "
                  "--output-dir <dir> [--low <0..1>] [--high <0..1>]\n");
}

bool parseFiniteDouble (const juce::String& text, double& value)
{
    const char* utf8 = text.toRawUTF8();
    char* end = nullptr;
    value = std::strtod (utf8, &end);
    return end != utf8 && *end == '\0' && std::isfinite (value);
}

bool parseArguments (int argc, char** argv, Options& options, juce::String& error)
{
    bool haveInput = false, haveBpm = false, haveAxis = false, haveBody = false, haveOutput = false;
    bool haveLow = false, haveHigh = false;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String option = juce::String::fromUTF8 (argv[i]);
        if (! option.startsWith ("--"))
        {
            error = "unexpected positional argument: " + option;
            return false;
        }
        if (i + 1 >= argc)
        {
            error = "missing value for " + option;
            return false;
        }
        const juce::String value = juce::String::fromUTF8 (argv[++i]);
        const auto duplicate = [&] (bool& flag)
        {
            if (flag)
            {
                error = "option supplied more than once: " + option;
                return true;
            }
            flag = true;
            return false;
        };

        if (option == "--input")
        {
            if (duplicate (haveInput)) return false;
            options.input = value;
        }
        else if (option == "--bpm")
        {
            if (duplicate (haveBpm)) return false;
            if (! parseFiniteDouble (value, options.bpm) || options.bpm <= 0.0)
            {
                error = "--bpm must be a positive finite number";
                return false;
            }
        }
        else if (option == "--axis")
        {
            if (duplicate (haveAxis)) return false;
            options.axis = value.toLowerCase();
        }
        else if (option == "--body")
        {
            if (duplicate (haveBody)) return false;
            options.body = value;
        }
        else if (option == "--output-dir")
        {
            if (duplicate (haveOutput)) return false;
            options.outputDir = value;
        }
        else if (option == "--low" || option == "--high")
        {
            bool& seen = option == "--low" ? haveLow : haveHigh;
            if (duplicate (seen)) return false;
            double parsed = 0.0;
            if (! parseFiniteDouble (value, parsed) || parsed < 0.0 || parsed > 1.0)
            {
                error = option + " must be in the inclusive range 0..1";
                return false;
            }
            (option == "--low" ? options.low : options.high) = (float) parsed;
        }
        else
        {
            error = "unknown option: " + option;
            return false;
        }
    }

    if (! haveInput || ! haveBpm || ! haveAxis || ! haveBody || ! haveOutput)
    {
        error = "--input, --bpm, --axis, --body, and --output-dir are required";
        return false;
    }
    if (options.input.isEmpty() || options.body.isEmpty() || options.outputDir.isEmpty())
    {
        error = "paths and body name must not be empty";
        return false;
    }
    if (options.low >= options.high)
    {
        error = "--low must be less than --high";
        return false;
    }
    return true;
}

const AxisSpec* findAxis (const juce::String& name)
{
    static constexpr AxisSpec axes[] = {
        { "output", ParamID::slamDrive, trench::curves::Axis::slam },
        { "input",  ParamID::preamp,    trench::curves::Axis::preamp },
        { "z",      ParamID::chew,      trench::curves::Axis::bite },
        { "morph",  ParamID::morph,     trench::curves::Axis::morph },
        { "q",      ParamID::q,         trench::curves::Axis::q },
        { "follow", ParamID::envAmount, trench::curves::Axis::follow },
    };
    for (const auto& axis : axes)
        if (name == axis.name)
            return &axis;
    return nullptr;
}

int resolveBody (const juce::String& requested)
{
    const char* utf8 = requested.toRawUTF8();
    char* end = nullptr;
    const long numeric = std::strtol (utf8, &end, 10);
    if (end != utf8 && *end == '\0')
        return numeric >= 0 && numeric < trench::bodyCount() ? (int) numeric : -1;

    int count = 0;
    trench::bodyRoster (count);
    for (int index = 0; index < count; ++index)
        if (requested.equalsIgnoreCase (trench::bodyDisplayName (index)))
            return index;
    return -1;
}

void setParameter (PluginProcessor& processor, const char* id, float denormalized)
{
    auto* parameter = processor.apvts.getParameter (id);
    if (parameter == nullptr)
        throw std::runtime_error ((juce::String ("processor has no parameter ") + id).toStdString());
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (denormalized));
}

bool waitForBody (PluginProcessor& processor, int bodyIndex)
{
    constexpr int kAttempts = 200;
    for (int attempt = 0; attempt < kAttempts; ++attempt)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
        if (processor.getLoadedBodyIndex() == bodyIndex && processor.getLastLoadOk())
            return true;
    }
    return false;
}

Metrics measure (const juce::AudioBuffer<float>& audio)
{
    long double sumSquares = 0.0;
    double peak = 0.0;
    const int channels = audio.getNumChannels();
    const int frames = audio.getNumSamples();
    for (int channel = 0; channel < channels; ++channel)
    {
        const float* samples = audio.getReadPointer (channel);
        for (int frame = 0; frame < frames; ++frame)
        {
            const double sample = samples[frame];
            if (! std::isfinite (sample))
                throw std::runtime_error ("processor produced a non-finite audio sample");
            sumSquares += sample * sample;
            peak = std::max (peak, std::abs (sample));
        }
    }
    const auto count = (long double) channels * (long double) frames;
    return { count > 0.0 ? std::sqrt ((double) (sumSquares / count)) : 0.0, peak };
}

void writeFloatWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate)
{
    if (file.exists() && ! file.deleteFile())
        throw std::runtime_error (("cannot replace output file: " + file.getFullPathName()).toStdString());

    auto fileStream = file.createOutputStream();
    if (fileStream == nullptr || ! fileStream->openedOk())
        throw std::runtime_error (("cannot open output file: " + file.getFullPathName()).toStdString());
    std::unique_ptr<juce::OutputStream> stream = std::move (fileStream);

    juce::WavAudioFormat wav;
    const auto writerOptions = juce::AudioFormatWriterOptions {}
                                   .withSampleRate (sampleRate)
                                   .withNumChannels (audio.getNumChannels())
                                   .withBitsPerSample (32)
                                   .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer = wav.createWriterFor (stream, writerOptions);
    if (writer == nullptr)
        throw std::runtime_error (("cannot create 32-bit float WAV writer: " + file.getFullPathName()).toStdString());
    if (! writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()))
        throw std::runtime_error (("failed while writing output file: " + file.getFullPathName()).toStdString());
}

juce::var renderToJson (const Render& render, int index)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("index", index);
    object->setProperty ("knob", (double) render.knob);
    object->setProperty ("internal", (double) render.internal);
    object->setProperty ("rmsBeforeMatch", render.rmsBeforeMatch);
    object->setProperty ("levelMatchGain", render.levelMatchGain);
    object->setProperty ("commonScale", render.commonScale);
    object->setProperty ("finalRms", render.finalRms);
    object->setProperty ("finalPeak", render.finalPeak);
    object->setProperty ("wav", render.outputFile.getFullPathName());
    return juce::var (object);
}
}

int main (int argc, char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI juceInit;
        Options options;
        juce::String error;
        if (! parseArguments (argc, argv, options, error))
        {
            std::fprintf (stderr, "TRENCH_Stimuli: %s\n", error.toRawUTF8());
            usage();
            return 2;
        }

        const auto* axis = findAxis (options.axis);
        if (axis == nullptr)
        {
            std::fprintf (stderr, "TRENCH_Stimuli: unsupported axis: %s\n", options.axis.toRawUTF8());
            usage();
            return 2;
        }

        trench::rescanBodyRoster();
        const int bodyIndex = resolveBody (options.body);
        if (bodyIndex < 0)
        {
            std::fprintf (stderr, "TRENCH_Stimuli: body must be an in-range index or exact display name: %s\n",
                          options.body.toRawUTF8());
            return 2;
        }
        const auto resolvedBody = trench::bodyDisplayName (bodyIndex);

        const juce::File inputFile (options.input);
        if (! inputFile.existsAsFile() || ! inputFile.hasFileExtension ("wav"))
        {
            std::fprintf (stderr, "TRENCH_Stimuli: input must be a readable .wav file: %s\n",
                          inputFile.getFullPathName().toRawUTF8());
            return 2;
        }

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (inputFile));
        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        {
            std::fprintf (stderr, "TRENCH_Stimuli: WAV is unreadable or empty: %s\n",
                          inputFile.getFullPathName().toRawUTF8());
            return 2;
        }
        if (reader->numChannels < 1 || reader->numChannels > 2)
        {
            std::fprintf (stderr, "TRENCH_Stimuli: WAV must have one or two channels (found %u)\n",
                          reader->numChannels);
            return 2;
        }
        if (reader->lengthInSamples > std::numeric_limits<int>::max())
        {
            std::fprintf (stderr, "TRENCH_Stimuli: WAV is too long for an in-memory session render\n");
            return 2;
        }

        const int channels = (int) reader->numChannels;
        const int frames = (int) reader->lengthInSamples;
        const double sampleRate = reader->sampleRate;
        juce::AudioBuffer<float> source (channels, frames);
        if (! reader->read (&source, 0, frames, 0, true, true))
        {
            std::fprintf (stderr, "TRENCH_Stimuli: failed while reading WAV samples\n");
            return 2;
        }
        measure (source); // Validate source samples before starting any render.

        const juce::File outputDirectory (options.outputDir);
        if (! outputDirectory.exists() && ! outputDirectory.createDirectory())
        {
            std::fprintf (stderr, "TRENCH_Stimuli: cannot create output directory: %s\n",
                          outputDirectory.getFullPathName().toRawUTF8());
            return 2;
        }
        if (! outputDirectory.isDirectory())
        {
            std::fprintf (stderr, "TRENCH_Stimuli: output path is not a directory: %s\n",
                          outputDirectory.getFullPathName().toRawUTF8());
            return 2;
        }

        std::array<std::unique_ptr<Render>, kLevelCount> renders;
        for (int level = 0; level < kLevelCount; ++level)
        {
            auto render = std::make_unique<Render> (channels, frames);
            render->knob = options.low + (float) level * (options.high - options.low) / (float) (kLevelCount - 1);
            render->internal = trench::curves::curveMap (axis->curveAxis, render->knob);
            if (! std::isfinite (render->internal))
                throw std::runtime_error ("shipping curve produced a non-finite internal value");

            render->audio.makeCopyOf (source, true);
            PluginProcessor processor;
            processor.setPlayConfigDetails (channels, channels, sampleRate, kBlockSize);
            processor.prepareToPlay (sampleRate, kBlockSize);
            setParameter (processor, ParamID::body, (float) bodyIndex);
            if (! waitForBody (processor, bodyIndex))
                throw std::runtime_error (("body did not load successfully: " + resolvedBody).toStdString());
            setParameter (processor, axis->parameter, render->knob);

            juce::MidiBuffer midi;
            for (int start = 0; start < frames; start += kBlockSize)
            {
                const int count = juce::jmin (kBlockSize, frames - start);
                float* channelData[2] = { render->audio.getWritePointer (0) + start, nullptr };
                if (channels == 2)
                    channelData[1] = render->audio.getWritePointer (1) + start;
                juce::AudioBuffer<float> block (channelData, channels, count);
                midi.clear();
                processor.processBlock (block, midi);
            }
            processor.releaseResources();

            const auto rawMetrics = measure (render->audio);
            render->rmsBeforeMatch = rawMetrics.rms;
            render->finalPeak = rawMetrics.peak;
            renders[(size_t) level] = std::move (render);
        }

        if (options.axis == "output")
        {
            const double referenceRms = renders[0]->rmsBeforeMatch;
            if (referenceRms <= 0.0)
                throw std::runtime_error ("OUTPUT level-0 standard is silent; RMS matching is impossible");
            for (auto& render : renders)
            {
                if (render->rmsBeforeMatch <= 0.0)
                    throw std::runtime_error ("an OUTPUT comparison is silent; RMS matching is impossible");
                render->levelMatchGain = referenceRms / render->rmsBeforeMatch;
            }
        }

        double largestMatchedPeak = 0.0;
        for (const auto& render : renders)
            largestMatchedPeak = std::max (largestMatchedPeak, render->finalPeak * render->levelMatchGain);
        const double commonScale = largestMatchedPeak > 0.99 ? 0.99 / largestMatchedPeak : 1.0;
        if (! std::isfinite (commonScale) || commonScale <= 0.0 || commonScale > 1.0)
            throw std::runtime_error ("could not derive a finite anti-clip scale");

        juce::Array<juce::var> levelJson;
        for (int level = 0; level < kLevelCount; ++level)
        {
            auto& render = *renders[(size_t) level];
            render.commonScale = commonScale;
            render.audio.applyGain ((float) (render.levelMatchGain * commonScale));
            const auto finalMetrics = measure (render.audio);
            render.finalRms = finalMetrics.rms;
            render.finalPeak = finalMetrics.peak;
            if (render.finalPeak > 0.990001)
                throw std::runtime_error ("anti-clip scaling failed to hold a render below 0.99 peak");

            const auto filename = juce::String (axis->name) + "_L" + juce::String (level)
                                + "_" + juce::String (render.knob, 3) + ".wav";
            render.outputFile = outputDirectory.getChildFile (filename);
            writeFloatWav (render.outputFile, render.audio, sampleRate);
            levelJson.add (renderToJson (render, level));
            std::printf ("L%d knob %.3f internal %.9g rms %.9g peak %.9g -> %s\n",
                         level, render.knob, render.internal, render.finalRms, render.finalPeak,
                         render.outputFile.getFullPathName().toRawUTF8());
        }

        auto* manifestObject = new juce::DynamicObject();
        manifestObject->setProperty ("source", inputFile.getFullPathName());
        manifestObject->setProperty ("bpm", options.bpm);
        manifestObject->setProperty ("axis", options.axis);
        manifestObject->setProperty ("requestedBody", options.body);
        manifestObject->setProperty ("resolvedBody", resolvedBody);
        manifestObject->setProperty ("resolvedBodyIndex", bodyIndex);
        manifestObject->setProperty ("sampleRate", sampleRate);
        manifestObject->setProperty ("channels", channels);
        manifestObject->setProperty ("frames", frames);
        manifestObject->setProperty ("low", (double) options.low);
        manifestObject->setProperty ("high", (double) options.high);
        manifestObject->setProperty ("processorBuildIdentifier", PluginProcessor::processorBuildIdentifier());
        manifestObject->setProperty ("defaults", "All non-target controls used their declared APVTS defaults; Movement remained OFF.");
        manifestObject->setProperty ("levels", juce::var (levelJson));

        const juce::File manifest = outputDirectory.getChildFile ("manifest.json");
        if (! manifest.replaceWithText (juce::JSON::toString (juce::var (manifestObject), true)))
            throw std::runtime_error (("cannot write manifest: " + manifest.getFullPathName()).toStdString());

        std::printf ("axis %s  body %s [%d]  %.0f Hz  %d ch  %d frames  BPM %.6g\n",
                     axis->name, resolvedBody.toRawUTF8(), bodyIndex, sampleRate, channels, frames, options.bpm);
        std::printf ("common scale %.9g\nmanifest %s\n", commonScale, manifest.getFullPathName().toRawUTF8());
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::fprintf (stderr, "TRENCH_Stimuli: %s\n", exception.what());
        return 1;
    }
}
