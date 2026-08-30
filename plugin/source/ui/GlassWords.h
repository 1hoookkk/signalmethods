#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include "../parameters/TrenchParameters.h"
#include "../dsp/Movement.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include <vector>
namespace trench::ui
{
struct LightMenuLnF : juce::LookAndFeel_V4
{
    juce::Font getPopupMenuFont() override { return displayFont (13.0f, false); }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (juce::Colour (0xfffcfcfd));
        g.setColour (juce::Colour (0xff5a5750));
        g.drawRect (juce::Rectangle<int> (0, 0, w, h), 1);
    }
    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool, bool isActive, bool isHighlighted, bool isTicked,
                            bool, const juce::String& text, const juce::String&,
                            const juce::Drawable*, const juce::Colour*) override
    {
        const bool hot = isHighlighted && isActive;
        if (hot)
        {
            g.setColour (juce::Colour (0xff3cc8be).withAlpha (0.16f));
            g.fillRect (area.reduced (1));
            g.setColour (juce::Colour (0xff3cc8be).withAlpha (0.85f));
            g.fillRect (area.getX() + 1, area.getY() + 1, 2, area.getHeight() - 2);
        }
        g.setFont (displayFont (13.0f, isTicked));
        g.setColour (juce::Colour (0xff2a2722).withAlpha (isActive ? 1.0f : 0.4f));
        g.drawText (text, area.reduced (8, 0), juce::Justification::centredLeft, false);
        if (isTicked)
        {
            juce::Path check;
            const float cx = (float) area.getX() + 9.0f, cy = (float) area.getCentreY();
            check.startNewSubPath (cx - 3.0f, cy);
            check.lineTo (cx - 1.0f, cy + 2.5f);
            check.lineTo (cx + 3.5f, cy - 3.0f);
            g.setColour (juce::Colour (0xff2a2722));
            g.strokePath (check, { 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        }
    }
};

class GlassWords final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    GlassWords (juce::AudioProcessorValueTreeState& apvts, const Theme& theme)
        : t (theme), param (apvts.getParameter (ParamID::movePreset))
    {
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Movement");
        setHelpText ("Movement - click the name to punch it in/out, wheel to step, double-click for the list");
        setTooltip ("Movement: click = on/off, wheel = step, double-click = list");
    }
    std::function<bool()> livePhraseProvider;
    void setActive (bool isActive)
    {
        if (isActive == active) return;
        active = isActive;
        repaint();
    }
    std::vector<int> selectable() const
    {
        std::vector<int> out;
        if (param == nullptr) return out;
        const auto names = param->getAllValueStrings();
        const bool liveReady = livePhraseProvider == nullptr || livePhraseProvider();
        for (int i = 1; i < names.size(); ++i)
        {
            if (i == trench::Movement::kGrowlIndex) continue;
            if (names[i] == "LIVE" && ! liveReady) continue;
            out.push_back (i);
        }
        return out;
    }
    bool armed() const { return shapeIndex() != 0; }
    void step (int dir)
    {
        const auto list = selectable();
        if (list.empty()) return;
        int at = dir > 0 ? -1 : 0;
        if (armed())
            for (int i = 0; i < (int) list.size(); ++i)
                if (list[(size_t) i] == shapeIndex()) at = i;
        const int n = (int) list.size();
        write ((float) list[(size_t) (((at + dir) % n + n) % n)]);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wd) override
    {
        if (! juce::approximatelyEqual (wd.deltaY, 0.0f))
            step (wd.deltaY > 0 ? 1 : -1);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()) || param == nullptr)
            return;
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, param);
            return;
        }
        if (armed())
        {
            lastArmed = shapeIndex();
            write (0.0f);
        }
        else
        {
            const auto list = selectable();
            int target = lastArmed;
            if (target <= 0 && ! list.empty()) target = list.front();
            if (target > 0) write ((float) target);
        }
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || param == nullptr) return;
        juce::PopupMenu m;
        m.setLookAndFeel (&menuLnF);
        const bool on = armed();
        m.addItem (1, "OFF", true, ! on);
        m.addSeparator();
        const auto names = param->getAllValueStrings();
        const auto list = selectable();
        for (int i = 1; i < names.size(); ++i)
            m.addItem (i + 2, names[i], std::find (list.begin(), list.end(), i) != list.end(), on && shapeIndex() == i);
        juce::Component::SafePointer<GlassWords> self (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [self] (int id)
                         {
                             if (self == nullptr || id <= 0) return;
                             self->write (id == 1 ? 0.0f : (float) (id - 2));
                         });
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds();
        const auto ink = t.curveColour();
        const bool on = armed();
        const juce::Rectangle<float> lamp { (float) b.getX() + 1.0f, (float) b.getCentreY() - 2.5f, 5.0f, 5.0f };
        g.setColour (active ? t.modulationLamp() : ink.withAlpha (on ? 0.70f : 0.30f));
        g.fillEllipse (lamp);
        g.setFont (displayFont (10.5f, false));
        g.setColour (active ? t.modulationLamp().withAlpha (0.95f) : ink.withAlpha (on || hover ? 0.85f : 0.55f));
        g.drawText (stateWord(), b.withTrimmedLeft (11), juce::Justification::centredLeft, false);
    }
