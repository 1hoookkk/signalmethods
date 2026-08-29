#pragma once
#include "ParamInteraction.h"
#include "Theme.h"
#include "BinaryData.h"
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
        if (strip.isNull())
            strip = juce::ImageCache::getFromMemory (BinaryData::trench_knob_strip_png,
                                                     BinaryData::trench_knob_strip_pngSize);
        constexpr int frameSize = 96, frameCount = 61;
        const int frame = juce::jlimit (0, frameCount - 1,
                                        juce::roundToInt ((1.0f - value) * (float) (frameCount - 1)));
        const float d = kKnobD;
        const float frameD = d * (96.0f / 76.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        {
            const auto shade = juce::Colour (0xff2a1f12);
            juce::ColourGradient cast (shade.withAlpha (0.42f), c.x + d * 0.06f, c.y + d * 0.10f,
                                       shade.withAlpha (0.0f), c.x + d * 0.06f, c.y + d * 0.62f, true);
            g.setGradientFill (cast);
            g.fillEllipse (c.x - d * 0.54f + d * 0.06f, c.y - d * 0.54f + d * 0.10f, d * 1.08f, d * 1.08f);
        }
        g.drawImage (strip,
                     (int) (c.x - frameD * 0.5f), (int) (c.y - frameD * 0.5f), (int) frameD, (int) frameD,
                     frame * frameSize, 0, frameSize, frameSize, false);
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path cap;
            cap.addEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d);
            g.reduceClipRegion (cap);
            const float rimD = d - 1.2f;
            juce::Path rim;
            rim.addEllipse (c.x - rimD * 0.5f, c.y - rimD * 0.5f, rimD, rimD);
            juce::ColourGradient edge (juce::Colours::white.withAlpha (0.55f), c.x, c.y - d * 0.5f,
                                       juce::Colour (0xff100c07).withAlpha (0.75f), c.x, c.y + d * 0.5f, false);
            edge.addColour (0.5, juce::Colours::transparentBlack);
            g.setGradientFill (edge);
            g.strokePath (rim, juce::PathStrokeType (1.1f));
        }
    }
    Theme t;
    juce::String label;
    mutable juce::Image strip;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float defaultDenorm = 0.0f;
    float dragStartY = 0.0f;
    float valueAtStart = 0.0f;
    bool hover = false;
};
}
