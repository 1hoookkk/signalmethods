#pragma once

#include "Plot.h"
#include "app/Session.h"

namespace hs
{
struct Stage
{
    Stage (Session& session, const plot::Curves& curves);
    void layout (juce::Rectangle<int> area);
    void paint (juce::Graphics& g, int litRow, bool bladeLit, bool carving) const;
    Words words() const;
    juce::Point<float> peakPoint (int row) const;
    int peakAt (juce::Point<int> p) const;
    juce::Point<float> zeroPoint (int row) const;
    int zeroAt (juce::Point<int> p) const;
    float bladeX() const;
    double cascadeDb (int row) const;
    static Words solved (const Words& words, int row, bool zero, double hz, double targetDb);
    static Section seed (double hz);

    juce::Rectangle<int> area, magnitude, carveKey, modeKey;
    bool showHardware = false;
    bool zerosMode = false;
    double carve = 0.0;

private:
    Session& session;
    const plot::Curves& curves;
};
}