private:
    int shapeIndex() const noexcept
    {
        if (param == nullptr) return 0;
        return juce::jlimit (0, juce::jmax (0, param->getAllValueStrings().size() - 1),
                             juce::roundToInt (param->convertFrom0to1 (param->getValue())));
    }
    juce::String stateWord() const
    {
        if (param == nullptr) return "OFF";
        if (armed()) return param->getCurrentValueAsText();
        const auto names = param->getAllValueStrings();
        if (lastArmed > 0 && lastArmed < names.size()) return names[lastArmed];
        return "OFF";
    }
    void write (float denorm)
    {
        if (param == nullptr) return;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (denorm));
        param->endChangeGesture();
        repaint();
    }
    Theme t;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    LightMenuLnF menuLnF;
    bool hover = false;
    bool active = false;
    int lastArmed = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassWords)
};

class FollowLamp final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    static constexpr float kDepth = 0.6f;
    FollowLamp (juce::AudioProcessorValueTreeState& apvts, const Theme& theme)
        : t (theme), param (apvts.getParameter (ParamID::envAmount))
    {
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Follow");
        setHelpText ("Follow - the filter breathes with the input's hits; click to switch");
        setTooltip ("Follow: click = on/off");
    }
    bool isOn() const { return param != nullptr && param->getValue() > 0.0f; }
    void toggle()
    {
        if (param == nullptr) return;
        param->beginChangeGesture();
        param->setValueNotifyingHost (isOn() ? 0.0f : kDepth);
        param->endChangeGesture();
        repaint();
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition())) return;
        if (e.mods.isPopupMenu()) { showParamContextMenu (*this, param); return; }
        toggle();
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds();
        const auto ink = t.curveColour();
        const bool on = isOn();
        const juce::Rectangle<float> lamp { (float) b.getX() + 1.0f, (float) b.getCentreY() - 2.5f, 5.0f, 5.0f };
        g.setColour (on ? ink.withAlpha (0.95f) : ink.withAlpha (0.40f));
        g.fillEllipse (lamp);
        g.setFont (displayFont (10.5f, false));
        g.setColour (on ? ink.withAlpha (0.95f) : ink.withAlpha (hover ? 0.92f : 0.68f));
        g.drawText ("FOLLOW", b.withTrimmedLeft (11), juce::Justification::centredLeft, false);
    }
private:
    Theme t;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool hover = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FollowLamp)
};

class GlassValue final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    GlassValue (juce::AudioProcessorValueTreeState& apvts, const Theme& theme,
                const juce::String& paramID, juce::String word)
        : t (theme), label (std::move (word)), param (apvts.getParameter (paramID))
    {
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setTitle (label);
        setHelpText (label + " - drag up/down or wheel; double-click to reset");
        setTooltip (label + ": drag/wheel, double-click reset");
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showParamContextMenu (*this, param); return; }
        if (attachment != nullptr) attachment->beginGesture();
        e.source.enableUnboundedMouseMovement (true, false);
        dragStartY = e.position.y;
        valueAtStart = value();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (attachment == nullptr || param == nullptr || e.mods.isPopupMenu()) return;
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (
            juce::jlimit (0.0f, 1.0f, valueAtStart + (dragStartY - e.position.y) / 72.0f * fine)));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (attachment != nullptr) attachment->endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (attachment != nullptr && param != nullptr)
            attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wd) override
    {
        if (attachment == nullptr || param == nullptr) return;
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, value() + wd.deltaY * 0.08f)));
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds();
        const auto ink = t.curveColour();
        const bool on = value() > 0.0f;
        const juce::Rectangle<float> lamp { (float) b.getX() + 1.0f, (float) b.getCentreY() - 2.5f, 5.0f, 5.0f };
        g.setColour (ink.withAlpha (on ? 0.85f : 0.30f));
        g.fillEllipse (lamp);
        g.setFont (displayFont (10.5f, false));
        g.setColour (ink.withAlpha (on || hover ? 0.85f : 0.55f));
        g.drawText (label, b.withTrimmedLeft (11), juce::Justification::centredLeft, false);
    }
private:
    float value() const noexcept { return param != nullptr ? param->getValue() : 0.0f; }
    Theme t;
    juce::String label;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float dragStartY = 0.0f, valueAtStart = 0.0f;
    bool hover = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassValue)
};
}
