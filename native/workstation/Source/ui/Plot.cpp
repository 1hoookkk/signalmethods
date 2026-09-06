#include "Plot.h"
#include "Look.h"
#include <cmath>

namespace hs::plot
{
double xOf (double hz, juce::Rectangle<int> r) { return r.getX() + r.getWidth() * std::log (hz / 20.0) / std::log (1000.0); }
double yOf (double db, juce::Rectangle<int> r) { return r.getBottom() - r.getHeight() * (db + 30.0) / 60.0; }
double hzAt (int x, juce::Rectangle<int> r) { return std::clamp (20.0 * std::pow (1000.0, (x - r.getX()) / (double) r.getWidth()), 20.0, 20000.0); }
double dbAt (int y, juce::Rectangle<int> r) { return std::clamp ((r.getBottom() - y) * 60.0 / r.getHeight() - 30.0, -30.0, 30.0); }

juce::Colour inkOf (const Star& s)
{
    if (s.kind == "vowel") return juce::Colour (Look::kBlue);
    if (s.kind == "capture") return juce::Colour (Look::kOrange);
    if (s.kind == "read") return juce::Colour (Look::kPurple);
    if (s.kind == "body") return juce::Colour (Look::kGreen);
    return juce::Colour (Look::kInk);
}

Curves::Curves() : hz (curveHz()) {}

const std::vector<double>& Curves::db (const Words& words) const
{
    if (const auto it = cache.find (words); it != cache.end()) return it->second;
    if (cache.size() > 600) cache.clear();
    return cache.emplace (words, responseDb (words, hz)).first->second;
}

void Curves::draw (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour, float width, bool cached) const
{
    juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (r);
    const std::vector<double> live = cached ? std::vector<double>() : responseDb (words, hz);
    const auto& values = cached ? db (words) : live;
    juce::Path p;
    for (size_t i = 0; i < hz.size(); ++i)
    {
        const float x = (float) xOf (hz[i], r), y = (float) std::clamp (yOf (values[i], r), (double) r.getY(), (double) r.getBottom());
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (width));
}

void Curves::cell (juce::Graphics& g, juce::Rectangle<int> tag, juce::Rectangle<int> plot, const juce::String& letter, const juce::String& name, const Words* words, bool lit, bool target) const
{
    g.setFont (Look::font (11.0f));
    g.setColour (lit ? Look::orange : Look::dim);
    g.drawText (letter, tag.withTrimmedLeft (3), juce::Justification::centredLeft);
    g.setColour (Look::text);
    g.drawText (name.isNotEmpty() ? name : juce::String ("+"), tag.withTrimmedLeft (16).withTrimmedRight (14), juce::Justification::centredLeft);
    Look::glyph (g, Look::Glyph::caret, tag.withTrimmedLeft (tag.getWidth() - 12).withSizeKeepingCentre (12, 12), Look::dim);
    Look::axes (g, plot);
    g.setColour (Look::faint); g.drawHorizontalLine ((int) std::round (yOf (0.0, plot)), (float) plot.getX() + 1, (float) plot.getRight() - 1);
    if (words != nullptr) draw (g, plot, *words, lit ? Look::orange : Look::blue, lit ? 1.7f : 1.2f);
    if (target) { g.setColour (Look::orange); g.drawRect (plot.expanded (2), 2); }
}
}
