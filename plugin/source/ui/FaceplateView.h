#pragma once

#include "Theme.h"
#include "WheelControl.h"

namespace trench::ui
{

class FaceplateView : public juce::Component
{
public:
    FaceplateView (juce::Image panel, const Theme& theme)
        : panelImage (std::move (panel)), t (theme)
    {
        if (const char* override = std::getenv ("TRENCH_PLATE_FILE"))
            panelImage = juce::ImageFileFormat::loadFrom (juce::File (juce::String::fromUTF8 (override)));
        const bool silver = t.themeParam ("plateSilver", 0.0) > 0.0;
        const bool dark = t.themeParam ("plateDark", 0.0) > 0.0;
        if (panelImage.isValid() && (silver || dark))
        {
            panelImage = panelImage.createCopy();
            juce::Image::BitmapData data (panelImage, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < data.height; ++y)
                for (int x = 0; x < data.width; ++x)
                {
                    const juce::Colour c = data.getPixelColour (x, y);
                    const float lum = c.getPerceivedBrightness();
                    const float lifted = silver ? juce::jlimit (0.0f, 1.0f, 0.30f + lum * 0.78f)
                                                : juce::jlimit (0.0f, 1.0f, 0.06f + lum * 0.30f);
                    data.setPixelColour (x, y, juce::Colour::fromFloatRGBA (lifted, lifted, lifted * 1.02f, c.getFloatAlpha()));
                }
        }
        if (panelImage.isValid())
        {
            shadowPlate = juce::Image (juce::Image::ARGB, panelImage.getWidth(), panelImage.getHeight(), true, juce::SoftwareImageType());
            {
                juce::Graphics sg (shadowPlate);
                sg.drawImageAt (panelImage, 0, 0);
            }
            juce::Image::BitmapData data (shadowPlate, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < data.height; ++y)
                for (int x = 0; x < data.width; ++x)
                {
                    const juce::Colour c = data.getPixelColour (x, y);
                    data.setPixelColour (x, y, juce::Colour::fromFloatRGBA (c.getFloatRed() * kShadowFloor, c.getFloatGreen() * kShadowFloor,
                                                                            c.getFloatBlue() * kShadowFloor, c.getFloatAlpha()));
                }
        }
        setOpaque (false);
        setBufferedToImage (true);
        setInterceptsMouseClicks (false, false);
    }

    void paint (juce::Graphics& g) override
    {

        if (t.themeParam ("plateDrawn", 0.0) > 0.0)
        {
            const auto b = getLocalBounds().toFloat();
            const auto plate = b.reduced (3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (plate.translated (0.0f, 2.0f).expanded (1.0f), 13.0f);
            juce::ColourGradient metal (juce::Colour (0xffe2dac8), plate.getX(), plate.getY(),
                                        juce::Colour (0xffbfb59f), plate.getX(), plate.getBottom(), false);
            metal.addColour (0.5, juce::Colour (0xffd3cab5));
            g.setGradientFill (metal);
            g.fillRoundedRectangle (plate, 12.0f);
            {
                juce::Graphics::ScopedSaveState save (g);
                juce::Path clip;
                clip.addRoundedRectangle (plate, 12.0f);
                g.reduceClipRegion (clip);
                juce::Random brush (7);
                for (float y = plate.getY(); y < plate.getBottom(); y += 1.0f)
                {
                    const float a = 0.02f + brush.nextFloat() * 0.06f;
                    g.setColour ((brush.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (a));
                    g.fillRect (plate.getX(), y, plate.getWidth(), 1.0f);
                }
                juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.18f), plate.getX(), plate.getY(),
                                            juce::Colours::transparentWhite, plate.getX(), plate.getY() + plate.getHeight() * 0.25f, false);
                g.setGradientFill (sheen);
                g.fillRect (plate.withHeight (plate.getHeight() * 0.25f));
            }
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawRoundedRectangle (plate.reduced (1.2f), 11.0f, 1.0f);
            g.setColour (juce::Colour (0xff5c4f3a).withAlpha (0.85f));
            g.drawRoundedRectangle (plate, 12.0f, 1.4f);
            g.setColour (juce::Colour (0xff8d8674));
            for (const auto p : { juce::Point<float> (14.0f, 14.0f), juce::Point<float> (b.getRight() - 14.0f, 14.0f),
                                  juce::Point<float> (14.0f, b.getBottom() - 14.0f), juce::Point<float> (b.getRight() - 14.0f, b.getBottom() - 14.0f) })
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillEllipse (p.x - 3.5f, p.y - 2.5f, 7.0f, 7.0f);
                g.setColour (juce::Colour (0xff9e9684));
                g.fillEllipse (p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f);
                g.setColour (juce::Colour (0xff3d3628));
                g.drawLine (p.x - 2.2f, p.y - 1.0f, p.x + 2.2f, p.y + 1.0f, 1.0f);
            }
        }
        else if (panelImage.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (panelImage, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        }

        drawPlateShadow (g, t.rect ("morphWell"));
        drawPlateShadow (g, t.rect ("qWell"));
        for (const char* id : { "morphReadout", "qReadout", "inputReadout", "outputReadout" })
            drawReadoutShadow (g, t.rect (id));
    }

