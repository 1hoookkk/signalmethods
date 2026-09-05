#pragma once

#include "Frame.h"
#include "Delaunay.h"
#include <optional>

namespace ws
{
struct Anchor { int frame; std::array<double, 2> p; };
struct Blend { std::array<int, 3> anchors; std::array<double, 3> w; };

class Library
{
public:
    std::vector<Frame> frames;
    std::vector<Anchor> anchors;
    std::vector<Triangle> tris;
    int axisX = 6, axisY = 7;

    bool loadJson (const juce::File& file);
    bool loadBodies (const juce::File& dir);
    void computePca();
    std::array<double, 2> coordOf (const Frame& f) const;
    void sort();
    void retriangulate();
    std::optional<Blend> blendAt (double u, double v) const;
    Words wordsOf (const Blend& b) const;
    int addCapture (const Words& words, std::array<double, 2> at);
    int addNamed (const Words& words, const juce::String& name, int group, bool capture);
    void addSchwa();
    int addGroupMean (int group);
};
}
