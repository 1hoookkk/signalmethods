#include "PluginProcessor.h"
#include "TrenchBodyRoster.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
struct Options
{
    juce::String input;
    juce::String output;
    juce::String body;
    juce::String bodyFile;
    juce::String morphSpec { "0:0,end:1" };
    juce::String qSpec;
    double q = 0.0;
    double bite = 0.0;
    double distortion = -1.0;
    double preamp = 0.0;
    double slam = 0.0;
    double seconds = 0.0;
    double rate = 44100.0;
    int block = 256;
    bool haveBite = false;
    bool havePreamp = false;
    bool haveSlam = false;
    bool ring = false;
    bool haveSeconds = false;
    int move = 0;
    int length = -1;
    double bpm = 0.0;
};

class RenderPlayHead final : public juce::AudioPlayHead
{
public:
    double bpm = 120.0;
    double sampleRate = 44100.0;
    juce::int64 sample = 0;
    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        juce::AudioPlayHead::PositionInfo info;
        info.setBpm (bpm);
        info.setTimeInSamples (sample);
        info.setTimeInSeconds ((double) sample / sampleRate);
        info.setPpqPosition ((double) sample / sampleRate * bpm / 60.0);
        info.setIsPlaying (true);
        info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        return info;
    }
};

struct MorphPoint
{
    double time = 0.0;
    double value = 0.0;
};

void usage()
{
    std::fprintf (stderr,
                  "usage: TRENCH_Render --in <wav> --out <wav> --body <display name substring> | --bodyfile <body240>\n"
                  "                     [--morph \"t:v,t:v,...\"] [--q v] [--qcurve \"t:v,...\"] [--bite v] [--input v]\n"
                  "                     [--output v] [--seconds s] [--rate 44100] [--block 256]\n");
}

bool parseFiniteDouble (const juce::String& text, double& value)
{
    const char* utf8 = text.toRawUTF8();
    char* end = nullptr;
    value = std::strtod (utf8, &end);
    return end != utf8 && *end == '\0' && std::isfinite (value);
}

bool parseUnit (const juce::String& option, const juce::String& text, double& value, juce::String& error)
{
    if (! parseFiniteDouble (text, value) || value < 0.0 || value > 1.0)
    {
        error = option + " must be in the inclusive range 0..1";
        return false;
    }
    return true;
}

bool parseArguments (int argc, char** argv, Options& options, juce::String& error)
{
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

        if (option == "--in")
            options.input = value;
        else if (option == "--out")
            options.output = value;
        else if (option == "--body")
            options.body = value;
        else if (option == "--bodyfile")
            options.bodyFile = value;
        else if (option == "--morph")
            options.morphSpec = value;
        else if (option == "--q")
        {
            if (! parseUnit (option, value, options.q, error))
                return false;
        }
        else if (option == "--qcurve")
            options.qSpec = value;
        else if (option == "--bite")
        {
            if (! parseUnit (option, value, options.bite, error))
                return false;
            options.haveBite = true;
        }
        else if (option == "--distortion")
        {
            if (! parseUnit (option, value, options.distortion, error))
                return false;
        }
        else if (option == "--ring")
        {
            options.ring = value.getIntValue() != 0;
        }
        else if (option == "--move")
        {
            options.move = value.getIntValue();
            if (options.move < 0 || options.move > 64)
            {
                error = "--move must be a preset index 0..64";
                return false;
            }
        }
        else if (option == "--length")
        {
            options.length = value.getIntValue();
            if (options.length < 0 || options.length > 4)
            {
                error = "--length must be 0..4 (1/4 bar .. 4 bars)";
                return false;
            }
        }
        else if (option == "--bpm")
        {
            double parsed = 0.0;
            if (! parseFiniteDouble (value, parsed) || parsed < 20.0 || parsed > 400.0)
            {
                error = "--bpm must be between 20 and 400";
                return false;
            }
            options.bpm = parsed;
        }
        else if (option == "--input")
        {
            double db = 0.0;
            if (! parseFiniteDouble (value, db) || db < -24.0 || db > 24.0)
            {
                error = option + " must be a level in dB from -24 to 24";
                return false;
            }
            options.preamp = db;
            options.havePreamp = true;
        }
        else if (option == "--output")
        {
            double db = 0.0;
            if (! parseFiniteDouble (value, db) || db < -24.0 || db > 24.0)
            {
                error = option + " must be a level in dB from -24 to 24";
                return false;
            }
            options.slam = db;
            options.haveSlam = true;
        }
        else if (option == "--seconds")
        {
            if (! parseFiniteDouble (value, options.seconds) || options.seconds <= 0.0)
            {
                error = "--seconds must be a positive finite number";
                return false;
            }
            options.haveSeconds = true;
        }
        else if (option == "--rate")
        {
            if (! parseFiniteDouble (value, options.rate) || options.rate < 8000.0 || options.rate > 384000.0)
            {
                error = "--rate must be between 8000 and 384000";
                return false;
            }
        }
        else if (option == "--block")
        {
            options.block = value.getIntValue();
            if (options.block < 16 || options.block > 8192)
            {
                error = "--block must be between 16 and 8192";
                return false;
            }
        }
        else
        {
            error = "unknown option: " + option;
            return false;
        }
    }

    if (options.input.isEmpty() || options.output.isEmpty() || (options.body.isEmpty() && options.bodyFile.isEmpty()))
    {
        error = "--in, --out and --body or --bodyfile are required";
        return false;
    }
    return true;
}

