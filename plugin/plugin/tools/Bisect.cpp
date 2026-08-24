// THE EYES-CLOSED ROOM, SPEAKING TO THE SHIPPING ENGINE. A clip loops through
// the real PluginProcessor and the operator answers the bisection plan by ear:
// A is the interval's low anchor, B its high anchor, C the candidate, arrows
// nudge (Shift fine), Enter accepts, Esc leaves without writing. Nothing is on
// screen but a pulsing dot, and it is only there so the keys have somewhere to
// land. Never shipped inside the VST3.

#include "PluginProcessor.h"
#include "TrenchBodyRoster.h"
#include "parameters/CurveMap.h"
#include "parameters/TrenchParameters.h"
#include "trench/core/bisection.hpp"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace
{
bool running = true;

const char* axisParamId (const juce::String& axis)
{
    if (axis == "morph")  return ParamID::morph;
    if (axis == "q")      return ParamID::q;
    if (axis == "bite")   return ParamID::chew;
    if (axis == "follow") return ParamID::envAmount;
    return nullptr;
}

juce::String argValue (const juce::StringArray& args, const juce::String& name)
{
    const auto at = args.indexOf (name);
    return (at >= 0 && at + 1 < args.size()) ? args[at + 1] : juce::String();
}

int bodyIndexFor (const juce::String& wanted)
{
    if (wanted.isEmpty())
        return -1;
    if (wanted.containsOnly ("0123456789"))
        return juce::jlimit (0, trench::bodyCount() - 1, wanted.getIntValue());
    for (int i = 0; i < trench::bodyCount(); ++i)
        if (trench::bodyDisplayName (i).equalsIgnoreCase (wanted)
            || trench::bodyBaseForIndex (i).equalsIgnoreCase (wanted))
            return i;
    return -2;
}

// The device has nothing worth hearing on its input, so the clip is the input:
// it loops into the block and the shipping processor works on it in place.
class ClipPlayer final : public juce::AudioIODeviceCallback
{
public:
    ClipPlayer (PluginProcessor& p, juce::AudioBuffer<float>&& c)
        : processor (p), clip (std::move (c)) {}

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        const auto block = device->getCurrentBufferSizeSamples();
        rate = device->getCurrentSampleRate();
        processor.setPlayConfigDetails (2, 2, rate, block);
        processor.prepareToPlay (rate, block);
        scratch.setSize (2, block);
    }

    void audioDeviceStopped() override { processor.releaseResources(); }

    void audioDeviceIOCallbackWithContext (const float* const*, int,
                                           float* const* out, int numOut, int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        scratch.setSize (2, numSamples, false, false, true);
        const int frames = clip.getNumSamples();
        int done = 0;
        while (done < numSamples && frames > 0)
        {
            const int n = juce::jmin (numSamples - done, frames - readPos);
            for (int ch = 0; ch < 2; ++ch)
                scratch.copyFrom (ch, done, clip, juce::jmin (ch, clip.getNumChannels() - 1), readPos, n);
            done += n;
            readPos = (readPos + n) % frames;
        }
        juce::MidiBuffer midi;
        processor.processBlock (scratch, midi);
        for (int ch = 0; ch < numOut; ++ch)
            if (out[ch] != nullptr)
                juce::FloatVectorOperations::copy (out[ch], scratch.getReadPointer (juce::jmin (ch, 1)), numSamples);
    }

private:
    PluginProcessor& processor;
    juce::AudioBuffer<float> clip, scratch;
    double rate = 48000.0;
    int readPos = 0;
};

class Room final : public juce::Component, private juce::Timer
{
public:
    Room (PluginProcessor& p, juce::String axisName, juce::String bodyName, juce::File outFile)
        : processor (p), axis (std::move (axisName)), body (std::move (bodyName)),
          out (std::move (outFile)),
          session ((std::uint64_t) juce::Time::currentTimeMillis())
    {
        param = processor.apvts.getParameter (axisParamId (axis));
        setSize (520, 360);
        setWantsKeyboardFocus (true);
        beginTrial();
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (26, 31, 35));
        if (done)
        {
            g.setColour (juce::Colour (118, 127, 131));
            g.drawText ("done", getLocalBounds(), juce::Justification::centred);
            return;
        }
        const auto breath = 0.5 + 0.5 * std::sin (pulse);
        g.setColour (juce::Colour (118, 127, 131).withAlpha ((float) (0.22 + 0.30 * breath)));
        g.fillEllipse ((float) getWidth() * 0.5f - 5.0f, (float) getHeight() * 0.5f - 5.0f, 10.0f, 10.0f);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (done)
            return true;
        const bool fine = key.getModifiers().isShiftDown();
        const auto code = key.getKeyCode();
        if (code == 'A')                           driveTo (session.current().lo);
        else if (code == 'B')                      driveTo (session.current().hi);
        else if (code == 'C')                      driveTo (candidate);
        else if (code == juce::KeyPress::leftKey)  nudge (fine ? -0.004 : -0.02);
        else if (code == juce::KeyPress::rightKey) nudge (fine ?  0.004 :  0.02);
        else if (code == juce::KeyPress::returnKey)
        {
            session.accept (candidate);
            if (session.finished())
                finish();
            else
                beginTrial();
        }
        else if (code == juce::KeyPress::escapeKey)
        {
            std::printf ("aborted; nothing written\n");
            std::fflush (stdout);
            running = false;
        }
        else
            return false;
        return true;
    }