    void drawShadowMask (juce::Graphics& g, juce::Rectangle<float> area,
                         const std::function<void (juce::Graphics&)>& paintMask) const
    {
        constexpr float k = 4.0f;
        juce::Image mask (juce::Image::SingleChannel, (int) std::ceil (area.getWidth() * k), (int) std::ceil (area.getHeight() * k),
                          true, juce::SoftwareImageType());
        {
            juce::Graphics mg (mask);
            mg.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (k));
            paintMask (mg);
        }
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (mask, juce::AffineTransform::scale (1.0f / k).translated (area.getX(), area.getY()));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (shadowPlate, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    }

    void drawPlateShadow (juce::Graphics& g, juce::Rectangle<float> well) const
    {
        if (well.isEmpty() || ! shadowPlate.isValid())
            return;
        drawShadowMask (g, well.expanded (4.0f, 16.0f), [well] (juce::Graphics& mg)
        {
            const auto ink = juce::Colours::white;
            const float cx = well.getCentreX();
            const float cy = well.getBottom();
            const float rx = well.getWidth() * 0.5f;
            const float ry = well.getHeight() * 0.40f;
            {
                juce::Graphics::ScopedSaveState lens (mg);
                mg.reduceClipRegion (juce::Rectangle<float> (well.getX() - 4.0f, cy, well.getWidth() + 8.0f, ry + 2.0f).getSmallestIntegerContainer());
                mg.addTransform (juce::AffineTransform::scale (1.0f, ry / rx, cx, cy));
                juce::ColourGradient sh (ink.withAlpha (0.78f), cx, cy, ink.withAlpha (0.0f), cx + rx, cy, true);
                sh.addColour (0.55, ink.withAlpha (0.62f));
                sh.addColour (0.82, ink.withAlpha (0.24f));
                mg.setGradientFill (sh);
                mg.fillEllipse (cx - rx, cy - rx, rx * 2.0f, rx * 2.0f);
            }
            juce::Path contact;
            contact.addRoundedRectangle (well.getX() + 1.0f, cy - 0.6f, well.getWidth() - 2.0f, 1.8f, 0.9f);
            mg.setColour (ink.withAlpha (0.95f));
            mg.fillPath (contact);
        });
    }

    void drawReadoutShadow (juce::Graphics& g, juce::Rectangle<float> box) const
    {
        if (box.isEmpty() || ! shadowPlate.isValid())
            return;
        drawShadowMask (g, box.expanded (6.0f), [box] (juce::Graphics& mg)
        {
            juce::Path outline;
            outline.addRoundedRectangle (box, 3.0f);
            juce::DropShadow (juce::Colours::white.withAlpha (0.85f), 3, { 0, 1 }).drawForPath (mg, outline);
        });
    }
private:

    static constexpr float kShadowFloor = 0.34f;
    juce::Image panelImage;
    juce::Image shadowPlate;
    Theme t;
};

}
