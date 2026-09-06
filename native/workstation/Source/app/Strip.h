#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <trench/core/packed_body.hpp>
#include <array>
#include <cstdint>
#include <vector>

namespace hs
{
constexpr int kRows = 6;
constexpr int kWords = 5;
using Words = std::array<trench::core::PackedSection, kRows>;
using Corners = std::array<Words, 4>;
using Bytes = std::array<std::uint8_t, trench::core::kLegacyBodyBytes>;

struct Origin
{
    juce::String kind, body, corner, parentA, parentB;
    double morph = 0.0;
    bool operator== (const Origin& o) const { return kind == o.kind && body == o.body && corner == o.corner && parentA == o.parentA && parentB == o.parentB && morph == o.morph; }
};

struct Anchor
{
    juce::String name;
    Origin origin;
    Words q0 {}, q1 {};
};

struct Strip
{
    std::vector<Anchor> anchors;
    int square = 1;
    double morph = 0.0, q = 0.0;
    int selected = 0;
    int captures = 0;
    int count() const { return (int) anchors.size(); }
    int squares() const { return std::max (1, count() - 1); }
};

bool admit (const Words& words);
Strip insert (Strip s, int k, const Anchor& c);
Strip keep (Strip s);
Strip move (Strip s, int k, int direction);
Strip remove (Strip s, int k);
Strip step (Strip s, double dm, double dq);
Strip jumpTo (Strip s, int k);
Corners cornersOf (const Strip& s, int k = 0);
trench::core::PackedBody bodyOf (const Corners& c);
Words lerp (const Corners& c, double morph, double q);
Words wordsAt (const Strip& s);
Bytes bytesOf (const Corners& c);
bool writeBody (const Strip& s, const juce::File& file);
bool save (const Strip& s, const juce::File& file);
Strip open (const juce::File& file);
std::vector<double> curveHz();
std::vector<double> responseDb (const Words& words, const std::vector<double>& hz);
std::array<double, 4> formantsOf (const Words& words);
}
