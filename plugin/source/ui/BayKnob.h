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
    bool boxOnly = false;
    bool deckStyle = false;

    static juce::Drawable* svgAsset (const char* data, int size, std::unique_ptr<juce::Drawable>& slot)
    {
        if (slot == nullptr)
            slot = juce::Drawable::createFromImageData (data, size);
        return slot.get();
    }

    void paintDeckSvg (juce::Graphics& g)
    {
        static std::unique_ptr<juce::Drawable> body, bodyCream, pointer, pointerInk, ticks;
        const bool cream = t.themeParam ("knobStyle", 1.0) >= 2.0;
        auto* bodyDrawable = cream ? svgAsset (BinaryData::knob_body_cream_svg, BinaryData::knob_body_cream_svgSize, bodyCream)
                                   : svgAsset (BinaryData::knob_body_svg, BinaryData::knob_body_svgSize, body);
        auto* pointerDrawable = cream ? svgAsset (BinaryData::knob_pointer_svg, BinaryData::knob_pointer_svgSize, pointerInk)
                                      : svgAsset (BinaryData::knob_pointer_svg, BinaryData::knob_pointer_svgSize, pointer);
        auto* tickDrawable = svgAsset (BinaryData::tick_ring_svg, BinaryData::tick_ring_svgSize, ticks);
        if (bodyDrawable == nullptr || pointerDrawable == nullptr || tickDrawable == nullptr)
            return;
        const auto b = getLocalBounds().toFloat();
        const float value = currentNormalised();
        const float ringD = juce::jmin (b.getWidth(), b.getHeight() - (cream ? 34.0f : 20.0f)) * 0.82f;
        const float knobD = ringD * 0.72f;
        const juce::Point<float> c (b.getCentreX(), b.getY() + ringD * 0.5f + 6.0f);
        const juce::Colour ink = t.labelInk();
        pointerDrawable->replaceColour (juce::Colour (0xff3cc8be), cream ? ink : t.accent());
        tickDrawable->replaceColour (juce::Colour (0xffe7dec9), ink.withAlpha (0.9f));
        if (cream)
        {
            const auto place = [&] (juce::Drawable* d, float diameter, float rotation)
            {
                const float s = diameter / 200.0f;
                const auto xf = juce::AffineTransform::scale (s)
                                    .translated (c.x - diameter * 0.5f, c.y - diameter * 0.5f)
                                    .rotated (rotation, c.x, c.y);
                d->draw (g, 1.0f, xf);
            };
            place (tickDrawable, ringD, 0.0f);
            place (bodyDrawable, knobD, 0.0f);
            place (pointerDrawable, knobD, juce::degreesToRadians (-135.0f + 270.0f * value));
            drawBayCaption (g, juce::Rectangle<float> (b.getX(), c.y + ringD * 0.5f + 3.0f, b.getWidth(), 12.5f), label, t);
            const auto box = juce::Rectangle<float> ((float) kBayValueWidth, (float) kBayValueHeight)
                                 .withCentre ({ b.getCentreX(), c.y + ringD * 0.5f + 25.0f });
            drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
            drawCrispText (g, box.reduced (4.0f, 1.0f), formatValue != nullptr ? formatValue (value) : juce::String (juce::roundToInt (value * 100.0f)),
                           kBayValuePt, t.textColour ("morphReadout", juce::Colour (0xff2a2722)));
            return;
        }
        const auto place = [&] (juce::Drawable* d, float diameter, float rotation)
        {
            const float s = diameter / 200.0f;
            const auto xf = juce::AffineTransform::scale (s)
                                .translated (c.x - diameter * 0.5f, c.y - diameter * 0.5f)
                                .rotated (rotation, c.x, c.y);
            d->draw (g, 1.0f, xf);
        };
        place (tickDrawable, ringD, 0.0f);
        place (bodyDrawable, knobD, 0.0f);
        place (pointerDrawable, knobD, juce::degreesToRadians (-135.0f + 270.0f * value));
        g.setFont (juce::Font (juce::FontOptions ("Tahoma", 9.0f, juce::Font::bold)).withExtraKerningFactor (0.06f));
        g.setColour (ink);
        g.drawText (label.toUpperCase(), juce::Rectangle<int> ((int) b.getX(), (int) (c.y + ringD * 0.5f + 5.0f), (int) b.getWidth(), 12), juce::Justification::centred, false);
        g.setFont (juce::Font (juce::FontOptions ("Tahoma", 8.0f, juce::Font::plain)));
        g.setColour (ink.withAlpha (hover ? 0.9f : 0.55f));
        g.drawText (formatValue != nullptr ? formatValue (value) : juce::String (juce::roundToInt (value * 100.0f)),
                    juce::Rectangle<int> ((int) b.getX(), (int) (c.y + ringD * 0.5f + 17.0f), (int) b.getWidth(), 11), juce::Justification::centred, false);
    }

    void paintDeck (juce::Graphics& g)
    {
        if (t.themeParam ("knobSvg", 0.0) > 0.0)
        {
            paintDeckSvg (g);
            return;
        }
        const auto b = getLocalBounds().toFloat();
        const float value = currentNormalised();
        const float d = kBayKnobDiameter;
        const juce::Point<float> c (b.getCentreX(), b.getY() + d * 0.5f + 12.0f);
        const float ring = d * 0.5f + 2.0f;
        drawCap (g, c, value);
        drawBayCaption (g, juce::Rectangle<float> (b.getX(), c.y + ring + 6.0f, b.getWidth(), 12.5f), label, t);
        const auto box = juce::Rectangle<float> ((float) kBayValueWidth, (float) kBayValueHeight)
                             .withCentre ({ b.getCentreX(), c.y + ring + 28.0f });
        drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
        drawCrispText (g, box.reduced (4.0f, 1.0f), formatValue != nullptr ? formatValue (value) : juce::String (juce::roundToInt (value * 100.0f)),
                       kBayValuePt, t.textColour ("morphReadout", juce::Colour (0xff2a2722)));
    }

    void paint (juce::Graphics& g) override
    {
        if (deckStyle)
        {
            paintDeck (g);
            return;
        }
        const auto b = getLocalBounds().toFloat();
        const float value = currentNormalised();
        if (boxOnly)
        {
            const auto box = juce::Rectangle<float> ((float) kBayValueWidth, (float) kBayValueHeight).withCentre (b.getCentre());
            drawBayCaption (g, juce::Rectangle<float> (box.getX() - 10.0f, box.getY() - 12.5f, box.getWidth() + 20.0f, 12.5f), label, t);
            drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
            drawCrispText (g, box.reduced (4.0f, 1.0f), formatValue != nullptr ? formatValue (value) : juce::String (juce::roundToInt (value * 100.0f)),
                           kBayValuePt, t.textColour ("morphReadout", juce::Colour (0xff2a2722)));
            return;
        }
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
        {
            strip = juce::ImageCache::getFromMemory (BinaryData::trench_knob_strip_png,
                                                     BinaryData::trench_knob_strip_pngSize);
            if (strip.isValid() && t.themeParam ("knobSilver", 0.0) > 0.0)
            {
                strip = strip.createCopy();
                juce::Image::BitmapData data (strip, juce::Image::BitmapData::readWrite);
                for (int y = 0; y < data.height; ++y)
                    for (int x = 0; x < data.width; ++x)
                    {
                        const juce::Colour c = data.getPixelColour (x, y);
                        const float lum = c.getPerceivedBrightness();
                        const float lifted = juce::jlimit (0.0f, 1.0f, 0.22f + lum * 1.35f);
                        data.setPixelColour (x, y, juce::Colour::fromFloatRGBA (lifted, lifted, lifted, c.getFloatAlpha()));
                    }
            }
        }
        if (t.themeParam ("knobDots", 0.0) > 0.0)
        {
            const float ring = kBayKnobDiameter * 0.68f;
            for (int i = 0; i < 11; ++i)
            {
                const float a = juce::degreesToRadians (-135.0f + 27.0f * (float) i);
                const float px = c.x + std::sin (a) * ring;
                const float py = c.y - std::cos (a) * ring;
                const bool active = (float) i / 10.0f <= value + 0.001f;
                g.setColour (t.accent().withAlpha (active ? 1.0f : 0.35f));
                g.fillEllipse (px - 1.4f, py - 1.4f, 2.8f, 2.8f);
            }
        }
        constexpr int frameSize = 96, frameCount = 61;
        const int frame = juce::jlimit (0, frameCount - 1,
                                        juce::roundToInt ((1.0f - value) * (float) (frameCount - 1)));
        const float d = kBayKnobDiameter;
        const float frameD = d * (96.0f / 76.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (strip,
                     (int) (c.x - frameD * 0.5f), (int) (c.y - frameD * 0.5f), (int) frameD, (int) frameD,
                     frame * frameSize, 0, frameSize, frameSize, false);
        const float rim = d * 0.47f;
        const float a0 = juce::degreesToRadians (215.0f);
        const float a1 = juce::degreesToRadians (245.0f);
        juce::Path glint;
        glint.addCentredArc (c.x, c.y, rim, rim, 0.0f, a0, a1, true);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.strokePath (glint, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (glint, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
