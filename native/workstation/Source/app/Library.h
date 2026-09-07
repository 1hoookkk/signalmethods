#pragma once

#include "Quad.h"
#include "dsp/VectorFit.h"
#include <optional>

namespace hs
{
constexpr double kSchwaF1 = 500.0, kSchwaF2 = 1500.0, kNeutralF3 = 2500.0, kNeutralF4 = 3300.0;

std::vector<Star> loadLibrary (const juce::File& p2kDir);
std::vector<Star> loadVowels (const juce::File& bankFile);
std::vector<Star> loadBodies (const juce::File& dir);
std::vector<Star> loadTable (const juce::File& csv, const juce::String& bank);
std::vector<Star> loadReads (const juce::File& dir, const juce::File& census);
Star schwa();
Star madeVowel (double f1, double f2);
std::optional<Star> readWav (const juce::File& wav);
std::optional<Star> fitWav (const juce::File& wav);
Words fittedWords (const Fitted& fit, double sampleRateHz);
juce::File bodyFile (const juce::File& p2kDir, const juce::String& body);
void unityDc (Words& words);
Words vowelWords (const std::array<double, 4>& formants);
Words transposed (const Words& words, double ratio);
Words relaxed (const Words& words);
Words lensed (const Words& words, double f1, double f2);
juce::String formantName (const Words& words);
}
