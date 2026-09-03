#pragma once
#include <functional>
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
    void setDimmed (bool d)
    {
        if (dimmed == d)
            return;
        dimmed = d;
        setAlpha (d ? 0.35f : 1.0f);
        setInterceptsMouseClicks (! d, false);
        setMouseCursor (d ? juce::MouseCursor::NormalCursor : juce::MouseCursor::UpDownResizeCursor);
        repaint();
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
        {
            attachment->beginGesture();
            gestureOpen = true;
        }
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
        if (attachment != nullptr && gestureOpen)
            attachment->endGesture();
        gestureOpen = false;
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
        const auto b = getLocalBounds().toFloat();
        const float value = currentNormalised();
        const float d = kBayKnobDiameter;
        const float blockW = (float) kBayValueWidth + 26.0f;
        const float startX = b.getX() + juce::jmax (0.0f, (b.getWidth() - (d + 4.0f + blockW)) * 0.5f);
        const juce::Point<float> c { startX + d * 0.5f, b.getCentreY() };
        const float blockX = startX + d + 4.0f;
        const float boxTop = b.getCentreY() - (float) kBayValueHeight * 0.5f;
        const float capW = juce::jmin (blockW, b.getRight() - blockX);
        drawBayCaption (g, juce::Rectangle<float> (startX, boxTop - 12.5f, d + 4.0f + capW, 12.5f), label, t);
        drawCap (g, c, value);
        const float boxW = juce::jmin ((float) kBayValueWidth, b.getRight() - blockX - 1.0f);
        const auto box = juce::Rectangle<float> (boxW, (float) kBayValueHeight)
                             .withCentre ({ blockX + capW * 0.5f, b.getCentreY() });
        drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
        drawCrispText (g, box.reduced (4.0f, 1.0f), formatValue != nullptr ? formatValue (value) : juce::String (juce::roundToInt (value * 100.0f)),
                       kBayValuePt, t.textColour ("morphReadout", juce::Colour (0xff2a2722)));
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
        const float d = kBayKnobDiameter;
        const float frameD = d * (96.0f / 76.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (strip,
                     (int) (c.x - frameD * 0.5f), (int) (c.y - frameD * 0.5f), (int) frameD, (int) frameD,
                     frame * frameSize, 0, frameSize, frameSize, false);
    }
public:
    std::function<juce::String (float)> formatValue;
private:
    Theme t;
    juce::String label;
    mutable juce::Image strip;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float defaultDenorm = 0.0f;
    float dragStartY = 0.0f;
    bool gestureOpen = false;
    float valueAtStart = 0.0f;
    bool hover = false;
    bool dimmed = false;
};
}
