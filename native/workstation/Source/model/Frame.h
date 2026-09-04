#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <trench/core/packed_body.hpp>
#include <array>
#include <cstdint>
#include <vector>

namespace ws
{
constexpr double kDatumHz = trench::core::kP2kDatumHz;
constexpr int kRows = 6;
constexpr int kWords = 5;
constexpr int kMeasures = 8;
constexpr int kCurvePoints = 96;
constexpr int kGroups = 7;

using Words = std::array<std::array<std::uint16_t, kWords>, kRows>;
using Curve = std::array<double, kCurvePoints>;

extern const char* const kGroupNames[kGroups];
extern const char* const kMeasureNames[kMeasures];
extern const char* const kPoseEnds[kMeasures][2];

struct RowGeom
{
    bool pole = false;
    double pHz = 0.0, pR = 0.0;
    bool zero = false;
    double zHz = 0.0, zR = 0.0;
};

struct Frame
{
    juce::String name;
    Words words {};
    std::array<RowGeom, kRows> rows {};
    std::array<double, kMeasures> m {};
    bool capture = false;
    int group = 0;
};

int groupOf (const juce::String& name);
double resDb (double radius);
double octOf (double hz);
std::array<RowGeom, kRows> geometryOf (const Words& words);
trench::core::Cascade cascadeOf (const Words& words);
double responseDb (const trench::core::Cascade& cascade, double hz);
Curve curveOf (const Words& words);
void measure (Frame& frame);
juce::Colour hueOf (double t);
Words blend (const std::vector<const Words*>& parents, const std::vector<double>& weights);
}
