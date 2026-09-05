#pragma once

#include <juce_graphics/juce_graphics.h>
#include "Scene.h"

namespace ws
{
void drawScene (juce::Graphics& g, const std::vector<Batch>& batches);
}
