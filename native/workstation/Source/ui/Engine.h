#pragma once

#include "Plot.h"
#include "app/Session.h"

namespace hs
{
struct Engine
{
    Engine (Session& session, const plot::Curves& curves);
    void layout (juce::Rectangle<int> area);
    void paint (juce::Graphics& g) const;
    int sourceAt (juce::Point<int> p) const;
    int cornerKeyAt (juce::Point<int> p) const;

    juce::Rectangle<int> area, playing, label, writeKey, status;
    std::array<juce::Rectangle<int>, 4> keys, toKeys;

private:
    Session& session;
    const plot::Curves& curves;
};
}
