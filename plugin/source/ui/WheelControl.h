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
    static constexpr float kLampHue = 175.0f / 360.0f;
    static constexpr float kLampSat = 0.64f;
    static constexpr float kLampVal = 0.81f;
    static constexpr float kLampMinSat = 0.6f;

    static juce::Image tintLamp (const juce::Image& source, juce::Colour accent)
    {
        if (! source.isValid() || std::abs (accent.getHue() - kLampHue) < 0.002f
            && std::abs (accent.getSaturation() - kLampSat) < 0.01f && std::abs (accent.getBrightness() - kLampVal) < 0.01f)
            return source;
        auto out = source.createCopy();
        const float satScale = juce::jmax (accent.getSaturation(), kLampMinSat) / kLampSat;
        const float valScale = accent.getBrightness() / kLampVal;
        juce::Image::BitmapData data (out, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < data.height; ++y)
            for (int x = 0; x < data.width; ++x)
            {
                const auto c = data.getPixelColour (x, y);
                if (c.getAlpha() == 0)
                    continue;
                float h, sat, val;
                c.getHSB (h, sat, val);
                if (sat <= 0.15f || h < 140.0f / 360.0f || h > 205.0f / 360.0f)
                    continue;
                data.setPixelColour (x, y, juce::Colour::fromHSV (accent.getHue(), juce::jmin (1.0f, sat * satScale),
                                                                  juce::jmin (1.0f, val * valScale), c.getFloatAlpha()));
            }
        return out;
    }

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

    static constexpr float kWellGapX = 3.0f, kWellOverlap = 1.5f, kWellCorner = 3.5f;
    static constexpr float kRibTravel = 190.5f / 417.0f;
    static constexpr float kSilX0 = 7.0f, kSilX1 = 410.0f, kSilY0 = 8.0f, kSilY1 = 87.0f;
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
    static juce::Rectangle<float> silhouetteForHole (juce::Rectangle<float> hole)
    {
        return hole.reduced (kWellGapX, 0.0f).expanded (0.0f, kWellOverlap);
    }

    void paint (juce::Graphics& g) override
    {
        if (! strip.isValid())
            return;

        const int fw = strip.getWidth() / numFrames;
        const int fh = strip.getHeight();
        if (fw <= 0 || fh <= 0)
            return;

        const int last = (numFrames == 129 ? 128 : numFrames) - 1;
        const int frame = juce::jlimit (0, last, juce::roundToInt (displayNormalised() * (float) last));

        const auto wheelRect = getLocalBounds().toFloat();
        const auto hole = wheelRect.reduced (kSideOverhang, 0.0f).withTrimmedTop (kDrumProud).withTrimmedBottom (kDrumBelow);
        const auto sil = silhouetteForHole (hole);
        const float sx = sil.getWidth() / (kSilX1 - kSilX0), sy = sil.getHeight() / (kSilY1 - kSilY0);
        const juce::Rectangle<float> frameRect { sil.getX() - kSilX0 * sx, sil.getY() - kSilY0 * sy,
                                                 (float) fw * sx, (float) fh * sy };
        shownFrameWidth = frameRect.getWidth();
        juce::Graphics::ScopedSaveState well (g);
        {
            juce::Path opening;
            opening.addRoundedRectangle (hole, kWellCorner);
            g.reduceClipRegion (opening);
        }

        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

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
    float shownFrameWidth = 0.0f;
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

        const float throwPx = juce::jmax (1.0f, kRibTravel * (shownFrameWidth > 0.0f ? shownFrameWidth : (float) getWidth()));
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