private:
    void timerCallback() override { pulse += 0.09; repaint(); }

    void beginTrial() { driveTo (session.midpoint()); }

    void driveTo (double value)
    {
        candidate = value;
        if (param != nullptr)
            param->setValueNotifyingHost ((float) juce::jlimit (0.0, 1.0, value));
    }

    void nudge (double fraction)
    {
        const auto& trial = session.current();
        driveTo (std::clamp (candidate + fraction * (trial.hi - trial.lo), trial.lo, trial.hi));
    }

    void finish()
    {
        const auto text = trench::core::bisect::curve_json (
            axis.toStdString(), body.toStdString(), session.medians(), session.repeats(),
            session.monotone());
        out.getParentDirectory().createDirectory();
        out.replaceWithText (text);
        const auto points = session.medians();
        for (std::size_t i = 0; i < trench::core::bisect::kPointCount; ++i)
            std::printf ("knob %.3f -> internal %.6f\n",
                         trench::core::bisect::knob_of_point (i), points[i]);
        std::printf ("%s%s\n", out.getFullPathName().toRawUTF8(),
                     session.monotone() ? "" : "  (NOT monotone)");
        std::fflush (stdout);
        done = true;
        stopTimer();
        repaint();
        juce::Timer::callAfterDelay (700, [] { running = false; });
    }

    PluginProcessor& processor;
    juce::String axis, body;
    juce::File out;
    trench::core::bisect::Session session;
    juce::RangedAudioParameter* param = nullptr;
    double candidate = 0.0;
    double pulse = 0.0;
    bool done = false;
};

class Window final : public juce::DocumentWindow
{
public:
    explicit Window (juce::Component* content)
        : juce::DocumentWindow ("TRENCH", juce::Colour (26, 31, 35), juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (content, true);
        centreWithSize (content->getWidth(), content->getHeight());
        setVisible (true);
        toFront (true);
        content->grabKeyboardFocus();
    }
    void closeButtonPressed() override { running = false; }
};
}

int main (int argc, char** argv)
{
    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String (juce::CharPointer_UTF8 (argv[i])));

    const auto axis = argValue (args, "--axis").toLowerCase();
    const auto clipPath = argValue (args, "--clip");
    if (axisParamId (axis) == nullptr || clipPath.isEmpty())
    {
        std::printf ("usage: TRENCH_Bisect --axis morph|q|bite|follow --clip path.wav "
                     "[--body <index-or-name>] [--out dir]\n");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (clipPath)));
    if (reader == nullptr)
    {
        std::printf ("cannot read %s\n", clipPath.toRawUTF8());
        return 1;
    }
    juce::AudioBuffer<float> clip ((int) juce::jmax (1u, reader->numChannels),
                                   (int) reader->lengthInSamples);
    reader->read (&clip, 0, clip.getNumSamples(), 0, true, true);

    PluginProcessor processor;
    const auto bodyArg = argValue (args, "--body");
    const auto index = bodyIndexFor (bodyArg);
    if (index == -2)
    {
        std::printf ("no body called %s\n", bodyArg.toRawUTF8());
        return 1;
    }
    if (index >= 0)
        if (auto* p = processor.apvts.getParameter (ParamID::body))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
    const auto bodyName = trench::bodyBaseForIndex (index >= 0 ? index : processor.getLoadedBodyIndex());

    // The axis is measured in its own units. The table this session will become
    // must not sit in the path while it is being measured.
    trench::curves::bypass().store (true);

    const auto outArg = argValue (args, "--out");
    const auto dir = outArg.isNotEmpty()
                       ? juce::File (outArg)
                       : juce::File (TRENCH_BISECT_ROOT).getChildFile ("dev").getChildFile ("curves");
    const auto outFile = dir.getChildFile (axis + "_"
                                           + juce::File (clipPath).getFileNameWithoutExtension()
                                           + ".curve.json");

    ClipPlayer player (processor, std::move (clip));
    juce::AudioDeviceManager devices;
    const auto deviceError = devices.initialiseWithDefaultDevices (0, 2);
    if (deviceError.isNotEmpty())
        std::printf ("audio device: %s\n", deviceError.toRawUTF8());
    devices.addAudioCallback (&player);

    {
        Window window (new Room (processor, axis, bodyName, outFile));
        std::printf ("%s on %s, body %s - A low, B high, C candidate, arrows nudge, Enter accept, Esc abort\n",
                     axis.toRawUTF8(), clipPath.toRawUTF8(), bodyName.toRawUTF8());
        std::fflush (stdout);
        while (running)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    }

    devices.removeAudioCallback (&player);
    devices.closeAudioDevice();
    return 0;
}
