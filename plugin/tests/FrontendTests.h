#pragma once
#include "PluginEditor.h"

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
    auto* trenchEditor = dynamic_cast<PluginEditor*> (editor.get());
    const float savedScale = trenchEditor != nullptr ? trenchEditor->getUiScale() : 1.0f;
    if (trenchEditor != nullptr) trenchEditor->setUiScale (1.0f);
    check (editor->getWidth() == 310 && editor->getHeight() == 506, "face retains the requested 310 x 506 size");
    auto* face = editor->findChildWithID ("face");
    check (face != nullptr, "the face lives in one scalable panel");
    if (face == nullptr) return failed + 1;
    trench::ui::TypeSelectorView* selector = nullptr;
    trench::ui::BodyBrowser* browser = nullptr;
    trench::ui::ModulationChip* chip = nullptr;
    trench::ui::GraphDisplay* glass = nullptr;
    std::vector<trench::ui::DeskKnob*> knobs;
    int wheels = 0, values = 0, others = 0;
    const juce::Rectangle<float> notch { 753.0f * 310.0f / 1024.0f, 1185.0f * 506.0f / 1536.0f, 310.0f, 506.0f };
    bool insideChassis = true, clearOfNotch = true, key = false;
    for (auto* child : face->getChildren())
    {
        if (auto* s = dynamic_cast<trench::ui::TypeSelectorView*> (child)) selector = s;
        if (auto* b = dynamic_cast<trench::ui::BodyBrowser*> (child); b != nullptr && b->getTitle() == "Bodies") browser = b;
        if (auto* g = dynamic_cast<trench::ui::GraphDisplay*> (child)) glass = g;
        if (! child->isVisible()) continue;
        insideChassis = insideChassis && face->getLocalBounds().contains (child->getBounds());
        if (auto* c = dynamic_cast<trench::ui::ModulationChip*> (child)) chip = c;
        if (auto* k = dynamic_cast<trench::ui::DeskKnob*> (child)) knobs.push_back (k);
        if (dynamic_cast<trench::ui::WheelControl*> (child)) ++wheels;
        if (dynamic_cast<trench::ui::ValueReadout*> (child)) ++values;
        if (dynamic_cast<trench::ui::KeySnapBox*> (child)) key = true;
        if (dynamic_cast<trench::ui::ModulationBay*> (child)
            || dynamic_cast<trench::ui::MixKnob*> (child)) ++others;
        if (dynamic_cast<trench::ui::DeskKnob*> (child) || dynamic_cast<trench::ui::ValueReadout*> (child))
            clearOfNotch = clearOfNotch && ! child->getBounds().toFloat().intersects (notch);
    }
    check (insideChassis, "face controls remain inside the chassis");
    check (selector && chip && key && wheels == 2 && values == 4 && knobs.size() == 2 && others == 0,
           "BODY, KEY, MORPH, Q, the movement chip, INPUT and OUTPUT are the whole face");
    check (clearOfNotch, "INPUT and OUTPUT sit on the plate, clear of the notch");
    {
        trench::UiLayout wheelLayout;
        trench::ui::WheelControl wheel (processor.apvts, ParamID::morph, {}, trench::ui::Theme { wheelLayout });
        wheel.setSize (110, 30);
        auto* parameter = processor.apvts.getParameter (ParamID::morph);
        int starts = 0, ends = 0;
        wheel.onGestureStart = [&] { ++starts; };
        wheel.onGestureEnd = [&] { ++ends; };
        const auto event = [&] (float x, int flags = juce::ModifierKeys::leftButtonModifier)
        {
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { x, 15 }, juce::ModifierKeys (flags),
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &wheel, &wheel, {}, { x, 15 }, {}, 1, true);
        };
        parameter->setValueNotifyingHost (0.9f);
        wheel.mouseDown (event (55));
        check (std::abs (parameter->getValue() - 0.5f) < 1.0e-6f, "wheel center click sets 50 percent independently of its previous value");
        wheel.mouseDrag (event (105));
        check (parameter->getValue() == 1.0f, "wheel right edge reaches 100 percent");
        wheel.mouseDrag (event (5));
        check (parameter->getValue() == 0.0f, "wheel left edge reaches zero");
        wheel.mouseUp (event (5));
        parameter->setValueNotifyingHost (0.5f);
        const int fine = juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier;
        wheel.mouseDown (event (10, fine));
        check (parameter->getValue() == 0.5f, "Shift-click preserves the current wheel position");
        wheel.mouseDrag (event (21, fine));
        check (std::abs (parameter->getValue() - 0.525f) < 1.0e-6f, "Shift-drag makes a fine relative adjustment");
        wheel.mouseUp (event (21, fine));
        juce::MouseWheelDetails scroll;
        scroll.deltaY = 0.25f;
        wheel.mouseWheelMove (event (55), scroll);
        wheel.mouseDoubleClick (event (55));
        check (starts == 4 && ends == 4 && parameter->getValue() == parameter->getDefaultValue(),
               "drag, fine drag, scroll and reset each bracket a complete manual gesture");
        wheel.mouseUp (event (55, juce::ModifierKeys::rightButtonModifier));
        check (ends == 4, "a context-menu release does not end a manual gesture");
    }
    {
        int wheelsBottom = 0, knobsTop = face->getHeight();
        for (auto* child : face->getChildren())
            if (dynamic_cast<trench::ui::WheelControl*> (child) != nullptr) wheelsBottom = juce::jmax (wheelsBottom, child->getBottom());
        for (auto* k : knobs) knobsTop = juce::jmin (knobsTop, k->getY());
        check (chip && glass && ! glass->getBounds().intersects (chip->getBounds())
                   && chip->getY() >= wheelsBottom - 4 && chip->getBottom() <= knobsTop,
               "the movement dropdown sits under the wheels, off the glass and above INPUT and OUTPUT");
    }
    if (selector && browser)
    {
        selector->onOpenBrowser (selector->selectedBody());
        check (browser->isVisible() && editor->getLocalArea (browser, browser->getLocalBounds()) == editor->getLocalBounds(),
               "body browser remains reachable");
        browser->close (false);
    }
    check (processor.apvts.getParameter (ParamID::distortion) == nullptr && processor.apvts.getParameter (ParamID::output) != nullptr,
           "Distortion is retired and OUTPUT is a host parameter");
    if (knobs.size() == 2)
    {
        const char* ids[] = { ParamID::preamp, ParamID::output };
        bool driven = true;
        for (size_t i = 0; i < 2; ++i)
        {
            const float before = processor.apvts.getRawParameterValue (ids[i])->load();
            juce::MouseWheelDetails wheel;
            wheel.deltaY = 0.25f;
            knobs[i]->mouseWheelMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 4, 4 }, {},
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, knobs[i], knobs[i], {}, { 4, 4 }, {}, 1, false), wheel);
            driven = driven && processor.apvts.getRawParameterValue (ids[i])->load() > before;
            processor.apvts.getParameter (ids[i])->setValueNotifyingHost (0.0f);
        }
        check (driven, "the knobs drive INPUT and OUTPUT in that order");
    }
    if (trenchEditor != nullptr)
    {
        trenchEditor->setUiScale (2.0f);
        const auto scaledChip = chip != nullptr ? editor->getLocalArea (chip, chip->getLocalBounds()) : juce::Rectangle<int>();
        check (editor->getWidth() == 620 && editor->getHeight() == 1012 && face->getBounds() == juce::Rectangle<int> (0, 0, 310, 506)
               && chip != nullptr && scaledChip.getWidth() == chip->getWidth() * 2,
               "200% doubles the editor and scales every control with it");
        trenchEditor->setUiScale (savedScale);
    }
    {
        MorphFollower follower;
        bool advances = true;
        double shown = follower.advance (0.0, 0.0), previous = shown;
        for (int tick = 1; tick <= 200; ++tick)
        {
            shown = follower.advance (0.5, tick * 4.0);
            advances = advances && shown >= previous;
            previous = shown;
        }
        check (advances && std::abs (shown - 0.5) < 0.01, "the displayed Morph keeps advancing toward the audio value when updates arrive faster than frames");
        trench::ui::WheelControl* morphWheel = nullptr;
        for (auto* child : face->getChildren())
            if (auto* w = dynamic_cast<trench::ui::WheelControl*> (child); w != nullptr && w->getTitle() == "Morph") morphWheel = w;
        processor.setPlayConfigDetails (2, 2, 48000.0, 256);
        processor.prepareToPlay (48000.0, 256);
        processor.apvts.getParameter (ParamID::movePreset)->setValueNotifyingHost (
            processor.apvts.getParameter (ParamID::movePreset)->convertTo0to1 (7.0f));
        juce::AudioBuffer<float> audio (2, 256);
        juce::MidiBuffer midi;
        float lowest = 1.0f, highest = 0.0f;
        for (int round = 0; round < 40; ++round)
        {
            for (int block = 0; block < 4; ++block) { audio.clear(); processor.processBlock (audio, midi); }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            if (morphWheel != nullptr)
            {
                lowest = std::min (lowest, morphWheel->shownNormalised());
                highest = std::max (highest, morphWheel->shownNormalised());
            }
        }
        check (morphWheel != nullptr && highest - lowest > 0.1f, "the rendered MORPH wheel follows a running movement through the editor timer");
        processor.apvts.getParameter (ParamID::movePreset)->setValueNotifyingHost (0.0f);
    }
    if (! chip) return failed + 1;
    check (chip->displayText() == "Modulation: off", "the chip reads Off with no movement");
    chip->keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
    check (processor.apvts.getRawParameterValue (ParamID::movePreset)->load() == 1
           && chip->displayText().startsWith (trench::kFuncGenPatterns[0].name)
           && chip->displayText().endsWith ("1/2 bar")
           && processor.apvts.getRawParameterValue (ParamID::moveLength)->load() == 1,
           "Next auditions a movement at its authored length and the chip names it with its length");
    chip->selectPattern (5);
    check (processor.apvts.getRawParameterValue (ParamID::moveLength)->load() == 2
           && chip->displayText().endsWith (juce::String::fromUTF8 ("1 bar \xc2\xb7 lands")),
           "a one-shot movement arrives at its authored 1 bar and reads lands");
    chip->selectLength (4);
    check (chip->displayText().startsWith (trench::kFuncGenPatterns[4].name) && chip->displayText().contains ("4 bars"),
           "the chip names the movement and its length");
    {
        bool onlyMovements = true, tide = false, lands = false, pulse = false, ordered = true;
        juce::String heading;
        int lastChoice = -1;
        for (const auto& row : chip->browserRows())
        {
            onlyMovements = onlyMovements && (row.heading || (row.body >= 0 && row.body <= trench::kNumFuncGenPatterns) || row.body >= 400);
            if (row.heading) { heading = row.text; lastChoice = -1; continue; }
            if (row.body >= 1 && row.body <= trench::kNumFuncGenPatterns)
            {
                const int choice = trench::Movement::authoredLengthChoice (trench::kFuncGenPatterns[row.body - 1]);
                ordered = ordered && choice >= lastChoice;
                lastChoice = choice;
            }
            if (row.body == 20) tide = heading == "Sways" && row.text.endsWith ("4 bars");
            if (row.body == 5) lands = heading == "Lands" && row.text.endsWith ("1 bar");
            if (row.body == 22) pulse = heading == "Pulses" && row.text.endsWith ("1/4 bar");
        }
        check (onlyMovements && tide && lands && pulse && ordered,
               "the movement browser groups movements by what they do, shortest first, each with its length");
        const auto before = processor.apvts.getRawParameterValue (ParamID::moveLength)->load();
        chip->setRateBox ({ 200.0f, 0.0f, 40.0f, 20.0f });
        chip->mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 210.0f, 10.0f },
            juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, chip, chip, {}, { 210.0f, 10.0f }, {}, 1, false));
        check (processor.apvts.getRawParameterValue (ParamID::moveLength)->load() == before,
               "the length box shows the movement's own length and cannot be set from the face");
    }
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    PluginProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    check (restored.apvts.getRawParameterValue (ParamID::movePreset)->load() == 5
           && restored.apvts.getRawParameterValue (ParamID::moveLength)->load() == 4,
           "the chosen movement and length survive project recall");
    return failed;
}
