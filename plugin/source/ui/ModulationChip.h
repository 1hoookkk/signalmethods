#pragma once
#include "Theme.h"
#include "SelectorLookAndFeel.h"
#include "BodyBrowser.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
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
        setTooltip ("Choose a motion pattern, length and playback. MORPH sets the lower end of its travel.");
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        if (length != nullptr)
            lengthAttachment = std::make_unique<juce::ParameterAttachment> (*length, [this] (float) { repaint(); });
        if (playback != nullptr)
            playbackAttachment = std::make_unique<juce::ParameterAttachment> (*playback, [this] (float) { repaint(); });
    }
    int selectedPattern() const noexcept { return param != nullptr ? param->getIndex() : 0; }
    void selectPattern (int index)
    {
        if (attachment == nullptr || param == nullptr) return;
        const int wanted = juce::jlimit (0, param->choices.size() - 1, index);
        const bool wasCustom = custom != nullptr && custom->getValue() > 0.5f;
        if (wasCustom) { custom->beginChangeGesture(); custom->setValueNotifyingHost (0); custom->endChangeGesture(); }
        attachment->setValueAsCompleteGesture ((float) wanted);
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
    juce::String displayText() const
    {
        if (custom != nullptr && custom->getValue() > 0.5f && customName != nullptr)
            return customName();
        if (selectedPattern() == 0 || param == nullptr) return "Modulation: off";
        auto label = param->choices[selectedPattern()];
        const auto separator = " " + juce::String::charToString (0x00b7) + " ";
        if (length != nullptr && length->getIndex() > 0)
            label += separator + juce::String (1 << (length->getIndex() - 1)) + " bar" + (length->getIndex() > 1 ? "s" : "");
        if (playback != nullptr && playback->getIndex() > 0)
            label += separator + (playback->getIndex() == 1 ? "Loop" : "Once");
        return label;
    }
    std::vector<BodyBrowser::Row> browserRows() const
    {
        std::vector<BodyBrowser::Row> out;
        if (param == nullptr) return out;
        const bool usingCustom = custom != nullptr && custom->getValue() > 0.5f;
        for (int i = 0; i < param->choices.size(); ++i)
            out.push_back ({ param->choices[i], i, ! usingCustom && selectedPattern() == i });
        const auto saved = savedNames != nullptr ? savedNames() : juce::StringArray();
        if (! saved.isEmpty())
        {
            out.push_back ({ "Saved", -1, false, true });
            for (int i = 0; i < saved.size(); ++i)
                out.push_back ({ saved[i], 400 + i, usingCustom && customName != nullptr && customName() == saved[i] });
        }
        if (length != nullptr)
        {
            out.push_back ({ "Length", -1, false, true });
            for (int i = 0; i < length->choices.size(); ++i)
                out.push_back ({ length->choices[i], 100 + i, length->getIndex() == i });
        }
        if (playback != nullptr)
        {
            out.push_back ({ "Playback", -1, false, true });
            for (int i = 0; i < playback->choices.size(); ++i)
                out.push_back ({ playback->choices[i], 200 + i, playback->getIndex() == i });
        }
        if (selectedPattern() > 0 && onRestart != nullptr)
            out.push_back ({ "Restart", 300 });
        return out;
    }
    void commitBrowserRow (int id)
    {
        if (id >= 400) { if (onSaved != nullptr) onSaved (id - 400); }
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
        menu.addSubMenu ("Length", lengths);
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
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasDraggedSinceMouseDown() && e.getNumberOfClicks() == 1) showPatterns();
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
        const auto b = getLocalBounds().toFloat();
        drawFrostedGlassControl (g, b, 3.0f, active || isMouseOver (true), t);
        const float chevronW = juce::jmin (20.0f, b.getHeight());
        const float divider = b.getRight() - chevronW;
        g.setColour (juce::Colour (0xff2e2b26).withAlpha (0.35f));
        g.fillRect (divider, b.getY() + 3.0f, 1.0f, b.getHeight() - 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillRect (divider + 1.0f, b.getY() + 3.0f, 1.0f, b.getHeight() - 6.0f);
        const auto ink = t.labelInk();
        const float cx = divider + chevronW * 0.5f + 0.5f, cy = b.getCentreY();
        juce::Path chevron;
        chevron.startNewSubPath (cx - 3.5f, cy - 1.8f);
        chevron.lineTo (cx, cy + 1.8f);
        chevron.lineTo (cx + 3.5f, cy - 1.8f);
        g.setColour (ink);
        g.strokePath (chevron, juce::PathStrokeType (1.2f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
        g.setFont (displayFont (juce::jmin (13.0f, b.getHeight() * 0.6f), false));
        g.drawText (displayText(), b.withRight (divider).reduced (5.0f, 0.0f).toNearestInt(),
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
