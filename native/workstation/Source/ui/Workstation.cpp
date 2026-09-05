#include "Workstation.h"
#include "../render/SvgRenderer.h"
#include <cmath>

namespace ws
{
namespace
{
juce::Font mono (float h) { return juce::Font (juce::FontOptions ("Consolas", h, juce::Font::plain)); }
const juce::Colour kLine (0xff383838), kText (0xffc8c8c8), kOn (0xff00e5ff), kDim (0xff7a7a7a), kKey (0xff161616), kKeyLine (0xff4a4a4a);
juce::Rectangle<int> px (juce::Rectangle<float> r) { return r.toNearestInt(); }
}

Workstation::Workstation (Library& library, bool useGL) : lib (library), gl (useGL)
{
    setOpaque (true);
    setSize (1280, 800);
    L.duration = tl.duration;
    if (gl)
    {
        ctx.setRenderer (this);
        ctx.setComponentPaintingEnabled (true);
        ctx.setContinuousRepainting (false);
        ctx.attachTo (*this);
    }
}

Workstation::~Workstation()
{
    if (gl) ctx.detach();
}

void Workstation::demo()
{
    lib.axisX = 6;
    lib.axisY = 7;
    lib.sort();
    lit = 0;
    pairMode = true; pairA = 52; pairB = 53; pairT = 0.35;
    const auto& pa = lib.anchors[(size_t) pairA].p;
    const auto& pb = lib.anchors[(size_t) pairB].p;
    tl.keys = { { 0.5, { 0.3, 0.35 }, juce::Colour (0xff00ccff) }, { 3.0, { 0.55, 0.6 }, juce::Colour (0xffff00ff) }, { 6.5, { 0.4, 0.75 }, juce::Colours::yellow } };
    tl.playhead = 2.1;
    probe = std::array<double, 2> { pa[0] + (pb[0] - pa[0]) * pairT, pa[1] + (pb[1] - pa[1]) * pairT };
    body.corner = { 52, 53, 54, 55 };
    body.rowOn[4] = false;
    body.morph = 0.35;
    body.q = 0.6;
    playBody = true;
    openEditor (1);
}

bool Workstation::exportBody (const juce::File& file)
{
    const bool ok = body.exportTo (lib.frames, file);
    status = ok ? "wrote " + file.getFileName() : "body needs four corners";
    redraw();
    return ok;
}

Words Workstation::playingWords() const
{
    if (playBody && body.ready()) return body.wheelWords (lib.frames);
    if (const auto b = current()) return lib.wordsOf (*b);
    return Words {};
}

void Workstation::setWheel (juce::Point<float> p)
{
    body.morph = juce::jlimit (0.0, 1.0, (double) (p.x - L.square.getX()) / L.square.getWidth());
    body.q = juce::jlimit (0.0, 1.0, (double) (L.square.getBottom() - p.y) / L.square.getHeight());
    playBody = true;
    redraw();
}

void Workstation::openEditor (int corner)
{
    if (body.corner[(size_t) corner] < 0) { status = "corner is empty"; redraw(); return; }
    const auto& src = lib.frames[(size_t) body.corner[(size_t) corner]];
    if (! src.capture)
    {
        Frame f = src;
        f.capture = true;
        f.name = "edit " + src.name;
        f.group = kGroups - 1;
        lib.frames.push_back (f);
        body.corner[(size_t) corner] = (int) lib.frames.size() - 1;
    }
    editing = corner;
    playBody = true;
    status = "editing " + lib.frames[(size_t) body.corner[(size_t) corner]].name;
    redraw();
}

void Workstation::dragHandle (juce::Point<float> p)
{
    auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
    const auto r = L.stageRect (dragStage);
    const double hz = L.hzAt (r, p.x);
    const double db = L.dbAt (r, p.y);
    if (mode == Mode::dragPole)
    {
        setPole (f.words, dragStage, hz, 1.0 - std::pow (10.0, -juce::jlimit (0.0, 60.0, db + 30.0) / 20.0));
        if (lockRow[(size_t) dragStage] && f.rows[(size_t) dragStage].zero) setZero (f.words, dragStage, hz, f.rows[(size_t) dragStage].zR);
    }
    else setZero (f.words, dragStage, hz, 1.0 - std::pow (10.0, -juce::jlimit (0.0, 60.0, 30.0 - db) / 20.0));
    measure (f);
    redraw();
}

int Workstation::f1Row (const Frame& f) const
{
    int best = -1;
    double lo = 1e9;
    for (int s = 0; s < kRows; ++s)
        if (f.rows[(size_t) s].pole && f.rows[(size_t) s].pR > 0.85 && f.rows[(size_t) s].pHz < lo) { lo = f.rows[(size_t) s].pHz; best = s; }
    return best;
}

void Workstation::setOpen (float x)
{
    openAmount = juce::jlimit (0.0, 1.0, (double) (x - openBar.getX()) / openBar.getWidth());
    auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
    const int row = f1Row (f);
    if (row < 0) { status = "no F1 pole in this frame"; redraw(); return; }
    const double f1 = 250.0 * std::pow (900.0 / 250.0, openAmount);
    const double b1 = 60.0 + 60.0 * openAmount;
    setPole (f.words, row, f1, std::exp (-juce::MathConstants<double>::pi * b1 / kDatumHz));
    measure (f);
    status = "F1 " + juce::String ((int) std::round (f1)) + " Hz  B1 " + juce::String ((int) std::round (b1)) + " Hz  row " + juce::String (row + 1);
    redraw();
}

void Workstation::sharpenQ()
{
    if (body.corner[0] < 0 || body.corner[1] < 0) { status = "fill M0 Q0 and M1 Q0 first"; redraw(); return; }
    for (int i = 0; i < 2; ++i)
    {
        Frame f = lib.frames[(size_t) body.corner[(size_t) i]];
        f.capture = true;
        f.group = kGroups - 1;
        f.name = "sharp " + f.name.replace (" Q0", " Q1");
        sharpen (f.words, 0.25);
        measure (f);
        lib.frames.push_back (f);
        body.corner[(size_t) (2 + i)] = (int) lib.frames.size() - 1;
    }
    playBody = true;
    status = "Q corners made from the M corners, radius kept to a quarter of its distance from one";
    redraw();
}

void Workstation::ceiling()
{
    if (editing < 0) return;
    auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
    setZero (f.words, kRows - 1, 20000.0, 0.9995);
    measure (f);
    status = "row 6 zero parked at the ceiling";
    redraw();
}

void Workstation::assignCorner (int i)
{
    editCorner = i;
    if (copyFrom >= 0 && body.corner[(size_t) copyFrom] >= 0)
    {
        Frame f = lib.frames[(size_t) body.corner[(size_t) copyFrom]];
        f.capture = true;
        f.group = kGroups - 1;
        f.name = "copy " + f.name;
        lib.frames.push_back (f);
        body.corner[(size_t) i] = (int) lib.frames.size() - 1;
        copyFrom = -1;
        playBody = true;
        status = "corner " + juce::String (i) + " = " + f.name;
        redraw();
        return;
    }
    if (pickFor >= 0 && pickFor < (int) lib.anchors.size())
    {
        body.corner[(size_t) i] = lib.anchors[(size_t) pickFor].frame;
        pickFor = -1;
    }
    else if (pairMode && pairA >= 0 && pairB >= 0)
    {
        const int idx = lib.addCapture (lib.wordsOf (*current()), *probe);
        body.corner[(size_t) i] = idx;
    }
    else if (const auto b = current())
    {
        int single = -1;
        for (int k = 0; k < 3; ++k) if (b->w[(size_t) k] > 0.999) single = lib.anchors[(size_t) b->anchors[(size_t) k]].frame;
        body.corner[(size_t) i] = single >= 0 ? single : lib.addCapture (lib.wordsOf (*b), *probe);
    }
    else return;
    playBody = true;
    status = "corner " + juce::String (i) + " = " + lib.frames[(size_t) body.corner[(size_t) i]].name;
    redraw();
}

void Workstation::redraw()
{
    if (gl) ctx.triggerRepaint();
    repaint();
}

void Workstation::timerCallback()
{
    double t = playFrom + (juce::Time::getMillisecondCounterHiRes() - playT0) / 1000.0;
    if (t >= tl.duration)
    {
        if (tl.loop) { playT0 = juce::Time::getMillisecondCounterHiRes(); playFrom = 0.0; t = 0.0; }
        else { tl.playing = false; stopTimer(); t = tl.duration; }
    }
    tl.playhead = t;
    if (const auto p = tl.pathAt (t)) probe = p;
    redraw();
}

void Workstation::layoutKeys()
{
    L.compute ((float) getWidth(), (float) getHeight());
    keys.clear();
    const float kx = L.field.getX() + 48.0f, kw = 62.0f, kh = 14.0f;
    for (int i = 0; i < kMeasures; ++i)
    {
        keys.push_back ({ "ax" + juce::String (i), kMeasureNames[i], { kx + i * (kw + 3.0f), 6.0f, kw, kh }, lib.axisX == i });
        keys.push_back ({ "ay" + juce::String (i), kMeasureNames[i], { kx + i * (kw + 3.0f), 22.0f, kw, kh }, lib.axisY == i });
    }
    const float rightKeys = L.field.getRight();
    keys.push_back ({ "sort", "SORT", { rightKeys - 56.0f, 6.0f, 56.0f, kh }, false });
    keys.push_back ({ "clear", "CLEAR", { rightKeys - 56.0f, 22.0f, 56.0f, kh }, false });
    for (int i = 0; i < kGroups; ++i) keys.push_back ({ "lit" + juce::String (i), kGroupNames[i], { 8.0f, 8.0f + i * 16.0f, L.groups.getWidth() - 16.0f, kh }, lit == i });
    keys.push_back ({ "capture", "CAPTURE", { L.info.getX() + 8.0f, L.info.getY() + 8.0f, 76.0f, kh }, false });
    static const char* cornerNames[] = { "M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1" };
    const auto sq = L.square;
    for (int i = 0; i < 4; ++i)
    {
        const float x = (i & 1) ? sq.getRight() + 8.0f : sq.getX() - 52.0f, y = (i & 2) ? sq.getY() : sq.getBottom() - kh;
        keys.push_back ({ "corner" + juce::String (i), cornerNames[i], { x, y, 44.0f, kh }, body.corner[(size_t) i] >= 0 });
    }
    const float bx = sq.getRight() + 64.0f;
    for (int s = 0; s < kRows; ++s)
        keys.push_back ({ "row" + juce::String (s), juce::String (s + 1), { bx + s * 24.0f, sq.getY(), 20.0f, kh }, body.rowOn[(size_t) s] });
    keys.push_back ({ "playbody", "BODY", { bx, sq.getY() + 24.0f, 68.0f, kh }, playBody });
    keys.push_back ({ editing >= 0 ? "close" : "edit", editing >= 0 ? "CLOSE" : "EDIT", { bx + 72.0f, sq.getY() + 24.0f, 68.0f, kh }, editing >= 0 });
    keys.push_back ({ "export", "EXPORT", { bx, sq.getY() + 48.0f, 68.0f, kh }, false });
    keys.push_back ({ "copy", "COPY", { bx + 72.0f, sq.getY() + 48.0f, 68.0f, kh }, copyFrom >= 0 });
    keys.push_back ({ "sharpen", "SHARPEN", { bx + 144.0f, sq.getY() + 48.0f, 68.0f, kh }, false });
    keys.push_back ({ "unity", "UNITY", { bx + 144.0f, sq.getY() + 24.0f, 68.0f, kh }, body.unity });
    if (editing >= 0)
    {
        for (int s = 0; s < kRows; ++s)
        {
            const auto r = L.stageRect (s);
            keys.push_back ({ "lock" + juce::String (s), "LOCK", { L.field.getX() + 240.0f, r.getY() + 2.0f, 48.0f, kh }, lockRow[(size_t) s] });
        }
        keys.push_back ({ "ceiling", "CEILING", { L.field.getX() + 240.0f, L.stageRect (kRows - 1).getY() + 20.0f, 56.0f, kh }, false });
    }
    keys.push_back ({ "surface", "FIELD", { L.resp.getX() + 8.0f, L.resp.getY() + 6.0f, 56.0f, kh }, showSurface });
    keys.push_back ({ "pair", "PAIR", { L.resp.getX() + 72.0f, L.resp.getY() + 6.0f, 56.0f, kh }, pairMode });
    keys.push_back ({ "play", tl.playing ? "STOP" : "PLAY", { 8.0f, L.tl.getY() + 8.0f, 64.0f, kh }, tl.playing });
    keys.push_back ({ "loop", "LOOP", { 8.0f, L.tl.getY() + 26.0f, 64.0f, kh }, tl.loop });
    keys.push_back ({ "addkey", "+ KEY", { 8.0f, L.tl.getY() + 44.0f, 64.0f, kh }, false });
}

std::optional<Blend> Workstation::current() const
{
    if (pairMode && pairA >= 0 && pairB >= 0) return Blend { { pairA, pairB, pairA }, { 1.0 - pairT, pairT, 0.0 } };
    return probe ? lib.blendAt ((*probe)[0], (*probe)[1]) : std::nullopt;
}

void Workstation::setPairT (juce::Point<float> p)
{
    const auto a = L.fromField (lib.anchors[(size_t) pairA].p), b = L.fromField (lib.anchors[(size_t) pairB].p);
    const float dx = b.x - a.x, dy = b.y - a.y, len2 = std::max (1e-6f, dx * dx + dy * dy);
    pairT = juce::jlimit (0.0, 1.0, (double) (((p.x - a.x) * dx + (p.y - a.y) * dy) / len2));
    const auto& pa = lib.anchors[(size_t) pairA].p;
    const auto& pb = lib.anchors[(size_t) pairB].p;
    probe = std::array<double, 2> { pa[0] + (pb[0] - pa[0]) * pairT, pa[1] + (pb[1] - pa[1]) * pairT };
    redraw();
}

void Workstation::setProbe (juce::Point<float> p)
{
    probe = L.toField (p);
    playBody = false;
    status = current() ? "" : "outside the anchors";
    redraw();
}

void Workstation::scrubTo (float x)
{
    tl.playhead = L.tAt (x);
    if (const auto p = tl.pathAt (tl.playhead)) probe = p;
    redraw();
}

void Workstation::press (const juce::String& id)
{
    if (id.startsWith ("ax")) lib.axisX = id.substring (2).getIntValue();
    else if (id.startsWith ("ay")) lib.axisY = id.substring (2).getIntValue();
    else if (id.startsWith ("lit")) { const int g = id.substring (3).getIntValue(); lit = lit == g ? -1 : g; }
    else if (id == "sort") lib.sort();
    else if (id == "clear") { lib.anchors.clear(); lib.tris.clear(); }
    else if (id == "capture") capture();
    else if (id == "surface") showSurface = ! showSurface;
    else if (id.startsWith ("corner")) assignCorner (id.substring (6).getIntValue());
    else if (id.startsWith ("row")) { const int s = id.substring (3).getIntValue(); body.rowOn[(size_t) s] = ! body.rowOn[(size_t) s]; }
    else if (id == "export") exportBody (exportDir.getChildFile ("ws_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".body240"));
    else if (id == "playbody") playBody = ! playBody;
    else if (id == "edit") openEditor (editCorner);
    else if (id == "copy") { copyFrom = copyFrom >= 0 ? -1 : editCorner; status = copyFrom >= 0 ? "click the corner to paste into" : ""; }
    else if (id == "sharpen") sharpenQ();
    else if (id == "unity") body.unity = ! body.unity;
    else if (id == "ceiling") ceiling();
    else if (id.startsWith ("lock")) { const int s = id.substring (4).getIntValue(); lockRow[(size_t) s] = ! lockRow[(size_t) s]; }
    else if (id == "close") editing = -1;
    else if (id == "pair") { pairMode = ! pairMode; pairA = pairB = -1; pairT = 0.0; status = pairMode ? "pick two anchors" : ""; }
    else if (id == "play")
    {
        tl.playing = ! tl.playing;
        if (tl.playing) { playT0 = juce::Time::getMillisecondCounterHiRes(); playFrom = tl.playhead >= tl.duration ? 0.0 : tl.playhead; startTimerHz (60); }
        else stopTimer();
    }
    else if (id == "loop") tl.loop = ! tl.loop;
    else if (id == "addkey")
    {
        if (! probe) probe = std::array<double, 2> { 0.5, 0.5 };
        tl.keys.push_back ({ tl.playhead, *probe, juce::Colour::fromHSV (juce::Random::getSystemRandom().nextFloat(), 0.8f, 0.9f, 1.0f) });
    }
    redraw();
}

void Workstation::capture()
{
    const auto b = current();
    if (! b) { status = "outside the anchors"; redraw(); return; }
    const int idx = lib.addCapture (lib.wordsOf (*b), *probe);
    status = "captured " + lib.frames[(size_t) idx].name;
    redraw();
}

int Workstation::anchorAt (juce::Point<float> p) const
{
    if (! L.field.contains (p)) return -1;
    for (int i = (int) lib.anchors.size() - 1; i >= 0; --i)
        if (L.fromField (lib.anchors[(size_t) i].p).getDistanceFrom (p) < 6.0f) return i;
    return -1;
}

int Workstation::trayAt (juce::Point<float> p) const
{
    if (! L.tray.contains (p)) return -1;
    const int row = (int) ((p.y - L.tray.getY() - 2.0f) / 13.0f) + trayScroll;
    return row >= 0 && row < (int) lib.frames.size() ? row : -1;
}

int Workstation::keyAt (juce::Point<float> p) const
{
    for (int i = 0; i < (int) tl.keys.size(); ++i)
        if (juce::Point<float> (L.tx (tl.keys[(size_t) i].t), L.tlAx.getCentreY()).getDistanceFrom (p) < 8.0f) return i;
    return -1;
}

void Workstation::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    for (const auto& k : keys)
        if (k.box.contains (p)) { press (k.id); return; }
    if (const int t = trayAt (p); t >= 0)
    {
        if (pickFor >= 0 && pickFor < (int) lib.anchors.size()) { lib.anchors[(size_t) pickFor].frame = t; pickFor = -1; lib.retriangulate(); redraw(); return; }
        mode = Mode::dragFrame; dragFrame = t; dragPos = p; dragStart = p; return;
    }
    if (const int a = anchorAt (p); a >= 0)
    {
        if (pairMode)
        {
            if (pairA < 0) { pairA = a; status = "pick the far anchor"; }
            else if (pairB < 0 && a != pairA) { pairB = a; pairT = 0.0; probe = lib.anchors[(size_t) pairA].p; status = ""; }
            else { pairA = a; pairB = -1; status = "pick the far anchor"; }
            redraw();
            return;
        }
        mode = Mode::dragAnchor; dragAnchor = a; dragPos = p; dragStart = p; return;
    }
    if (pairMode && pairA >= 0 && pairB >= 0 && L.field.contains (p)) { mode = Mode::pair; setPairT (p); return; }
    if (const int k = keyAt (p); k >= 0) { mode = Mode::dragKey; dragKey = k; dragStart = p; return; }
    if (L.tlAx.contains (p)) { mode = Mode::scrub; scrubTo (p.x); return; }
    if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
    if (L.square.contains (p) && body.ready()) { mode = Mode::wheel; setWheel (p); return; }
    if (editing >= 0 && openBar.contains (p)) { mode = Mode::open; setOpen (p.x); return; }
    if (editing >= 0 && L.field.contains (p))
    {
        const auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
        for (int s = 0; s < kRows; ++s)
        {
            const auto r = L.stageRect (s);
            if (! r.contains (p)) continue;
            const auto& g = f.rows[(size_t) s];
            if (g.pole && juce::Point<float> (L.sx (r, g.pHz), L.sy (r, sectionDb (f.words, s, g.pHz))).getDistanceFrom (p) < 10.0f) { mode = Mode::dragPole; dragStage = s; return; }
            if (g.zero && juce::Point<float> (L.sx (r, g.zHz), L.sy (r, sectionDb (f.words, s, g.zHz))).getDistanceFrom (p) < 10.0f) { mode = Mode::dragZero; dragStage = s; return; }
            mode = Mode::dragPole; dragStage = s; dragHandle (p); return;
        }
        return;
    }
    if (L.field.contains (p)) { mode = Mode::probe; setProbe (p); return; }
}

void Workstation::pickHz (float x)
{
    const double t = juce::jlimit (0.0, 1.0, (double) (x - L.rx (20.0)) / (L.rx (20000.0) - L.rx (20.0)));
    fieldHz = 20.0 * std::pow (1000.0, t);
    redraw();
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
        if (cornerHit >= 0) { body.corner[(size_t) cornerHit] = a.frame; playBody = true; status = "corner " + juce::String (cornerHit) + " = " + lib.frames[(size_t) a.frame].name; }
        else if (L.tlAx.contains (p)) tl.keys.push_back ({ L.tAt (p.x), a.p, hueOf (lib.frames[(size_t) a.frame].m[0]) });
        else if (L.field.contains (p)) { a.p = L.toField (p); lib.retriangulate(); }
        else { lib.anchors.erase (lib.anchors.begin() + dragAnchor); lib.retriangulate(); }
    }
    else if (mode == Mode::dragAnchor && ! moved)
    {
        pickFor = pickFor == dragAnchor ? -1 : dragAnchor;
        status = pickFor >= 0 ? "pick a frame for " + lib.frames[(size_t) lib.anchors[(size_t) pickFor].frame].name : "";
    }
    else if (mode == Mode::dragKey && ! moved)
    {
        probe = tl.keys[(size_t) dragKey].p;
        tl.playhead = tl.keys[(size_t) dragKey].t;
    }
    mode = Mode::none;
    dragFrame = dragAnchor = dragKey = -1;
    redraw();
}

void Workstation::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto p = e.position;
    if (L.tray.contains (p)) { trayScroll = std::max (0, trayScroll - (int) std::round (w.deltaY * 30)); redraw(); }
    else if (L.field.contains (p))
    {
        const auto before = L.toField (p);
        L.zoom = juce::jlimit (0.5, 6.0, L.zoom * (w.deltaY > 0 ? 1.1 : 0.9));
        const auto after = L.fromField (before);
        L.pan[0] += p.x - after.x;
        L.pan[1] += p.y - after.y;
        redraw();
    }
}

