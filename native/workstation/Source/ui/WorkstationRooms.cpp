#include "Workstation.h"
#include "Style.h"
#include <algorithm>
#include <cmath>

namespace ws
{
void Workstation::buildStrip()
{
    stripOrder.clear();
    for (int i = 0; i < (int) lib.frames.size(); ++i)
    {
        const auto& f = lib.frames[(size_t) i];
        if (! f.capture && ! floorOpen (f.group)) continue;
        stripOrder.push_back (i);
    }
    const int ref = body.corner[0];
    std::vector<double> key (lib.frames.size(), 0.0);
    for (const int i : stripOrder)
    {
        const auto& f = lib.frames[(size_t) i];
        if (sortNear && ref >= 0) key[(size_t) i] = i == ref ? -1.0 : leadCost (lib.frames[(size_t) ref].chord, f.chord);
        else key[(size_t) i] = Stitch::shapeOf (f.chord).root;
    }
    std::stable_sort (stripOrder.begin(), stripOrder.end(), [&] (int a, int b) { return key[(size_t) a] < key[(size_t) b]; });
    stripDirty = false;
    scanPos = juce::jlimit (0.0, std::max (0.0, (double) stripOrder.size() - 1.0), scanPos);
}

juce::Rectangle<float> Workstation::scanRect() const
{
    return { L.field.getX() + 60.0f, L.field.getY() + 40.0f, L.field.getWidth() - 120.0f, L.field.getHeight() - 110.0f };
}

double Workstation::scanAt (juce::Point<float> p) const
{
    const auto r = scanRect();
    if (stripOrder.size() < 2) return 0.0;
    return juce::jlimit (0.0, (double) stripOrder.size() - 1.0, (double) (p.x - r.getX()) / r.getWidth() * ((double) stripOrder.size() - 1.0));
}

Words Workstation::scanWords() const
{
    if (stripDirty) const_cast<Workstation*> (this)->buildStrip();
    if (stripOrder.empty()) return Words {};
    const int a = (int) std::floor (scanPos), b = std::min ((int) stripOrder.size() - 1, a + 1);
    const double t = scanPos - a;
    const auto& fa = lib.frames[(size_t) stripOrder[(size_t) a]];
    if (b == a || t < 1e-4) return fa.words;
    const auto& fb = lib.frames[(size_t) stripOrder[(size_t) b]];
    const auto led = leadTo (fa.chord, fb.chord);
    return pairMorph (compile (led.a, kDatumHz), compile (led.b, kDatumHz), t).words;
}

int Workstation::scanFrame() const
{
    if (stripOrder.empty()) return -1;
    return stripOrder[(size_t) juce::jlimit (0, (int) stripOrder.size() - 1, (int) std::round (scanPos))];
}

void Workstation::setScan (juce::Point<float> p)
{
    if (stripDirty) buildStrip();
    scanPos = scanAt (p);
    playFrame = scanFrame();
    playBody = false;
    const auto& f = lib.frames[(size_t) playFrame];
    const auto sh = Stitch::shapeOf (f.chord);
    status = f.name + "   " + noteName (sh.root) + "   " + intervalsOf (f.chord) + "   " + juce::String (sh.width, sh.width < 1.0 ? 2 : 1) + " st";
    redraw();
}

void Workstation::takeScan()
{
    if (stripOrder.empty()) return;
    int slot = selectedSlot;
    if (slot < 0) { for (int k = 0; k < 4; ++k) if (! chosen[(size_t) k]) { slot = k; break; } }
    if (slot < 0) slot = 3;
    const int a = (int) std::floor (scanPos);
    const double t = scanPos - a;
    int frame = scanFrame();
    if (t > 0.02 && t < 0.98) frame = lib.addNamed (scanWords(), "scan " + lib.frames[(size_t) stripOrder[(size_t) a]].name.substring (0, 12) + " > " + lib.frames[(size_t) stripOrder[(size_t) std::min ((int) stripOrder.size() - 1, a + 1)]].name.substring (0, 12), kGroups - 1, true);
    body.corner[(size_t) slot] = frame;
    chosen[(size_t) slot] = true;
    fillCorners();
    editCorner = slot;
    selectedSlot = -1;
    stripDirty = true;
    status = juce::String (kCornerNames[slot]) + "  " + lib.frames[(size_t) frame].name;
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
    const auto r = scanRect();
    const int n = (int) stripOrder.size();
    if (n == 0) return;
    const float mid = std::floor (r.getCentreY());
    g.setColour (kFrame);
    g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
    const auto xOf = [&] (double i) { return r.getX() + (float) (i / std::max (1.0, (double) n - 1.0)) * r.getWidth(); };
    int lastC = -999;
    for (int i = 0; i < n; ++i)
    {
        const auto& f = lib.frames[(size_t) stripOrder[(size_t) i]];
        const auto sh = Stitch::shapeOf (f.chord);
        const float x = xOf (i);
        g.setColour (f.capture ? kLive : hueOf (std::fmod (sh.root, 12.0) / 12.0));
        g.drawVerticalLine ((int) x, mid - 10.0f, mid + 10.0f);
        if (! sortNear)
        {
            const int c = (int) std::floor (sh.root / 12.0);
            if (c > lastC)
            {
                g.setColour (kDim);
                g.drawText (noteName (c * 12.0), (int) x - 14, (int) mid + 16, 28, 12, juce::Justification::centred);
                lastC = c;
            }
        }
    }
    if (sortNear)
    {
        g.setColour (kDim);
        g.drawText ("nearest", (int) r.getX(), (int) mid + 16, 60, 12, juce::Justification::centredLeft);
        g.drawText ("farthest", (int) r.getRight() - 60, (int) mid + 16, 60, 12, juce::Justification::centredRight);
    }
    for (int k = 0; k < 4; ++k)
    {
        if (body.corner[(size_t) k] < 0) continue;
        for (int i = 0; i < n; ++i)
            if (stripOrder[(size_t) i] == body.corner[(size_t) k])
            {
                const float x = xOf (i);
                g.setColour (kChosen);
                g.drawVerticalLine ((int) x, mid - 18.0f, mid + 18.0f);
                g.drawText (kCornerNames[k], (int) x - 20, (int) mid - 32, 40, 12, juce::Justification::centred);
            }
    }
    const float nx = xOf (scanPos);
    g.setColour (kLive);
    g.drawVerticalLine ((int) nx, r.getY(), r.getBottom());
    g.setColour (kText);
    g.drawText (status, (int) L.field.getX() + 8, (int) L.field.getY() + 10, (int) L.field.getWidth() - 16, 14, juce::Justification::centredLeft);
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
        const int x = right ? (int) r.getRight() - 300 : (int) r.getX() + 60, y = top ? (int) r.getY() - 34 : (int) r.getBottom() + 24;
        const auto name = body.corner[(size_t) i] >= 0 ? lib.frames[(size_t) body.corner[(size_t) i]].name : juce::String ("empty");
        g.setColour (body.corner[(size_t) i] >= 0 ? kText : kDim);
        g.drawText (name, x, y, 240, 12, right ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }
    g.setColour (compare ? kChosen : kDim);
    g.drawText (compare ? "comparing with " + juce::String (kCornerNames[0]) : "MORPH " + juce::String (body.morph, 3) + "   Q " + juce::String (body.q, 3), (int) r.getX(), (int) r.getBottom() + 44, (int) r.getWidth(), 12, juce::Justification::centred);
}
}
