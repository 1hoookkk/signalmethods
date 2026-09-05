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
constexpr int kCurvePoints = 160;
constexpr int kGroups = 7;
constexpr double kMaxWidth = 120.0;

using Words = std::array<std::array<std::uint16_t, kWords>, kRows>;
using Curve = std::array<double, kCurvePoints>;

extern const char* const kGroupNames[kGroups];
extern const char* const kMeasureNames[kMeasures];
extern const char* const kPoseEnds[kMeasures][2];

struct Voice
{
    bool on = false;
    double note = 60.0;
    double width = 12.0;
};

struct Stage
{
    Voice pole, zero;
    double gainDb = 0.0;
    std::array<std::uint16_t, kWords> raw { 0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF };
};

using Chord = std::array<Stage, kRows>;

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
    Chord chord {};
    Words words {};
    std::array<RowGeom, kRows> rows {};
    std::array<double, kMeasures> m {};
    bool capture = false;
    int group = 0;
};

double noteOf (double hz);
double hzOf (double note);
double widthOf (double hz, double radius, double datum);
double radiusOf (double hz, double width, double datum);
juce::String noteName (double note);
Chord decompile (const Words& words, double datum);
Words compile (const Chord& chord, double datum);
Chord chordFrom (double rootNote, const std::array<double, kRows>& intervals, double width, double gainDb);
void setChord (Frame& frame, const Chord& chord);
void setWords (Frame& frame, const Words& words, double datum);
void fitVoice (Voice& voice, bool pole, double datum);

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
void setPole (Words& words, int row, double hz, double radius);
void setZero (Words& words, int row, double hz, double radius);
double sectionDb (const Words& words, int row, double hz);
void unityDc (Words& words);
void sharpen (Chord& chord, double keep);
}
