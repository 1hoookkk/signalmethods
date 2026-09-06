#pragma once

#include "Quad.h"
#include <optional>

namespace hs
{
std::vector<Star> loadLibrary (const juce::File& p2kDir);
std::vector<Star> loadVowels (const juce::File& bankFile);
std::optional<Star> readWav (const juce::File& wav);
juce::File bodyFile (const juce::File& p2kDir, const juce::String& body);
void unityDc (Words& words);
Words vowelWords (const std::array<double, 4>& formants);
}
