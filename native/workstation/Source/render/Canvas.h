#pragma once

#include "Scene.h"

namespace ws
{
class Canvas
{
public:
    explicit Canvas (std::vector<Batch>& out) : batches (out) {}

    void setColour (juce::Colour c) { colour = c; }
    void setFont (float h, bool monospace = false) { fontH = h; fontMono = monospace; }
    void fillRect (juce::Rectangle<float> r);
    void fillRect (juce::Rectangle<int> r) { fillRect (r.toFloat()); }
    void fillRect (float x, float y, float w, float h) { fillRect (juce::Rectangle<float> (x, y, w, h)); }
    void drawRect (juce::Rectangle<int> r, int thickness = 1);
    void drawRect (juce::Rectangle<float> r, int thickness = 1) { drawRect (r.toNearestInt(), thickness); }
    void drawHorizontalLine (int y, float x1, float x2);
    void drawVerticalLine (int x, float y1, float y2);
    void fillEllipse (float x, float y, float w, float h);
    void drawText (const juce::String& s, int x, int y, int w, int h, juce::Justification just);
    void drawText (const juce::String& s, juce::Rectangle<int> r, juce::Justification just) { drawText (s, r.getX(), r.getY(), r.getWidth(), r.getHeight(), just); }
    void drawImageAt (const juce::Image& img, int x, int y);

private:
    std::vector<Batch>& batches;
    juce::Colour colour = juce::Colours::black;
    float fontH = 11.0f;
    bool fontMono = false;

    Batch& open (Batch::Kind kind, bool round);
};
}
