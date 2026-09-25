#pragma once
#include "ParamInteraction.h"
#include "Theme.h"
#include "../parameters/TrenchParameters.h"
#include "BinaryData.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>
#include <memory>
namespace trench::ui
{
class MixKnob final : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    MixKnob (juce::AudioProcessorValueTreeState& apvts, const Theme& theme,
             const juce::String& paramID,
             juce::String displayLabel = "MIX")
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
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { repaint(); });
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
        dragStartY = e.position.y;
        valueAtStart = currentNormalised();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (attachment == nullptr || param == nullptr || e.mods.isPopupMenu())
            return;
        const float travel = 72.0f;
        const float scale = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const float next = juce::jlimit (0.0f, 1.0f,
                                         valueAtStart + (dragStartY - e.position.y) / travel * scale);
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
        const float next = juce::jlimit (0.0f, 1.0f,
                                         currentNormalised() + wheel.deltaY * 0.08f);
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (next));
        repaint();
    }
    void setActivity (float a) noexcept
    {
        a = juce::jlimit (0.0f, 1.0f, a);
        if (std::abs (a - activity) > 0.01f)
        {
            activity = a;
            repaint();
        }
    }
    void paint (juce::Graphics& g) override
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (isEnabled() ? 1.0f : 0.32f);
        const auto b = getLocalBounds().toFloat();
        const float value = currentNormalised();
        const auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (param);
        const float d = kBayKnobDiameter;
        const auto knob = juce::Rectangle<float> (d, d)
                              .withCentre ({ b.getX() + d * 0.5f, b.getCentreY() });
        const float blockX = b.getX() + d + 4.0f;
        const float boxTop = b.getCentreY() - (float) kBayValueHeight * 0.5f;
        const float capW = juce::jmin ((float) kBayValueWidth + 26.0f,
                                       b.getRight() - blockX);
        drawBayCaption (g, juce::Rectangle<float> (blockX, boxTop - 11.0f, capW, 11.0f),
                        label, t);
        const auto c = knob.getCentre();
        const auto teal = t.rollerIllumination();
        {
            if (strip.isNull())
            {
                static juce::Image* machinedPtr = new juce::Image();
                juce::Image& machined = *machinedPtr;
                if (machined.isNull())
                {
                    machined = juce::ImageCache::getFromMemory (BinaryData::trench_knob_strip_png,
                                                                BinaryData::trench_knob_strip_pngSize)
                                   .createCopy();
                    juce::Image::BitmapData bd (machined, juce::Image::BitmapData::readWrite);
                    const auto deepen = [] (juce::uint8 v)
                    {
                        return (juce::uint8) juce::roundToInt (
                            std::pow ((float) v / 255.0f, 1.45f) * 255.0f);
                    };
                    for (int py = 0; py < bd.height; ++py)
                        for (int px = 0; px < bd.width; ++px)
                        {
                            const auto colr = bd.getPixelColour (px, py);
                            if (colr.getAlpha() > 0)
                                bd.setPixelColour (px, py,
                                    juce::Colour::fromRGBA (deepen (colr.getRed()),
                                                            deepen (colr.getGreen()),
                                                            deepen (colr.getBlue()),
                                                            colr.getAlpha()));
                        }
                }
                strip = machined;
            }
            constexpr int frameSize = 96, frameCount = 61;
            const int frame = juce::jlimit (0, frameCount - 1,
                                            juce::roundToInt ((1.0f - value) * (float) (frameCount - 1)));
            const float frameD = d * (96.0f / 76.0f);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (strip,
                         (int) (c.x - frameD * 0.5f), (int) (c.y - frameD * 0.5f),
                         (int) frameD, (int) frameD,
                         frame * frameSize, 0, frameSize, frameSize,
                         false);
        }
        {
            if (activity > 0.01f && value > 0.001f)
            {
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, d * 0.36f, d * 0.36f, 0.0f,
                                   -2.36f, juce::jmap (value, -2.36f, 2.36f), true);
                g.setColour (teal.withAlpha (0.40f * activity));
                g.strokePath (arc, { 5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
            }
        }
        {
            const float boxW = juce::jmin ((float) kBayValueWidth, b.getRight() - blockX - 1.0f);
            const auto box = juce::Rectangle<float> (boxW, (float) kBayValueHeight)
                                 .withCentre ({ blockX + capW * 0.5f, b.getCentreY() });
            drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
            drawCrispText (g, box.reduced (4.0f, 1.0f),
                           choice != nullptr ? choice->getCurrentChoiceName()
                                             : juce::String (juce::roundToInt (value * 100.0f)),
                           kBayValuePt, t.textColour ("morphReadout", juce::Colour (0xff2a2722)), true);
        }
    }
private:
    float activity = 0.0f;
    float currentNormalised() const noexcept
    {
        return param != nullptr ? param->getValue() : 0.0f;
    }
    Theme t;
    juce::String label;
    juce::Image strip;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float defaultDenorm = 0.0f;
    float dragStartY = 0.0f;
    float valueAtStart = 0.0f;
    bool hover = false;
};
}