FieldMesh Workstation::fieldMesh() const
{
    FieldMesh m;
    m.hz = fieldHz;
    for (const auto& t : lib.tris)
    {
        const int ids[3] = { t.a, t.b, t.c };
        for (int k = 0; k < 3; ++k)
        {
            const auto p = L.fromField (lib.anchors[(size_t) ids[k]].p);
            m.v.push_back ({ p.x, p.y, k == 0 ? 1.0f : 0.0f, k == 1 ? 1.0f : 0.0f, k == 2 ? 1.0f : 0.0f, (float) lib.anchors[(size_t) t.a].frame, (float) lib.anchors[(size_t) t.b].frame, (float) lib.anchors[(size_t) t.c].frame });
        }
    }
    return m;
}

std::vector<Batch> Workstation::scene() const
{
    std::vector<Batch> out;
    const auto ks = tl.sorted();
    const auto b = current();
    const bool inEditor = editing >= 0 && body.corner[(size_t) editing] >= 0;
    if (inEditor)
    {
        const auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
        Batch grids { Batch::lines, false, {} };
        Batch curves { Batch::strip, false, {} };
        const auto cr = L.cascadeRect();
        const auto cv = curveOf (f.words);
        for (int i = 0; i < kCurvePoints; ++i) curves.v.push_back (vertex ({ L.sx (cr, 20.0 * std::pow (1000.0, i / double (kCurvePoints - 1))), L.sy (cr, juce::jlimit (-30.0, 30.0, cv[(size_t) i])) }, juce::Colours::white, 1.5f));
        out.push_back (curves);
        Batch handles { Batch::points, false, {} };
        Batch zeroHandles { Batch::points, true, {} };
        for (int s = 0; s < kRows; ++s)
        {
            const auto r = L.stageRect (s);
            const auto& g = f.rows[(size_t) s];
            const bool on = body.rowOn[(size_t) s];
            Batch sc { Batch::strip, false, {} };
            for (int i = 0; i < kCurvePoints; ++i)
            {
                const double hz = 20.0 * std::pow (1000.0, i / double (kCurvePoints - 1));
                sc.v.push_back (vertex ({ L.sx (r, hz), L.sy (r, juce::jlimit (-30.0, 30.0, sectionDb (f.words, s, hz))) }, on ? juce::Colours::white : kDim, 1.2f));
            }
            out.push_back (sc);
            if (g.pole) handles.v.push_back (vertex ({ L.sx (r, g.pHz), L.sy (r, juce::jlimit (-30.0, 30.0, sectionDb (f.words, s, g.pHz))) }, juce::Colours::white, 7.0f));
            if (g.zero) zeroHandles.v.push_back (vertex ({ L.sx (r, g.zHz), L.sy (r, juce::jlimit (-30.0, 30.0, sectionDb (f.words, s, g.zHz))) }, kOn, 6.0f));
        }
        out.push_back (grids);
        out.push_back (handles);
        out.push_back (zeroHandles);
    }
    if (! inEditor)
    {
    if (! gl && showSurface)
    {
        Batch surface { Batch::tris, false, {} };
        for (const auto& t : lib.tris)
        {
            const Blend centre { { t.a, t.b, t.c }, { 1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0 } };
            const auto c = levelColour (responseDb (cascadeOf (lib.wordsOf (centre)), fieldHz));
            for (int id : { t.a, t.b, t.c }) surface.v.push_back (vertex (L.fromField (lib.anchors[(size_t) id].p), c, 1.0f));
        }
        out.push_back (surface);
    }
    Batch grid { Batch::lines, false, {} };
    for (int i = 1; i < 4; ++i)
    {
        const auto a = L.fromField ({ i / 4.0, 0.0 }), c = L.fromField ({ i / 4.0, 1.0 }), d = L.fromField ({ 0.0, i / 4.0 }), f = L.fromField ({ 1.0, i / 4.0 });
        grid.v.push_back (vertex (a, kLine, 1.0f)); grid.v.push_back (vertex (c, kLine, 1.0f));
        grid.v.push_back (vertex (d, kLine, 1.0f)); grid.v.push_back (vertex (f, kLine, 1.0f));
    }
    out.push_back (grid);
    Batch links { Batch::lines, false, {} };
    for (size_t i = 0; i + 1 < ks.size(); ++i) { links.v.push_back (vertex (L.fromField (ks[i].p), juce::Colour (0xaaff00ff), 1.0f)); links.v.push_back (vertex (L.fromField (ks[i + 1].p), juce::Colour (0xaaff00ff), 1.0f)); }
    if (pairMode && pairA >= 0 && pairB >= 0)
    {
        links.v.push_back (vertex (L.fromField (lib.anchors[(size_t) pairA].p), juce::Colours::white, 1.5f));
        links.v.push_back (vertex (L.fromField (lib.anchors[(size_t) pairB].p), juce::Colours::white, 1.5f));
    }
    else if (b)
        for (int i = 0; i < 3; ++i) { links.v.push_back (vertex (L.fromField (*probe), kOn, 1.0f)); links.v.push_back (vertex (L.fromField (lib.anchors[(size_t) b->anchors[(size_t) i]].p), kOn, 1.0f)); }
    out.push_back (links);
    Batch pts { Batch::points, true, {} };
    for (int i = 0; i < (int) lib.anchors.size(); ++i)
    {
        const auto& a = lib.anchors[(size_t) i];
        const auto& f = lib.frames[(size_t) a.frame];
        const bool chosen = i == pairA || i == pairB;
        const bool on = (lit < 0 || f.group == lit || f.capture) && (! pairMode || pairB < 0 || chosen);
        auto c = f.capture ? juce::Colours::yellow : hueOf (f.m[0]);
        if (! on) c = c.withAlpha (0.25f);
        pts.v.push_back (vertex (L.fromField (a.p), c, chosen || i == dragAnchor || i == pickFor ? 12.0f : (on ? 8.0f : 5.0f)));
    }
    for (const auto& k : ks) pts.v.push_back (vertex (L.fromField (k.p), k.colour, 9.0f));
    if (probe) pts.v.push_back (vertex (L.fromField (*probe), juce::Colours::yellow, 9.0f));
    if (mode == Mode::dragFrame) pts.v.push_back (vertex (dragPos, hueOf (lib.frames[(size_t) dragFrame].m[0]), 12.0f));
    if (mode == Mode::dragAnchor) pts.v.push_back (vertex (dragPos, juce::Colours::white, 12.0f));
    out.push_back (pts);
    }
    Batch marker { Batch::lines, false, {} };
    marker.v.push_back (vertex ({ L.rx (fieldHz), L.ry (30.0) }, juce::Colour (0xff00ffff), 1.0f));
    marker.v.push_back (vertex ({ L.rx (fieldHz), L.ry (-30.0) }, juce::Colour (0xff00ffff), 1.0f));
    out.push_back (marker);
    Batch curve { Batch::strip, false, {} };
    if (b || (playBody && body.ready()))
    {
        const auto cv = curveOf (playingWords());
        for (int i = 0; i < kCurvePoints; ++i)
            curve.v.push_back (vertex ({ L.rx (20.0 * std::pow (1000.0, i / double (kCurvePoints - 1))), L.ry (juce::jlimit (-30.0, 30.0, cv[(size_t) i])) }, juce::Colours::white, 1.5f));
    }
    out.push_back (curve);
    Batch rings { Batch::lines, false, {} };
    for (double db : { 20.0, 40.0, 60.0 })
    {
        const double rr = 1.0 - std::pow (10.0, -db / 20.0);
        juce::Point<float> prev;
        for (int i = 0; i <= 40; ++i)
        {
            const auto p = L.armaXY (20.0 * std::pow (2.0, 10.0 * i / 40.0), rr);
            if (i > 0) { rings.v.push_back (vertex (prev, kLine, 1.0f)); rings.v.push_back (vertex (p, kLine, 1.0f)); }
            prev = p;
        }
    }
    for (int o = 0; o <= 10; o += 2) { rings.v.push_back (vertex (L.armaXY (20.0 * std::pow (2.0, o), 0.0), kLine, 1.0f)); rings.v.push_back (vertex (L.armaXY (20.0 * std::pow (2.0, o), 0.999), kLine, 1.0f)); }
    out.push_back (rings);
    if (playBody && body.ready())
    {
        const auto sq = L.square;
        Batch sqLines { Batch::lines, false, {} };
        for (int i = 0; i <= 2; ++i)
        {
            sqLines.v.push_back (vertex ({ sq.getX() + i * sq.getWidth() / 2, sq.getY() }, kLine, 1.0f)); sqLines.v.push_back (vertex ({ sq.getX() + i * sq.getWidth() / 2, sq.getBottom() }, kLine, 1.0f));
            sqLines.v.push_back (vertex ({ sq.getX(), sq.getY() + i * sq.getHeight() / 2 }, kLine, 1.0f)); sqLines.v.push_back (vertex ({ sq.getRight(), sq.getY() + i * sq.getHeight() / 2 }, kLine, 1.0f));
        }
        out.push_back (sqLines);
        Batch wheel { Batch::points, true, {} };
        for (int i = 0; i < 4; ++i)
            wheel.v.push_back (vertex ({ i & 1 ? sq.getRight() : sq.getX(), i & 2 ? sq.getY() : sq.getBottom() }, hueOf (lib.frames[(size_t) body.corner[(size_t) i]].m[0]), 8.0f));
        wheel.v.push_back (vertex ({ sq.getX() + (float) body.morph * sq.getWidth(), sq.getBottom() - (float) body.q * sq.getHeight() }, juce::Colours::yellow, 9.0f));
        out.push_back (wheel);
    }
    if (b || (playBody && body.ready()))
    {
        const auto rows = geometryOf (playingWords());
        Batch glides { Batch::lines, false, {} };
        Batch poles { Batch::points, false, {} };
        Batch zeros { Batch::points, true, {} };
        for (int s = 0; s < kRows; ++s)
        {
            const auto& r = rows[(size_t) s];
            if (! r.pole) continue;
            const auto here = L.armaXY (r.pHz, r.pR);
            std::vector<std::pair<int, double>> parents;
            if (playBody && body.ready()) { const auto w = body.weights(); for (int i = 0; i < 4; ++i) parents.push_back ({ body.corner[(size_t) i], w[(size_t) i] }); }
            else for (int i = 0; i < 3; ++i) parents.push_back ({ lib.anchors[(size_t) b->anchors[(size_t) i]].frame, b->w[(size_t) i] });
            for (const auto& [frameIdx, weight] : parents)
            {
                if (weight < 0.08) continue;
                const auto& pr = lib.frames[(size_t) frameIdx].rows[(size_t) s];
                if (! pr.pole) continue;
                const auto c = kOn.withAlpha ((float) (0.15 + 0.6 * weight));
                glides.v.push_back (vertex (here, c, 1.0f));
                glides.v.push_back (vertex (L.armaXY (pr.pHz, pr.pR), c, 1.0f));
                poles.v.push_back (vertex (L.armaXY (pr.pHz, pr.pR), kDim, 4.0f));
            }
            poles.v.push_back (vertex (here, juce::Colours::white, 8.0f));
            if (r.zero) zeros.v.push_back (vertex (L.armaXY (r.zHz, r.zR), kOn, 7.0f));
        }
        out.push_back (glides);
        out.push_back (poles);
        out.push_back (zeros);
    }
    Batch tline { Batch::lines, false, {} };
    for (size_t i = 0; i + 1 < ks.size(); ++i) { tline.v.push_back (vertex ({ L.tx (ks[i].t), L.tlAx.getCentreY() }, kOn, 1.0f)); tline.v.push_back (vertex ({ L.tx (ks[i + 1].t), L.tlAx.getCentreY() }, kOn, 1.0f)); }
    tline.v.push_back (vertex ({ L.tx (tl.playhead), L.tlAx.getY() }, juce::Colours::yellow, 1.0f));
    tline.v.push_back (vertex ({ L.tx (tl.playhead), L.tlAx.getBottom() }, juce::Colours::yellow, 1.0f));
    out.push_back (tline);
    Batch tpts { Batch::points, false, {} };
    for (const auto& k : ks) tpts.v.push_back (vertex ({ L.tx (k.t), L.tlAx.getCentreY() }, k.colour, 8.0f));
    out.push_back (tpts);
    return out;
}

