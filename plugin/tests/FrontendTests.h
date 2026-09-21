#pragma once
#include "PluginEditor.h"
#include "ui/MixKnob.h"

inline int frontendTests()
{
    int failed = 0;
    const auto check = [&] (bool ok, const char* what)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
        if (! ok) ++failed;
    };
    PluginProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    check (editor->getWidth() == 310 && editor->getHeight() == 506, "face retains the requested 310 x 506 size");
    trench::ui::TypeSelectorView* selector = nullptr;
    trench::ui::BodyBrowser* browser = nullptr;
    trench::ui::ModulationBay* move = nullptr;
    int wheels = 0, values = 0, utilities = 0;
    for (auto* child : editor->getChildren())
    {
        if (auto* s = dynamic_cast<trench::ui::TypeSelectorView*> (child)) selector = s;
        if (auto* b = dynamic_cast<trench::ui::BodyBrowser*> (child)) browser = b;
        if (auto* m = dynamic_cast<trench::ui::ModulationBay*> (child)) move = m;
        if (! child->isVisible()) continue;
        check (editor->getLocalBounds().contains (child->getBounds()), "face control remains inside the chassis");
        if (dynamic_cast<trench::ui::WheelControl*> (child)) ++wheels;
        if (dynamic_cast<trench::ui::ValueReadout*> (child)) ++values;
        if (dynamic_cast<trench::ui::DeskKnob*> (child)
            || dynamic_cast<trench::ui::KeySnapBox*> (child)
            || dynamic_cast<trench::ui::ModulationChip*> (child)) ++utilities;
    }
    check (selector && move && wheels == 2 && values == 2 && utilities == 0,
           "only BODY, MORPH, Q and the MOVE block occupy the permanent face");
    if (selector && browser)
    {
        selector->onOpenBrowser (selector->selectedBody());
        check (browser->isVisible() && editor->getLocalArea (browser, browser->getLocalBounds()) == editor->getLocalBounds(),
               "body browser remains reachable");
        browser->close (false);
    }

    if (! move) return failed + 1;
    juce::Component* duration = nullptr;
    juce::TextButton* selectorButton = nullptr;
    trench::ui::MixKnob* notch = nullptr;
    for (auto* child : move->getChildren())
    {
        check (dynamic_cast<juce::ComboBox*> (child) == nullptr
               && dynamic_cast<trench::ui::ValueReadout*> (child) == nullptr,
               "MOVE has no property form or text fields");
        if (child->getTitle() == "Movement duration") duration = child;
        check (child->getTitle() != "Movement playback", "playback behaviour belongs to the preset, not another control");
        if (child->getTitle() == "Movement") selectorButton = dynamic_cast<juce::TextButton*> (child);
        if (auto* k = dynamic_cast<trench::ui::MixKnob*> (child)) notch = k;
    }
    check (duration && selectorButton && notch && move->getNumChildComponents() == 3,
           "MOVE exposes the movement controls and the distortion knob");
    if (! duration || ! selectorButton || ! notch) return failed + 1;
    check (! duration->isVisible(), "Off hides duration");
    const int faceChildren = editor->getNumChildComponents();
    move->keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
    check (processor.apvts.getRawParameterValue (ParamID::movePreset)->load() == 1
           && duration->isVisible(),
           "Next auditions a movement immediately and reveals its small performance settings");
    move->selectPattern (5);
    move->keyPressed (juce::KeyPress (juce::KeyPress::leftKey));
    check (processor.apvts.getRawParameterValue (ParamID::movePreset)->load() == 4,
           "Previous auditions the preceding movement without a popup");
    const int swayWidth = selectorButton->getWidth();
    move->selectPattern (2);
    check (selectorButton->getWidth() > swayWidth, "preset field expands to fit Backbeat Bloom");
    check (duration->getX() == selectorButton->getX() && duration->getY() >= selectorButton->getBottom(),
           "duration stacks under the preset as its width changes");
    move->selectPattern (8);
    move->selectLength (2);
    duration->keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    check (processor.apvts.getRawParameterValue (ParamID::moveLength)->load() == 3
           && move->durationText() == "4 BAR", "duration arrows set the whole phrase to four bars");
    const juce::MouseEvent down (juce::Desktop::getInstance().getMainMouseSource(), { 4, 4 }, {},
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, duration, duration, {}, { 4, 4 }, {}, 1, false);
    const juce::MouseEvent drag (juce::Desktop::getInstance().getMainMouseSource(), { 4, -8 }, {},
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, duration, duration, {}, { 4, 4 }, {}, 1, true);
    duration->mouseDown (down); duration->mouseDrag (drag);
    check (move->lengthChoice() == 4 && move->durationText() == "8 BAR",
           "dragging the duration stretches the phrase without opening a menu");
    duration->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    check (processor.apvts.getRawParameterValue (ParamID::movePlayback)->load() == 0
           && move->isOnce(), "Long Return supplies its one-shot behaviour without a mode control");
    move->selectPattern (4);
    check (! move->isOnce(), "Eighth Sway supplies its looping behaviour without a mode control");
    move->selectPattern (8);
    check (editor->getNumChildComponents() == faceChildren,
           "performing movement never materializes another panel");
    move->selectLength (0);
    check (move->durationText() == "1 BAR", "Long Return reports its own one-bar completion");
    move->selectLength (3);
    for (auto* child : move->getChildren())
        if (child->isVisible()) check (move->getLocalBounds().contains (child->getBounds()), "inline control fits within MOVE");
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    PluginProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    check (restored.apvts.getRawParameterValue (ParamID::movePreset)->load() == 8
           && restored.apvts.getRawParameterValue (ParamID::moveLength)->load() == 3
           && restored.apvts.getRawParameterValue (ParamID::movePlayback)->load() == 0,
           "the chosen movement, duration and authored behaviour survive project recall");
    processor.apvts.getParameter (ParamID::movePreset)->setValueNotifyingHost (0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    check (! duration->isVisible(),
           "host automation to Off collapses the performance settings");
    auto* motionParameter = processor.apvts.getParameter (ParamID::movePreset);
    motionParameter->setValueNotifyingHost (motionParameter->convertTo0to1 (8));
    move->refreshMotion();
    for (auto* child : move->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child); button && button->getTitle() == "Movement")
            check (button->getButtonText() == "Long Return",
                   "automation cannot leave the selected movement label stale between callbacks");
    check (processor.apvts.getParameter (ParamID::preamp) && processor.apvts.getParameter (ParamID::keySnap),
           "input and key remain available to the host without cluttering MOVE");
    {
        const float before = processor.apvts.getRawParameterValue (ParamID::distortion)->load();
        juce::MouseWheelDetails notchWheel;
        notchWheel.deltaY = 0.25f;
        notch->mouseWheelMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 4, 4 }, {},
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, notch, notch, {}, { 4, 4 }, {}, 1, false), notchWheel);
        check (processor.apvts.getRawParameterValue (ParamID::distortion)->load() > before,
               "the compact knob drives the distortion parameter");
        processor.apvts.getParameter (ParamID::distortion)->setValueNotifyingHost (0.0f);
    }
    return failed;
}
