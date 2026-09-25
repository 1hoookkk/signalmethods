#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/ModulationChip.h"
#include "TrenchBodyRoster.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>
#include <functional>

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
