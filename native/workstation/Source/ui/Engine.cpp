#include "Engine.h"
#include "Look.h"
#include <cmath>

namespace hs
{
Engine::Engine (Session& s, const plot::Curves& c) : session (s), curves (c) {}

void Engine::layout (juce::Rectangle<int> r)
{
    area = r;
    playing = { area.getX(), area.getY() + 2, 84, 56 };
    label = { playing.getRight() + 10, area.getY() + 2, std::max (40, area.getWidth() - 94), 56 };
    for (int i = 0; i < 4; ++i) toKeys[(size_t) i] = { area.getX() + i * 38, area.getY() + 70, 34, 22 };
    writeKey = { toKeys[3].getRight() + 12, area.getY() + 70, 24, 22 };
    keys[0] = { area.getX(), area.getY() + 102, 24, 22 };
    keys[1] = { area.getX() + 30, area.getY() + 102, 62, 22 };
    keys[2] = { area.getX() + 98, area.getY() + 102, 24, 22 };
    keys[3] = { area.getX() + 128, area.getY() + 102, std::max (24, area.getWidth() - 128), 22 };
    status = { area.getX(), area.getBottom() - 18, area.getWidth(), 18 };
}

int Engine::sourceAt (juce::Point<int> p) const
{
    for (int i = 0; i < 4; ++i) if (keys[(size_t) i].contains (p)) return i;
    return -1;
}

int Engine::cornerKeyAt (juce::Point<int> p) const
{
    for (int i = 0; i < 4; ++i) if (toKeys[(size_t) i].contains (p)) return i;
    return -1;
}

void Engine::paint (juce::Graphics& g) const
{
    Look::axes (g, playing);
    g.setColour (Look::faint); g.drawHorizontalLine ((int) std::round (plot::yOf (0.0, playing)), (float) playing.getX() + 1, (float) playing.getRight() - 1);
    if (session.sounding) curves.draw (g, playing, session.heard, Look::blue, 1.4f, false);
    g.setFont (Look::font (12.0f));
    g.setColour (Look::text);
    g.drawText (session.playingLabel, label, juce::Justification::centredLeft);
    const bool placeable = session.placeable();
    g.setFont (Look::font (11.0f));
    for (int i = 0; i < 4; ++i)
    {
        const auto colour = placeable ? Look::ink : Look::faint;
        Look::glyph (g, Look::Glyph::arrow, toKeys[(size_t) i].withWidth (16).reduced (1), colour);
        g.setColour (colour);
        g.drawText (juce::String::charToString (Session::kCornerLetters[i]), toKeys[(size_t) i].withTrimmedLeft (17), juce::Justification::centredLeft);
    }
    Look::glyph (g, Look::Glyph::write, writeKey.reduced (2), session.quad.complete() ? Look::ink : Look::faint);
    const bool on[] = { session.playing, session.source == 0, session.source == 1, session.source == 2 };
    const Look::Glyph glyphs[] = { Look::Glyph::play, Look::Glyph::saw, Look::Glyph::noise, Look::Glyph::loop };
    for (int i = 0; i < 4; ++i)
    {
        const auto colour = on[i] ? Look::blue : Look::dim;
        Look::glyph (g, glyphs[i], keys[(size_t) i].withWidth (22).reduced (2), colour);
        g.setColour (colour);
        if (i == 1) g.drawText (noteName (440.0 * std::pow (2.0, (session.note - 69) / 12.0)), keys[1].withTrimmedLeft (26), juce::Justification::centredLeft);
        if (i == 3 && session.loopName.isNotEmpty()) g.drawText (session.loopName, keys[3].withTrimmedLeft (26), juce::Justification::centredLeft);
    }
    if (session.status.startsWith ("cannot") || session.status.startsWith ("no audio") || session.status.endsWith (".body240"))
    {
        g.setFont (Look::font (10.0f));
        g.setColour (Look::dim);
        g.drawText (session.status, status, juce::Justification::centredLeft);
    }
}
}
