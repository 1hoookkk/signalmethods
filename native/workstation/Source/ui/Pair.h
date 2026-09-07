#pragma once

#include "Plot.h"
#include "app/Session.h"

namespace hs
{
struct Pair
{
    Pair (Session& session, const plot::Curves& curves);
    void layout (juce::Rectangle<int> area);
    void paint (juce::Graphics& g, juce::Point<int> dropPoint, bool dropping) const;
    int cellAt (juce::Point<int> p) const;
    int tagAt (juce::Point<int> p) const;
    bool onRail (juce::Point<int> p) const;
    double sweepAt (juce::Point<int> p) const;

    juce::Rectangle<int> area, rail;
    std::array<juce::Rectangle<int>, 2> cell, tag, plot;

private:
    Session& session;
    const plot::Curves& curves;
};
}
