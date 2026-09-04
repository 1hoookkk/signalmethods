#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
namespace trench::ui
{
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
        if (attachment != nullptr) { attachment->beginGesture(); gestureOpen = true; }
        e.source.enableUnboundedMouseMovement (true, false);
        dragStartY = e.position.y;
        dragStartX = e.position.x;
        valueAtStart = value();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (attachment == nullptr || param == nullptr || e.mods.isPopupMenu()) return;
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const float travel = (e.position.x - dragStartX) + (dragStartY - e.position.y);
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (
            juce::jlimit (0.0f, 1.0f, valueAtStart + travel / 72.0f * fine)));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (attachment != nullptr && gestureOpen) attachment->endGesture(); gestureOpen = false; }
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
        const auto b = getLocalBounds().toFloat();
        const auto ink = t.curveColour();
        const float v = value();
        const bool on = v > 0.0f;
        const juce::Rectangle<float> lamp { b.getX() + 1.0f, b.getCentreY() - 2.5f, 5.0f, 5.0f };
        g.setColour (ink.withAlpha (on ? 0.85f : 0.30f));
        g.fillEllipse (lamp);
        g.setFont (displayFont (10.5f, true));
        g.setColour (ink.withAlpha (on || hover ? 0.90f : 0.60f));
        const auto wordArea = b.withTrimmedLeft (11.0f).withWidth (34.0f);
        g.drawText (label, wordArea, juce::Justification::centredLeft, false);
        const juce::Rectangle<float> track { wordArea.getRight() + 2.0f, b.getCentreY() - 2.0f, 30.0f, 4.0f };
        g.setColour (ink.withAlpha (0.22f));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (ink.withAlpha (hover ? 0.95f : 0.80f));
        g.fillRoundedRectangle (track.withWidth (juce::jmax (4.0f, track.getWidth() * v)), 2.0f);
        g.setFont (displayFont (10.5f, false));
        g.setColour (ink.withAlpha (on || hover ? 0.90f : 0.60f));
        g.drawText (juce::String (juce::roundToInt (v * 100.0f)),
                    b.withLeft (track.getRight() + 4.0f), juce::Justification::centredLeft, false);
    }
private:
    float value() const noexcept { return param != nullptr ? param->getValue() : 0.0f; }
    Theme t;
    juce::String label;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float dragStartY = 0.0f, dragStartX = 0.0f, valueAtStart = 0.0f;
    bool hover = false;
    bool gestureOpen = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassValue)
};
}
