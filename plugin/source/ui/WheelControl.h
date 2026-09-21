#pragma once

#include "Theme.h"
#include "ParamInteraction.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>
#include <memory>

namespace trench::ui
{

class WheelControl : public juce::Component,
                     public juce::SettableTooltipClient
{
public:

    static constexpr int kStripFrameWidth = 417;
    static constexpr float kBodyDarken = 1.0f;

    static juce::Image recolourGlow (const juce::Image& source, juce::Colour target)
    {
        juce::Image out = source.createCopy();
        juce::Image::BitmapData data (out, juce::Image::BitmapData::readWrite);
        const float mintHue = juce::Colour (0xff3cc8be).getHue();
        const float mintSat = juce::Colour (0xff3cc8be).getSaturation();
        const float targetHue = target.getHue();
        const float targetSat = target.getSaturation();
        for (int y = 0; y < data.height; ++y)
        {
            for (int x = 0; x < data.width; ++x)
            {
                const juce::Colour c = data.getPixelColour (x, y);
                if (c.getAlpha() == 0)
                    continue;
                const float sat = c.getSaturation();
                float dh = std::abs (c.getHue() - mintHue);
                dh = juce::jmin (dh, 1.0f - dh);
                if (sat < 0.18f || dh > 0.09f)
                {
                    data.setPixelColour (x, y, juce::Colour::fromHSV (c.getHue(), sat,
                                                                     c.getBrightness() * kBodyDarken,
                                                                     c.getFloatAlpha()));
                    continue;
                }
                const float scaled = juce::jlimit (0.0f, 1.0f, targetSat * juce::jmin (1.0f, sat / mintSat));
                data.setPixelColour (x, y, juce::Colour::fromHSV (targetHue, scaled, c.getBrightness(), c.getFloatAlpha()));
            }
        }
        return out;
    }

    WheelControl (juce::AudioProcessorValueTreeState& apvts, juce::String paramID,
                  juce::Image filmstrip, const Theme& theme)
        : strip (std::move (filmstrip)), t (theme)
    {
        numFrames = juce::jmax (1, strip.getWidth() / kStripFrameWidth);
        if (strip.isValid() && t.rollerIllumination() != juce::Colour (0xff3cc8be))
            strip = recolourGlow (strip, t.rollerIllumination());
        jassert (! strip.isValid() || strip.getWidth() % kStripFrameWidth == 0);
        isQControl = paramID.containsIgnoreCase ("q") || paramID.containsIgnoreCase ("slam");
        param = apvts.getParameter (paramID);
        jassert (param != nullptr);
        if (param != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { repaint(); });
            defaultDenorm = param->convertFrom0to1 (param->getDefaultValue());
            const auto name = param->getName (32);
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            setTitle (name);
            setHelpText (name + " - drag left/right or mouse-wheel; double-click to reset");
            setTooltip (name + ": drag/wheel, double-click reset");
            attachment->sendInitialUpdate();
        }
        setInterceptsMouseClicks (true, false);

        setPaintingIsUnclipped (true);
    }

