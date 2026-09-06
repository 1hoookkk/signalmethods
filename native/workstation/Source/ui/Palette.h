#pragma once

#include "Plot.h"
#include "app/Session.h"

namespace hs
{
struct Palette
{
    Palette (Session& session, const plot::Curves& curves);
    void layout (juce::Rectangle<int> area);
    void paint (juce::Graphics& g, int transposingFrom) const;
    juce::Point<float> chartPoint (double f1, double f2) const;
    std::pair<double, double> formantsAt (juce::Point<int> p) const;
    int pointAt (juce::Point<int> p) const;
    int cardAt (juce::Point<int> p) const;
    int tabAt (juce::Point<int> p) const;
    juce::Rectangle<int> card (int index) const;
    std::vector<int> cards() const;
    void scrollBy (int pixels);
    void showTab (int which);
    void followKind (const juce::String& kind);
    static bool onChart (const Star& s);
    static constexpr double kF1Low = 200.0, kF1High = 1100.0, kF2Low = 600.0, kF2High = 3500.0;

    juce::Rectangle<int> area, chart, picker, dropZone, keepKey;
    std::array<juce::Rectangle<int>, 3> tabs;
    int tab = 0;

private:
    Session& session;
    const plot::Curves& curves;
    int scroll = 0;
};
}
