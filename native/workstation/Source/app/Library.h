#pragma once

#include "Quad.h"

namespace hs
{
std::vector<Star> loadLibrary (const juce::File& p2kDir);
juce::File bodyFile (const juce::File& p2kDir, const juce::String& body);
}