void Workstation::paint (juce::Graphics& g)
{
    layoutKeys();
    if (! gl)
        drawSvg (g, sceneToSvg (scene(), (float) getWidth(), (float) getHeight()), (float) getWidth(), (float) getHeight());
    g.setFont (mono (11.0f));
    paintChrome (g);
}

void Workstation::newOpenGLContextCreated() { renderer.create (ctx); }
void Workstation::openGLContextClosing() { renderer.destroy(); }

void Workstation::renderOpenGL()
{
    layoutKeys();
    renderer.uploadWords (lib.frames);
    renderer.draw (showSurface ? fieldMesh() : FieldMesh {}, scene(), (float) getWidth(), (float) getHeight(), (float) ctx.getRenderingScale());
}

void Workstation::paintChrome (juce::Graphics& g)
{
    if (pickFor >= 0) { g.setColour (kOn); g.drawRect (px (L.tray).reduced (1), 1); }
    g.setColour (kLine);
    g.drawVerticalLine ((int) L.groups.getRight() - 1, 0.0f, L.tl.getY());
    g.drawVerticalLine ((int) L.resp.getX(), 0.0f, L.tl.getY());
    g.drawHorizontalLine ((int) L.tl.getY(), 0.0f, (float) getWidth());
    g.drawHorizontalLine ((int) L.tray.getY(), 0.0f, L.tray.getRight());
    g.drawHorizontalLine ((int) L.resp.getBottom(), L.resp.getX(), L.resp.getRight());
    g.drawHorizontalLine ((int) L.body.getBottom(), L.body.getX(), L.body.getRight());
    g.drawHorizontalLine ((int) L.info.getBottom(), L.info.getX(), L.info.getRight());
    g.drawRect (px (L.field), 1);
    g.drawRect (px (L.tlAx), 1);
    for (const auto& k : keys)
    {
        const auto r = px (k.box);
        g.setColour (k.on ? kOn : kKey);
        g.fillRect (r);
        g.setColour (k.on ? kOn : kKeyLine);
        g.drawRect (r, 1);
        g.setColour (k.on ? juce::Colours::black : kText);
        g.drawText (k.label, r.reduced (6, 0), juce::Justification::centredLeft);
    }
    g.setColour (kDim);
    g.drawText ("across", (int) L.field.getX(), 6, 44, 14, juce::Justification::centredLeft);
    g.drawText ("up", (int) L.field.getX(), 22, 44, 14, juce::Justification::centredLeft);
    const int maxRows = (int) ((L.tray.getHeight() - 8.0f) / 14.0f);
    trayScroll = juce::jlimit (0, std::max (0, (int) lib.frames.size() - maxRows), trayScroll);
    for (int r = 0; r < maxRows && r + trayScroll < (int) lib.frames.size(); ++r)
    {
        const auto& f = lib.frames[(size_t) (r + trayScroll)];
        const float y = L.tray.getY() + 6.0f + r * 14.0f;
        g.setColour (f.capture ? juce::Colours::yellow : hueOf (f.m[0]));
        g.fillEllipse (L.tray.getX() + 9.0f, y + 4.5f, 5.0f, 5.0f);
        g.setColour (lit < 0 || f.group == lit || f.capture ? kText : kDim);
        g.drawText (f.name, (int) L.tray.getX() + 20, (int) y, (int) L.tray.getWidth() - 28, 14, juce::Justification::centredLeft);
    }
    g.setColour (kDim);
    if (editing >= 0 && body.corner[(size_t) editing] >= 0)
    {
        const auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
        const auto cr = L.cascadeRect();
        const auto gridOf = [&] (juce::Rectangle<float> r, bool labels)
        {
            g.setColour (kLine);
            g.drawRect (r, 1.0f);
            const bool full = r.getHeight() > 100.0f;
            for (double hz : { 100.0, 1000.0, 10000.0 }) g.drawVerticalLine ((int) L.sx (r, hz), r.getY(), r.getBottom());
            if (full) for (double hz : { 50.0, 200.0, 500.0, 2000.0, 5000.0 }) g.drawVerticalLine ((int) L.sx (r, hz), r.getY(), r.getBottom());
            if (full) for (double db : { -20.0, -10.0, 10.0, 20.0 }) g.drawHorizontalLine ((int) L.sy (r, db), r.getX(), r.getRight());
            g.setColour (kDim);
            g.drawHorizontalLine ((int) L.sy (r, 0.0), r.getX(), r.getRight());
            if (labels)
            {
                for (double hz : { 100.0, 1000.0, 10000.0 }) g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String (hz, 0), (int) L.sx (r, hz) - 14, (int) r.getBottom() + 2, 28, 12, juce::Justification::centred);
                for (double db : { 20.0, 0.0, -20.0 }) g.drawText (juce::String (db, 0), (int) r.getX() - 34, (int) L.sy (r, db) - 6, 30, 12, juce::Justification::centredRight);
            }
        };
        gridOf (cr, true);
        g.setColour (kOn);
        g.drawText (f.name, (int) cr.getX(), (int) cr.getY() - 14, (int) cr.getWidth() / 2, 12, juce::Justification::centredLeft);
        openBar = { cr.getRight() - 220.0f, cr.getY() - 13.0f, 160.0f, 10.0f };
        g.setColour (kDim);
        g.drawText ("open", (int) openBar.getX() - 40, (int) openBar.getY() - 2, 36, 14, juce::Justification::centredRight);
        g.setColour (juce::Colours::black);
        g.fillRect (openBar);
        g.setColour (juce::Colour (0xff777777));
        g.drawRect (openBar, 1.0f);
        g.setColour (kOn);
        g.fillRect (openBar.getX() + 1.0f, openBar.getY() + 1.0f, (openBar.getWidth() - 2.0f) * (float) openAmount, openBar.getHeight() - 2.0f);
        g.setColour (kDim);
        g.drawText (juce::String ((int) std::round (250.0 * std::pow (900.0 / 250.0, openAmount))) + " Hz", (int) openBar.getRight() + 4, (int) openBar.getY() - 2, 56, 14, juce::Justification::centredLeft);
        for (int s = 0; s < kRows; ++s)
        {
            const auto r = L.stageRect (s);
            gridOf (r, s == kRows - 1);
            const auto& gm = f.rows[(size_t) s];
            const int x0 = (int) L.field.getX() + 8, y0 = (int) r.getY() + 2;
            g.setColour (body.rowOn[(size_t) s] ? kText : kDim);
            g.drawText (juce::String (s + 1) + (body.rowOn[(size_t) s] ? "" : "  off"), x0, y0, 40, 13, juce::Justification::centredLeft);
            g.setColour (gm.pole ? kText : kDim);
            g.drawText (gm.pole ? "pole  " + juce::String ((int) std::round (gm.pHz)).paddedLeft (' ', 6) + " Hz   r " + juce::String (gm.pR, 3) + "   " + juce::String (resDb (gm.pR), 1) + " dB" : "pole  real", x0 + 40, y0, 240, 13, juce::Justification::centredLeft);
            g.setColour (gm.zero ? kText : kDim);
            g.drawText (gm.zero ? "zero  " + juce::String ((int) std::round (gm.zHz)).paddedLeft (' ', 6) + " Hz   r " + juce::String (gm.zR, 3) : "zero  real", x0 + 40, y0 + 14, 240, 13, juce::Justification::centredLeft);
        }
    }
    else
    {
        g.drawText (kMeasureNames[lib.axisX], (int) L.field.getRight() - 100, (int) L.field.getBottom() - 16, 92, 12, juce::Justification::centredRight);
        g.drawText (kMeasureNames[lib.axisY], (int) L.field.getX() + 8, (int) L.field.getY() + 4, 92, 12, juce::Justification::centredLeft);
    }
    g.setColour (kLine);
    g.drawRect (juce::Rectangle<int> ((int) L.rx (20.0), (int) L.ry (30.0), (int) (L.rx (20000.0) - L.rx (20.0)), (int) (L.ry (-30.0) - L.ry (30.0))), 1);
    for (double f : { 100.0, 1000.0, 10000.0 }) g.drawVerticalLine ((int) L.rx (f), L.ry (30.0), L.ry (-30.0));
    for (double d : { -20.0, 20.0 }) g.drawHorizontalLine ((int) L.ry (d), L.rx (20.0), L.rx (20000.0));
    g.setColour (kDim);
    g.drawHorizontalLine ((int) L.ry (0.0), L.rx (20.0), L.rx (20000.0));
    for (double f : { 100.0, 1000.0, 10000.0 }) g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f, 0), (int) L.rx (f) - 14, (int) L.ry (-30.0) + 4, 28, 12, juce::Justification::centred);
    for (double d : { 20.0, 0.0, -20.0 }) g.drawText (juce::String (d, 0), (int) L.resp.getX() + 6, (int) L.ry (d) - 6, 28, 12, juce::Justification::centredRight);
    const auto b = current();
    int y = (int) L.info.getY() + 30;
    g.setColour (kText);
    if (probe) g.drawText ("x " + juce::String ((*probe)[0], 3) + "   y " + juce::String ((*probe)[1], 3), (int) L.info.getX() + 96, (int) L.info.getY() + 8, 200, 14, juce::Justification::centredLeft);
    g.setColour (kOn);
    g.drawText (juce::String ((int) std::round (fieldHz)) + " Hz", (int) L.rx (fieldHz) + 4, (int) L.resp.getY() + 8, 60, 12, juce::Justification::centredLeft);
    g.setColour (kText);
    g.setColour (kLine);
    g.drawRect (px (L.square), 1);
    g.setColour (kDim);
    g.drawText ("M " + juce::String (body.morph, 2) + "   Q " + juce::String (body.q, 2), (int) L.square.getX(), (int) L.square.getBottom() + 6, (int) L.square.getWidth(), 12, juce::Justification::centred);
    for (int i = 0; i < 4; ++i)
        if (body.corner[(size_t) i] >= 0)
            g.drawText (lib.frames[(size_t) body.corner[(size_t) i]].name, (int) L.square.getRight() + 64, (int) L.square.getY() + 70 + i * 13, (int) L.body.getRight() - (int) L.square.getRight() - 72, 12, juce::Justification::centredLeft);
    if (b && ! playBody)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (b->w[(size_t) i] <= 0.0) continue;
            const auto& f = lib.frames[(size_t) lib.anchors[(size_t) b->anchors[(size_t) i]].frame];
            g.setColour (hueOf (f.m[0]));
            g.fillEllipse (L.info.getX() + 8.0f, (float) y + 4.0f, 5.0f, 5.0f);
            g.setColour (kText);
            g.drawText (juce::String ((int) std::round (b->w[(size_t) i] * 100)) + "%  " + f.name, (int) L.info.getX() + 18, y, (int) L.info.getWidth() - 22, 13, juce::Justification::centredLeft);
            y += 13;
        }
        y += 8;
    }
    if (b || (playBody && body.ready()))
    {
        if (playBody) y = (int) L.info.getY() + 26;
        const auto rows = geometryOf (playingWords());
        g.setColour (kDim);
        g.drawText ("row   pole Hz     r     zero Hz     r", (int) L.info.getX() + 8, y, (int) L.info.getWidth() - 12, 13, juce::Justification::centredLeft);
        y += 14;
        for (int s = 0; s < kRows; ++s)
        {
            const auto& r = rows[(size_t) s];
            juce::String line = juce::String (s + 1).paddedLeft (' ', 3) + "  ";
            line += (r.pole ? juce::String ((int) std::round (r.pHz)).paddedLeft (' ', 7) + "  " + juce::String (r.pR, 3) : juce::String ("   real        "));
            line += "  ";
            line += (r.zero ? juce::String ((int) std::round (r.zHz)).paddedLeft (' ', 7) + "  " + juce::String (r.zR, 3) : juce::String ("   real        "));
            g.setColour (r.pole ? hueOf (octOf (r.pHz) / 9.0) : kDim);
            g.fillRect (L.info.getX() + 2.0f, (float) y + 3.0f, 3.0f, 8.0f);
            g.setColour (kText);
            g.drawText (line, (int) L.info.getX() + 8, y, (int) L.info.getWidth() - 12, 13, juce::Justification::centredLeft);
            y += 13;
        }
    }
    g.setColour (kDim);
    for (double db : { 20.0, 40.0, 60.0 }) { const auto p = L.armaXY (20.0, 1.0 - std::pow (10.0, -db / 20.0)); g.drawText (juce::String ((int) db) + " dB", (int) p.x - 44, (int) p.y - 6, 40, 12, juce::Justification::centredRight); }
    for (int o = 0; o <= 10; o += 2) { const double hz = 20.0 * std::pow (2.0, o); const auto p = L.armaXY (hz, 0.9995); g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 1) + "k" : juce::String (hz, 0), (int) p.x - 16, (int) p.y - 15, 32, 12, juce::Justification::centred); }
    if (b)
    {
        const auto rows = geometryOf (lib.wordsOf (*b));
        for (int s = 0; s < kRows; ++s) if (rows[(size_t) s].pole) { const auto p = L.armaXY (rows[(size_t) s].pHz, rows[(size_t) s].pR); g.setColour (kText); g.drawText (juce::String (s + 1), (int) p.x + 6, (int) p.y - 13, 12, 12, juce::Justification::centredLeft); }
    }
    if (pairMode && pairA >= 0 && pairB >= 0 && editing < 0)
    {
        g.setColour (kOn);
        g.drawText (lib.frames[(size_t) lib.anchors[(size_t) pairA].frame].name + "  >  " + lib.frames[(size_t) lib.anchors[(size_t) pairB].frame].name + "   " + juce::String (pairT, 3), (int) L.field.getX() + 100, (int) L.field.getY() + 2, (int) L.field.getWidth() - 200, 12, juce::Justification::centred);
    }
    g.setColour (kLine);
    g.drawHorizontalLine ((int) L.arma.getY(), L.arma.getX(), L.arma.getRight());
    g.setColour (kDim);
    for (int t = 0; t <= (int) tl.duration; ++t) g.drawText (juce::String (t), (int) L.tx (t) - 8, (int) L.tlAx.getBottom() + 4, 20, 12, juce::Justification::centred);
    g.drawText (juce::String (tl.playhead, 2) + " s", (int) L.tlAx.getRight() - 60, (int) L.tlAx.getY() - 14, 58, 12, juce::Justification::centredRight);
    g.setColour (kOn);
    g.drawText (status, (int) L.field.getX() + 8, (int) L.field.getBottom() - 16, (int) L.field.getWidth() - 120, 12, juce::Justification::centredLeft);
}
}
