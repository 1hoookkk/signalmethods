#include "PinMenu.h"
#include "Look.h"
#include "Plot.h"

namespace hs
{
void PinMenu::show (const Session& session, int which, bool inCube, juce::Rectangle<int> anchor, juce::Rectangle<int> bounds)
{
    open = true; target = which; cube = inCube; scroll = 0;
    const int count = (int) session.stars.size();
    const int h = std::min (18 * count, std::max (90, bounds.getBottom() - anchor.getBottom() - 30));
    int y = anchor.getBottom() + 2;
    if (y + h > bounds.getBottom() - 8) y = std::max (bounds.getY() + 8, anchor.getY() - h - 2);
    const int width = std::max (anchor.getWidth(), 200);
    int x = anchor.getX();
    if (x + width > bounds.getRight() - 8) x = std::max (bounds.getX() + 8, bounds.getRight() - 8 - width);
    rect = { x, y, width, h };
    const int current = pinned (session);
    if (current > 4) scroll = std::min (current * 18 - 36, std::max (0, 18 * count - h));
}

int PinMenu::itemAt (const Session& session, juce::Point<int> p) const
{
    if (! open || ! rect.contains (p)) return -1;
    const int i = (p.y - rect.getY() + scroll) / 18;
    return i >= 0 && i < (int) session.stars.size() ? i : -1;
}

int PinMenu::pinned (const Session& session) const
{
    if (target < 0) return -1;
    return cube ? session.cube.pins[(size_t) target] : session.quad.pins[(size_t) Session::kCornerPin[target]];
}

void PinMenu::wheel (const Session& session, int step)
{
    scroll = std::clamp (scroll + step * 54, 0, std::max (0, 18 * (int) session.stars.size() - rect.getHeight()));
}

void PinMenu::paint (juce::Graphics& g, const Session& session) const
{
    if (! open) return;
    g.setColour (Look::panel); g.fillRect (rect);
    g.setColour (Look::dim); g.drawRect (rect);
    g.setFont (Look::font (11.0f));
    const int count = (int) session.stars.size();
    const int first = scroll / 18, last = std::min (count, first + rect.getHeight() / 18 + 2);
    const int current = pinned (session);
    juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (rect);
    for (int i = first; i < last; ++i)
    {
        juce::Rectangle<int> line (rect.getX(), rect.getY() + i * 18 - scroll, rect.getWidth(), 18);
        if (current == i) { g.setColour (Look::grid); g.fillRect (line); }
        g.setColour (plot::inkOf (session.stars[(size_t) i]));
        g.drawText (session.stars[(size_t) i].name, line.reduced (8, 0), juce::Justification::centredLeft);
    }
}
}
