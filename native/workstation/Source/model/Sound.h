#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "Frame.h"
#include <vector>

namespace ws
{
class Sound
{
public:
    juce::File file;
    std::shared_ptr<std::vector<float>> mono = std::make_shared<std::vector<float>>();
    double regionA = 0.0, regionB = 0.0;
    double sampleRate = 44100.0;
    juce::Image spectrogram;
    int columns = 0;
    double seconds = 0.0;
    double slice = 0.0;
    bool speech = true;

    bool load (const juce::File& wav);
    void render (int width, int height);
    std::vector<float> window (double atSeconds, double lengthSeconds) const;
    Curve sliceMagnitude (double atSeconds) const;
    std::optional<Words> frameAt (double atSeconds) const;
    static std::vector<juce::File> scan (const juce::File& root);
};
}
