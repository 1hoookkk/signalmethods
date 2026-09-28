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

        drawWheelContactShadow (g, t.rect ("morphWell").withTrimmedBottom (-1.35f).translated (1.0f, 0.0f));
        drawWheelContactShadow (g, t.rect ("qWell").withTrimmedBottom (-1.35f).translated (1.0f, 0.0f));
        if (! roomFrame.isEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.62f));
            g.strokePath (roomFramePath (roomFrame.translated (0.8f, 0.8f), 5.5f, roomGapX0, roomGapX1), juce::PathStrokeType (1.0f));
            g.setColour (juce::Colour (0xff453424).withAlpha (0.55f));
            g.strokePath (roomFramePath (roomFrame, 5.5f, roomGapX0, roomGapX1), juce::PathStrokeType (1.0f));
            if (roomCaption.isNotEmpty())
            {
                g.setFont (displayFont (10.5f, true));
                const auto box = juce::Rectangle<float> (roomGapX0, roomFrame.getY() - 7.0f, roomGapX1 - roomGapX0, 14.0f).toNearestInt();
                drawEngravedText (g, roomCaption, box, juce::Justification::centred, t.labelInk(), 0.45f);
            }
        }
    }

    void setRoomCaption (const juce::String& text) { roomCaption = text; repaint(); }
    void setRoomFrame (juce::Rectangle<float> frame, float gapX0, float gapX1)
    {
        if (frame != roomFrame || gapX0 != roomGapX0 || gapX1 != roomGapX1)
        {
            roomFrame = frame;
            roomGapX0 = gapX0;
            roomGapX1 = gapX1;
            repaint();
        }
    }

    static juce::Path roomFramePath (juce::Rectangle<float> r, float rad, float gapX0, float gapX1)
    {
        constexpr float q = juce::MathConstants<float>::halfPi;
        juce::Path p;
        p.startNewSubPath (juce::jlimit (r.getX() + rad, r.getRight() - rad, gapX1), r.getY());
        p.lineTo (r.getRight() - rad, r.getY());
        p.addCentredArc (r.getRight() - rad, r.getY() + rad, rad, rad, 0.0f, 0.0f, q);
        p.lineTo (r.getRight(), r.getBottom() - rad);
        p.addCentredArc (r.getRight() - rad, r.getBottom() - rad, rad, rad, 0.0f, q, 2.0f * q);
        p.lineTo (r.getX() + rad, r.getBottom());
        p.addCentredArc (r.getX() + rad, r.getBottom() - rad, rad, rad, 0.0f, 2.0f * q, 3.0f * q);
        p.lineTo (r.getX(), r.getY() + rad);
        p.addCentredArc (r.getX() + rad, r.getY() + rad, rad, rad, 0.0f, 3.0f * q, 4.0f * q);
        p.lineTo (juce::jlimit (r.getX() + rad, r.getRight() - rad, gapX0), r.getY());
        return p;
    }

    static void drawWheelContactShadow (juce::Graphics& g, juce::Rectangle<float> well)
    {
        if (well.isEmpty())
            return;
        const float castH = 10.0f;
        const float cx = well.getCentreX();
        const float overlap = 1.5f;
        const float cy = well.getBottom() - overlap;
        const float rx = well.getWidth() * 0.48f;
        {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (juce::Rectangle<int> ((int) well.getX(), (int) std::floor (cy),
                                                  (int) well.getWidth(), (int) (castH + overlap)));
        g.addTransform (juce::AffineTransform::scale (1.0f, (castH + overlap) / rx, cx, cy));
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.88f), cx, cy,
                                 juce::Colours::transparentBlack, cx + rx, cy, true);
        sh.addColour (0.50, juce::Colours::black.withAlpha (0.64f));
        sh.addColour (0.82, juce::Colours::black.withAlpha (0.26f));
        g.setGradientFill (sh);
        g.fillEllipse (cx - rx, cy - rx, rx * 2.0f, rx * 2.0f);
        }
        {
            juce::Graphics::ScopedSaveState lipState (g);
            const float lipRx = well.getWidth() * 0.58f, lipRy = 6.0f;
            g.reduceClipRegion (juce::Rectangle<int> ((int) (cx - lipRx), (int) std::floor (cy), (int) (2.0f * lipRx), (int) lipRy + 1));
            g.addTransform (juce::AffineTransform::scale (1.0f, lipRy / lipRx, cx, cy));
            juce::ColourGradient lip (juce::Colours::black.withAlpha (0.75f), cx, cy, juce::Colours::transparentBlack, cx + lipRx, cy, true);
            lip.addColour (0.88, juce::Colours::black.withAlpha (0.62f));
            g.setGradientFill (lip);
            g.fillEllipse (cx - lipRx, cy - lipRx, lipRx * 2.0f, lipRx * 2.0f);
        }
    }
private:

    juce::Image panelImage;
    juce::Rectangle<float> roomFrame;
    float roomGapX0 = 0.0f, roomGapX1 = 0.0f;
    juce::String roomCaption;
    Theme t;
};

}
