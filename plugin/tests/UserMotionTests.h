#pragma once
#include "PluginProcessor.h"
#include "../tools/MotionEditor.h"
#include "ui/ModulationChip.h"
#include "ui/ModulationBay.h"
#include <thread>

inline int userMotionTests()
{
    int failed = 0;
    const auto check = [&] (bool ok, const char* what)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
        if (! ok) ++failed;
    };
    bool faithful = true;
    for (int index = 1; index <= trench::kNumFuncGenPatterns; ++index)
        for (int length : { 0, 3 })
            for (int playback : { 0, 1, 2 })
                for (int transition : { 0, 1, 2 })
                {
                    const auto copy = trench::UserMotion::copyFactory (index, transition, length, playback);
                    const auto pattern = copy.pattern();
                    trench::Movement original, duplicate;
                    original.prepare (48000); duplicate.prepare (48000);
                    trench::MovementTransport t;
                    t.playing = true;
                    for (int tick = 0; tick < 1300; ++tick)
                    {
                        t.ppq = (double) tick * 0.025;
                        float a = 0, b = 0;
                        original.render (&a, 1, 0.2f, t, index, transition, length, playback);
                        duplicate.render (&b, 1, 0.2f, t, index, 0, length, playback, 0, &pattern, copy.loopSteps);
                        faithful = faithful && std::abs (a - b) < 1.0e-5f;
                    }
                }
    check (faithful, "editable copies preserve every factory trajectory, direction, interpolation and duration");

    {
        trench::UserMotion rated;
        rated.values = { -1.0f, 1.0f };
        rated.steps = 2;
        rated.smooth = false;
        rated.direction = 0;
        rated.length = 0;
        rated.rateHz = 2.0;
        const auto pattern = rated.pattern();
        std::vector<float> at60, at120;
        for (auto* target : { &at60, &at120 })
        {
            const double bpm = target == &at60 ? 60.0 : 120.0;
            trench::Movement movement;
            movement.prepare (48000.0);
            trench::MovementTransport t;
            t.playing = true;
            t.bpm = bpm;
            for (int block = 0; block < 100; ++block)
            {
                t.ppq = (double) (block * 256) * bpm / 60.0 / 48000.0;
                float out[256] = {};
                movement.render (out, 256, 0.0f, t, 0, 0, 0, 0, 0, &pattern);
                target->insert (target->end(), out, out + 256);
            }
        }
        bool same = at60.size() == at120.size();
        for (std::size_t i = 0; same && i < at60.size(); ++i)
            same = std::abs (at60[i] - at120[i]) < 1.0e-6f;
        check (same, "a free step rate runs on its own clock, not the host tempo");
        check (at60[23999] == 0.0f && at60[24000] == 1.0f,
               "2 Hz advances one step every half second");
    }

    trench::UserMotionState publication;
    trench::UserMotion a, b;
    a.values.fill (-0.75f); a.steps = 8; a.smooth = false;
    b.values.fill (0.5f); b.steps = 16; b.smooth = true;
    publication.set (a);
    trench::UserMotionState::Audio snapshot;
    publication.readAudio (snapshot);
    std::atomic<bool> finished { false };
    std::thread writer ([&]
    {
        for (int i = 0; i < 6000; ++i) publication.set (i % 2 ? a : b);
        finished.store (true);
    });
    bool coherent = true;
    do
    {
        publication.readAudio (snapshot);
        const bool first = snapshot.steps == 8;
        coherent = coherent && snapshot.steps == (first ? 8 : 16) && snapshot.smooth == ! first;
        for (float value : snapshot.values) coherent = coherent && value == (first ? -0.75f : 0.5f);
    } while (! finished.load());
    writer.join();
    check (coherent, "audio snapshots remain coherent while points are edited concurrently");

    PluginProcessor processor;
    const auto set = [&] (const char* id, float value)
    {
        auto* p = processor.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    processor.prepareToPlay (48000, 512);
    processor.setEditorOpen (true);
    set (ParamID::morph, 0);
    const auto settle = [&]
    {
        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;
        for (int i = 0; i < 150; ++i) { audio.clear(); processor.processBlock (audio, midi); }
        return processor.getEffectiveMorphForUi();
    };
    a.name = "Test draft";
    processor.applyUserMotion (a);
    const auto low = settle();
    processor.applyUserMotion (b);
    const auto high = settle();
    check (std::abs (low - 0.125f) < 0.001f && std::abs (high - 0.75f) < 0.001f,
           "live point edits reach the processor's effective MORPH position");
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    PluginProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    const auto recalled = restored.userMotion.get();
    check (restored.usingUserMotion() && std::equal (b.values.begin(), b.values.begin() + b.steps, recalled.values.begin())
           && recalled.steps == b.steps && recalled.dirty && recalled.name == b.name
           && recalled.length == b.length && recalled.playback == b.playback,
           "project recall restores unsaved points and custom movement selection");
    auto bad = b.toJson();
    bad.getDynamicObject()->setProperty ("points", juce::Array<juce::var> { -2.0, 1.0 });
    trench::UserMotion invalid;
    check (! trench::UserMotion::fromJson (bad, invalid), "out-of-range movement files are rejected");
    auto oldTree = processor.apvts.copyState();
    oldTree.removeProperty ("userMotion", nullptr);
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*oldTree.createXml(), oldState);
    restored.setStateInformation (oldState.getData(), (int) oldState.getSize());
    check (! restored.usingUserMotion(), "missing movement payload cannot inherit an active custom draft");

    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("trench-motion-tests-" + juce::Uuid().toString());
    trench::UiLayout layout;
    trench::ui::Theme theme { layout };
    trench::ui::ModulationChip chip (processor.apvts, theme);
    chip.selectPattern (4);
    check (! processor.usingUserMotion(), "selecting a factory movement exits custom mode");
    const auto original = processor.motionForEditing();
    trench::ui::MotionEditor editor (processor, theme, root);
    editor.setSize (278, 442);
    check (! processor.usingUserMotion(), "opening Edit leaves the audible preset untouched");
    trench::ui::MotionCanvas* canvas = nullptr;
    juce::ComboBox* smoothing = nullptr;
    juce::ComboBox* rateBox = nullptr;
    for (auto* child : editor.getChildren())
    {
        check (editor.getLocalBounds().contains (child->getBounds()), "authoring control fits its overlay");
        if (auto* c = dynamic_cast<trench::ui::MotionCanvas*> (child)) canvas = c;
        if (child->getTitle() == "Motion smoothing") smoothing = dynamic_cast<juce::ComboBox*> (child);
        if (child->getTitle() == "Motion rate") rateBox = dynamic_cast<juce::ComboBox*> (child);
    }
    check (canvas && smoothing && rateBox, "motion points, Smooth/Step and Rate are reachable");
    if (canvas && smoothing)
    {
        canvas->keyPressed (juce::KeyPress (juce::KeyPress::upKey));
        check (processor.usingUserMotion() && processor.userMotion.get().values[0] > original.values[0],
               "editing a point auditions a separate live draft");
        editor.undoEdit();
        check (processor.userMotion.get().values == original.values, "Undo restores the previous movement points");
        const auto point = canvas->point (2);
        const juce::MouseEvent drag (juce::Desktop::getInstance().getMainMouseSource(), { point.x, canvas->plot().getY() }, {},
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, canvas, canvas, {}, {}, {}, 1, true);
        canvas->mouseDown (drag);
        canvas->mouseDrag (drag);
        check (processor.userMotion.get().values[2] == 1.0f, "dragging a point reaches the audible draft");
        editor.undoEdit();
        smoothing->setSelectedId (2, juce::sendNotificationSync);
        check (! processor.userMotion.get().smooth, "Step changes the live interpolation");
        rateBox->setSelectedItemIndex (5, juce::sendNotificationSync);
        check (processor.userMotion.get().rateHz == 4.0,
               "the rate control gives a drawn movement a free step rate");
        check (editor.saveAs ("My sway"), "Save As writes the edited movement");
        check (editor.saveAs ("My sway"), "saving another copy succeeds without replacing the original");
        const auto saved = trench::MotionLibrary::load (root);
        check (saved.size() == 2 && saved[0].name == "My sway" && saved[1].name == "My sway 2"
               && ! saved[0].smooth && saved[0].values == original.values,
               "saved named movements round-trip and preserve their source");
        check (! processor.userMotion.get().dirty, "saving clears the draft marker");
        chip.selectPattern (0);
        check (! processor.usingUserMotion(), "Off stops custom modulation as well as factory modulation");
        juce::Component face;
        face.setSize (310, 506);
        trench::ui::ModulationBay bay (processor, chip, theme, root);
        face.addAndMakeVisible (bay);
        bay.setBounds (40, 330, 170, 58);
        bay.selectSaved (0);
        check (processor.usingUserMotion() && processor.userMotion.get().name == "My sway"
               && ! processor.userMotion.get().smooth,
               "shipping MOVE finds and plays a saved movement without an authoring panel");
        bay.stepPattern (1);
        check (processor.usingUserMotion() && processor.userMotion.get().name == "My sway 2",
               "Next auditions saved movements alongside the factory bank");
        bay.stepPattern (1);
        check (! processor.usingUserMotion() && chip.selectedPattern() == 1,
               "Next wraps from the saved library into the factory bank");
        bay.selectPattern (0);
        check (! processor.usingUserMotion(), "shipping MOVE Off exits a saved movement");
    }
    std::printf ("Motion test artifacts: %s\n", root.getFullPathName().toRawUTF8());
    return failed;
}
