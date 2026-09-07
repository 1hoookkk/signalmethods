#include "Mother.h"
#include "Look.h"

namespace hs
{
Mother::Mother (Session& s, const plot::Curves& c) : session (s), curves (c) {}

int Mother::pinOf (int index) { return (index / 4) * 4 + Session::kCornerPin[index % 4]; }

void Mother::layout (juce::Rectangle<int> r)
{
    area = r;
    const double reach = 0.55;
    const int size = std::max (48, (int) std::min ((area.getWidth() - 64) / (2.0 + reach), (area.getHeight() - 30) / (2.0 + reach)));
    const int shift = (int) std::round (size * reach);
    face[0] = { area.getX(), area.getY() + 22 + shift, 2 * size, 2 * size };
    face[1] = { area.getX() + shift, area.getY() + 22, 2 * size, 2 * size };
    for (int side = 0; side < 2; ++side)
        for (int n = 0; n < 4; ++n)
        {
            const size_t i = (size_t) (side * 4 + n);
            cell[i] = { face[(size_t) side].getX() + (n & 1) * size, face[(size_t) side].getY() + (n >> 1) * size, size, size };
            tag[i] = cell[i].withHeight (16);
            plot[i] = cell[i].withTrimmedTop (16).reduced (3);
        }
    depth = { face[1].getRight() + 22, face[1].getY(), 20, face[0].getBottom() - face[1].getY() - 30 };
    bakeKey = { depth.getX() - 4, face[0].getBottom() - 24, 28, 22 };
}

juce::Point<float> Mother::corner (int side, int n) const
{
    const auto f = face[(size_t) std::clamp (side, 0, 1)];
    return { (float) ((n & 1) ? f.getRight() : f.getX()), (float) ((n & 2) ? f.getBottom() : f.getY()) };
}

juce::Point<float> Mother::planePoint (double x, double y) const
{
    const auto a = juce::Point<float> ((float) (face[0].getX() + x * face[0].getWidth()), (float) (face[0].getBottom() - y * face[0].getHeight()));
    const auto b = juce::Point<float> ((float) (face[1].getX() + x * face[1].getWidth()), (float) (face[1].getBottom() - y * face[1].getHeight()));
    return a + (b - a) * (float) session.cube.z;
}

juce::Point<float> Mother::probePoint() const { return planePoint (session.cube.x, session.cube.y); }

std::pair<double, double> Mother::probeAt (juce::Point<int> p) const
{
    const double z = session.cube.z;
    const double x0 = face[0].getX() + z * (face[1].getX() - face[0].getX());
    const double y0 = face[0].getBottom() + z * (face[1].getBottom() - face[0].getBottom());
    return { std::clamp ((p.x - x0) / face[0].getWidth(), 0.0, 1.0), std::clamp ((y0 - p.y) / face[0].getHeight(), 0.0, 1.0) };
}

bool Mother::onVolume (juce::Point<int> p) const
{
    return face[0].getUnion (face[1]).contains (p);
}

int Mother::cellAt (juce::Point<int> p) const
{
    for (int i = 0; i < 4; ++i) if (cell[(size_t) i].contains (p)) return i;
    for (int i = 4; i < 8; ++i) if (cell[(size_t) i].contains (p)) return i;
    return -1;
}

int Mother::tagAt (juce::Point<int> p) const
{
    for (int i = 0; i < 4; ++i) if (tag[(size_t) i].contains (p)) return i;
    if (face[0].contains (p)) return -1;
    for (int i = 4; i < 8; ++i) if (tag[(size_t) i].contains (p)) return i;
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
    auto paintFace = [&] (int side, float alpha) {
        for (int n = 0; n < 4; ++n)
        {
            const int i = side * 4 + n;
            const int pin = pinOf (i);
            const int star = session.cube.pins[(size_t) pin];
            const bool lit = session.editingCube && session.editing == pin;
            juce::Graphics::ScopedSaveState saved (g);
            g.setOpacity (alpha);
            curves.cell (g, tag[(size_t) i], plot[(size_t) i], juce::String (pin + 1), star >= 0 ? session.stars[(size_t) star].name : juce::String(),
                         star >= 0 ? &session.stars[(size_t) star].words : nullptr, lit, dropping && cellAt (dropPoint) == i);
        }
    };
    paintFace (1, 0.55f);
    g.setColour (Look::rule.withAlpha (0.35f));
    for (int n = 0; n < 4; ++n) g.drawLine (juce::Line<float> (corner (0, n), corner (1, n)), 1.0f);
    juce::Path plane;
    plane.startNewSubPath (planePoint (0.0, 1.0));
    plane.lineTo (planePoint (1.0, 1.0));
    plane.lineTo (planePoint (1.0, 0.0));
    plane.lineTo (planePoint (0.0, 0.0));
    plane.closeSubPath();
    g.setColour (Look::blue.withAlpha (0.08f)); g.fillPath (plane);
    g.setColour (Look::blue.withAlpha (0.55f)); g.strokePath (plane, juce::PathStrokeType (1.0f));
    paintFace (0, 1.0f);
    {
        const auto p = probePoint();
        juce::Path diamond;
        diamond.addQuadrilateral (p.x, p.y - 8.0f, p.x + 8.0f, p.y, p.x, p.y + 8.0f, p.x - 8.0f, p.y);
        g.setColour (Look::panel); g.fillPath (diamond);
        g.setColour (Look::blue); g.strokePath (diamond, juce::PathStrokeType (1.8f));
        g.fillEllipse (p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f);
    }
    g.setColour (Look::faint); g.fillRect (depth.withX (depth.getCentreX()).withWidth (1));
    g.setColour (Look::blue); g.fillEllipse ((float) depth.getCentreX() - 5.0f, (float) (depth.getBottom() - depth.getHeight() * session.cube.z) - 5.0f, 10.0f, 10.0f);
    Look::glyph (g, Look::Glyph::use, bakeKey.reduced (3), session.cube.complete() ? Look::ink : Look::faint);
}
}