bool parseMorphSpec (const juce::String& spec, double clipSeconds, std::vector<MorphPoint>& points, juce::String& error)
{
    points.clear();
    const auto pairs = juce::StringArray::fromTokens (spec, ",", "");
    for (const auto& raw : pairs)
    {
        const auto text = raw.trim();
        if (text.isEmpty())
            continue;
        const int colon = text.indexOfChar (':');
        if (colon <= 0 || colon == text.length() - 1)
        {
            error = "morph point must be time:value, got \"" + text + "\"";
            return false;
        }
        const auto timeText = text.substring (0, colon).trim();
        const auto valueText = text.substring (colon + 1).trim();

        MorphPoint point;
        if (timeText.equalsIgnoreCase ("end"))
            point.time = clipSeconds;
        else if (! parseFiniteDouble (timeText, point.time) || point.time < 0.0)
        {
            error = "morph time must be a non-negative number or \"end\", got \"" + timeText + "\"";
            return false;
        }
        if (! parseFiniteDouble (valueText, point.value) || point.value < 0.0 || point.value > 1.0)
        {
            error = "morph value must be in 0..1, got \"" + valueText + "\"";
            return false;
        }
        points.push_back (point);
    }

    if (points.empty())
    {
        error = "morph curve has no points";
        return false;
    }
    std::stable_sort (points.begin(), points.end(),
                      [] (const MorphPoint& a, const MorphPoint& b) { return a.time < b.time; });
    return true;
}

double morphAt (const std::vector<MorphPoint>& points, double seconds)
{
    if (seconds <= points.front().time)
        return points.front().value;
    if (seconds >= points.back().time)
        return points.back().value;
    for (size_t i = 1; i < points.size(); ++i)
    {
        const auto& a = points[i - 1];
        const auto& b = points[i];
        if (seconds <= b.time)
        {
            const double span = b.time - a.time;
            if (span <= 0.0)
                return b.value;
            const double t = (seconds - a.time) / span;
            return a.value + t * (b.value - a.value);
        }
    }
    return points.back().value;
}

int resolveBody (const juce::String& requested)
{
    int count = 0;
    trench::bodyRoster (count);
    for (int index = 0; index < count; ++index)
        if (trench::bodyDisplayName (index).containsIgnoreCase (requested))
            return index;
    return -1;
}

