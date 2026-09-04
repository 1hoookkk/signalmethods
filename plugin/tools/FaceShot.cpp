#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TrenchBodyRoster.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>

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
    holder.addToDesktop (juce::ComponentPeer::windowIsTemporary);
    holder.setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
    const auto shoot = [&] (const char* stem)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        save (holder.createComponentSnapshot (holder.getLocalBounds()), juce::String (stem) + ".png");
        save (holder.createComponentSnapshot (holder.getLocalBounds(), true, 2.0f), juce::String (stem) + "_200.png");
    };
    shoot ("trench_face");
    const auto set = [&] (const char* id, float denorm)
    {
        if (auto* prm = processor.apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (denorm));
    };
    {
        int n = 0;
        trench::bodyRoster (n);
        int pick = n > 1 ? 1 : 0;
        for (int i = 0; i < n; ++i)
            if (trench::bodyDisplayName (i).containsIgnoreCase ("Crisp")) { pick = i; break; }
        set (ParamID::body, (float) pick);
    }
    set (ParamID::morph, 0.68f);
    set (ParamID::q, 0.30f);
    set (ParamID::keySnap, 13.0f);
    set (ParamID::movePreset, 1.0f);
    set (ParamID::preamp, 0.35f);
    set (ParamID::slamDrive, 0.70f);
    set (ParamID::envAmount, 0.6f);
    set (ParamID::chew, 0.25f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (700);
    shoot ("trench_face_active");
    processor.editorBeingDeleted (editor);
    holder.removeChildComponent (editor);
    delete editor;
    return 0;
}
