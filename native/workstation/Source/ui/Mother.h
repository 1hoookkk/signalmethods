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
    juce::Point<float> probePoint (int side) const;
    int cellAt (juce::Point<int> p) const;
    int tagAt (juce::Point<int> p) const;
    int sideAt (juce::Point<int> p) const;
    int pinAt (juce::Point<int> p) const;
    static int pinOf (int cell);

    juce::Rectangle<int> area, depth, bakeKey;
    std::array<juce::Rectangle<int>, 2> face;
    std::array<juce::Rectangle<int>, 8> cell, tag, plot;

private:
    Session& session;
    const plot::Curves& curves;
};
}
