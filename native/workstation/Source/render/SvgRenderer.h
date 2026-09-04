#pragma once

#include <juce_graphics/juce_graphics.h>
#include "Scene.h"

namespace ws
{
juce::String sceneToSvg (const std::vector<Batch>& batches, float width, float height);
void drawSvg (juce::Graphics& g, const juce::String& svg, float width, float height);
}
