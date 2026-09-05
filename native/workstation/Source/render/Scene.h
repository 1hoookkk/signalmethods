#pragma once

#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace ws
{
struct Vertex { float x, y, r, g, b, a, size; };

struct Batch
{
    enum Kind { points, lines, strip };
    Kind kind;
    bool round;
    std::vector<Vertex> v;
};

Vertex vertex (juce::Point<float> p, juce::Colour c, float size);
}
