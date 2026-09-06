#pragma once

#include "Quad.h"
#include <optional>

namespace hs
{
constexpr double kSchwaF1 = 500.0, kSchwaF2 = 1500.0, kNeutralF3 = 2500.0, kNeutralF4 = 3300.0;

std::vector<Star> loadLibrary (const juce::File& p2kDir);
std::vector<Star> loadVowels (const juce::File& bankFile);
Star schwa();
Star madeVowel (double f1, double f2);
std::optional<Star> readWav (const juce::File& wav);
juce::File bodyFile (const juce::File& p2kDir, const juce::String& body);
void unityDc (Words& words);
Words vowelWords (const std::array<double, 4>& formants);
juce::String formantName (const Words& words);
}
