#pragma once

#include "Plot.h"
#include "app/Session.h"

namespace hs
{
struct Body
{
    Body (Session& session, const plot::Curves& curves);
    void layout (juce::Rectangle<int> area);
    void paint (juce::Graphics& g, int dropCorner) const;
    juce::Point<float> puckPoint() const;
    int cornerAt (juce::Point<int> p) const;
    int tagAt (juce::Point<int> p) const;

    juce::Rectangle<int> area;
    std::array<juce::Rectangle<int>, 4> box, tag, plot;

private:
    Session& session;
    const plot::Curves& curves;
};
}
