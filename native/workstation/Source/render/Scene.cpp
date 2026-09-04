#include "Scene.h"

namespace ws
{
Vertex vertex (juce::Point<float> p, juce::Colour c, float size)
{
    return { p.x, p.y, c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue(), c.getFloatAlpha(), size };
}
}
