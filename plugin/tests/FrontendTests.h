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
    bool insideChassis = true, clearOfNotch = true, key = false, slamToggles = false;
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
        if (auto* slam = dynamic_cast<trench::ui::SlamButton*> (child))
        {
            slam->mouseUp (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 3.0f, 3.0f },
                juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, slam, slam, {}, { 3.0f, 3.0f }, {}, 1, false));
            slamToggles = processor.apvts.getRawParameterValue (ParamID::inputSlam)->load() > 0.5f;
            processor.apvts.getParameter (ParamID::inputSlam)->setValueNotifyingHost (0.0f);
        }
        if (dynamic_cast<trench::ui::ModulationBay*> (child)
            || dynamic_cast<trench::ui::MixKnob*> (child)) ++others;
        if (dynamic_cast<trench::ui::DeskKnob*> (child) || dynamic_cast<trench::ui::ValueReadout*> (child))
            clearOfNotch = clearOfNotch && ! child->getBounds().toFloat().intersects (notch);
    }
    check (insideChassis, "face controls remain inside the chassis");
    check (selector && chip && key && wheels == 2 && values == 4 && knobs.size() == 2 && others == 0,
           "BODY, KEY, MORPH, Q, the movement chip, INPUT and OUTPUT are the whole face");
    check (clearOfNotch, "INPUT and OUTPUT sit on the plate, clear of the notch");
    check (slamToggles, "the SLAM button on the face switches the preamp before the filter");
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
    for (auto* knob : knobs)
    {
        auto* param = processor.apvts.getParameter (knob->getTitle() == "INPUT" ? ParamID::preamp : ParamID::output);
        trench::ui::ValueReadout* readout = nullptr;
        for (auto* child : face->getChildren())
            if (auto* r = dynamic_cast<trench::ui::ValueReadout*> (child); r != nullptr && r->getTitle() == param->getName (24)) readout = r;
        const auto setDb = [&] (float db) { param->setValueNotifyingHost (param->convertTo0to1 (db)); };
        const auto atDb = [&] (float db) { return std::abs (param->convertFrom0to1 (param->getValue()) - db) < 0.001f; };
        const auto event = [&] (juce::Component* c, float y, bool fine = false)
        {
            const int flags = juce::ModifierKeys::leftButtonModifier | (fine ? juce::ModifierKeys::shiftModifier : 0);
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 10, y }, juce::ModifierKeys (flags),
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, c, c, {}, { 10, 20 }, {}, 1, true);
        };
        for (auto* control : { static_cast<juce::Component*> (knob), static_cast<juce::Component*> (readout) })
        {
            if (control == nullptr) { check (false, "gain readout exists"); continue; }
            setDb (-6.0f);
            control->mouseDown (event (control, 20));
            control->mouseDrag (event (control, 10));
            const bool coarse = atDb (-4.0f);
            control->mouseDrag (event (control, 10, true));
            const bool shiftHeld = atDb (-4.0f);
            control->mouseDrag (event (control, 5, true));
            const bool fine = atDb (-3.9f);
            control->mouseDrag (event (control, 5));
            check (coarse && shiftHeld && fine && atDb (-3.9f), "gain knob and readout allow fine adjustment mid-drag without a level jump");
            control->mouseUp (event (control, 5));
            setDb (23.0f);
            control->mouseDown (event (control, 20));
            control->mouseDrag (event (control, -1000));
            const bool bounded = atDb (24.0f);
            control->mouseDrag (event (control, -999));
            check (bounded && atDb (23.8f), "gain drag reverses immediately after reaching the limit");
            control->mouseUp (event (control, -999));
            setDb (-0.7f);
            control->mouseDown (event (control, 20));
            control->mouseDrag (event (control, 17));
            check (atDb (0.0f), "gain drag catches unity when crossing the center");
            control->mouseUp (event (control, 17));
            setDb (0.0f);
            control->keyPressed (juce::KeyPress (juce::KeyPress::upKey));
            const bool keyCoarse = atDb (0.5f);
            control->keyPressed (juce::KeyPress (juce::KeyPress::downKey, juce::ModifierKeys::shiftModifier, 0));
            check (keyCoarse && atDb (0.4f), "gain keys use half-dB steps and Shift uses tenths");
            juce::MouseWheelDetails scroll;
            scroll.deltaY = 0.25f;
            scroll.isSmooth = false;
            control->mouseWheelMove (event (control, 20), scroll);
            const bool scrollCoarse = atDb (0.9f);
            control->mouseWheelMove (event (control, 20, true), scroll);
            check (scrollCoarse && atDb (1.0f), "gain wheel and readout scrolling use the same coarse and fine steps");
        }
        if (readout != nullptr)
        {
            for (const float db : { -24.0f, -6.0f, 0.0f, 6.0f, 24.0f })
            {
                setDb (db);
                readout->setNormalised (0.0f);
                readout->mouseDoubleClick (event (readout, 20));
                juce::TextEditor* entry = nullptr;
                for (auto* child : readout->getChildren())
                    if (auto* text = dynamic_cast<juce::TextEditor*> (child)) entry = text;
                const bool actualDb = entry != nullptr && std::abs (entry->getText().getFloatValue() - db) < 0.001f;
                if (entry != nullptr) entry->onReturnKey();
                check (actualDb && readout->getNumChildComponents() == 0 && atDb (db),
                       "opening and accepting gain entry preserves the current dB value, even before the display refreshes");
            }
            readout->mouseDoubleClick (event (readout, 20));
            for (auto* child : readout->getChildren())
                if (auto* entry = dynamic_cast<juce::TextEditor*> (child))
                {
                    entry->setText ("-12.3 dB", false);
                    entry->onReturnKey();
                    break;
                }
            check (atDb (-12.3f), "gain entry accepts an exact signed dB value");
        }
        knob->mouseDoubleClick (event (knob, 20));
        check (atDb (0.0f), "gain knob double-click restores unity");
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
           && chip->displayText() == trench::kFuncGenPatterns[0].name
           && processor.apvts.getRawParameterValue (ParamID::moveLength)->load() == 1,
           "Next auditions a movement at its authored length and the chip shows its name only");
    chip->selectPattern (5);
    check (processor.apvts.getRawParameterValue (ParamID::moveLength)->load() == 2
           && chip->displayText() == trench::kFuncGenPatterns[4].name,
           "a one-shot movement arrives at its authored 1 bar");
    chip->selectLength (4);
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
            if (row.body == 20) tide = heading == "Sways" && row.detail == "4";
            if (row.body == 5) lands = heading == "Lands" && row.detail == "1";
            if (row.body == 22) pulse = heading == "Pulses" && row.detail == juce::String::charToString (0x00bc);
        }
        check (onlyMovements && tide && lands && pulse && ordered,
               "the movement browser groups movements by what they do, shortest first, each with its length");
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
