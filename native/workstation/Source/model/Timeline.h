#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>
#include <optional>
#include <vector>

namespace ws
{
struct Key
{
    double t;
    std::array<double, 2> p;
    juce::Colour colour;
};

class Timeline
{
public:
    std::vector<Key> keys;
    double duration = 8.0;
    double playhead = 0.0;
    bool playing = false;
    bool loop = true;

    std::vector<Key> sorted() const;
    std::optional<std::array<double, 2>> pathAt (double t) const;
};
}
