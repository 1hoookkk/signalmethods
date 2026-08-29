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
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { repaint(); });
            attachment->sendInitialUpdate();
            defaultDenorm = param->convertFrom0to1 (param->getDefaultValue());
        }
    }

    void setCompact (bool c) { if (compact != c) { compact = c; repaint(); } }
    void setScale (float s) { scale = s; repaint(); }

    void setDimmed (bool d)
    {
        if (dimmed == d)
            return;
        dimmed = d;

        setAlpha (d ? 0.35f : (isEnabled() ? 1.0f : 0.32f));
        setInterceptsMouseClicks (! d, false);
        setMouseCursor (d ? juce::MouseCursor::NormalCursor
                          : juce::MouseCursor::UpDownResizeCursor);
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
    void paint (juce::Graphics& g) override
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (isEnabled() ? 1.0f : 0.32f);

        const auto b = getLocalBounds().toFloat();
        const float value = currentNormalised();
        const auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (param);
        const float d = compact ? 26.0f : kBayKnobDiameter * scale;
        const float boxWv = (float) kBayValueWidth * scale, boxHv = (float) kBayValueHeight * scale;

        const float blockW = boxWv + 26.0f * scale;
        const float startX = b.getX() + juce::jmax (0.0f, (b.getWidth() - (d + 4.0f + blockW)) * 0.5f);
        const auto knob = compact
            ? juce::Rectangle<float> (d, d).withCentre ({ b.getCentreX(), b.getY() + d * 0.5f + 1.0f })
            : juce::Rectangle<float> (d, d).withCentre ({ startX + d * 0.5f, b.getCentreY() });
        const float blockX = startX + d + 4.0f;

        constexpr float captionH = 10.0f;
        constexpr float captionGap = 1.0f;
        const float contentH = captionH + captionGap + boxHv;
        const float contentTop = b.getCentreY() - contentH * 0.5f;
        const float boxTop = contentTop + captionH + captionGap;
        const float capW = juce::jmin (blockW,
                                       b.getRight() - blockX);
        if (! compact)
            drawBayCaption (g, juce::Rectangle<float> (blockX, contentTop, capW, captionH),
                            label, t);
        const auto c = knob.getCentre();

        {

            if (strip.isNull())
            {

                static juce::Image* machinedPtr = new juce::Image();
                juce::Image& machined = *machinedPtr;
                if (machined.isNull())
                    machined = juce::ImageCache::getFromMemory (BinaryData::trench_knob_strip_png,
                                                                BinaryData::trench_knob_strip_pngSize);
                strip = machined;
            }
            constexpr int frameSize = 96, frameCount = 61;
            const int frame = juce::jlimit (0, frameCount - 1,
                                            juce::roundToInt ((1.0f - value) * (float) (frameCount - 1)));

            const float frameD = d * (96.0f / 76.0f);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

            {
                const auto shade = juce::Colour (0xff2a1f12);
                juce::ColourGradient cast (shade.withAlpha (0.42f), c.x + d * 0.06f, c.y + d * 0.10f,
                                           shade.withAlpha (0.0f), c.x + d * 0.06f, c.y + d * 0.62f, true);
                g.setGradientFill (cast);
                g.fillEllipse (c.x - d * 0.54f + d * 0.06f, c.y - d * 0.54f + d * 0.10f,
                               d * 1.08f, d * 1.08f);
            }

            g.drawImage (strip,
                         (int) (c.x - frameD * 0.5f), (int) (c.y - frameD * 0.5f),
                         (int) frameD, (int) frameD,
                         frame * frameSize, 0, frameSize, frameSize,
                         false);

            {
                juce::Graphics::ScopedSaveState save (g);
                juce::Path cap;
                cap.addEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d);
                g.reduceClipRegion (cap);

                const float rimD = d - 1.2f;
                juce::Path rim;
                rim.addEllipse (c.x - rimD * 0.5f, c.y - rimD * 0.5f, rimD, rimD);
                juce::ColourGradient edge (juce::Colours::white.withAlpha (0.55f),
                                           c.x, c.y - d * 0.5f,
                                           juce::Colour (0xff100c07).withAlpha (0.75f),
                                           c.x, c.y + d * 0.5f, false);
                edge.addColour (0.5, juce::Colours::transparentBlack);
                g.setGradientFill (edge);
                g.strokePath (rim, juce::PathStrokeType (1.1f));
            }
        }

        if (compact)
        {
            const auto cap = juce::Rectangle<float> (b.getX(), knob.getBottom() + 2.0f,
                                                     b.getWidth(), 10.0f);
            drawBayCaption (g, cap,
                            hover ? (choice != nullptr ? choice->getCurrentChoiceName()
                                                       : juce::String (juce::roundToInt (value * 100.0f)))
                                  : label, t);
        }
        else
        {

            const float boxW = juce::jmin (boxWv, b.getRight() - blockX - 1.0f);

            const auto box = juce::Rectangle<float> (blockX + (capW - boxW) * 0.5f,
                                                     boxTop, boxW, boxHv);

            drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
            drawCrispText (g, box.reduced (4.0f, 1.0f),
                           choice != nullptr ? choice->getCurrentChoiceName()
                                             : juce::String (juce::roundToInt (value * 100.0f)),
                           kBayValuePt * scale, t.textColour ("morphReadout", juce::Colour (0xff2a2722)));
        }
    }
private:
    float currentNormalised() const noexcept
    {
        return param != nullptr ? param->getValue() : 0.0f;
    }
    Theme t;
    juce::String label;
    bool compact = false;
    float scale = 1.0f;
    juce::Image strip;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float defaultDenorm = 0.0f;
    float dragStartY = 0.0f;
    float valueAtStart = 0.0f;
    bool hover = false;
    bool dimmed = false;
};
}
