#pragma once
#include "Theme.h"
#include "SelectorLookAndFeel.h"
#include "BodyBrowser.h"
#include "../parameters/TrenchParameters.h"
#include "../dsp/Movement.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>
#include <numeric>
namespace trench::ui
{
class ModulationChip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModulationChip (juce::AudioProcessorValueTreeState& apvts, const Theme& theme)
        : t (theme), param (dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamID::movePreset))),
          custom (apvts.getParameter (ParamID::moveCustom)),
          length (dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamID::moveLength))),
          playback (dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamID::movePlayback)))
    {
        setInterceptsMouseClicks (true, false);
        setWantsKeyboardFocus (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Modulation");
        setTooltip ("Choose a motion pattern, its rate and playback. It moves MORPH around where the wheel sits; hold the wheel to stop it.");
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        if (length != nullptr)
            lengthAttachment = std::make_unique<juce::ParameterAttachment> (*length, [this] (float) { repaint(); });
        if (playback != nullptr)
            playbackAttachment = std::make_unique<juce::ParameterAttachment> (*playback, [this] (float) { repaint(); });
    }
    int selectedPattern() const noexcept { return param != nullptr ? param->getIndex() : 0; }
    static constexpr int kEchoRow = 500;
    std::function<void (bool)> onEcho;
    std::function<bool()> echoArmed;
    bool echoOn() const { return echoArmed != nullptr && echoArmed(); }
    void selectPattern (int index)
    {
        if (attachment == nullptr || param == nullptr) return;
        if (echoOn() && onEcho != nullptr) onEcho (false);
        const int wanted = juce::jlimit (0, param->choices.size() - 1, index);
        const bool wasCustom = custom != nullptr && custom->getValue() > 0.5f;
        if (wasCustom) { custom->beginChangeGesture(); custom->setValueNotifyingHost (0); custom->endChangeGesture(); }
        attachment->setValueAsCompleteGesture ((float) wanted);
        if (wanted >= 1 && wanted <= trench::kNumFuncGenPatterns)
        {
            selectLength (trench::Movement::authoredLengthChoice (trench::kFuncGenPatterns[wanted - 1]));
            selectPlayback (0);
        }
    }
    std::function<void()> onRestart;
    std::function<juce::String()> customName;
    std::function<juce::StringArray()> savedNames;
    std::function<void (int)> onSaved;
    std::function<void()> onEdit;
    void selectLength (int index)
    {
        if (lengthAttachment != nullptr && length != nullptr)
            lengthAttachment->setValueAsCompleteGesture ((float) juce::jlimit (0, length->choices.size() - 1, index));
    }
    void selectPlayback (int index)
    {
        if (playbackAttachment != nullptr && playback != nullptr)
            playbackAttachment->setValueAsCompleteGesture ((float) juce::jlimit (0, playback->choices.size() - 1, index));
    }
    juce::String nameText() const
    {
        if (echoOn()) return "Echo";
        if (custom != nullptr && custom->getValue() > 0.5f && customName != nullptr)
            return customName();
        if (selectedPattern() == 0 || param == nullptr) return "Modulation: off";
        return param->choices[selectedPattern()];
    }
    juce::String displayText() const { return nameText(); }
    static constexpr const char* kRoles[] = { "Pulses", "Sways", "Climbs", "Falls", "Lands" };
    static const char* role (const trench::FuncGenPattern& pattern)
    {
        if (pattern.direction == 5) return "Lands";
        if (pattern.direction == 1) return "Falls";
        const juce::String name (pattern.name);
        for (const auto* pulse : { "Triplet Relay", "Rail Switch", "Relay Teeth", "Nerve Tick", "Flip Relay", "Square Bloom" })
            if (name == pulse) return "Pulses";
        for (const auto* sway : { "Backbeat Bloom", "Eighth Sway", "Quarter Arc", "Pendulum Teeth", "Wide Breath", "Long Arc", "Slow Tide", "Orbit" })
            if (name == sway) return "Sways";
        return "Climbs";
    }
    static juce::String barsText (double bars)
    {
        const double sixteenths = bars * 16.0;
        const int rounded = juce::roundToInt (sixteenths);
        if (std::abs (sixteenths - rounded) > 1.0e-6 || rounded % 2 != 0)
            return juce::String (bars, 2) + " bars";
        const int whole = rounded / 16, eighths = (rounded % 16) / 2;
        const int common = std::gcd (eighths, 8);
        const auto fraction = eighths > 0 ? juce::String (eighths / common) + "/" + juce::String (8 / common) : juce::String();
        const auto number = whole == 0 ? fraction
                          : fraction.isEmpty() ? juce::String (whole) : juce::String (whole) + " " + fraction;
        return number + (bars > 1.0 ? " bars" : " bar");
    }
    static juce::String barsShort (double bars)
    {
        return barsText (bars).upToFirstOccurrenceOf (" bar", false, false)
                   .replace ("1/4", juce::String::charToString (0x00bc))
                   .replace ("1/2", juce::String::charToString (0x00bd))
                   .replace ("3/4", juce::String::charToString (0x00be));
    }
    std::vector<BodyBrowser::Row> browserRows() const
    {
        std::vector<BodyBrowser::Row> out;
        if (param == nullptr) return out;
        const bool usingCustom = custom != nullptr && custom->getValue() > 0.5f;
        out.push_back ({ "Off", 0, ! usingCustom && selectedPattern() == 0 && ! echoOn() });
        out.push_back ({ "Echo", kEchoRow, echoOn() });
        for (const auto* group : kRoles)
        {
            bool heading = false;
            for (int choice = 0; choice < trench::Movement::kRateChoices; ++choice)
                for (int i = 1; i <= trench::kNumFuncGenPatterns && i < param->choices.size(); ++i)
                {
                    const auto& pattern = trench::kFuncGenPatterns[i - 1];
                    if (juce::String (role (pattern)) != group || trench::Movement::authoredLengthChoice (pattern) != choice) continue;
                    if (! heading)
                    {
                        out.push_back ({ group, -1, false, true, "bars" });
                        heading = true;
                    }
                    out.push_back ({ param->choices[i], i, ! usingCustom && selectedPattern() == i, false,
                                     barsShort (trench::Movement::rateBars (choice)) });
                }
        }
        const auto saved = savedNames != nullptr ? savedNames() : juce::StringArray();
        if (! saved.isEmpty())
        {
            out.push_back ({ "Saved", -1, false, true });
            for (int i = 0; i < saved.size(); ++i)
                out.push_back ({ saved[i], 400 + i, usingCustom && customName != nullptr && customName() == saved[i] });
        }
        return out;
    }
    void commitBrowserRow (int id)
    {
        if (id == kEchoRow) { if (onEcho != nullptr) onEcho (true); repaint(); }
        else if (id >= 400)
        {
            if (echoOn() && onEcho != nullptr) onEcho (false);
            if (onSaved != nullptr) onSaved (id - 400);
        }
        else if (id == 300) { if (onRestart != nullptr) onRestart(); }
        else if (id >= 200) selectPlayback (id - 200);
        else if (id >= 100) selectLength (id - 100);
        else if (id >= 0) selectPattern (id);
    }
    void showPatterns()
    {
        if (onEdit) { onEdit(); return; }
        if (param == nullptr) return;
        juce::SharedResourcePointer<SelectorLookAndFeel> look;
        juce::PopupMenu menu;
        menu.setLookAndFeel (&*look);
        const bool usingCustom = custom != nullptr && custom->getValue() > 0.5f;
        for (int i = 0; i < param->choices.size(); ++i)
            menu.addItem (i + 1, param->choices[i], true, ! usingCustom && selectedPattern() == i);
        const auto saved = savedNames != nullptr ? savedNames() : juce::StringArray();
        if (! saved.isEmpty())
        {
            menu.addSeparator();
            for (int i = 0; i < saved.size(); ++i)
                menu.addItem (400 + i, saved[i], true, usingCustom && customName != nullptr && customName() == saved[i]);
        }
        menu.addSeparator();
        juce::PopupMenu lengths, modes;
        lengths.setLookAndFeel (&*look);
        modes.setLookAndFeel (&*look);
        if (length != nullptr)
            for (int i = 0; i < length->choices.size(); ++i)
                lengths.addItem (100 + i, length->choices[i], true, length->getIndex() == i);
        if (playback != nullptr)
            for (int i = 0; i < playback->choices.size(); ++i)
                modes.addItem (200 + i, playback->choices[i], true, playback->getIndex() == i);
        menu.addSubMenu ("Rate", lengths);
        menu.addSubMenu ("Playback", modes);
        menu.addItem (300, "Restart", selectedPattern() > 0 && onRestart != nullptr);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
            [safe = juce::Component::SafePointer<ModulationChip> (this), look] (int result)
            {
                if (safe == nullptr || result <= 0) return;
                if (result >= 400) { if (safe->onSaved != nullptr) safe->onSaved (result - 400); }
                else if (result == 300) { if (safe->onRestart != nullptr) safe->onRestart(); }
                else if (result >= 200) safe->selectPlayback (result - 200);
                else if (result >= 100) safe->selectLength (result - 100);
                else safe->selectPattern (result - 1);
            });
    }
    bool isOn() const { return echoOn() || selectedPattern() > 0 || (custom != nullptr && custom->getValue() > 0.5f); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasDraggedSinceMouseDown() && e.getNumberOfClicks() == 1) showPatterns();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (wheel.deltaY == 0.0f)
            return;
        selectPattern (selectedPattern() - (wheel.deltaY > 0.0f ? 1 : -1));
    }
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey) { showPatterns(); return true; }
        if (key == juce::KeyPress::leftKey) { selectPattern (selectedPattern() - 1); return true; }
        if (key == juce::KeyPress::rightKey) { selectPattern (selectedPattern() + 1); return true; }
        return false;
    }
    void setActive (bool isActive)
    {
        if (isActive == active) return;
        active = isActive;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const bool lit = active || isMouseOver (true);
        const auto b = getLocalBounds().toFloat();
        drawFrostedGlassControl (g, b, 3.0f, lit, t);
        const float chevronW = juce::jmin (20.0f, b.getHeight());
        const float divider = b.getRight() - chevronW;
        const auto ink = t.textColour ("typeName", juce::Colour (0xff1a1713));
        const auto arrow = b.withLeft (divider).withSizeKeepingCentre (9.0f, 5.0f).translated (0.0f, 0.5f);
        juce::Path arrowPath;
        arrowPath.addTriangle (arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
        g.setColour (juce::Colours::white.withAlpha (0.40f));
        g.fillPath (arrowPath, juce::AffineTransform::translation (0.0f, 1.0f));
        g.setColour (ink);
        g.fillPath (arrowPath);
        g.setFont (displayFont (juce::jmin (13.0f, b.getHeight() * 0.68f), false));
        g.drawText (displayText(), b.withRight (divider).reduced (6.0f, 0.0f).toNearestInt(),
                    juce::Justification::centredLeft, true);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
private:
    Theme t;
    juce::AudioParameterChoice* param = nullptr;
    juce::RangedAudioParameter* custom = nullptr;
    juce::AudioParameterChoice* length = nullptr;
    juce::AudioParameterChoice* playback = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment, lengthAttachment, playbackAttachment;
    bool active = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationChip)
};
}
