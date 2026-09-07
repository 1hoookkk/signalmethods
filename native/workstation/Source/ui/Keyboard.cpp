#include "Keyboard.h"
#include "Look.h"

namespace hs
{
Keyboard::Keyboard (Session& s) : session (s) {}

void Keyboard::layout (juce::Rectangle<int> r) { area = r; }

juce::Rectangle<int> Keyboard::pianoKey (int midi) const
{
    if (midi < 36 || midi > 71) return {};
    static const int offsets[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    const int pitch = midi % 12, white = (midi - 36) / 12 * 7 + offsets[pitch];
    const bool black = pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10;
    const int leftEdge = area.getX() + area.getWidth() * white / 21;
    const int rightEdge = area.getX() + area.getWidth() * (white + 1) / 21;
    if (black) return { rightEdge - (rightEdge - leftEdge) / 3, area.getY(), std::max (4, (rightEdge - leftEdge) * 2 / 3), area.getHeight() * 3 / 5 };
    return { leftEdge, area.getY(), rightEdge - leftEdge, area.getHeight() };
}

int Keyboard::noteAt (juce::Point<int> p) const
{
    for (int midi = 36; midi <= 71; ++midi)
        if (pianoKey (midi).getHeight() < area.getHeight() && pianoKey (midi).contains (p)) return midi;
    for (int midi = 36; midi <= 71; ++midi)
        if (pianoKey (midi).contains (p)) return midi;
    return -1;
}

void Keyboard::paint (juce::Graphics& g) const
{
    const int base = area.getBottom() - 1;
    g.setColour (Look::faint);
    g.fillRect (area.getX(), base, area.getWidth(), 1);
    for (int white = 0; white <= 21; ++white)
    {
        const int x = std::min (area.getX() + area.getWidth() * white / 21, area.getRight() - 1);
        g.fillRect (x, base - 6, 1, 6);
    }
    for (int midi = 36; midi <= 71; ++midi)
    {
        const auto r = pianoKey (midi);
        if (r.getHeight() >= area.getHeight()) continue;
        g.fillRect (r.getCentreX(), area.getY() + 6, 1, 3);
    }
    g.setFont (Look::font (9.0f));
    for (int midi = 36; midi <= 71; midi += 12)
    {
        const auto r = pianoKey (midi);
        g.setColour (Look::faint);
        g.fillRect (r.getX(), base - 12, 1, 12);
        g.setColour (Look::dim);
        g.drawText ("C" + juce::String (midi / 12 - 1), r.getX() + 3, base - 13, 22, 11, juce::Justification::centredLeft);
    }
    for (int midi = 36; midi <= 71; ++midi)
    {
        if (midi != session.heldNote && ! (session.playing && midi == session.note)) continue;
        const auto r = pianoKey (midi);
        g.setColour (r.getHeight() < area.getHeight() ? Look::blue : Look::ink);
        g.fillRect (r.getCentreX() - 1, base - 20, 3, 20);
    }
}
}
