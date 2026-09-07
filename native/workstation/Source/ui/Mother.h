#pragma once

#include "Plot.h"
#include "app/Session.h"

namespace hs
{
struct Mother
{
    Mother (Session& session, const plot::Curves& curves);
    void layout (juce::Rectangle<int> area);
    void paint (juce::Graphics& g, juce::Point<int> dropPoint, bool dropping) const;
    int cellAt (juce::Point<int> p) const;
    int tagAt (juce::Point<int> p) const;
    int railAt (juce::Point<int> p) const;
    double valueAt (int rail, juce::Point<int> p) const;
    bool onOctaves (juce::Point<int> p) const;

    juce::Rectangle<int> area, octavesKey, bakeKey;
    std::array<juce::Rectangle<int>, 3> rail;
    std::array<juce::Rectangle<int>, 2> cell, tag, plot;

private:
    Session& session;
    const plot::Curves& curves;
};
}
