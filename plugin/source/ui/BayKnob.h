#pragma once
#include "ParamInteraction.h"
#include "Theme.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <memory>
namespace trench::ui
{
class BayKnob final : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    static constexpr float kKnobD    = 30.0f;
    static constexpr float kWellD    = 34.0f;
    static constexpr float kCaptionH = kLabelPt + 1.0f;
    static constexpr float kStackH   = kCaptionH + 2.0f + (float) kBayValueHeight + 3.0f + kWellD;
    BayKnob (juce::AudioProcessorValueTreeState& apvts, const Theme& theme,
             const juce::String& paramID, juce::String displayLabel)
        : t (theme), label (std::move (displayLabel)),
          param (apvts.getParameter (paramID))
    {
        setInterceptsMouseClicks (true, false);
        setPaintingIsUnclipped (true);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setTitle (label);
        setHelpText (label + " - drag up/down; Shift for fine; double-click to reset");
        setTooltip (label + ": drag/wheel, Shift fine, double-click reset");
        if (param != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
            attachment->sendInitialUpdate();
            defaultDenorm = param->convertFrom0to1 (param->getDefaultValue());
        }
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, param);
            return;
        }
        if (attachment != nullptr)
            attachment->beginGesture();
        e.source.enableUnboundedMouseMovement (true, false);
        dragStartY = e.position.y;
        valueAtStart = currentNormalised();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (attachment == nullptr || param == nullptr || e.mods.isPopupMenu())
            return;
        const float travel = 72.0f;
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const float next = juce::jlimit (0.0f, 1.0f, valueAtStart + (dragStartY - e.position.y) / travel * fine);
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (next));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (attachment != nullptr)
            attachment->endGesture();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (attachment != nullptr)
            attachment->setValueAsCompleteGesture (defaultDenorm);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (attachment == nullptr || param == nullptr)
            return;
        const float next = juce::jlimit (0.0f, 1.0f, currentNormalised() + wheel.deltaY * 0.08f);
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (next));
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (isEnabled() ? 1.0f : 0.32f);
        const auto b = getLocalBounds().toFloat();
        const float value = currentNormalised();
        const float top = b.getCentreY() - kStackH * 0.5f;
        const float cx = b.getCentreX();
        drawBayCaption (g, juce::Rectangle<float> (b.getX(), top, b.getWidth(), kCaptionH), label, t);
        const auto box = juce::Rectangle<float> ((float) kBayValueWidth, (float) kBayValueHeight)
                             .withCentre ({ cx, top + kCaptionH + 2.0f + (float) kBayValueHeight * 0.5f });
        drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
        const auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (param);
        drawCrispText (g, box.reduced (4.0f, 1.0f),
                       choice != nullptr ? choice->getCurrentChoiceName()
                                         : juce::String (juce::roundToInt (value * 100.0f)),
                       kBayValuePt, t.textColour ("morphReadout", juce::Colour (0xff2a2722)));
        const juce::Point<float> c { cx, box.getBottom() + 3.0f + kWellD * 0.5f };
        drawCap (g, c, value);
    }
private:
    float currentNormalised() const noexcept { return param != nullptr ? param->getValue() : 0.0f; }
    void drawCap (juce::Graphics& g, juce::Point<float> c, float value) const
    {
        const float r = kKnobD * 0.5f;
        const auto cap = juce::Rectangle<float> (kKnobD, kKnobD).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (cap.translated (0.0f, 2.2f).expanded (1.2f));
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillEllipse (cap.translated (0.0f, 3.8f).expanded (0.6f));
        juce::ColourGradient body (juce::Colour (0xff3b3b3e), c.x - r * 0.6f, c.y - r * 0.7f,
                                   juce::Colour (0xff0e0e10), c.x + r * 0.5f, c.y + r * 0.9f, true);
        body.addColour (0.55, juce::Colour (0xff1c1c1f));
        g.setGradientFill (body);
        g.fillEllipse (cap);
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addEllipse (cap);
            g.reduceClipRegion (clip);
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            for (int i = 0; i < 36; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
                const juce::Point<float> o { c.x + std::cos (a) * (r - 0.5f), c.y + std::sin (a) * (r - 0.5f) };
                const juce::Point<float> in { c.x + std::cos (a) * (r - 4.0f), c.y + std::sin (a) * (r - 4.0f) };
                g.drawLine ({ o, in }, 1.0f);
            }
        }
        juce::ColourGradient rim (juce::Colours::white.withAlpha (0.40f), c.x, cap.getY(),
                                  juce::Colours::black.withAlpha (0.85f), c.x, cap.getBottom(), false);
        g.setGradientFill (rim);
        g.drawEllipse (cap.reduced (0.6f), 1.1f);
        const float inset = r - 5.0f;
        juce::ColourGradient face (juce::Colour (0xff2a2a2d), c.x, c.y - inset,
                                   juce::Colour (0xff151517), c.x, c.y + inset, false);
        g.setGradientFill (face);
        g.fillEllipse (juce::Rectangle<float> (inset * 2.0f, inset * 2.0f).withCentre (c));
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (juce::Rectangle<float> (inset * 2.0f, inset * 2.0f).withCentre (c), 0.8f);
        const float angle = juce::degreesToRadians (-135.0f + 270.0f * juce::jlimit (0.0f, 1.0f, value));
        const juce::Point<float> dir { std::sin (angle), -std::cos (angle) };
        const juce::Point<float> p0 = c + dir * (inset * 0.30f);
        const juce::Point<float> p1 = c + dir * (inset * 0.92f);
        const auto glow = t.rollerIllumination();
        g.setColour (glow.withAlpha (hover ? 0.55f : 0.35f));
        g.drawLine ({ p0, p1 }, 4.5f);
        g.setColour (glow.brighter (0.25f));
        g.drawLine ({ p0, p1 }, 1.8f);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (juce::Rectangle<float> (2.4f, 2.4f).withCentre (p1));
    }
    Theme t;
    juce::String label;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float defaultDenorm = 0.0f;
    float dragStartY = 0.0f;
    float valueAtStart = 0.0f;
    bool hover = false;
};
}
