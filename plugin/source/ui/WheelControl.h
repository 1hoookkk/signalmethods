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

    WheelControl (juce::AudioProcessorValueTreeState& apvts, juce::String paramID,
                  juce::Image filmstrip, const Theme& theme)
        : strip (std::move (filmstrip)), t (theme)
    {
        numFrames = juce::jmax (1, strip.getWidth() / kStripFrameWidth);
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

    static constexpr float kSeatSide = 2.7f;
    static constexpr float kSeatBottom = 4.1f;
    static constexpr float kSeatCorner = 3.5f;
    static constexpr float kDrumProud = 2.46f;
    static constexpr float kDrumBelow = 4.49f;
    static constexpr float kSideOverhang = 5.0f;
    static juce::Rectangle<float> drumForHole (juce::Rectangle<float> hole)
    {
        return { hole.getX() - kSideOverhang, hole.getY() - kDrumProud,
                 hole.getWidth() + 2.0f * kSideOverhang, hole.getHeight() + kDrumProud + kDrumBelow };
    }

    void drawLamp (juce::Graphics& g, juce::Rectangle<float> drum, float value) const
    {
        if (value <= 0.0f)
            return;
        const float fade = juce::jmin (1.0f, value / 0.03f);
        const auto ink = t.curveColour().interpolatedWith (t.curveHighlight(), 0.30f);
        const float cx = drum.getX() + drum.getWidth() * 0.4988f;
        const float r = drum.getWidth() * 0.48f;
        const float cy = drum.getY() + drum.getHeight() * 0.495f;
        const float pitch = juce::degreesToRadians (7.6f);
        const float head = juce::degreesToRadians (-61.0f + 138.0f * value);
        const float arc = juce::degreesToRadians (84.0f);
        const float ledH = drum.getHeight() * 0.19f;
        for (int k = 0; (float) k * pitch <= arc; ++k)
        {
            const float a = head - (float) k * pitch;
            if (std::abs (a) >= juce::MathConstants<float>::halfPi)
                continue;
            const float along = 1.0f - (float) k * pitch / arc;
            const float level = fade * (0.25f + 0.75f * along) * std::sqrt (std::cos (a));
            const float x = cx + r * std::sin (a);
            const float w = r * pitch * std::cos (a) * 0.70f;
            const auto core = juce::Rectangle<float> (w, ledH).withCentre ({ x, cy });
            const auto haloArea = core.expanded (core.getWidth() * 0.55f, ledH * 0.75f);
            juce::ColourGradient halo (ink.withAlpha (0.30f * level), x, cy,
                                       ink.withAlpha (0.0f), haloArea.getRight(), cy, true);
            g.setGradientFill (halo);
            g.fillEllipse (haloArea);
            g.setColour (ink.withAlpha (0.62f * level));
            g.fillRoundedRectangle (core, ledH * 0.3f);
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (! strip.isValid())
            return;

        const int fw = strip.getWidth() / numFrames;
        const int fh = strip.getHeight();
        if (fw <= 0 || fh <= 0)
            return;

        const int frame = 0;

        const auto wheelRect = getLocalBounds().toFloat();
        const auto frameRect = wheelRect;

        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        juce::Graphics::ScopedSaveState seatClip (g);
        {
            const auto hole = wheelRect.reduced (kSideOverhang, 0.0f).withTrimmedTop (kDrumProud).withTrimmedBottom (kDrumBelow);
            juce::Path seat;
            seat.addRoundedRectangle (hole.getX(), hole.getY(), hole.getWidth(), hole.getHeight(),
                                      kSeatCorner, kSeatCorner, true, true, true, true);
            g.reduceClipRegion (seat);
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

        drawLamp (g, frameRect, displayNormalised());

        {
            juce::Graphics::ScopedSaveState crown (g);
            g.reduceClipRegion (scaledFrame,
                                juce::AffineTransform::scale (frameRect.getWidth()  / (float) pw,
                                                              frameRect.getHeight() / (float) ph)
                                    .translated (frameRect.getX(), frameRect.getY()));
            const float top = frameRect.getY(), h = frameRect.getHeight();
            juce::ColourGradient belly (juce::Colours::white.withAlpha (0.0f), 0.0f, top + h * 0.06f,
                                        juce::Colours::white.withAlpha (0.0f), 0.0f, top + h * 0.56f, false);
            belly.addColour (0.42, juce::Colours::white.withAlpha (0.16f));
            g.setGradientFill (belly);
            g.fillRect (frameRect);
            juce::ColourGradient under (juce::Colours::black.withAlpha (0.0f), 0.0f, top + h * 0.58f,
                                        juce::Colours::black.withAlpha (0.42f), 0.0f, frameRect.getBottom(), false);
            g.setGradientFill (under);
            g.fillRect (frameRect);
        }

        {
            const auto wheel = frameRect;
            const auto shade = juce::Colour (0xff17110a);
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (scaledFrame,
                                juce::AffineTransform::scale (frameRect.getWidth()  / (float) pw,
                                                              frameRect.getHeight() / (float) ph)
                                    .translated (frameRect.getX(), frameRect.getY()));
            const float endW = wheel.getWidth() * 0.19f;
            juce::ColourGradient left (shade.withAlpha (0.92f), wheel.getX(), wheel.getCentreY(),
                                       shade.withAlpha (0.0f), wheel.getX() + endW, wheel.getCentreY(), false);
            left.addColour (0.30, shade.withAlpha (0.50f));
            left.addColour (0.65, shade.withAlpha (0.16f));
            g.setGradientFill (left);
            g.fillRect (wheel.withWidth (endW));
            juce::ColourGradient right (shade.withAlpha (0.92f), wheel.getRight(), wheel.getCentreY(),
                                        shade.withAlpha (0.0f), wheel.getRight() - endW, wheel.getCentreY(), false);
            right.addColour (0.30, shade.withAlpha (0.50f));
            right.addColour (0.65, shade.withAlpha (0.16f));
            g.setGradientFill (right);
            g.fillRect (wheel.withLeft (wheel.getRight() - endW));
        }

        if (hovering || pressing)
        {
            juce::ColourGradient lift (juce::Colours::white.withAlpha (pressing ? 0.10f : 0.06f),
                                       0.0f, wheelRect.getY() + wheelRect.getHeight() * 0.30f,
                                       juce::Colours::transparentBlack, 0.0f, wheelRect.getBottom(), false);
            g.setGradientFill (lift);
            g.fillRect (wheelRect);
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
