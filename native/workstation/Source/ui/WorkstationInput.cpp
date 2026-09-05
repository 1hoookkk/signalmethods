#include "Workstation.h"
#include "Style.h"
#include <cmath>

namespace ws
{
int Workstation::anchorAt (juce::Point<float> p) const
{
    if (L.room != Room::frames || ! L.field.contains (p)) return -1;
    for (int i = (int) lib.anchors.size() - 1; i >= 0; --i)
        if (visible (i) && L.fromField (lib.anchors[(size_t) i].p).getDistanceFrom (p) < 6.0f) return i;
    return -1;
}

int Workstation::trayAt (juce::Point<float> p) const
{
    if (! L.tray.contains (p)) return -1;
    const int row = (int) ((p.y - L.tray.getY() - 6.0f) / 14.0f) + trayScroll;
    return row >= 0 && row < (int) trayRows.size() ? row : -1;
}

int Workstation::keyAt (juce::Point<float> p) const
{
    if (! L.timelineOpen) return -1;
    for (int i = 0; i < (int) tl.keys.size(); ++i)
        if (juce::Point<float> (L.tx (tl.keys[(size_t) i].t), L.tlAx.getCentreY()).getDistanceFrom (p) < 8.0f) return i;
    return -1;
}

void Workstation::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    for (const auto& k : keys)
        if (k.box.contains (p)) { press (k.id); return; }
    if (L.room == Room::sound)
    {
        if (L.tray.contains (p))
        {
            const int row = (int) ((p.y - L.tray.getY() - 6.0f) / 14.0f) + wavScroll;
            if (row >= 0 && row < (int) wavs.size()) loadWav (row);
            return;
        }
        if (p.y >= L.field.getBottom() && p.y <= L.field.getBottom() + 16.0f && p.x >= L.field.getX() && p.x <= L.field.getRight()) { mode = Mode::region; setRegion (p.x, true); return; }
        if (L.field.contains (p)) { mode = Mode::slice; setSlice (p.x); return; }
        if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
        return;
    }
    if (L.room == Room::edit)
    {
        if (openBar.contains (p)) { mode = Mode::open; setOpen (p.x); return; }
        if (L.field.contains (p) && body.corner[(size_t) editing] >= 0)
        {
            const auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
            for (int s = 0; s < kRows; ++s)
            {
                const auto r = L.stageRect (s);
                if (! r.contains (p)) continue;
                const auto& g = f.rows[(size_t) s];
                dragStage = s;
                if (g.zero && juce::Point<float> (L.sx (r, g.zHz), L.sy (r, sectionDb (f.words, s, g.zHz))).getDistanceFrom (p) < 10.0f) { mode = Mode::dragZero; return; }
                mode = Mode::dragPole;
                dragHandle (p);
                return;
            }
        }
        if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
        return;
    }
    if (const int t = trayAt (p); t >= 0)
    {
        const auto& row = trayRows[(size_t) t];
        if (row.header) { open[(size_t) row.group] = ! open[(size_t) row.group]; redraw(); return; }
        if (pickFor >= 0 && pickFor < (int) lib.anchors.size()) { lib.anchors[(size_t) pickFor].frame = row.frame; pickFor = -1; lib.retriangulate(); redraw(); return; }
        mode = Mode::dragFrame; dragFrame = row.frame; dragPos = p; dragStart = p; return;
    }
    if (const int a = anchorAt (p); a >= 0)
    {
        if (pairMode)
        {
            if (pairA < 0) { pairA = a; status = "pick the far anchor"; }
            else if (pairB < 0 && a != pairA) { pairB = a; pairT = 0.0; probe = lib.anchors[(size_t) pairA].p; status = ""; }
            else { pairA = a; pairB = -1; status = "pick the far anchor"; }
            playBody = false;
            redraw();
            return;
        }
        mode = Mode::dragAnchor; dragAnchor = a; dragPos = p; dragStart = p; return;
    }
    if (const int k = keyAt (p); k >= 0) { mode = Mode::dragKey; dragKey = k; dragStart = p; return; }
    if (L.timelineOpen && L.tlAx.contains (p)) { mode = Mode::scrub; scrubTo (p.x); return; }
    if (L.square.contains (p) && body.ready()) { mode = Mode::wheel; setWheel (p); return; }
    if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
    if (pairMode && pairA >= 0 && pairB >= 0 && L.field.contains (p)) { mode = Mode::pair; setPairT (p); return; }
    if (L.field.contains (p)) { mode = Mode::probe; setProbe (p); return; }
}

void Workstation::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position;
    switch (mode)
    {
        case Mode::probe: setProbe (p); break;
        case Mode::scrub: scrubTo (p.x); break;
        case Mode::pickHz: pickHz (p.x); break;
        case Mode::pair: setPairT (p); break;
        case Mode::wheel: setWheel (p); break;
        case Mode::dragPole:
        case Mode::dragZero: dragHandle (p); break;
        case Mode::open: setOpen (p.x); break;
        case Mode::slice: setSlice (p.x); break;
        case Mode::region: setRegion (p.x, false); break;
        case Mode::dragFrame:
        case Mode::dragAnchor: dragPos = p; redraw(); break;
        case Mode::dragKey: tl.keys[(size_t) dragKey].t = L.tAt (p.x); redraw(); break;
        case Mode::none: break;
    }
}

void Workstation::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.position;
    const bool moved = p.getDistanceFrom (dragStart) > 6.0f;
    if (mode == Mode::dragFrame && L.field.contains (p) && moved)
    {
        lib.anchors.push_back ({ dragFrame, L.toField (p) });
        lib.retriangulate();
    }
    else if (mode == Mode::dragAnchor && moved)
    {
        auto& a = lib.anchors[(size_t) dragAnchor];
        int cornerHit = -1;
        for (const auto& k : keys) if (k.id.startsWith ("corner") && k.box.contains (p)) cornerHit = k.id.substring (6).getIntValue();
        if (cornerHit >= 0) { body.corner[(size_t) cornerHit] = a.frame; editCorner = cornerHit; playBody = true; }
        else if (L.timelineOpen && L.tlAx.contains (p)) tl.keys.push_back ({ L.tAt (p.x), a.p, kData });
        else if (L.field.contains (p)) { a.p = L.toField (p); lib.retriangulate(); }
        else { lib.anchors.erase (lib.anchors.begin() + dragAnchor); lib.retriangulate(); }
    }
    else if (mode == Mode::dragAnchor && ! moved)
    {
        pickFor = pickFor == dragAnchor ? -1 : dragAnchor;
        status = pickFor >= 0 ? "picked, click a corner" : "";
    }
    else if (mode == Mode::dragKey && ! moved)
    {
        probe = tl.keys[(size_t) dragKey].p;
        tl.playhead = tl.keys[(size_t) dragKey].t;
        playBody = false;
    }
    mode = Mode::none;
    dragFrame = dragAnchor = dragKey = -1;
    redraw();
}

void Workstation::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position;
    if (L.tray.contains (p))
    {
        if (L.room == Room::sound) wavScroll = std::max (0, wavScroll - (int) std::round (w.deltaY * 30));
        else trayScroll = std::max (0, trayScroll - (int) std::round (w.deltaY * 30));
        redraw();
    }
    else if (L.room == Room::frames && L.field.contains (p))
    {
        const auto before = L.toField (p);
        L.zoom = juce::jlimit (0.5, 6.0, L.zoom * (w.deltaY > 0 ? 1.1 : 0.9));
        const auto after = L.fromField (before);
        L.pan[0] += p.x - after.x;
        L.pan[1] += p.y - after.y;
        redraw();
    }
}
}
