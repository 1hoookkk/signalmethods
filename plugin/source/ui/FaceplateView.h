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

        setOpaque (false);
        setBufferedToImage (true);
        setInterceptsMouseClicks (false, false);
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

        if (panelImage.isValid())
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
};

}
