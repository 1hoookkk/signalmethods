#include "Workstation.h"
#include "Style.h"
#include <algorithm>
#include <cmath>

namespace ws
{
namespace
{
double segmentT (juce::Point<float> p, juce::Point<float> a, juce::Point<float> b, float& dist)
{
    const double dx = b.x - a.x, dy = b.y - a.y, len2 = std::max (1e-6, dx * dx + dy * dy);
    const double t = juce::jlimit (0.0, 1.0, ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2);
    dist = juce::Point<float> ((float) (a.x + dx * t), (float) (a.y + dy * t)).getDistanceFrom (p);
    return t;
}

bool inverseBilinear (juce::Point<float> p, const juce::Point<float> c[4], double& m, double& q)
{
    m = 0.5; q = 0.5;
    for (int it = 0; it < 12; ++it)
    {
        const auto ex = [&] (double mm, double qq) { return juce::Point<float> ((float) ((c[0].x + (c[1].x - c[0].x) * mm) * (1.0 - qq) + (c[2].x + (c[3].x - c[2].x) * mm) * qq),
                                                                                (float) ((c[0].y + (c[1].y - c[0].y) * mm) * (1.0 - qq) + (c[2].y + (c[3].y - c[2].y) * mm) * qq)); };
        const auto at = ex (m, q);
        const double rx = p.x - at.x, ry = p.y - at.y;
        const double dmx = (c[1].x - c[0].x) * (1.0 - q) + (c[3].x - c[2].x) * q, dmy = (c[1].y - c[0].y) * (1.0 - q) + (c[3].y - c[2].y) * q;
        const double dqx = (c[2].x + (c[3].x - c[2].x) * m) - (c[0].x + (c[1].x - c[0].x) * m), dqy = (c[2].y + (c[3].y - c[2].y) * m) - (c[0].y + (c[1].y - c[0].y) * m);
        const double det = dmx * dqy - dqx * dmy;
        if (std::abs (det) < 1e-9) return false;
        m += (rx * dqy - dqx * ry) / det;
        q += (dmx * ry - rx * dmy) / det;
    }
    return m > -0.02 && m < 1.02 && q > -0.02 && q < 1.02;
}
}

int Workstation::anchorAt (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 3.0f;
    for (int i = 0; i < (int) st.items.size(); ++i)
    {
        const auto& it = st.items[(size_t) i];
        if (! floorOpen (it.group)) continue;
        const float d = view.project (it.p).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void Workstation::grab (int item)
{
    const auto& it = st.items[(size_t) item];
    Spot s;
    if (it.stub >= 0) s.stub = it.stub; else s.node = it.node;
    spot = s;
    playBody = false;
    const int stack = st.stackCount (it.p);
    status = st.itemName (it) + (stack > 1 ? "   +" + juce::String (stack - 1) + " more here" : juce::String());
}

int Workstation::nodeAt (juce::Point<float> p) const
{
    if (openFace < 0) return -1;
    int best = -1;
    float bestD = 8.0f;
    for (const int i : st.faces[(size_t) openFace].nodes)
    {
        const float d = view.project (st.nodes[(size_t) i].p).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

int Workstation::faceAt (juce::Point<float>) const
{
    return -1;
}

int Workstation::faceAtUnused (juce::Point<float> p) const
{
    int best = -1;
    float bestD = 7.0f;
    const auto near = st.neighbours (openFace);
    for (int i = 0; i < (int) st.faces.size(); ++i)
    {
        if (i == openFace || ! floorOpen (st.faces[(size_t) i].floor)) continue;
        if (openFace >= 0 && std::find (near.begin(), near.end(), i) == near.end()) continue;
        const float d = view.project (st.centres[(size_t) i]).getDistanceFrom (p);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void Workstation::openFilter (int face)
{
    openFace = face;
    spot = Spot { -1, -1, face, -1, 0.0, 0.5, 0.5 };
    playBody = false;
    status = st.faces[(size_t) face].name;
}

Spot Workstation::spotAt (juce::Point<float> p) const
{
    Spot s;
    if (L.room != Room::frames || ! L.field.contains (p)) return s;
    if (const int n = nodeAt (p); n >= 0) { s.node = n; return s; }
    for (int i = 0; i < (int) st.stubs.size(); ++i)
        if (floorOpen (st.stubs[(size_t) i].floor) && view.project (st.stubs[(size_t) i].p).getDistanceFrom (p) < 7.0f) { s.stub = i; return s; }
    float bestD = 5.0f;
    for (int i = 0; i < (int) st.edges.size(); ++i)
    {
        const auto& e = st.edges[(size_t) i];
        if (e.body != openFace) continue;
        float d = 0.0f;
        const double t = segmentT (p, view.project (st.nodes[(size_t) e.a].p), view.project (st.nodes[(size_t) e.b].p), d);
        if (d < bestD) { bestD = d; s.edge = i; s.t = t; }
    }
    if (s.edge >= 0) return s;
    if (openFace < 0)
    {
        double x = 0.0, y = 0.0;
        if (view.unproject (p, freeZ, x, y) && x > view.lo.x && x < view.hi.x && y > view.lo.y && y < view.hi.y) { s.free = true; s.x = x; s.y = y; s.z = freeZ; }
        return s;
    }
    double bestDepth = 1e18;
    for (int i = 0; i < (int) st.faces.size(); ++i)
    {
        const auto& f = st.faces[(size_t) i];
        if (i != openFace || f.nodes.size() < 4) continue;
        juce::Point<float> c[4];
        double depth = 0.0;
        for (int k = 0; k < 4; ++k) { c[k] = view.project (st.nodes[(size_t) f.nodes[(size_t) k]].p); depth += view.depth (st.nodes[(size_t) f.nodes[(size_t) k]].p); }
        double m = 0.0, q = 0.0;
        if (inverseBilinear (p, c, m, q) && depth < bestDepth) { bestDepth = depth; s.face = i; s.m = juce::jlimit (0.0, 1.0, m); s.q = juce::jlimit (0.0, 1.0, q); }
    }
    return s;
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
        if (k.box.contains (p))
        {
            if (k.id == "compare") { compare = true; mode = Mode::holdCompare; redraw(); return; }
            press (k.id);
            return;
        }
    if (L.room == Room::morph)
    {
        if (padRect().contains (p) && body.ready())
        {
            const auto r = padRect();
            padPressM = body.morph; padPressQ = body.q;
            padPressM0 = (p.x - r.getX()) / r.getWidth(); padPressQ0 = (r.getBottom() - p.y) / r.getHeight();
            mode = Mode::pad;
            if (! fine) setPad (p); else { playBody = true; compare = false; redraw(); }
            return;
        }
        if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
        return;
    }
    if (L.room == Room::frames)
    {
        if (L.field.contains (p) && p.y > L.field.getY() + 30.0f && p.y < L.field.getBottom() - 30.0f) { mode = Mode::scan; setScan (p); return; }
        if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
        return;
    }
    if (L.room == Room::sound)
    {
        if (L.tray.contains (p))
        {
            const int row = (int) ((p.y - L.tray.getY() - 6.0f) / 14.0f) + wavScroll;
            if (row >= 0 && row < (int) wavs.size()) loadWav (row);
            return;
        }
        if (L.field.contains (p) && sound.seconds > 0.0)
        {
            const float xa = L.field.getX() + (float) (std::min (sound.regionA, sound.regionB) / sound.seconds) * L.field.getWidth();
            const float xb = L.field.getX() + (float) (std::max (sound.regionA, sound.regionB) / sound.seconds) * L.field.getWidth();
            if (std::abs (p.x - xa) < 8.0f) { regionDragEnd = 0; mode = Mode::region; setRegion (p.x, false); return; }
            if (std::abs (p.x - xb) < 8.0f) { regionDragEnd = 1; mode = Mode::region; setRegion (p.x, false); return; }
        }
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
        if (row.header)
        {
            if (p.x >= L.tray.getRight() - 52.0f) groupMean (row.group);
            else { open[(size_t) row.group] = ! open[(size_t) row.group]; redraw(); }
            return;
        }
        mode = Mode::dragFrame; dragFrame = row.frame; dragPos = p; dragStart = p; return;
    }
    if (const int a = anchorAt (p); a >= 0 && L.field.contains (p) && L.room == Room::frames && ! e.mods.isRightButtonDown() && ! e.mods.isAltDown())
    {
        const auto& it = st.items[(size_t) a];
        if (pairMode && it.node >= 0)
        {
            if (pairA < 0) { pairA = it.node; status = "pick the far frame"; }
            else if (pairB < 0 && it.node != pairA) { pairB = it.node; pairTo (0.0); }
            else { pairA = it.node; pairB = -1; status = "pick the far frame"; }
            playBody = false;
            redraw();
            return;
        }
        Spot ds;
        if (it.stub >= 0) ds.stub = it.stub; else ds.node = it.node;
        grab (a);
        mode = Mode::dragSpot; dragSpot = ds; dragPos = p; dragStart = p; redraw(); return;
    }
    if (const int f = faceAt (p); f >= 0 && L.field.contains (p) && ! pairLive()) { openFilter (f); redraw(); return; }
    if (const int k = keyAt (p); k >= 0) { mode = Mode::dragKey; dragKey = k; dragStart = p; return; }
    if (L.timelineOpen && L.tlAx.contains (p)) { mode = Mode::scrub; scrubTo (p.x); return; }
    if (L.outer.contains (p) && body.ready()) { mode = Mode::wheel; setWheel (p); return; }
    if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
    if (pairLive() && L.field.contains (p)) { mode = Mode::pair; setPairT (p); return; }
    if (L.field.contains (p))
    {
        if (e.mods.isRightButtonDown() || e.mods.isAltDown()) { mode = Mode::orbit; dragStart = p; orbitAz = view.az; orbitEl = view.el; return; }
        if (e.mods.isShiftDown() && openFace < 0) { mode = Mode::lift; dragStart = p; if (! spot || ! spot->free) { Spot fs; fs.free = true; fs.z = freeZ; view.unproject (p, freeZ, fs.x, fs.y); spot = fs; } return; }
        const auto s = spotAt (p);
        if (s.stub >= 0) { mode = Mode::dragSpot; dragSpot = s; dragPos = p; dragStart = p; return; }
        if (s.valid()) { mode = Mode::surface; setSurface (p); return; }
        mode = Mode::orbit; dragStart = p; orbitAz = view.az; orbitEl = view.el; return;
    }
}

void Workstation::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position;
    switch (mode)
    {
        case Mode::pad: setPad (p); break;
        case Mode::scan: setScan (p); break;
        case Mode::holdCompare: break;
        case Mode::surface: setSurface (p); break;
        case Mode::lift:
            if (spot && spot->free)
            {
                spot->z = juce::jlimit (0.0, 1.0, spot->z - (p.y - dragStart.y) / (view.scale() * 1.0));
                dragStart = p;
                freeZ = spot->z;
                playBody = false;
                status = st.nameOf (*spot, open);
                redraw();
            }
            break;
        case Mode::orbit:
            view.az = orbitAz + (p.x - dragStart.x) * 0.4;
            view.el = juce::jlimit (-89.0, 89.0, orbitEl + (p.y - dragStart.y) * 0.4);
            redraw();
            break;
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
        case Mode::dragSpot: dragPos = p; redraw(); break;
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
        const auto& f = lib.frames[(size_t) dragFrame];
        const int near = nodeAt (p) >= 0 ? nodeAt (p) : st.nearestNode (f.words);
        const int stub = st.addStub (f.name, f.words, near, juce::jlimit (0, kGroups - 1, f.group));
        st.stubs[(size_t) stub].frame = dragFrame;
    }
    else if (mode == Mode::dragSpot && dragSpot && moved)
    {
        int cornerHit = -1;
        for (const auto& k : keys) if (k.id.startsWith ("corner") && k.box.contains (p)) cornerHit = k.id.substring (6).getIntValue();
        if (cornerHit >= 0) { body.corner[(size_t) cornerHit] = frameFor (*dragSpot); editCorner = cornerHit; playBody = true; }
        else if (L.timelineOpen && L.tlAx.contains (p)) tl.keys.push_back ({ L.tAt (p.x), *dragSpot });
        else if (dragSpot->stub >= 0 && ! L.field.contains (p)) st.stubs.erase (st.stubs.begin() + dragSpot->stub);
    }
    else if (mode == Mode::dragSpot && dragSpot && ! moved)
    {
        picked = dragSpot;
        spot = dragSpot;
        playBody = false;
    }
    else if (mode == Mode::orbit && ! moved && L.field.contains (p))
    {
        openFace = -1;
        picked.reset();
        status = "";
    }
    else if (mode == Mode::dragKey && ! moved)
    {
        spot = tl.keys[(size_t) dragKey].spot;
        tl.playhead = tl.keys[(size_t) dragKey].t;
        playBody = false;
    }
    if (mode == Mode::holdCompare) compare = false;
    mode = Mode::none;
    dragFrame = dragKey = -1;
    dragSpot.reset();
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
        if (stripDirty) buildStrip();
        scanPos = juce::jlimit (0.0, std::max (0.0, (double) stripOrder.size() - 1.0), scanPos + (w.deltaY > 0 ? 0.1 : -0.1));
        playFrame = scanFrame();
        playBody = false;
        redraw();
    }
    else if (false)
    {
        const double k = w.deltaY > 0 ? 1.1 : 1.0 / 1.1;
        view.zoom = juce::jlimit (0.3, 12.0, view.zoom * k);
        view.panX = (view.panX - (p.x - L.field.getCentreX())) * k + (p.x - L.field.getCentreX());
        view.panY = (view.panY - (p.y - L.field.getCentreY())) * k + (p.y - L.field.getCentreY());
        redraw();
    }
}
}
