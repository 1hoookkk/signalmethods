#pragma once

#include <juce_graphics/juce_graphics.h>
#include "Stitch.h"
#include <optional>
#include <vector>

namespace ws
{
struct Key
{
    double t;
    Spot spot;
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
    std::optional<Spot> pathAt (double t, const Stitch& st) const;
};
}