    void setParameter (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        attachment.reset();
        param = apvts.getParameter (paramID);
        isQControl = paramID.containsIgnoreCase ("q") || paramID.containsIgnoreCase ("slam");
        if (param != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { repaint(); });
            defaultDenorm = param->convertFrom0to1 (param->getDefaultValue());
            const auto name = param->getName (32);
            setTitle (name);
            setHelpText (name + " - drag left/right or mouse-wheel; double-click to reset");
            setTooltip (name + ": drag/wheel, double-click reset");
            attachment->sendInitialUpdate();
        }
        repaint();
    }

    void setDisplayOverride (bool active, float normalised)
    {
        normalised = juce::jlimit (0.0f, 1.0f, normalised);
        if (displayOverrideActive != active || ! juce::approximatelyEqual (displayOverrideValue, normalised))
        {
            displayOverrideActive = active;
            displayOverrideValue = normalised;
            repaint();
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hovering = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovering = false; repaint(); }

    std::function<void()> onGestureEnd;
    std::function<void()> onAltDragStart;
    std::function<void (float)> onAltDragSample;
    std::function<void()> onAltDragEnd;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, param);
            return;
        }
        pressing = true;
        altRecording = e.mods.isAltDown() && onAltDragSample != nullptr;
        if (attachment != nullptr)
        {
            attachment->beginGesture();
            gestureOpen = true;
        }

        e.source.enableUnboundedMouseMovement (true, false);
        dragStartX   = e.position.x;
        valueAtStart = currentNormalised();

        if (altRecording)
        {
            if (onAltDragStart != nullptr) onAltDragStart();
            onAltDragSample (currentNormalised());
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;
        if (e.mods.isShiftDown() && attachment != nullptr && param != nullptr)
        {

            const float w    = juce::jmax (1.0f, (float) getWidth());
            const float next = juce::jlimit (0.0f, 1.0f,
                                             valueAtStart + (e.position.x - dragStartX) / w * 0.25f);
            attachment->setValueAsPartOfGesture (param->convertFrom0to1 (next));
            repaint();
        }
        else
        {
            dragRelative (e);
        }
        if (altRecording && onAltDragSample != nullptr)
            onAltDragSample (currentNormalised());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        pressing = false;
        if (attachment != nullptr && gestureOpen)
            attachment->endGesture();
        gestureOpen = false;
        if (onGestureEnd != nullptr)
            onGestureEnd();
        if (altRecording)
        {
            altRecording = false;
            if (onAltDragEnd != nullptr) onAltDragEnd();
        }
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (attachment != nullptr)
            attachment->setValueAsCompleteGesture (defaultDenorm);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        juce::ignoreUnused (e);
        if (attachment == nullptr || param == nullptr)
            return;

        const float next = juce::jlimit (0.0f, 1.0f, currentNormalised() + wheel.deltaY * 0.08f);
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (next));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (! strip.isValid())
            return;

        const int fw = strip.getWidth() / numFrames;
        const int fh = strip.getHeight();
        if (fw <= 0 || fh <= 0)
            return;

        const int usableFrames = numFrames;
        const int last = usableFrames - 1;
        const int frame = juce::jlimit (0, last, juce::roundToInt (displayNormalised() * (float) last));

        static constexpr int kSeatDrop  = 0;
        static constexpr int kSeatShift = 0;

        const auto wheelRect = getLocalBounds().toFloat()
                                   .translated ((float) kSeatShift, (float) kSeatDrop);

        const float imageWidth = wheelRect.getWidth() + 8.0f;
        const auto frameRect = juce::Rectangle<float> (imageWidth, imageWidth * (float) fh / (float) fw)
                                   .withCentre (wheelRect.getCentre());

        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        juce::Graphics::ScopedSaveState slotClip (g);
        {
            const auto opening = wheelRect;
            juce::Path slot;
            slot.addRoundedRectangle (opening, 3.0f);
            g.reduceClipRegion (slot);
        }

        const float pixelScale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int pw = juce::jmax (1, juce::roundToInt (frameRect.getWidth()  * pixelScale));
        const int ph = juce::jmax (1, juce::roundToInt (frameRect.getHeight() * pixelScale));
        if (scaledIndex != frame || scaledFrame.getWidth() != pw
            || scaledFrame.getHeight() != ph)
        {
            scaledFrame = strip.getClippedImage ({ frame * fw, 0, fw, fh })
                              .rescaled (pw, ph, juce::Graphics::highResamplingQuality);
            scaledIndex = frame;
        }
        g.drawImage (scaledFrame, frameRect, juce::RectanglePlacement::stretchToFit);

        {
            juce::Graphics::ScopedSaveState light (g);
            g.reduceClipRegion (scaledFrame,
                                juce::AffineTransform::scale (frameRect.getWidth() / (float) pw,
                                                              frameRect.getHeight() / (float) ph)
                                    .translated (frameRect.getX(), frameRect.getY()));
            const auto front = wheelRect.withTop (wheelRect.getY() + wheelRect.getHeight() * 0.48f);
            const auto lightColour = juce::Colour (0xffd4ddd8);
            juce::ColourGradient sheen (lightColour.withAlpha (0.0f), front.getCentreX(), front.getY(),
                                        lightColour.withAlpha (0.0f), front.getCentreX(), front.getBottom(), false);
            sheen.addColour (0.35, lightColour.withAlpha (0.10f));
            sheen.addColour (0.62, lightColour.withAlpha (0.15f));
            g.setGradientFill (sheen);
            g.fillRect (front);
        }

        {
            const auto wheel = wheelRect;
            const auto shade = juce::Colour (0xff040608);
            juce::Graphics::ScopedSaveState save (g);

            const float endW = wheel.getWidth() * 0.18f;
            juce::ColourGradient left (shade.withAlpha (0.96f), wheel.getX(), wheel.getCentreY(),
                                       shade.withAlpha (0.0f), wheel.getX() + endW, wheel.getCentreY(), false);
            left.addColour (0.18, shade.withAlpha (0.72f));
            left.addColour (0.48, shade.withAlpha (0.32f));
            left.addColour (0.78, shade.withAlpha (0.07f));
            g.setGradientFill (left);
            g.fillRect (wheel.withWidth (endW));

            juce::ColourGradient right (shade.withAlpha (0.96f), wheel.getRight(), wheel.getCentreY(),
                                        shade.withAlpha (0.0f), wheel.getRight() - endW, wheel.getCentreY(), false);
            right.addColour (0.18, shade.withAlpha (0.72f));
            right.addColour (0.48, shade.withAlpha (0.32f));
            right.addColour (0.78, shade.withAlpha (0.07f));
            g.setGradientFill (right);
            g.fillRect (wheel.withLeft (wheel.getRight() - endW));


        }

    }

private:
    juce::Image scaledFrame;
    int scaledIndex = -1;

    float currentNormalised() const
    {
        return param != nullptr ? juce::jlimit (0.0f, 1.0f, param->getValue()) : 0.0f;
    }

    float displayNormalised() const
    {

        return (displayOverrideActive && ! pressing) ? displayOverrideValue : currentNormalised();
    }

    static constexpr float kPacketX0 = 0.0398f;
    static constexpr float kPacketX1 = 0.8367f;

    void dragRelative (const juce::MouseEvent& e)
    {
        if (attachment == nullptr || param == nullptr)
            return;

        const float throwPx = juce::jmax (1.0f, (kPacketX1 - kPacketX0) * (float) getWidth());
        const float next = juce::jlimit (0.0f, 1.0f,
                                         valueAtStart + (e.position.x - dragStartX) / throwPx);
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (next));
        repaint();
    }

    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::Image strip;
    int numFrames = 1;
    Theme t;
    bool hovering = false;
    bool pressing = false;
    bool gestureOpen = false;
    bool altRecording = false;
    bool isQControl = false;
    bool displayOverrideActive = false;
    float displayOverrideValue = 0.0f;
    float defaultDenorm = 0.0f;
    float dragStartX = 0.0f;
    float valueAtStart = 0.0f;
};

}
