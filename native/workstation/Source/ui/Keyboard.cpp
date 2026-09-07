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
    g.setFont (Look::font (9.0f));
    for (int layer = 0; layer < 2; ++layer)
        for (int midi = 36; midi <= 71; ++midi)
        {
            const auto r = pianoKey (midi);
            const bool black = r.getHeight() < area.getHeight();
            if (black != (layer == 1)) continue;
            g.setColour (midi == session.note ? Look::blue : black ? Look::ground : Look::text);
            g.fillRect (r);
            g.setColour (black ? Look::faint : Look::ground); g.drawRect (r);
            if (midi % 12 == 0)
            {
                g.setColour (midi == session.note ? Look::panel : Look::faint);
                g.drawText ("C" + juce::String (midi / 12 - 1), r.withTrimmedTop (r.getHeight() - 14), juce::Justification::centred);
            }
        }
}
}
