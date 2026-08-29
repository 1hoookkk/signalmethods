#include "PluginProcessor.h"
#include "PluginEditor.h"
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
    processor.editorBeingDeleted (editor);
    holder.removeChildComponent (editor);
    delete editor;
    return 0;
}
