#include "Scene.h"

namespace ws
{
Vertex vertex (juce::Point<float> p, juce::Colour c, float size)
{
    return { p.x, p.y, c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue(), c.getFloatAlpha(), size };
}

juce::Colour levelColour (double db)
{
    const float t = (float) juce::jlimit (0.0, 1.0, (db + 30.0) / 60.0);
    return juce::Colour (0xff0c0f1a).interpolatedWith (juce::Colour (0xffd8d8e6), t);
}
}
