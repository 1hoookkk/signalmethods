#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include "../dsp/KeySnap.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
namespace trench::ui
{
class KeySnapBox final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    KeySnapBox (juce::AudioProcessorValueTreeState& apvts, const Theme& theme)
        : t (theme), param (apvts.getParameter (ParamID::keySnap))
    {
        setInterceptsMouseClicks (true, false);
        setWantsKeyboardFocus (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Key Snap");
        setHelpText ("Click to switch automatic key following on or off. Right-click to choose a key.");
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { refreshState(); });
        refreshState();
    }
    void setSuggestionProviders (std::function<int()> primary)
    {
        primarySuggestion = std::move (primary);
        refreshSuggestion();
    }
    void refreshSuggestion()
    {
        const int next = suggestion();
        if (next == lastSuggestion) return;
        lastSuggestion = next;
        if (currentChoice() == trench::KeySnap::kAutoChoice) refreshState();
    }
    juce::String displayText() const
    {
        const int choice = currentChoice();
        if (choice == 0) return "OFF";
        if (choice == trench::KeySnap::kAutoChoice)
        {
            const int detected = suggestion();
            return detected >= 0 ? shortSuggestionText (detected) : juce::String ("AUTO");
        }
        return shortChoiceText (choice);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (param == nullptr || ! getLocalBounds().contains (e.position.toInt())) return;
        if (e.mods.isPopupMenu()) showKeyMenu();
        else if (e.mods.isLeftButtonDown()) toggle();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
        {
            toggle();
            return true;
        }
        if (key == juce::KeyPress::downKey)
        {
            showKeyMenu();
            return true;
        }
        return false;
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const auto label = b.removeFromLeft (26.0f);
        b.removeFromLeft (4.0f);
        const float height = juce::jmin (17.0f, b.getHeight());
        const auto box = b.withSizeKeepingCentre (b.getWidth(), height);
        g.setFont (displayFont (11.5f, true));
        g.setColour (t.labelInk());
        g.drawText ("KEY", label.toNearestInt(), juce::Justification::centredLeft, false);
        const bool listening = currentChoice() == trench::KeySnap::kAutoChoice;
        drawMutedBoneReadout (g, box, height * 0.17f, hover || listening, t);
        if (listening)
        {
            g.setColour (t.rollerIllumination().withAlpha (0.28f));
            g.fillRoundedRectangle (box.reduced (2.0f), juce::jmax (1.5f, height * 0.17f - 1.5f));
        }
        auto text = box.reduced (4.0f, 0.0f);
        if (hover)
        {
            const auto arrow = text.removeFromRight (9.0f).withSizeKeepingCentre (6.0f, 3.5f);
            juce::Path v;
            v.startNewSubPath (arrow.getX(), arrow.getY());
            v.lineTo (arrow.getCentreX(), arrow.getBottom());
            v.lineTo (arrow.getRight(), arrow.getY());
            g.setColour (juce::Colour (0xff2a2722));
            g.strokePath (v, juce::PathStrokeType (1.2f));
        }
        g.setFont (displayFont (11.5f));
        g.setColour (juce::Colour (0xff2a2722));
        g.drawText (displayText(), text.toNearestInt(), juce::Justification::centred, false);
    }
private:
    void toggle()
    {
        selectChoice (currentChoice() == 0 ? trench::KeySnap::kAutoChoice : 0);
    }
    void selectChoice (int choice)
    {
        if (param == nullptr) return;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) choice));
        param->endChangeGesture();
        if (onAnnounce != nullptr)
            onAnnounce (choice == 0 ? juce::String ("KEY OFF: THE BODY AS WRITTEN")
                        : choice == trench::KeySnap::kAutoChoice ? juce::String ("KEY: LISTENING, THE BODY TUNES TO YOUR KEY")
                        : "KEY: THE BODY TUNED TO " + shortChoiceText (choice));
    }
public:
    std::function<void (const juce::String&)> onAnnounce;
private:
    void showKeyMenu()
    {
        if (param == nullptr) return;
        const int selected = currentChoice();
        juce::PopupMenu menu;
        juce::SharedResourcePointer<SelectorLookAndFeel> look;
        menu.setLookAndFeel (&*look);
        menu.addItem (1, "Off", true, selected == 0);
        menu.addItem (trench::KeySnap::kAutoChoice + 1, "Auto", true, selected == trench::KeySnap::kAutoChoice);
        menu.addSeparator();
        for (int note = 0; note < 12; ++note)
            menu.addItem (note + 14, shortSuggestionText (note), true,
                          trench::KeySnap::active (selected) && trench::KeySnap::root (selected) == note);
        if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>();
            editor != nullptr && editor->getHostContext() != nullptr)
        {
            menu.addSeparator();
            menu.addItem (1000, "Host controls...");
        }
        const juce::Component::SafePointer<KeySnapBox> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
            [safe, look] (int result)
            {
                if (safe == nullptr || result == 0) return;
                if (result == 1000) showParamContextMenu (*safe, safe->param);
                else if (result >= 1 && result <= trench::KeySnap::kAutoChoice + 1)
                    safe->selectChoice (result - 1);
            });
    }
    void refreshState()
    {
        setDescription ("KEY " + displayText());
        setTooltip ("KEY " + displayText() + ". KEY tunes the body's main resonance to the root of the key. Click for AUTO: it listens to what you play and follows. "
                    "Right-click to pick the key yourself. OFF leaves the body where its corners were written.");
        repaint();
    }
    int suggestion() const
    {
        return primarySuggestion ? juce::jlimit (-1, 23, primarySuggestion()) : -1;
    }
    static juce::String shortSuggestionText (int label)
    {
        if (label < 0 || label >= 24) return "--";
        static constexpr const char* notes[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        return juce::String (notes[label % 12]);
    }
    static juce::String shortChoiceText (int choice)
    {
        return trench::KeySnap::active (choice) ? shortSuggestionText (trench::KeySnap::root (choice)) : juce::String ("--");
    }
    int currentChoice() const
    {
        if (param == nullptr) return 0;
        const int last = juce::jmax (0, param->getNumSteps() - 1);
        return juce::jlimit (0, last, juce::roundToInt (param->convertFrom0to1 (param->getValue())));
    }
    Theme t;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    std::function<int()> primarySuggestion;
    int lastSuggestion = -2;
    bool hover = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeySnapBox)
};
}

