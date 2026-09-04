#pragma once

#include <juce_graphics/juce_graphics.h>
#include <vector>

namespace ws
{
struct Vertex { float x, y, r, g, b, a, size; };

struct Batch
{
    enum Kind { points, lines, strip, tris };
    Kind kind;
    bool round;
    std::vector<Vertex> v;
};

struct FieldVertex { float x, y, bx, by, bz, ia, ib, ic; };

struct FieldMesh
{
    std::vector<FieldVertex> v;
    double hz = 1000.0;
};

Vertex vertex (juce::Point<float> p, juce::Colour c, float size);
juce::Colour levelColour (double db);
}
