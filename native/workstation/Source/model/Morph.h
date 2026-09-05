#pragma once

#include "Frame.h"

namespace ws
{
constexpr double kRadiusGuard = 0.9995;
constexpr double kPushLow = -2.0, kPushHigh = 3.0;
constexpr double kWheelLow = -1.0, kWheelHigh = 2.0;

struct Morph
{
    Words words {};
    std::array<bool, kRows> guarded {};
    bool outside = false;
};

struct Excess { double hz; bool above; };

bool guardRadius (Words& words, std::array<bool, kRows>& guarded);
Morph pairMorph (const Words& a, const Words& b, double t);
Morph wheelMorph (const std::array<Words, 4>& corners, double morph, double q);
std::vector<Excess> excessOf (const Words& words);
Words meanWords (const std::vector<const Words*>& parents);

struct Lead
{
    Chord a, b;
    std::array<int, kRows> map {};
    double cost = 0.0;
};

double dissolveWidth (double note);
Lead leadTo (const Chord& a, const Chord& b);
double leadCost (const Chord& a, const Chord& b);
Words leadWords (const Words& a, const Words& b);
juce::String intervalsOf (const Chord& c);
Words schwaWords();
}
