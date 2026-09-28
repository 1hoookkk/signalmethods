#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginEditor.h"
#include "ui/ModulationChip.h"
#include "TrenchBodyRoster.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{
void save (const juce::Image& img, const juce::String& name)
{
    auto f = juce::File::getCurrentWorkingDirectory().getChildFile (name);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s  %dx%d\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    PluginProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);
    auto* editor = processor.createEditorIfNeeded();
    juce::Component holder;
    holder.setSize (editor->getWidth(), editor->getHeight());
    holder.addAndMakeVisible (editor);
    const bool headless = std::getenv ("TRENCH_HEADLESS") != nullptr;
    if (! headless)
        holder.addToDesktop (juce::ComponentPeer::windowIsTemporary);
    if (auto* peer = holder.getPeer())
    {
        const auto engines = peer->getAvailableRenderingEngines();
        for (int i = 0; i < engines.size(); ++i)
            if (engines[i].containsIgnoreCase ("software")) peer->setCurrentRenderingEngine (i);
    }
    holder.setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
    const auto settle = [&] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
    const auto shoot = [&] (const char* stem, std::function<void (juce::Component&)> prepare = {})
    {
        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;
        for (int block = 0; block < 48; ++block)
        {
            audio.clear();
            processor.processBlock (audio, midi);
        }
        if (headless)
        {
            processor.editorBeingDeleted (editor);
            holder.removeChildComponent (editor);
            delete editor;
            editor = processor.createEditorIfNeeded();
            holder.addAndMakeVisible (editor);
            settle (900);
        }
        settle (150);
        if (prepare) { prepare (*editor); settle (200); }
        std::printf ("%s: movement=%g (%s)\n", stem,
                     processor.apvts.getRawParameterValue (ParamID::movePreset)->load(),
                     processor.motionForEditing().name.toRawUTF8());
        save (holder.createComponentSnapshot (holder.getLocalBounds(), true, 1.0f, juce::NativeImageType()), juce::String (stem) + ".png");
        save (holder.createComponentSnapshot (holder.getLocalBounds(), true, 2.0f, juce::NativeImageType()), juce::String (stem) + "_200.png");
        save (holder.createComponentSnapshot (holder.getLocalBounds(), true, 3.0f, juce::NativeImageType()), juce::String (stem) + "_300.png");
    };
    const auto set = [&] (const char* id, float denorm)
    {
        if (auto* prm = processor.apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (denorm));
    };
    const auto bodyIndex = [&] (const char* name) -> float
    {
        int n = 0;
        trench::bodyRoster (n);
        for (int i = 0; i < n; ++i)
            if (trench::bodyDisplayName (i).containsIgnoreCase (name)) return (float) i;
        return n > 1 ? 1.0f : 0.0f;
    };

    if (std::getenv ("TRENCH_FACESHOT_MOTION") != nullptr)
    {
        int march = 0;
        for (int i = 0; i < trench::kNumFuncGenPatterns; ++i)
            if (juce::String (trench::kFuncGenPatterns[i].name) == "Minor March") march = i + 1;
        set (ParamID::body, bodyIndex ("Talking Hedz"));
        set (ParamID::movePreset, (float) march);
        set (ParamID::moveLength, 4.0f);
        set (ParamID::morph, 0.25f);
        set (ParamID::q, 0.25f);
        settle (400);
        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;
        for (int step = 0; step <= 8; ++step)
        {
            for (int block = 0; block < 94; ++block)
            {
                audio.clear();
                processor.processBlock (audio, midi);
            }
            settle (150);
            std::printf ("half bar %d: effective morph %.4f, wheel parameter %.4f, movement %g\n", step,
                         processor.getEffectiveMorphForUi(), processor.apvts.getRawParameterValue (ParamID::morph)->load(),
                         processor.apvts.getRawParameterValue (ParamID::movePreset)->load());
            save (holder.createComponentSnapshot (holder.getLocalBounds(), true, 2.0f, juce::NativeImageType()), "trench_motion_" + juce::String (step) + ".png");
        }
        processor.editorBeingDeleted (editor);
        holder.removeChildComponent (editor);
        delete editor;
        return 0;
    }

    shoot ("trench_face");

    set (ParamID::body, bodyIndex ("Blade"));
    set (ParamID::movePreset, 0.0f);
    set (ParamID::keySnap, 0.0f);
    set (ParamID::preamp, 0.0f);
    set (ParamID::slamDrive, 0.0f);
    set (ParamID::distortion, 0.0f);
    set (ParamID::morph, 0.68f);
    set (ParamID::q, 0.30f);
    settle (400);
    shoot ("trench_face_active");
    set (ParamID::morph, 0.0f);
    settle (250);
    shoot ("trench_face_morph0");

    set (ParamID::morph, 1.0f);
    settle (250);
    shoot ("trench_face_morph1");

    set (ParamID::morph, 0.5f);
    set (ParamID::q, 1.0f);
    settle (250);
    shoot ("trench_face_qmax");

    set (ParamID::q, 0.30f);
    set (ParamID::preamp, 0.35f);
    set (ParamID::output, 0.6f);
    settle (250);
    shoot ("trench_face_bite");

    set (ParamID::distortion, 0.0f);
    set (ParamID::movePreset, 1.0f);
    settle (500);
    shoot ("trench_face_modulated");
    if (const char* frames = std::getenv ("TRENCH_FRAMES"))
    {
        const auto env = [] (const char* name, float fallback)
        {
            const char* v = std::getenv (name);
            return v != nullptr ? (float) std::atof (v) : fallback;
        };
        if (const char* body = std::getenv ("TRENCH_SHOW_BODY")) set (ParamID::body, bodyIndex (body));
        set (ParamID::morph, env ("TRENCH_SHOW_MORPH", 0.3f));
        set (ParamID::q, env ("TRENCH_SHOW_Q", 0.5f));
        set (ParamID::preamp, env ("TRENCH_SHOW_IN_GAIN", 0.0f));
        set (ParamID::output, env ("TRENCH_SHOW_OUT_GAIN", 0.0f));
        set (ParamID::movePreset, env ("TRENCH_SHOW_MOVE", 7.0f));
        set (ParamID::moveLength, env ("TRENCH_SHOW_LENGTH", 1.0f));
        struct Clock final : juce::AudioPlayHead
        {
            double bpm = 120.0, rate = 48000.0;
            juce::int64 sample = 0;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setBpm (bpm); info.setIsPlaying (true);
                info.setTimeInSamples (sample); info.setTimeInSeconds ((double) sample / rate);
                info.setPpqPosition ((double) sample / rate * bpm / 60.0);
                info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
                return info;
            }
        } clock;
        clock.bpm = env ("TRENCH_SHOW_BPM", 120.0f);
        juce::AudioBuffer<float> source;
        double rate = 48000.0;
        if (const char* in = std::getenv ("TRENCH_SHOW_IN"))
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (juce::String::fromUTF8 (in))));
            if (reader != nullptr)
            {
                rate = reader->sampleRate;
                source.setSize (2, (int) reader->lengthInSamples);
                reader->read (&source, 0, (int) reader->lengthInSamples, 0, true, true);
                if (reader->numChannels == 1) source.copyFrom (1, 0, source, 0, 0, source.getNumSamples());
            }
        }
        clock.rate = rate;
        processor.setPlayHead (&clock);
        processor.setPlayConfigDetails (2, 2, rate, 512);
        processor.prepareToPlay (rate, 512);
        settle (300);
        if (const char* seconds = std::getenv ("TRENCH_SHOW_SECONDS"); seconds != nullptr && source.getNumSamples() > 0)
        {
            const int wanted = (int) (std::atof (seconds) * rate);
            juce::AudioBuffer<float> looped (2, wanted);
            for (int at = 0; at < wanted; at += source.getNumSamples())
                for (int c = 0; c < 2; ++c)
                    looped.copyFrom (c, at, source, c, 0, juce::jmin (source.getNumSamples(), wanted - at));
            source = std::move (looped);
        }
        struct Event { double from = 0.0, to = 0.0; juce::String id, text; float a = 0.0f, b = 0.0f; };
        std::vector<Event> script;
        if (const char* text = std::getenv ("TRENCH_SHOW_SCRIPT"))
            for (const auto& item : juce::StringArray::fromTokens (juce::String::fromUTF8 (text), ";", {}))
            {
                Event e;
                const auto when = item.upToFirstOccurrenceOf (":", false, false);
                const auto what = item.fromFirstOccurrenceOf (":", false, false);
                e.from = when.upToFirstOccurrenceOf ("-", false, false).getDoubleValue();
                e.to = when.contains ("-") ? when.fromFirstOccurrenceOf ("-", false, false).getDoubleValue() : e.from;
                e.id = what.upToFirstOccurrenceOf ("=", false, false).trim();
                e.text = what.fromFirstOccurrenceOf ("=", false, false).trim();
                e.a = e.text.upToFirstOccurrenceOf (">", false, false).getFloatValue();
                e.b = e.text.contains (">") ? e.text.fromFirstOccurrenceOf (">", false, false).getFloatValue() : e.a;
                script.push_back (e);
            }
        const auto applyScript = [&] (double now)
        {
            for (auto& e : script)
            {
                if (now < e.from) continue;
                const float k = e.to > e.from ? (float) juce::jlimit (0.0, 1.0, (now - e.from) / (e.to - e.from)) : 1.0f;
                const float v = e.a + (e.b - e.a) * k;
                if (e.id == "body") { if (e.text.isNotEmpty()) { set (ParamID::body, bodyIndex (e.text.toRawUTF8())); e.text.clear(); } }
                else if (e.id == "move") set (ParamID::movePreset, v);
                else if (e.id == "length") set (ParamID::moveLength, v);
                else if (e.id == "in") set (ParamID::preamp, v);
                else if (e.id == "out") set (ParamID::output, v);
                else if (e.id == "q") set (ParamID::q, v);
                else if (e.id == "morph") set (ParamID::morph, v);
            }
        };
        const int blocksPerFrame = 3;
        const int count = source.getNumSamples() > 0
            ? juce::jmin (std::atoi (frames), (source.getNumSamples() + blocksPerFrame * 512 - 1) / (blocksPerFrame * 512))
            : juce::jlimit (1, 600, std::atoi (frames));
        juce::AudioBuffer<float> rendered (2, count * blocksPerFrame * 512);
        rendered.clear();
        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;
        for (int frame = 0; frame < count; ++frame)
        {
            applyScript ((double) frame * blocksPerFrame * 512 / rate);
            for (int block = 0; block < blocksPerFrame; ++block)
            {
                const int at = (frame * blocksPerFrame + block) * 512;
                audio.clear();
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512 && at + i < source.getNumSamples(); ++i)
                        audio.setSample (c, i, source.getSample (c, at + i));
                processor.processBlock (audio, midi);
                for (int c = 0; c < 2; ++c) rendered.copyFrom (c, at, audio, c, 0, 512);
                clock.sample = at + 512;
            }
            settle (33);
            save (holder.createComponentSnapshot (holder.getLocalBounds(), true, 2.0f, juce::NativeImageType()),
                  "frame_" + juce::String (frame).paddedLeft ('0', 3) + ".png");
        }
        if (const char* out = std::getenv ("TRENCH_SHOW_OUT"))
        {
            juce::File file (juce::String::fromUTF8 (out));
            file.deleteFile();
            std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
            juce::WavAudioFormat wav;
            if (stream != nullptr)
                if (auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (24)))
                    writer->writeFromAudioSampleBuffer (rendered, 0, rendered.getNumSamples());
        }
        std::printf ("showcase: %d frames at %.3f fps, %d samples at %.0f Hz\n", count, rate / (blocksPerFrame * 512.0), rendered.getNumSamples(), rate);
        processor.setPlayHead (nullptr);
    }
    set (ParamID::morph, 0.0f);
    set (ParamID::movePreset, 8.0f);
    set (ParamID::moveLength, 4.0f);
    set (ParamID::movePlayback, 2.0f);
    settle (250);
    shoot ("trench_face_four_bars");
    shoot ("trench_face_move_inline");
    set (ParamID::output, 0.35f);
    settle (250);
    shoot ("trench_face_move_knob");

    set (ParamID::movePreset, 0.0f);
    set (ParamID::output, 0.0f);
    set (ParamID::body, bodyIndex ("Vowel Ah"));
    set (ParamID::morph, 0.4f);
    set (ParamID::q, 0.5f);
    set (ParamID::keySnap, 10.0f);
    settle (500);
    shoot ("trench_face_key");
    shoot ("trench_face_motion_list", [] (juce::Component& root)
    {
        std::function<trench::ui::ModulationChip* (juce::Component&)> find = [&] (juce::Component& c) -> trench::ui::ModulationChip*
        {
            if (auto* m = dynamic_cast<trench::ui::ModulationChip*> (&c)) return m;
            for (auto* child : c.getChildren())
                if (auto* m = find (*child)) return m;
            return nullptr;
        };
        if (auto* chip = find (root)) chip->showPatterns();
    });
    set (ParamID::keySnap, 0.0f);
    set (ParamID::movePreset, 0.0f);
    set (ParamID::distortion, 0.0f);
    set (ParamID::body, bodyIndex ("No filter"));
    set (ParamID::slamDrive, 1.0f);
    set (ParamID::preamp, 0.6f);
    set (ParamID::morph, 0.5f);
    settle (400);
    shoot ("trench_face_nofilter");
    processor.editorBeingDeleted (editor);
    holder.removeChildComponent (editor);
    delete editor;
    return 0;
}
