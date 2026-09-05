#pragma once

#include "Frame.h"
#include "Morph.h"
#include <array>

namespace ws
{
class Body
{
public:
    std::array<int, 4> corner { -1, -1, -1, -1 };
    std::array<bool, kRows> rowOn { true, true, true, true, true, true };
    double morph = 0.5, q = 0.5;
    bool unity = true;

    bool ready() const;
    Words cornerWords (const std::vector<Frame>& frames, int i) const;
    Words wheelWords (const std::vector<Frame>& frames) const;
    Morph wheelMorph (const std::vector<Frame>& frames) const;
    bool outside() const;
    std::array<double, 4> weights() const;
    std::array<std::uint8_t, trench::core::kLegacyBodyBytes> legacyBytes (const std::vector<Frame>& frames) const;
    bool exportTo (const std::vector<Frame>& frames, const juce::File& file) const;
};
}
