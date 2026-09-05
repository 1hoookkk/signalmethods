#pragma once

#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace ws
{
struct Vertex { float x, y, r, g, b, a, size; };

struct Text
{
    juce::Rectangle<float> box;
    juce::String s;
    juce::Colour colour;
    juce::Justification just = juce::Justification::centredLeft;
    float size = 11.0f;
    bool mono = false;
};

struct Batch
{
    enum Kind { points, lines, strip, rects, text, image };
    Kind kind;
    bool round;
    std::vector<Vertex> v;
    std::vector<Text> texts;
    juce::Image img;
    juce::Rectangle<float> box;
};

Vertex vertex (juce::Point<float> p, juce::Colour c, float size);
}
