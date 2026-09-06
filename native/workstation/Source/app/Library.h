#pragma once

#include "Strip.h"

namespace hs
{
struct Entry
{
    juce::String name, body, side;
    Words q0 {}, q1 {};
};

std::vector<Entry> loadLibrary (const juce::File& p2kDir);
Anchor factoryAnchor (const std::vector<Entry>& library, int k);
juce::File bodyFile (const juce::File& p2kDir, const juce::String& body);
}
