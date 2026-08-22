// GOD MODE proofs, headless. Everything here runs against a scratch file in the
// temp directory - the operator's own Documents/TRENCH/ui_layout.json is never
// read or written.
//
//   1. ROUND TRIP  - the overlay's move + save functions, then re-parsed JSON.
//   2. HOT RELOAD  - a real editor picks up an edited file and its component
//                    bounds actually move.
//   3. BAD JSON    - a half-written file leaves the previous layout standing.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UiLayout.h"
#include "ui/GodMode.h"
#include "ui/WheelControl.h"

#include <cstdio>

namespace
{
template <typename ComponentType>
ComponentType* findChildOfType (juce::Component& root, const juce::String& title = {})
{
    if (auto* match = dynamic_cast<ComponentType*> (&root))
        if (title.isEmpty() || match->getTitle().equalsIgnoreCase (title))
            return match;
    for (int i = 0; i < root.getNumChildComponents(); ++i)
        if (auto* match = findChildOfType<ComponentType> (*root.getChildComponent (i), title))
            return match;
    return nullptr;
}
juce::String fmt (juce::Rectangle<float> r)
{
    return juce::String::formatted ("[%.0f %.0f %.0f %.0f]", r.getX(), r.getY(),
                                    r.getWidth(), r.getHeight());
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const auto scratch = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("trench_godmode_proof.json");
    scratch.deleteFile();
    // Point the whole feature - save target AND file watch - at the scratch file
    // before anything constructs an editor.
    trench::ui::godLayoutFile() = scratch;

    // ---- 1. round trip -----------------------------------------------------
    auto layout = trench::UiLayout::defaults();
    const juce::String id { "morphReadout" };
    const auto before = layout.sourceRectFor (id);
    trench::ui::godMove (layout, id, 10.0f, -10.0f);   // one Shift+arrow nudge
    const auto edited = layout.sourceRectFor (id);
    const bool saved = trench::ui::godSaveLayout (layout, scratch);
    const auto reparsed = trench::UiLayout::fromJson (scratch.loadFileAsString()).sourceRectFor (id);
    const bool roundTrip = saved && edited == before.translated (10.0f, -10.0f)
                        && reparsed == edited;
    std::printf ("GODMODE  round trip  %s  before %s  moved %s  reloaded %s  %s\n",
                 id.toRawUTF8(), fmt (before).toRawUTF8(), fmt (edited).toRawUTF8(),
                 fmt (reparsed).toRawUTF8(), roundTrip ? "PASS" : "FAIL");

    // ---- a real editor, on a real (offscreen) window -----------------------
    trench::ui::godSaveLayout (trench::UiLayout::defaults(), scratch);
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

    auto* wheel = findChildOfType<trench::ui::WheelControl> (*editor, "Morph");
    if (wheel == nullptr)
    {
        std::printf ("GODMODE  morph wheel not found  FAIL\n");
        return 1;
    }
    const auto boundsAtStart = wheel->getBounds();

    // ---- 2. hot reload: edit the file, the face follows --------------------
    auto edit = trench::UiLayout::defaults();
    trench::ui::godMove (edit, "morphWheel", 40.0f, 40.0f);
    trench::ui::godSaveLayout (edit, scratch);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (1200);
    const auto boundsAfterEdit = wheel->getBounds();
    const auto expected = trench::ui::sourceRectToEditor (edit.sourceRectFor ("morphWheel"))
                              .getSmallestIntegerContainer();
    const bool hotReload = boundsAfterEdit == expected && boundsAfterEdit != boundsAtStart;
    std::printf ("GODMODE  hot reload  morphWheel editor px %s -> %s (expected %s)  %s\n",
                 boundsAtStart.toString().toRawUTF8(), boundsAfterEdit.toString().toRawUTF8(),
                 expected.toString().toRawUTF8(), hotReload ? "PASS" : "FAIL");

    // ---- 3. a half-written file must change nothing ------------------------
    scratch.replaceWithText ("{ \"elements\": { \"morphWheel\": { \"rect\": [1");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (1200);
    const auto boundsAfterGarbage = wheel->getBounds();
    const bool survivedGarbage = boundsAfterGarbage == boundsAfterEdit;
    std::printf ("GODMODE  bad JSON    morphWheel editor px %s (unchanged=%d)  %s\n",
                 boundsAfterGarbage.toString().toRawUTF8(), (int) survivedGarbage,
                 survivedGarbage ? "PASS" : "FAIL");

    scratch.deleteFile();
    const bool pass = roundTrip && hotReload && survivedGarbage;
    std::printf ("GODMODE  ALL  %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
