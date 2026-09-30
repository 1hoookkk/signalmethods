#pragma once

#include "BinaryData.h"
#include <juce_graphics/juce_graphics.h>

namespace trench::ui
{
class ControlArtwork
{
public:
    ControlArtwork()
    {
        const auto atlas = juce::ImageCache::getFromMemory (BinaryData::trench_control_art_png,
                                                           BinaryData::trench_control_art_pngSize);
        readout = atlas.getClippedImage ({ 0, 30, 37, 14 });
        wheelShadow = atlas.getClippedImage ({ 0, 22, 96, 7 }).createCopy();
        juce::Image::BitmapData pixels (wheelShadow, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < pixels.height; ++y)
            for (int x = 0; x < pixels.width; ++x)
                if (pixels.getPixelColour (x, y).getRed() != 0)
                    pixels.setPixelColour (x, y, juce::Colours::transparentBlack);
    }

    void drawReadout (juce::Graphics& g, juce::Rectangle<float> bounds) const
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (readout, bounds, juce::RectanglePlacement::stretchToFit);
    }

    void drawReadoutShadow (juce::Graphics& g, juce::Rectangle<float> bounds) const
    {
        juce::Path outline;
        outline.addRoundedRectangle (bounds.reduced (1.4f).withTrimmedTop (1.0f), 2.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.16f), 2, { 0, 1 }).drawForPath (g, outline);
    }

    void drawWheelShadow (juce::Graphics& g, juce::Rectangle<float> wheel) const
    {
        const float sx = wheel.getWidth() / 85.0f;
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (wheelShadow,
                     { wheel.getX() - 7.0f * sx, wheel.getBottom() - 1.0f, 96.0f * sx, 7.0f * sx },
                     juce::RectanglePlacement::stretchToFit);
    }

private:
    juce::Image readout, wheelShadow;
};
}
