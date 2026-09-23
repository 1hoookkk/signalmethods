#pragma once
#include "BinaryData.h"
#include <juce_core/juce_core.h>

inline juce::MemoryBlock fixtureBody (const char* fileName)
{
    for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
        if (juce::String (BinaryData::originalFilenames[r]) == fileName)
        {
            int size = 0;
            const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[r], size);
            return juce::MemoryBlock (data, (size_t) size);
        }
    juce::MemoryBlock block;
    juce::File (juce::String (TRENCH_TABLE_STITCH_ROOT)).getChildFile ("plugin/presets/bodies").getChildFile (fileName).loadFileAsData (block);
    return block;
}
