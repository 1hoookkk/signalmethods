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
            setHelpText (name + " - click or drag to position; Shift-drag for fine adjustment; double-click to reset");
            setTooltip (name + ": click/drag, Shift fine, double-click reset");
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
            setHelpText (name + " - click or drag to position; Shift-drag for fine adjustment; double-click to reset");
            setTooltip (name + ": click/drag, Shift fine, double-click reset");
            attachment->sendInitialUpdate();
        }
        repaint();
    }

    void setOpening (juce::Rectangle<float> openingInParent)
    {
        setBounds (drumForHole (openingInParent).getSmallestIntegerContainer());
        opening = openingInParent - getPosition().toFloat();
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

    float shownNormalised() const noexcept { return displayNormalised(); }
    void setEchoLit (bool lit)
    {
        if (echoLit != lit) { echoLit = lit; repaint(); }
    }
    void mouseEnter (const juce::MouseEvent&) override { hovering = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovering = false; repaint(); }

    std::function<void()> onGestureStart;
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
        if (onGestureStart != nullptr)
            onGestureStart();

        dragStartX   = e.position.x;
        valueAtStart = currentNormalised();
        if (! e.mods.isShiftDown())
            dragToPosition (e);
        valueAtStart = currentNormalised();

        if (altRecording)
        {
            if (onAltDragStart != nullptr) onAltDragStart();
            onAltDragSample (currentNormalised());
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! pressing || e.mods.isPopupMenu())
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
            dragToPosition (e);
        }
        dragStartX = e.position.x;
        valueAtStart = currentNormalised();
        if (altRecording && onAltDragSample != nullptr)
            onAltDragSample (currentNormalised());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! pressing) return;
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
        {
            if (! pressing && onGestureStart != nullptr) onGestureStart();
            if (gestureOpen) attachment->setValueAsPartOfGesture (defaultDenorm);
            else attachment->setValueAsCompleteGesture (defaultDenorm);
            if (! pressing && onGestureEnd != nullptr) onGestureEnd();
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        juce::ignoreUnused (e);
        if (attachment == nullptr || param == nullptr)
            return;

        const float next = juce::jlimit (0.0f, 1.0f, currentNormalised() + wheel.deltaY * 0.08f);
        if (! pressing && onGestureStart != nullptr) onGestureStart();
        if (gestureOpen) attachment->setValueAsPartOfGesture (param->convertFrom0to1 (next));
        else attachment->setValueAsCompleteGesture (param->convertFrom0to1 (next));
        if (! pressing && onGestureEnd != nullptr) onGestureEnd();
        repaint();
    }

    static constexpr float kWellGapX = -3.0f, kHeightFrom = -6.0f, kOpeningCorner = 3.5f;
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
        const float w = hole.getWidth() - 2.0f * kWellGapX;
        const float h = (hole.getWidth() - 2.0f * kHeightFrom) * (87.0f - 8.0f) / (410.0f - 7.0f);
        return juce::Rectangle<float> (w, h).withCentre (hole.getCentre());
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
        auto frameRect = juce::Rectangle<float> { sil.getX() - kSilX0 * sx + 3.4f, sil.getY() - kSilY0 * sy + 2.0f,
                                                 (float) fw * sx, (float) fh * sy }.expanded (1.0f);
        if (! opening.isEmpty())
            frameRect.setY (opening.getY() - kSilY0 * sy);
        shownFrameWidth = frameRect.getWidth();
        juce::Graphics::ScopedSaveState behind (g);
        {
            juce::Path clip;
            clip.addRoundedRectangle (opening.isEmpty() ? hole : opening, kOpeningCorner);
            g.reduceClipRegion (clip);
        }

        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

        const float pixelScale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const auto snap = [pixelScale] (float v) { return std::round (v * pixelScale) / pixelScale; };
        const juce::Rectangle<float> dest { snap (frameRect.getX()), snap (frameRect.getY()),
                                            snap (frameRect.getRight()) - snap (frameRect.getX()),
                                            snap (frameRect.getBottom()) - snap (frameRect.getY()) };
        if (scaledIndex != frame || scaledEcho != echoLit)
        {
            scaledFrame = strip.getClippedImage ({ frame * fw, 0, fw, fh });
            if (echoLit)
            {
                scaledFrame = scaledFrame.createCopy();
                const float lampHue = t.rollerIllumination().getHue();
                juce::Image::BitmapData data (scaledFrame, juce::Image::BitmapData::readWrite);
                for (int y = 0; y < data.height; ++y)
                    for (int x = 0; x < data.width; ++x)
                    {
                        const auto c = data.getPixelColour (x, y);
                        if (c.getSaturation() > 0.45f && std::abs (c.getHue() - lampHue) < 0.06f)
                            data.setPixelColour (x, y, juce::Colour::fromHSV (c.getHue(), c.getSaturation() * 0.8f,
                                                                              juce::jmin (1.0f, c.getBrightness() * 1.6f), c.getFloatAlpha()));
                    }
            }
            scaledIndex = frame;
            scaledEcho = echoLit;
        }
        g.drawImage (scaledFrame, dest, juce::RectanglePlacement::stretchToFit);

        {
            const auto wheel = frameRect;
            const auto shade = juce::Colour (0xff17110a);
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (scaledFrame,
                                juce::AffineTransform::scale (dest.getWidth()  / (float) fw,
                                                              dest.getHeight() / (float) fh)
                                    .translated (dest.getX(), dest.getY()));
            const float endW = wheel.getWidth() * 0.24f;
            juce::ColourGradient left (shade.withAlpha (0.97f), wheel.getX(), wheel.getCentreY(),
                                       shade.withAlpha (0.0f), wheel.getX() + endW, wheel.getCentreY(), false);
            left.addColour (0.30, shade.withAlpha (0.82f));
            left.addColour (0.65, shade.withAlpha (0.40f));
            g.setGradientFill (left);
            g.fillRect (wheel.withWidth (endW));
            juce::ColourGradient right (shade.withAlpha (0.97f), wheel.getRight(), wheel.getCentreY(),
                                        shade.withAlpha (0.0f), wheel.getRight() - endW, wheel.getCentreY(), false);
            right.addColour (0.30, shade.withAlpha (0.82f));
            right.addColour (0.65, shade.withAlpha (0.40f));
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
    juce::Rectangle<float> opening;
    juce::Image scaledFrame;
    float shownFrameWidth = 0.0f;
    int scaledIndex = -1;
    bool scaledEcho = false;

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

    void dragToPosition (const juce::MouseEvent& e)
    {
        if (attachment == nullptr || param == nullptr)
            return;

        const float travel = juce::jmax (1.0f, (float) getWidth() - 2.0f * kSideOverhang);
        const float next = juce::jlimit (0.0f, 1.0f, (e.position.x - kSideOverhang) / travel);
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
    bool echoLit = false;
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
