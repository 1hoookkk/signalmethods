#pragma once

#include <juce_core/juce_core.h>
#include <trench/core/packed_body.hpp>
#include <algorithm>
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

struct Star
{
    juce::String name, body, corner, kind, parentA, parentB;
    Words words {};
    double morph = 0.0, q = 0.0;
};

struct Quad
{
    std::array<int, 4> pins { -1, -1, -1, -1 };
    double morph = 0.0, q = 0.0;
    int captures = 0;
    bool complete() const { return pins[0] >= 0 && pins[1] >= 0 && pins[2] >= 0 && pins[3] >= 0; }
};

struct AuthoringCube
{
    std::array<int, 8> pins { -1, -1, -1, -1, -1, -1, -1, -1 };
    double x = 0.5, y = 0.5, z = 0.5;
    bool complete() const { return std::all_of (pins.begin(), pins.end(), [] (int pin) { return pin >= 0; }); }
};

trench::core::PackedBody cubeBodyOf (const AuthoringCube& cube, const std::vector<Star>& stars);
Words cubeWordsAt (const AuthoringCube& cube, const std::vector<Star>& stars);

extern const char* const kPinNames[4];

enum class RowType { rest, peak, notch };
struct Row
{
    RowType type = RowType::rest;
    int f = 64, g = 0;
};
constexpr int kFreqCodes = 128, kGainMin = -32, kGainMax = 31;

struct Section
{
    bool pole = false, zero = false;
    double poleHz = 0.0, poleRadius = 0.0, zeroHz = 0.0, zeroRadius = 0.0, scale = 1.0;
};
Section sectionOf (const trench::core::PackedSection& words);
trench::core::PackedSection sectionWords (const Section& section, std::uint16_t fifth, bool keepFifth = true);
double widthSt (double hz, double radius);
double radiusForWidth (double hz, double st);

trench::core::PackedSection rowWords (Row row, std::uint16_t fifth);
Row rowOf (const trench::core::PackedSection& words);
double rowHz (const trench::core::PackedSection& words);
double rowDb (const trench::core::PackedSection& words);
juce::String noteName (double hz);

bool admit (const Words& words);
Corners cornersOf (const Quad& quad, const std::vector<Star>& stars);
trench::core::PackedBody bodyOf (const Corners& c);
Words lerp (const Corners& c, double morph, double q);
Words wordsAt (const Quad& quad, const std::vector<Star>& stars);
Bytes bytesOf (const Corners& c);
bool writeBody (const Quad& quad, const std::vector<Star>& stars, const juce::File& file);
bool save (const Quad& quad, const std::vector<Star>& stars, size_t libraryCount, const juce::File& file, const AuthoringCube* cube = nullptr);
bool open (Quad& quad, std::vector<Star>& stars, size_t libraryCount, const juce::File& file, AuthoringCube* cube = nullptr);
std::vector<double> curveHz();
std::vector<double> responseDb (const Words& words, const std::vector<double>& hz);
double peakDb (const Words& words, const std::vector<double>& hz);
std::array<double, 4> formantsOf (const Words& words);
std::vector<double> hotCells (const Corners& c, int n, const std::vector<double>& hz);
}
