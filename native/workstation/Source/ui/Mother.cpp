#include "Mother.h"
#include "Look.h"

namespace hs
{
namespace
{
constexpr int kRailHeight = 20, kRailGap = 18, kRailTop = 26;
constexpr int kRailBlock = kRailTop + 3 * kRailHeight + 2 * kRailGap + 4;
}

Mother::Mother (Session& s, const plot::Curves& c) : session (s), curves (c) {}

void Mother::layout (juce::Rectangle<int> r)
{
    area = r;
    const int width = std::max (80, (area.getWidth() - 48) / 2);
    const int height = std::max (60, std::min (width, area.getHeight() - 22 - kRailBlock));
    cell[0] = { area.getX(), area.getY() + 22, width, height };
    cell[1] = { area.getRight() - width, area.getY() + 22, width, height };
    for (int i = 0; i < 2; ++i)
    {
        tag[(size_t) i] = cell[(size_t) i].withHeight (16);
        plot[(size_t) i] = cell[(size_t) i].withTrimmedTop (16).reduced (3);
    }
    const int left = area.getX() + 40, right = area.getRight() - 96;
    for (int i = 0; i < 3; ++i)
        rail[(size_t) i] = { left, cell[0].getBottom() + kRailTop + i * (kRailHeight + kRailGap), right - left, kRailHeight };
    octavesKey = { rail[1].getRight() + 8, rail[1].getY(), 52, 20 };
    bakeKey = { rail[2].getRight() + 8, rail[2].getY(), 28, 22 };
}

int Mother::cellAt (juce::Point<int> p) const
{
    for (int i = 0; i < 2; ++i) if (cell[(size_t) i].contains (p)) return i;
    return -1;
}

int Mother::tagAt (juce::Point<int> p) const
{
    for (int i = 0; i < 2; ++i) if (tag[(size_t) i].contains (p)) return i;
    return -1;
}

int Mother::railAt (juce::Point<int> p) const
{
    for (int i = 0; i < 3; ++i) if (rail[(size_t) i].expanded (0, 6).contains (p)) return i;
    return -1;
}

double Mother::valueAt (int which, juce::Point<int> p) const
{
    const auto r = rail[(size_t) std::clamp (which, 0, 2)];
    return std::clamp ((p.x - r.getX()) / (double) r.getWidth(), 0.0, 1.0);
}

bool Mother::onOctaves (juce::Point<int> p) const { return octavesKey.contains (p); }

void Mother::paint (juce::Graphics& g, juce::Point<int> dropPoint, bool dropping) const
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
    const bool live = session.inPair();
    const double at[3] = { session.pairT, session.frequency, session.stress };
    for (int i = 0; i < 3; ++i)
    {
        const auto r = rail[(size_t) i];
        g.setColour (Look::faint); g.fillRect (r.withY (r.getCentreY()).withHeight (1));
        const float x = (float) (r.getX() + r.getWidth() * at[i]);
        const float y = (float) r.getCentreY();
        juce::Path diamond;
        diamond.addQuadrilateral (x, y - 8.0f, x + 8.0f, y, x, y + 8.0f, x - 8.0f, y);
        g.setColour (Look::panel); g.fillPath (diamond);
        g.setColour (live ? Look::blue : Look::dim); g.strokePath (diamond, juce::PathStrokeType (1.8f));
        if (live) g.fillEllipse (x - 2.0f, y - 2.0f, 4.0f, 4.0f);
    }
    g.setFont (Look::font (11.0f));
    g.setColour (live ? Look::ink : Look::dim);
    g.drawText ((session.octaves < 0.0 ? juce::String() : juce::String ("+")) + juce::String (session.octaves, 1), octavesKey, juce::Justification::centred);
    const bool both = session.pairA >= 0 && session.pairB >= 0;
    Look::glyph (g, Look::Glyph::use, bakeKey.reduced (3), both ? Look::ink : Look::faint);
}
}
