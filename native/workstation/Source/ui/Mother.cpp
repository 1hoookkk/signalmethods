#include "Mother.h"
#include "Look.h"

namespace hs
{
Mother::Mother (Session& s, const plot::Curves& c) : session (s), curves (c) {}

int Mother::pinOf (int index) { return (index / 4) * 4 + Session::kCornerPin[index % 4]; }

void Mother::layout (juce::Rectangle<int> r)
{
    area = r;
    const int size = std::max (60, std::min ((area.getWidth() - 24) / 4, (area.getHeight() - 92) / 2));
    for (int side = 0; side < 2; ++side)
    {
        face[(size_t) side] = { area.getX() + side * (2 * size + 24), area.getY() + 22, 2 * size, 2 * size };
        for (int n = 0; n < 4; ++n)
        {
            const size_t i = (size_t) (side * 4 + n);
            cell[i] = { face[(size_t) side].getX() + (n & 1) * size, face[(size_t) side].getY() + (n >> 1) * size, size, size };
            tag[i] = cell[i].withHeight (16);
            plot[i] = cell[i].withTrimmedTop (16).reduced (3);
        }
    }
    depth = { area.getX(), face[0].getBottom() + 18, 4 * size + 24, 20 };
    bakeKey = { depth.getRight() - 28, depth.getBottom() + 6, 28, 22 };
}

juce::Point<float> Mother::probePoint (int side) const
{
    const auto f = face[(size_t) std::clamp (side, 0, 1)];
    return { (float) (f.getX() + session.cube.x * f.getWidth()), (float) (f.getBottom() - session.cube.y * f.getHeight()) };
}

int Mother::cellAt (juce::Point<int> p) const
{
    for (int i = 0; i < 8; ++i) if (cell[(size_t) i].contains (p)) return i;
    return -1;
}

int Mother::tagAt (juce::Point<int> p) const
{
    for (int i = 0; i < 8; ++i) if (tag[(size_t) i].contains (p)) return i;
    return -1;
}

int Mother::sideAt (juce::Point<int> p) const
{
    for (int side = 0; side < 2; ++side) if (face[(size_t) side].contains (p)) return side;
    return -1;
}

int Mother::pinAt (juce::Point<int> p) const
{
    const int i = cellAt (p);
    return i >= 0 ? pinOf (i) : -1;
}

void Mother::paint (juce::Graphics& g, juce::Point<int> dropPoint, bool dropping) const
{
    g.setFont (Look::font (11.0f));
    g.setColour (Look::dim);
    g.drawText ("mother", area.withHeight (18), juce::Justification::centredLeft);
    for (int i = 0; i < 8; ++i)
    {
        const int pin = pinOf (i);
        const int star = session.cube.pins[(size_t) pin];
        const bool lit = session.editingCube && session.editing == pin;
        curves.cell (g, tag[(size_t) i], plot[(size_t) i], juce::String (pin + 1), star >= 0 ? session.stars[(size_t) star].name : juce::String(),
                     star >= 0 ? &session.stars[(size_t) star].words : nullptr, lit, dropping && cell[(size_t) i].contains (dropPoint));
    }
    for (int side = 0; side < 2; ++side)
    {
        const auto p = probePoint (side);
        const float weight = side == 0 ? (float) (1.0 - session.cube.z) : (float) session.cube.z;
        juce::Path diamond;
        diamond.addQuadrilateral (p.x, p.y - 8.0f, p.x + 8.0f, p.y, p.x, p.y + 8.0f, p.x - 8.0f, p.y);
        g.setColour (Look::panel.withAlpha (0.6f + 0.4f * weight)); g.fillPath (diamond);
        g.setColour (Look::blue.withAlpha (0.35f + 0.65f * weight)); g.strokePath (diamond, juce::PathStrokeType (1.6f));
    }
    g.setColour (Look::faint); g.fillRect (depth.withY (depth.getCentreY()).withHeight (1));
    g.setColour (Look::blue); g.fillEllipse ((float) (depth.getX() + depth.getWidth() * session.cube.z) - 5.0f, (float) depth.getCentreY() - 5.0f, 10.0f, 10.0f);
    Look::glyph (g, Look::Glyph::use, bakeKey.reduced (3), session.cube.complete() ? Look::ink : Look::faint);
}
}
