#pragma once

#include "app/Quad.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace hs::plot
{
double xOf (double hz, juce::Rectangle<int> r);
double yOf (double db, juce::Rectangle<int> r);
double hzAt (int x, juce::Rectangle<int> r);
double dbAt (int y, juce::Rectangle<int> r);
juce::Colour inkOf (const Star& s);

class Curves
{
public:
    Curves();
    const std::vector<double>& grid() const { return hz; }
    const std::vector<double>& db (const Words& words) const;
    void draw (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour, float width, bool cached = true) const;
    void cell (juce::Graphics& g, juce::Rectangle<int> tag, juce::Rectangle<int> plot, const juce::String& letter, const juce::String& name, const Words* words, bool lit, bool target) const;

private:
    std::vector<double> hz;
    mutable std::map<Words, std::vector<double>> cache;
};
}
