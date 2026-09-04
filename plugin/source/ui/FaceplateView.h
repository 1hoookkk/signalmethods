#pragma once

#include "Theme.h"

namespace trench::ui
{

class FaceplateView : public juce::Component
{
public:
    FaceplateView (juce::Image panel, const Theme& theme)
        : panelImage (std::move (panel)), t (theme)
    {
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

    void setDeck (const juce::String& mode)
    {
        if (mode != deck)
        {
            deck = mode;
            repaint();
        }
    }

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

        if (! roomFrame.isEmpty())
        {
            g.setColour (juce::Colour (0xffa6967d).withAlpha (0.59f));
            g.strokePath (roomFramePath (roomFrame.reduced (0.8f), 4.7f, roomGapX0, roomGapX1),
                          juce::PathStrokeType (1.2f));
            g.setColour (juce::Colour (0xff453424).withAlpha (0.67f));
            g.strokePath (roomFramePath (roomFrame, 5.5f, roomGapX0, roomGapX1),
                          juce::PathStrokeType (1.2f));
        }

        drawWheelContactShadow (g, t.rect ("morphWheel"));
        drawWheelContactShadow (g, t.rect ("qWheel"));
        drawDeck (g);
    }

    void drawDeck (juce::Graphics& g)
    {
        const juce::Colour ink (0xff2a2722);
        const juce::Colour bone (0xffe7dec9);
        const juce::Colour dark (0xff1c1a17);
        const auto tiny = juce::Font (juce::FontOptions ("Tahoma", 7.5f, juce::Font::bold));
        const auto etched = [&] (const juce::String& text, juce::Rectangle<int> box, juce::Justification just)
        {
            g.setFont (tiny);
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.drawText (text, box.translated (0, 1), just, false);
            g.setColour (ink);
            g.drawText (text, box, just, false);
        };
        if (deck == "rig")
        {
            const juce::Rectangle<float> rocker (256.0f, 402.0f, 28.0f, 58.0f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (rocker.translated (0.0f, 1.5f).expanded (1.5f), 5.0f);
            g.setColour (dark);
            g.fillRoundedRectangle (rocker, 4.5f);
            juce::ColourGradient cap (juce::Colour (0xff4a4744), rocker.getX(), rocker.getY(),
                                      juce::Colour (0xff232120), rocker.getX(), rocker.getCentreY(), false);
            g.setGradientFill (cap);
            g.fillRoundedRectangle (rocker.withHeight (rocker.getHeight() * 0.5f).reduced (2.0f, 2.0f), 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillRect (rocker.getX() + 4.0f, rocker.getY() + 3.0f, rocker.getWidth() - 8.0f, 1.0f);
            g.setColour (t.accent());
            g.fillEllipse (rocker.getCentreX() - 2.5f, rocker.getBottom() - 12.0f, 5.0f, 5.0f);
            etched ("ENGAGE", juce::Rectangle<int> (244, 462, 52, 12), juce::Justification::centred);
            g.setFont (juce::Font (juce::FontOptions ("Arial", 9.0f, juce::Font::bold)).withExtraKerningFactor (0.18f));
            g.setColour (bone.withAlpha (0.55f));
            g.drawText ("MUSICAL FILTER", juce::Rectangle<int> (30, 439, 200, 14), juce::Justification::centredLeft, false);
            g.setColour (ink.withAlpha (0.55f));
            g.drawText ("MUSICAL FILTER", juce::Rectangle<int> (30, 438, 200, 14), juce::Justification::centredLeft, false);
            g.setColour (ink.withAlpha (0.35f));
            g.drawText ("SIGNAL METHODS", juce::Rectangle<int> (30, 452, 200, 12), juce::Justification::centredLeft, false);
        }
        else if (deck == "bay")
        {
            const juce::Rectangle<float> bay (22.0f, 372.0f, 216.0f, 92.0f);
            juce::ColourGradient inset (juce::Colour (0xff3a3632), bay.getX(), bay.getY(),
                                        juce::Colour (0xff26231f), bay.getX(), bay.getBottom(), false);
            g.setGradientFill (inset);
            g.fillRoundedRectangle (bay, 5.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (bay, 5.0f, 1.0f);
            g.setColour (bone.withAlpha (0.25f));
            g.drawLine (bay.getX() + 4.0f, bay.getBottom() - 1.0f, bay.getRight() - 4.0f, bay.getBottom() - 1.0f, 1.0f);
            for (int i = 0; i < 4; ++i)
            {
                const float x = bay.getX() + 128.0f + (float) i * 13.0f;
                const juce::Rectangle<float> slot (x, bay.getY() + 12.0f, 9.0f, 18.0f);
                g.setColour (juce::Colour (0xff0f0e0d));
                g.fillRoundedRectangle (slot, 1.5f);
                const bool up = (i % 3) != 1;
                g.setColour (juce::Colour (0xffd9d2c4));
                g.fillRoundedRectangle (slot.withHeight (8.0f).translated (0.0f, up ? 1.0f : 9.0f).reduced (1.5f, 0.0f), 1.0f);
            }
            g.setFont (tiny);
            g.setColour (bone.withAlpha (0.7f));
            g.drawText ("CASCADE", juce::Rectangle<int> ((int) bay.getX() + 128, (int) bay.getY() + 32, 52, 10), juce::Justification::centredLeft, false);
            for (int i = 0; i < 4; ++i)
            {
                const float x = bay.getX() + 128.0f + (float) i * 13.0f + 2.5f;
                const bool lit = i < 2;
                g.setColour (lit ? t.accent() : juce::Colour (0xff2b2926));
                g.fillEllipse (x, bay.getY() + 52.0f, 5.0f, 5.0f);
                if (lit)
                {
                    g.setColour (t.accent().withAlpha (0.25f));
                    g.fillEllipse (x - 2.0f, bay.getY() + 50.0f, 9.0f, 9.0f);
                }
            }
            g.setColour (bone.withAlpha (0.7f));
            g.drawText ("SECTION", juce::Rectangle<int> ((int) bay.getX() + 128, (int) bay.getY() + 60, 52, 10), juce::Justification::centredLeft, false);
            g.drawText ("DRIVE ENGINE  ·  REV 2", juce::Rectangle<int> ((int) bay.getX() + 8, (int) bay.getBottom() - 14, 140, 10), juce::Justification::centredLeft, false);
            for (int i = 0; i < 4; ++i)
            {
                const float cy = 404.0f + (float) i * 16.0f;
                g.setColour (juce::Colour (0xff141210));
                g.fillEllipse (263.0f, cy, 12.0f, 12.0f);
                g.setColour (juce::Colour (0xff8a8377));
                g.drawEllipse (263.0f, cy, 12.0f, 12.0f, 1.0f);
                g.setColour (juce::Colour (0xff3a3632));
                g.fillEllipse (266.5f, cy + 3.5f, 5.0f, 5.0f);
            }
            etched ("PATCH", juce::Rectangle<int> (250, 392, 40, 10), juce::Justification::centred);
        }
        else if (deck == "bench")
        {
            const auto steps = [&] (juce::Point<float> c, float r)
            {
                static const char* names[] = { "-12", "-6", "0", "+6", "+12", "CRUSH" };
                for (int i = 0; i < 6; ++i)
                {
                    const float a = juce::degreesToRadians (-135.0f + 54.0f * (float) i);
                    const juce::Point<float> p0 (c.x + std::sin (a) * (r + 2.0f), c.y - std::cos (a) * (r + 2.0f));
                    const juce::Point<float> p1 (c.x + std::sin (a) * (r + 6.0f), c.y - std::cos (a) * (r + 6.0f));
                    g.setColour (ink);
                    g.drawLine (p0.x, p0.y, p1.x, p1.y, 1.2f);
                    const juce::Point<float> pl (c.x + std::sin (a) * (r + 13.0f), c.y - std::cos (a) * (r + 13.0f));
                    g.setFont (juce::Font (juce::FontOptions ("Tahoma", 6.5f, juce::Font::bold)));
                    g.setColour (ink);
                    g.drawText (names[i], juce::Rectangle<int> ((int) pl.x - 12, (int) pl.y - 5, 24, 10), juce::Justification::centred, false);
                }
            };
            steps ({ 74.0f, 411.0f }, 15.0f);
            steps ({ 74.0f, 453.0f }, 15.0f);
            const auto toggle = [&] (float x, float y, bool up, const juce::String& name)
            {
                g.setColour (juce::Colour (0xff2b2926));
                g.fillEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f);
                g.setColour (juce::Colour (0xff0f0e0d));
                g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
                const float ty = up ? y - 14.0f : y + 14.0f;
                g.setColour (juce::Colour (0xffb9b2a6));
                g.drawLine (x, y, x, ty, 3.0f);
                g.setColour (juce::Colour (0xffe6e0d4));
                g.fillEllipse (x - 3.0f, ty - 3.0f, 6.0f, 6.0f);
                etched (name, juce::Rectangle<int> ((int) x - 24, (int) y + 18, 48, 10), juce::Justification::centred);
            };
            toggle (196.0f, 418.0f, true, "IMPULSE");
            toggle (226.0f, 418.0f, false, "PHASE");
            const juce::Point<float> bnc (270.0f, 428.0f);
            g.setColour (juce::Colour (0xff8a8377));
            g.fillEllipse (bnc.x - 13.0f, bnc.y - 13.0f, 26.0f, 26.0f);
            g.setColour (juce::Colour (0xffcfc8ba));
            g.fillEllipse (bnc.x - 10.5f, bnc.y - 10.5f, 21.0f, 21.0f);
            g.setColour (juce::Colour (0xff141210));
            g.fillEllipse (bnc.x - 6.5f, bnc.y - 6.5f, 13.0f, 13.0f);
            g.setColour (juce::Colour (0xffcfc8ba));
            g.fillEllipse (bnc.x - 1.5f, bnc.y - 1.5f, 3.0f, 3.0f);
            g.setColour (juce::Colour (0xff8a8377));
            g.fillRect (bnc.x - 13.0f, bnc.y - 2.0f, 4.0f, 4.0f);
            g.fillRect (bnc.x + 9.0f, bnc.y - 2.0f, 4.0f, 4.0f);
            etched ("CAL", juce::Rectangle<int> (250, 446, 40, 10), juce::Justification::centred);
        }
    }

    static void drawWheelContactShadow (juce::Graphics& g, juce::Rectangle<float> well)
    {
        if (well.isEmpty())
            return;

        {

            const auto opening_r = well.reduced (0.8f, 2.7f);
            juce::Graphics::ScopedSaveState save (g);
            juce::Path opening;
            opening.addRoundedRectangle (opening_r, opening_r.getHeight() * 0.34f);
            g.reduceClipRegion (opening);
            const float sideD = 7.0f;
            juce::ColourGradient left (juce::Colours::black.withAlpha (0.50f),
                                       opening_r.getX(), opening_r.getCentreY(),
                                       juce::Colours::transparentBlack,
                                       opening_r.getX() + sideD, opening_r.getCentreY(), false);
            g.setGradientFill (left);
            g.fillRect (opening_r.withWidth (sideD));
            juce::ColourGradient right (juce::Colours::black.withAlpha (0.50f),
                                        opening_r.getRight(), opening_r.getCentreY(),
                                        juce::Colours::transparentBlack,
                                        opening_r.getRight() - sideD, opening_r.getCentreY(), false);
            g.setGradientFill (right);
            g.fillRect (opening_r.withLeft (opening_r.getRight() - sideD));
        }
        const float castH = 7.0f;
        const float cx = well.getCentreX();
        const float overlap = 1.5f;
        const float cy = well.getBottom() - overlap;
        const float rx = well.getWidth() * 0.48f;
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (juce::Rectangle<int> ((int) well.getX(), (int) std::floor (cy),
                                                  (int) well.getWidth(), (int) (castH + overlap)));
        g.addTransform (juce::AffineTransform::scale (1.0f, (castH + overlap) / rx, cx, cy));
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.74f), cx, cy,
                                 juce::Colours::transparentBlack, cx + rx, cy, true);
        sh.addColour (0.50, juce::Colours::black.withAlpha (0.50f));
        sh.addColour (0.82, juce::Colours::black.withAlpha (0.18f));
        g.setGradientFill (sh);
        g.fillEllipse (cx - rx, cy - rx, rx * 2.0f, rx * 2.0f);
    }
private:

    static juce::Path roomFramePath (juce::Rectangle<float> r, float rad,
                                     float gapX0, float gapX1)
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

    juce::Image panelImage;
    Theme t;
    juce::Rectangle<float> roomFrame;
    float roomGapX0 = 0.0f, roomGapX1 = 0.0f;
    juce::String deck;
};

}