void listRoster()
{
    int count = 0;
    trench::bodyRoster (count);
    std::fprintf (stderr, "roster (%d entries):\n", count);
    for (int index = 0; index < count; ++index)
        std::fprintf (stderr, "  [%d] %s\n", index, trench::bodyDisplayName (index).toRawUTF8());
}

void setParameter (PluginProcessor& processor, const char* id, float denormalized)
{
    auto* parameter = processor.apvts.getParameter (id);
    if (parameter == nullptr)
        throw std::runtime_error ((juce::String ("processor has no parameter ") + id).toStdString());
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (denormalized));
}

float parameterDefault (PluginProcessor& processor, const char* id)
{
    auto* parameter = processor.apvts.getParameter (id);
    if (parameter == nullptr)
        throw std::runtime_error ((juce::String ("processor has no parameter ") + id).toStdString());
    return parameter->convertFrom0to1 (parameter->getDefaultValue());
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

double toDb (double linear)
{
    return linear > 0.0 ? 20.0 * std::log10 (linear) : -std::numeric_limits<double>::infinity();
}

void writeFloatWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate)
{
    if (file.exists() && ! file.deleteFile())
        throw std::runtime_error (("cannot replace output file: " + file.getFullPathName()).toStdString());
    file.getParentDirectory().createDirectory();

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

std::vector<float> readMonoAtRate (const juce::File& file, double targetRate, double& sourceRate, int& sourceChannels)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        throw std::runtime_error (("WAV is unreadable or empty: " + file.getFullPathName()).toStdString());
    if (reader->lengthInSamples > std::numeric_limits<int>::max() / 4)
        throw std::runtime_error ("WAV is too long for an in-memory render");

    sourceRate = reader->sampleRate;
    sourceChannels = (int) reader->numChannels;
    const int frames = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> source ((int) juce::jmax (1u, reader->numChannels), frames);
    if (! reader->read (&source, 0, frames, 0, true, true))
        throw std::runtime_error ("failed while reading WAV samples");

    std::vector<float> mono ((size_t) frames, 0.0f);
    const int channels = source.getNumChannels();
    for (int channel = 0; channel < channels; ++channel)
    {
        const float* samples = source.getReadPointer (channel);
        for (int frame = 0; frame < frames; ++frame)
            mono[(size_t) frame] += samples[frame];
    }
    if (channels > 1)
        for (auto& sample : mono)
            sample /= (float) channels;

    for (const auto sample : mono)
        if (! std::isfinite (sample))
            throw std::runtime_error ("source WAV holds a non-finite sample");

    if (std::abs (sourceRate - targetRate) < 1.0e-6)
        return mono;

    const double ratio = sourceRate / targetRate;
    const int outFrames = (int) std::floor ((double) frames / ratio);
    if (outFrames <= 0)
        throw std::runtime_error ("resampling produced no samples");
    std::vector<float> padded = mono;
    padded.resize ((size_t) frames + 64, 0.0f);
    std::vector<float> resampled ((size_t) outFrames, 0.0f);
    juce::LagrangeInterpolator interpolator;
    interpolator.reset();
    interpolator.process (ratio, padded.data(), resampled.data(), outFrames);
    return resampled;
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
            std::fprintf (stderr, "TRENCH_Render: %s\n", error.toRawUTF8());
            usage();
            return 2;
        }

        const juce::File inputFile = juce::File::getCurrentWorkingDirectory().getChildFile (options.input);
        if (! inputFile.existsAsFile())
        {
            std::fprintf (stderr, "TRENCH_Render: input WAV not found: %s\n",
                          inputFile.getFullPathName().toRawUTF8());
            return 2;
        }
        const juce::File outputFile = juce::File::getCurrentWorkingDirectory().getChildFile (options.output);

        trench::rescanBodyRoster();
        const int bodyIndex = options.body.isEmpty() ? 1 : resolveBody (options.body);
        if (bodyIndex < 0)
        {
            std::fprintf (stderr, "TRENCH_Render: no roster body contains \"%s\"\n", options.body.toRawUTF8());
            listRoster();
            return 2;
        }
        const auto bodyName = trench::bodyDisplayName (bodyIndex);
        juce::MemoryBlock bodyBytes;
        if (options.bodyFile.isNotEmpty())
        {
            const juce::File bodyFile = juce::File::getCurrentWorkingDirectory().getChildFile (options.bodyFile);
            if (! bodyFile.loadFileAsData (bodyBytes) || bodyBytes.getSize() != 240)
            {
                std::fprintf (stderr, "TRENCH_Render: not a 240-byte body: %s\n", bodyFile.getFullPathName().toRawUTF8());
                return 2;
            }
        }

        double sourceRate = 0.0;
        int sourceChannels = 0;
        auto mono = readMonoAtRate (inputFile, options.rate, sourceRate, sourceChannels);
        if (options.haveSeconds)
        {
            const auto wanted = (size_t) juce::jmax (1, (int) std::floor (options.seconds * options.rate));
            if (wanted < mono.size())
                mono.resize (wanted);
        }
        const int frames = (int) mono.size();
        if (frames <= 0)
        {
            std::fprintf (stderr, "TRENCH_Render: nothing to render\n");
            return 2;
        }
        const double clipSeconds = (double) frames / options.rate;

        std::vector<MorphPoint> morphCurve;
        if (! parseMorphSpec (options.morphSpec, clipSeconds, morphCurve, error))
        {
            std::fprintf (stderr, "TRENCH_Render: %s\n", error.toRawUTF8());
            return 2;
        }
        std::vector<MorphPoint> qCurve;
        if (options.qSpec.isNotEmpty() && ! parseMorphSpec (options.qSpec, clipSeconds, qCurve, error))
        {
            std::fprintf (stderr, "TRENCH_Render: %s\n", error.toRawUTF8());
            return 2;
        }

        PluginProcessor processor;
        RenderPlayHead playHead;
        playHead.bpm = options.bpm > 0.0 ? options.bpm : 120.0;
        playHead.sampleRate = options.rate;
        if (options.bpm > 0.0)
            processor.setPlayHead (&playHead);
        processor.setPlayConfigDetails (2, 2, options.rate, options.block);
        processor.prepareToPlay (options.rate, options.block);

        setParameter (processor, ParamID::body, (float) bodyIndex);
        if (! waitForBody (processor, bodyIndex))
            throw std::runtime_error (("body did not load: " + bodyName).toStdString());
        if (bodyBytes.getSize() == 240 && ! processor.installBodyBytes (bodyBytes.getData(), bodyBytes.getSize()))
            throw std::runtime_error (("installBodyBytes refused: " + options.bodyFile).toStdString());

        setParameter (processor, ParamID::movePreset, (float) options.move);
        if (options.length >= 0)
            setParameter (processor, ParamID::moveLength, (float) options.length);
        setParameter (processor, ParamID::keySnap, 0.0f);
        setParameter (processor, ParamID::q, (float) (qCurve.empty() ? options.q : morphAt (qCurve, 0.0)));

        const bool hasBite = processor.apvts.getParameter (ParamID::chew) != nullptr;
        const bool hasSlam = processor.apvts.getParameter (ParamID::slamDrive) != nullptr;
        const float biteValue = ! hasBite ? 0.0f : options.haveBite ? (float) options.bite
                                                 : parameterDefault (processor, ParamID::chew);
        const float preampValue = options.havePreamp ? (float) options.preamp
                                                     : parameterDefault (processor, ParamID::preamp);
        const float slamValue = ! hasSlam ? 0.0f : options.haveSlam ? (float) options.slam
                                                                    : parameterDefault (processor, ParamID::slamDrive);
        if (hasBite)
            setParameter (processor, ParamID::chew, biteValue);
        setParameter (processor, ParamID::preamp, preampValue);
        if (options.distortion >= 0.0)
            setParameter (processor, ParamID::distortion, (float) options.distortion);
        if (hasSlam)
            setParameter (processor, ParamID::slamDrive, slamValue);
        if (options.haveSlam)
            setParameter (processor, ParamID::output, (float) options.slam);
        setParameter (processor, ParamID::morph, (float) morphAt (morphCurve, 0.0));
        processor.dspBridge.setRingLeveller (options.ring);
        if (std::getenv ("TRENCH_NO_OUTPUT_MACKITY") != nullptr) { auto b = processor.dspBridge.getBypass(); b.outputDesk = false; processor.dspBridge.setBypass (b); }
        if (const char* stageSat = std::getenv ("TRENCH_STAGE_SAT")) processor.dspBridge.setStageSaturation (std::atof (stageSat));
        std::printf ("ring       %s\n", options.ring ? "on" : "off");

        std::printf ("body       %s [%d]\n", bodyName.toRawUTF8(), bodyIndex);
        if (bodyBytes.getSize() == 240)
            std::printf ("bodyfile   %s\n", options.bodyFile.toRawUTF8());
        std::printf ("source     %s  %.0f Hz  %d ch -> %.0f Hz mono, block %d\n",
                     inputFile.getFileName().toRawUTF8(), sourceRate, sourceChannels,
                     options.rate, options.block);
        std::printf ("morph      %s\n", options.morphSpec.toRawUTF8());
        if (! qCurve.empty())
            std::printf ("qcurve     %s\n", options.qSpec.toRawUTF8());
        std::printf ("q %.3f  bite %.3f  input %.3f  output %.3f\n",
                     options.q, (double) biteValue, (double) preampValue, (double) slamValue);
        std::printf ("movement   movePreset=%d%s  bpm %s  KEY keySnap=0 (OFF)\n", options.move, options.move == 0 ? " (OFF)" : "", options.bpm > 0.0 ? juce::String (options.bpm, 1).toRawUTF8() : "none");

        juce::MidiBuffer midi;
        {
            juce::AudioBuffer<float> silence (2, options.block);
            for (int warm = 0; warm < 2; ++warm)
            {
                silence.clear();
                midi.clear();
                processor.processBlock (silence, midi);
            }
        }

        juce::AudioBuffer<float> render (2, frames);
        for (int channel = 0; channel < 2; ++channel)
        {
            float* destination = render.getWritePointer (channel);
            for (int frame = 0; frame < frames; ++frame)
                destination[frame] = mono[(size_t) frame];
        }

        for (int start = 0; start < frames; start += options.block)
        {
            const int count = juce::jmin (options.block, frames - start);
            playHead.sample = (juce::int64) start;
            setParameter (processor, ParamID::morph,
                          (float) morphAt (morphCurve, (double) start / options.rate));
            if (! qCurve.empty())
                setParameter (processor, ParamID::q,
                              (float) morphAt (qCurve, (double) start / options.rate));
            float* channelData[2] = { render.getWritePointer (0) + start,
                                      render.getWritePointer (1) + start };
            juce::AudioBuffer<float> block (channelData, 2, count);
            midi.clear();
            processor.processBlock (block, midi);
        }
        processor.releaseResources();

        long double sumSquares = 0.0;
        double peak = 0.0;
        for (int channel = 0; channel < 2; ++channel)
        {
            const float* samples = render.getReadPointer (channel);
            for (int frame = 0; frame < frames; ++frame)
            {
                const double sample = samples[frame];
                if (! std::isfinite (sample))
                    throw std::runtime_error ("processor produced a non-finite sample");
                sumSquares += sample * sample;
                peak = std::max (peak, std::abs (sample));
            }
        }
        const double rms = std::sqrt ((double) (sumSquares / ((long double) frames * 2.0L)));

        writeFloatWav (outputFile, render, options.rate);

        std::printf ("wrote      %s\n", outputFile.getFullPathName().toRawUTF8());
        std::printf ("seconds    %.3f\npeak       %.2f dBFS\nrms        %.2f dBFS\n",
                     clipSeconds, toDb (peak), toDb (rms));
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::fprintf (stderr, "TRENCH_Render: %s\n", exception.what());
        return 1;
    }
}
