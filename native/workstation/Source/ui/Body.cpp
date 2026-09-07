#include "Body.h"
#include "Look.h"

namespace hs
{
Body::Body (Session& s, const plot::Curves& c) : session (s), curves (c) {}

void Body::layout (juce::Rectangle<int> r)
{
    area = r;
    const int half = area.getWidth() / 2;
    for (int n = 0; n < 4; ++n)
    {
        box[(size_t) n] = { area.getX() + (n & 1) * half, area.getY() + (n >> 1) * half, half, half };
        tag[(size_t) n] = box[(size_t) n].withHeight (16);
        plot[(size_t) n] = box[(size_t) n].withTrimmedTop (16).reduced (3);
    }
}

juce::Point<float> Body::puckPoint() const
{
    return { (float) (area.getX() + session.quad.morph / 100.0 * area.getWidth()), (float) (area.getBottom() - session.quad.q / 100.0 * area.getHeight()) };
}

int Body::cornerAt (juce::Point<int> p) const
{
    for (int n = 0; n < 4; ++n) if (box[(size_t) n].expanded (3).contains (p)) return n;
    return -1;
}

int Body::tagAt (juce::Point<int> p) const
{
    for (int n = 0; n < 4; ++n) if (tag[(size_t) n].contains (p)) return n;
    return -1;
}

void Body::paint (juce::Graphics& g, int dropCorner) const
{
    const auto corners = cornersOf (session.quad, session.stars);
    for (int n = 0; n < 4; ++n)
    {
        const int pin = session.quad.pins[(size_t) Session::kCornerPin[n]];
        const bool lit = session.anchorTarget < 0 && session.auditioning == -1 && session.onCorner() && session.target() == n;
        curves.cell (g, tag[(size_t) n], plot[(size_t) n], juce::String::charToString (Session::kCornerLetters[n]), session.cornerName (n),
                     pin >= 0 ? &corners[(size_t) Session::kCornerPin[n]] : nullptr, lit, dropCorner == n);
    }
    if (session.quad.complete() && session.auditioning == -1)
    {
        const auto pk = puckPoint();
        juce::Path diamond;
        diamond.addQuadrilateral (pk.x, pk.y - 8.0f, pk.x + 8.0f, pk.y, pk.x, pk.y + 8.0f, pk.x - 8.0f, pk.y);
        g.setColour (Look::panel); g.fillPath (diamond);
        g.setColour (Look::blue); g.strokePath (diamond, juce::PathStrokeType (1.8f));
        g.fillEllipse (pk.x - 2.0f, pk.y - 2.0f, 4.0f, 4.0f);
    }
}
}
