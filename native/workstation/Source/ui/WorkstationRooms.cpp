#include "Workstation.h"
#include "Style.h"
#include <algorithm>
#include <cmath>

namespace ws
{
namespace
{
constexpr float kRowH = 16.0f;
constexpr float kStripTop = 30.0f;
}

void Workstation::buildStrip()
{
    stripOrder.clear();
    for (int i = 0; i < (int) lib.frames.size(); ++i)
    {
        const auto& f = lib.frames[(size_t) i];
        if (! f.capture && ! floorOpen (f.group)) continue;
        stripOrder.push_back (i);
    }
    const int ref = body.corner[0] >= 0 ? body.corner[0] : playFrame;
    std::vector<double> key (lib.frames.size(), 0.0);
    for (const int i : stripOrder)
    {
        const auto& f = lib.frames[(size_t) i];
        if (sortNear && ref >= 0) key[(size_t) i] = i == ref ? -1.0 : leadCost (lib.frames[(size_t) ref].chord, f.chord);
        else key[(size_t) i] = Stitch::shapeOf (f.chord).root;
    }
    std::stable_sort (stripOrder.begin(), stripOrder.end(), [&] (int a, int b)
    {
        const bool ca = lib.frames[(size_t) a].capture, cb = lib.frames[(size_t) b].capture;
        if (ca != cb) return ca;
        return key[(size_t) a] < key[(size_t) b];
    });
    stripDirty = false;
}

int Workstation::stripAt (juce::Point<float> p) const
{
    if (L.room != Room::frames || ! L.field.contains (p) || p.y < L.field.getY() + kStripTop) return -1;
    const int row = (int) ((p.y - L.field.getY() - kStripTop) / kRowH) + stripScroll;
    return row >= 0 && row < (int) stripOrder.size() ? stripOrder[(size_t) row] : -1;
}

void Workstation::chooseFrame (int frame)
{
    playFrame = frame;
    playBody = false;
    if (selectedSlot >= 0)
    {
        body.corner[(size_t) selectedSlot] = frame;
        chosen[(size_t) selectedSlot] = true;
        fillCorners();
        editCorner = selectedSlot;
        stripDirty = true;
    }
    status = lib.frames[(size_t) frame].name + "   " + noteName (Stitch::shapeOf (lib.frames[(size_t) frame].chord).root) + "   " + intervalsOf (lib.frames[(size_t) frame].chord);
}

void Workstation::fillCorners()
{
    const bool none = ! (chosen[0] || chosen[1] || chosen[2] || chosen[3]);
    if (none) for (int k = 0; k < 4; ++k) if (body.corner[(size_t) k] >= 0) chosen[(size_t) k] = true;
    if (! chosen[0])
        for (int k = 1; k < 4; ++k) if (chosen[(size_t) k]) { body.corner[0] = body.corner[(size_t) k]; break; }
    if (body.corner[0] < 0) return;
    if (! chosen[1]) body.corner[1] = body.corner[0];
    if (! chosen[2]) body.corner[2] = body.corner[0];
    if (! chosen[3]) body.corner[3] = body.corner[1];
}

void Workstation::paintStrip (Canvas& g)
{
    if (stripDirty) buildStrip();
    const auto f = L.field;
    const int x0 = (int) f.getX() + 8;
    g.setColour (kDim);
    g.drawText ("frame", x0, (int) f.getY() + 8, 220, 14, juce::Justification::centredLeft);
    g.drawText ("root", x0 + 240, (int) f.getY() + 8, 60, 14, juce::Justification::centredLeft);
    g.drawText ("intervals", x0 + 310, (int) f.getY() + 8, 160, 14, juce::Justification::centredLeft);
    g.drawText ("width", x0 + 480, (int) f.getY() + 8, 60, 14, juce::Justification::centredLeft);
    g.drawText ("source", x0 + 550, (int) f.getY() + 8, 100, 14, juce::Justification::centredLeft);
    g.setColour (kLine);
    g.drawHorizontalLine ((int) (f.getY() + kStripTop - 2.0f), f.getX() + 1.0f, f.getRight() - 1.0f);
    const int maxRows = (int) ((f.getHeight() - kStripTop - 30.0f) / kRowH);
    stripScroll = juce::jlimit (0, std::max (0, (int) stripOrder.size() - maxRows), stripScroll);
    for (int r = 0; r < maxRows && r + stripScroll < (int) stripOrder.size(); ++r)
    {
        const int i = stripOrder[(size_t) (r + stripScroll)];
        const auto& fr = lib.frames[(size_t) i];
        const int y = (int) (f.getY() + kStripTop + r * kRowH);
        int slot = -1;
        for (int k = 0; k < 4; ++k) if (body.corner[(size_t) k] == i) { slot = k; break; }
        if (i == playFrame) { g.setColour (kKey); g.fillRect (juce::Rectangle<int> ((int) f.getX() + 1, y, (int) f.getWidth() - 2, (int) kRowH)); }
        const auto shape = Stitch::shapeOf (fr.chord);
        g.setColour (fr.capture ? kLive : hueOf (std::fmod (shape.root, 12.0) / 12.0));
        g.fillEllipse ((float) x0 + 2.0f, (float) y + 5.5f, 5.0f, 5.0f);
        g.setColour (slot >= 0 ? kChosen : kText);
        g.drawText ((slot >= 0 ? juce::String (kCornerNames[slot]) + "  " : juce::String()) + fr.name, x0 + 12, y, 226, (int) kRowH, juce::Justification::centredLeft);
        g.setColour (kText);
        g.drawText (shape.any ? noteName (shape.root) : "-", x0 + 240, y, 66, (int) kRowH, juce::Justification::centredLeft);
        g.setColour (kDim);
        g.drawText (intervalsOf (fr.chord), x0 + 310, y, 166, (int) kRowH, juce::Justification::centredLeft);
        g.drawText (shape.any ? juce::String (shape.width, shape.width < 1.0 ? 2 : 1) + " st" : "-", x0 + 480, y, 66, (int) kRowH, juce::Justification::centredLeft);
        g.drawText (fr.capture ? "capture" : juce::String (kGroupNames[juce::jlimit (0, kGroups - 1, fr.group)]).toLowerCase(), x0 + 550, y, 100, (int) kRowH, juce::Justification::centredLeft);
    }
}

juce::Rectangle<float> Workstation::padRect() const
{
    const auto f = L.field;
    const float side = std::floor (std::min (f.getWidth(), f.getHeight()) - 120.0f);
    return { std::floor (f.getCentreX() - side * 0.5f), std::floor (f.getCentreY() - side * 0.5f + 10.0f), side, side };
}

void Workstation::setPad (juce::Point<float> p)
{
    const auto r = padRect();
    double m = (p.x - r.getX()) / r.getWidth(), q = (r.getBottom() - p.y) / r.getHeight();
    if (fine)
    {
        m = padPressM + (m - padPressM0) * 0.1;
        q = padPressQ + (q - padPressQ0) * 0.1;
    }
    body.morph = juce::jlimit (0.0, 1.0, m);
    body.q = juce::jlimit (0.0, 1.0, q);
    playBody = true;
    compare = false;
    redraw();
}

void Workstation::paintPad (Canvas& g)
{
    const auto r = padRect();
    g.setColour (kFrame);
    g.drawRect (px (r), 1);
    g.setColour (kLine);
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 4.0f), r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4.0f), r.getX(), r.getRight());
    }
    for (int i = 0; i < 4; ++i)
    {
        const bool right = (i & 1) != 0, top = (i & 2) != 0;
        const int x = right ? (int) r.getRight() - 236 : (int) r.getX() + 4, y = top ? (int) r.getY() - 18 : (int) r.getBottom() + 6;
        const auto name = body.corner[(size_t) i] >= 0 ? lib.frames[(size_t) body.corner[(size_t) i]].name : juce::String("empty");
        g.setColour (body.corner[(size_t) i] >= 0 ? kText : kDim);
        g.drawText (juce::String (kCornerNames[i]) + "   " + name, x, y, 232, 12, right ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }
    g.setColour (compare ? kChosen : kDim);
    g.drawText (compare ? "comparing with " + juce::String (kCornerNames[0]) : "MORPH " + juce::String (body.morph, 3) + "   Q " + juce::String (body.q, 3), (int) r.getX(), (int) r.getBottom() + 26, (int) r.getWidth(), 12, juce::Justification::centred);
}
}
