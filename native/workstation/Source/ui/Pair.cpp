#include "Pair.h"
#include "Look.h"

namespace hs
{
Pair::Pair (Session& s, const plot::Curves& c) : session (s), curves (c) {}

void Pair::layout (juce::Rectangle<int> r)
{
    area = r;
    const int width = std::max (80, (area.getWidth() - 48) / 2);
    const int height = std::max (60, std::min (width, area.getHeight() - 22 - 64));
    cell[0] = { area.getX(), area.getY() + 22, width, height };
    cell[1] = { area.getRight() - width, area.getY() + 22, width, height };
    for (int i = 0; i < 2; ++i)
    {
        tag[(size_t) i] = cell[(size_t) i].withHeight (16);
        plot[(size_t) i] = cell[(size_t) i].withTrimmedTop (16).reduced (3);
    }
    rail = { area.getX() + 40, cell[0].getBottom() + 26, area.getWidth() - 80, 20 };
}

int Pair::cellAt (juce::Point<int> p) const
{
    for (int i = 0; i < 2; ++i) if (cell[(size_t) i].contains (p)) return i;
    return -1;
}

int Pair::tagAt (juce::Point<int> p) const
{
    for (int i = 0; i < 2; ++i) if (tag[(size_t) i].contains (p)) return i;
    return -1;
}

bool Pair::onRail (juce::Point<int> p) const
{
    return juce::Rectangle<int> (area.getX(), cell[0].getBottom(), area.getWidth(), area.getBottom() - cell[0].getBottom()).contains (p);
}

double Pair::sweepAt (juce::Point<int> p) const
{
    return std::clamp ((p.x - rail.getX()) / (double) rail.getWidth(), 0.0, 1.0);
}

void Pair::paint (juce::Graphics& g, juce::Point<int> dropPoint, bool dropping) const
{
    const int ends[2] = { session.pairA, session.pairB };
    for (int i = 0; i < 2; ++i)
    {
        const int star = ends[i];
        const bool valid = star >= 0 && star < (int) session.stars.size();
        const bool lit = valid && session.auditioning == star;
        curves.cell (g, tag[(size_t) i], plot[(size_t) i], juce::String(), valid ? session.stars[(size_t) star].name : juce::String(),
                     valid ? &session.stars[(size_t) star].words : nullptr, lit, dropping && cell[(size_t) i].contains (dropPoint));
    }
    g.setColour (Look::faint); g.fillRect (rail.withY (rail.getCentreY()).withHeight (1));
    const bool live = session.inPair();
    const float x = (float) (rail.getX() + rail.getWidth() * session.pairT);
    g.setColour (live ? Look::blue : Look::dim);
    juce::Path diamond;
    diamond.addQuadrilateral (x, (float) rail.getCentreY() - 8.0f, x + 8.0f, (float) rail.getCentreY(), x, (float) rail.getCentreY() + 8.0f, x - 8.0f, (float) rail.getCentreY());
    g.setColour (Look::panel); g.fillPath (diamond);
    g.setColour (live ? Look::blue : Look::dim); g.strokePath (diamond, juce::PathStrokeType (1.8f));
    if (live) g.fillEllipse (x - 2.0f, (float) rail.getCentreY() - 2.0f, 4.0f, 4.0f);
}
}
