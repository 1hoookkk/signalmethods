#pragma once

#include "Strip.h"

namespace hs
{
struct Entry
{
    juce::String name, body, corner;
    Words words {};
};

std::vector<Entry> loadLibrary (const juce::File& p2kDir);
int partnerOf (const std::vector<Entry>& library, int k, const juce::String& q);
Column factoryColumn (const std::vector<Entry>& library, int k);
juce::File bodyFile (const juce::File& p2kDir, const juce::String& body);
}
