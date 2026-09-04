#include "Workstation.h"
#include "../render/SvgRenderer.h"
#include <cmath>

namespace ws
{
namespace
{
juce::Font mono (float h) { return juce::Font (juce::FontOptions ("Consolas", h, juce::Font::plain)); }
const juce::Colour kLine (0xff444444), kText (0xffbbbbbb), kOn (0xff00ffff), kDim (0xff777777);
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
    tl.keys = { { 0.5, { 0.3, 0.35 }, juce::Colour (0xff00ccff) }, { 3.0, { 0.55, 0.6 }, juce::Colour (0xffff00ff) }, { 6.5, { 0.4, 0.75 }, juce::Colours::yellow } };
    tl.playhead = 2.1;
    probe = tl.pathAt (tl.playhead);
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
    const float kx = L.keyRow.getX() + 46.0f;
    for (int i = 0; i < kMeasures; ++i)
    {
        keys.push_back ({ "ax" + juce::String (i), kMeasureNames[i], { kx + i * 78.0f, 3.0f, 74.0f, 14.0f }, lib.axisX == i });
        keys.push_back ({ "ay" + juce::String (i), kMeasureNames[i], { kx + i * 78.0f, 19.0f, 74.0f, 14.0f }, lib.axisY == i });
    }
    keys.push_back ({ "sort", "SORT", { kx + kMeasures * 78.0f + 6.0f, 3.0f, 52.0f, 14.0f }, false });
    keys.push_back ({ "clear", "CLEAR", { kx + kMeasures * 78.0f + 6.0f, 19.0f, 52.0f, 14.0f }, false });
    for (int i = 0; i < kGroups; ++i) keys.push_back ({ "lit" + juce::String (i), kGroupNames[i], { 4.0f, 4.0f + i * 14.0f, L.groups.getWidth() - 8.0f, 13.0f }, lit == i });
    keys.push_back ({ "capture", "CAPTURE", { L.info.getX() + 6.0f, L.info.getY() + 6.0f, 80.0f, 14.0f }, false });
    keys.push_back ({ "play", tl.playing ? "STOP" : "PLAY", { 4.0f, L.tl.getY() + 6.0f, 60.0f, 14.0f }, tl.playing });
    keys.push_back ({ "loop", "LOOP", { 4.0f, L.tl.getY() + 24.0f, 60.0f, 14.0f }, tl.loop });
    keys.push_back ({ "addkey", "+ KEY", { 4.0f, L.tl.getY() + 42.0f, 60.0f, 14.0f }, false });
}

std::optional<Blend> Workstation::current() const
{
    return probe ? lib.blendAt ((*probe)[0], (*probe)[1]) : std::nullopt;
}

void Workstation::setProbe (juce::Point<float> p)
{
    probe = L.toField (p);
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
    if (const int a = anchorAt (p); a >= 0) { mode = Mode::dragAnchor; dragAnchor = a; dragPos = p; dragStart = p; return; }
    if (const int k = keyAt (p); k >= 0) { mode = Mode::dragKey; dragKey = k; dragStart = p; return; }
    if (L.tlAx.contains (p)) { mode = Mode::scrub; scrubTo (p.x); return; }
    if (L.resp.contains (p)) { mode = Mode::pickHz; pickHz (p.x); return; }
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
        if (L.tlAx.contains (p)) tl.keys.push_back ({ L.tAt (p.x), a.p, hueOf (lib.frames[(size_t) a.frame].m[0]) });
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
    if (! gl)
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
    for (int i = 0; i <= 4; ++i)
    {
        const auto a = L.fromField ({ i / 4.0, 0.0 }), c = L.fromField ({ i / 4.0, 1.0 }), d = L.fromField ({ 0.0, i / 4.0 }), f = L.fromField ({ 1.0, i / 4.0 });
        grid.v.push_back (vertex (a, kLine, 1.0f)); grid.v.push_back (vertex (c, kLine, 1.0f));
        grid.v.push_back (vertex (d, kLine, 1.0f)); grid.v.push_back (vertex (f, kLine, 1.0f));
    }
    out.push_back (grid);
    Batch links { Batch::lines, false, {} };
    for (size_t i = 0; i + 1 < ks.size(); ++i) { links.v.push_back (vertex (L.fromField (ks[i].p), juce::Colour (0xaaff00ff), 1.0f)); links.v.push_back (vertex (L.fromField (ks[i + 1].p), juce::Colour (0xaaff00ff), 1.0f)); }
    if (b)
        for (int i = 0; i < 3; ++i) { links.v.push_back (vertex (L.fromField (*probe), kOn, 1.0f)); links.v.push_back (vertex (L.fromField (lib.anchors[(size_t) b->anchors[(size_t) i]].p), kOn, 1.0f)); }
    out.push_back (links);
    Batch pts { Batch::points, true, {} };
    for (int i = 0; i < (int) lib.anchors.size(); ++i)
    {
        const auto& a = lib.anchors[(size_t) i];
        const auto& f = lib.frames[(size_t) a.frame];
        const bool on = lit < 0 || f.group == lit || f.capture;
        auto c = f.capture ? juce::Colours::yellow : hueOf (f.m[0]);
        if (! on) c = c.withAlpha (0.3f);
        pts.v.push_back (vertex (L.fromField (a.p), c, i == dragAnchor || i == pickFor ? 12.0f : (on ? 8.0f : 5.0f)));
    }
    for (const auto& k : ks) pts.v.push_back (vertex (L.fromField (k.p), k.colour, 9.0f));
    if (probe) pts.v.push_back (vertex (L.fromField (*probe), juce::Colours::yellow, 9.0f));
    if (mode == Mode::dragFrame) pts.v.push_back (vertex (dragPos, hueOf (lib.frames[(size_t) dragFrame].m[0]), 12.0f));
    if (mode == Mode::dragAnchor) pts.v.push_back (vertex (dragPos, juce::Colours::white, 12.0f));
    out.push_back (pts);
    Batch marker { Batch::lines, false, {} };
    marker.v.push_back (vertex ({ L.rx (fieldHz), L.ry (30.0) }, juce::Colour (0xff00ffff), 1.0f));
    marker.v.push_back (vertex ({ L.rx (fieldHz), L.ry (-30.0) }, juce::Colour (0xff00ffff), 1.0f));
    out.push_back (marker);
    Batch curve { Batch::strip, false, {} };
    if (b)
    {
        const auto cv = curveOf (lib.wordsOf (*b));
        for (int i = 0; i < kCurvePoints; ++i)
            curve.v.push_back (vertex ({ L.rx (20.0 * std::pow (1000.0, i / double (kCurvePoints - 1))), L.ry (juce::jlimit (-30.0, 30.0, cv[(size_t) i])) }, juce::Colours::white, 1.5f));
    }
    out.push_back (curve);
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
    renderer.draw (fieldMesh(), scene(), (float) getWidth(), (float) getHeight(), (float) ctx.getRenderingScale());
}

void Workstation::paintChrome (juce::Graphics& g)
{
    g.setColour (pickFor >= 0 ? kOn : kLine);
    g.drawRect (L.tray, 1.0f);
    g.setColour (kLine);
    g.drawVerticalLine ((int) L.field.getX(), 0.0f, L.tl.getY());
    g.drawVerticalLine ((int) L.resp.getX(), 0.0f, L.tl.getY());
    g.drawHorizontalLine ((int) L.tl.getY(), 0.0f, (float) getWidth());
    g.drawHorizontalLine ((int) L.field.getY(), L.field.getX(), L.field.getRight());
    g.drawHorizontalLine ((int) L.tray.getY(), 0.0f, L.tray.getRight());
    g.drawHorizontalLine ((int) L.resp.getBottom(), L.resp.getX(), L.resp.getRight());
    g.drawRect (L.tlAx, 1.0f);
    for (const auto& k : keys)
    {
        g.setColour (k.on ? kOn : juce::Colour (0xff1c1c1c));
        g.fillRect (k.box);
        g.setColour (k.on ? kOn : juce::Colour (0xff555555));
        g.drawRect (k.box, 1.0f);
        g.setColour (k.on ? juce::Colours::black : kText);
        g.drawText (k.label, k.box.toNearestInt().reduced (5, 0), juce::Justification::centredLeft);
    }
    g.setColour (kDim);
    g.drawText ("across", (int) L.keyRow.getX() + 4, 3, 40, 14, juce::Justification::centredLeft);
    g.drawText ("up", (int) L.keyRow.getX() + 4, 19, 40, 14, juce::Justification::centredLeft);
    const int maxRows = (int) ((L.tray.getHeight() - 4.0f) / 13.0f);
    trayScroll = juce::jlimit (0, std::max (0, (int) lib.frames.size() - maxRows), trayScroll);
    for (int r = 0; r < maxRows && r + trayScroll < (int) lib.frames.size(); ++r)
    {
        const auto& f = lib.frames[(size_t) (r + trayScroll)];
        const float y = L.tray.getY() + 2.0f + r * 13.0f;
        g.setColour (f.capture ? juce::Colours::yellow : hueOf (f.m[0]));
        g.fillEllipse (L.tray.getX() + 5.0f, y + 4.0f, 5.0f, 5.0f);
        g.setColour (lit < 0 || f.group == lit || f.capture ? kText : kDim);
        g.drawText (f.name, (int) L.tray.getX() + 14, (int) y, (int) L.tray.getWidth() - 16, 13, juce::Justification::centredLeft);
    }
    g.setColour (kDim);
    g.drawText (kMeasureNames[lib.axisX], (int) L.field.getRight() - 96, (int) L.field.getBottom() - 14, 92, 12, juce::Justification::centredRight);
    g.drawText (kMeasureNames[lib.axisY], (int) L.field.getX() + 4, (int) L.field.getY() + 2, 92, 12, juce::Justification::centredLeft);
    g.setColour (kLine);
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 }) g.drawVerticalLine ((int) L.rx (f), L.ry (30.0), L.ry (-30.0));
    for (double d : { -20.0, -10.0, 10.0, 20.0 }) g.drawHorizontalLine ((int) L.ry (d), L.rx (20.0), L.rx (20000.0));
    g.setColour (kDim);
    g.drawHorizontalLine ((int) L.ry (0.0), L.rx (20.0), L.rx (20000.0));
    for (double f : { 100.0, 1000.0, 10000.0 }) g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f, 0), (int) L.rx (f) - 14, (int) L.ry (-30.0) + 2, 28, 12, juce::Justification::centred);
    for (double d : { 20.0, 0.0, -20.0 }) g.drawText (juce::String (d, 0), (int) L.resp.getX() + 2, (int) L.ry (d) - 6, 28, 12, juce::Justification::centredRight);
    const auto b = current();
    int y = (int) L.info.getY() + 26;
    g.setColour (kText);
    if (probe) g.drawText ("x " + juce::String ((*probe)[0], 3) + "  y " + juce::String ((*probe)[1], 3), (int) L.info.getX() + 96, (int) L.info.getY() + 6, 200, 14, juce::Justification::centredLeft);
    g.setColour (kOn);
    g.drawText (juce::String ((int) std::round (fieldHz)) + " Hz", (int) L.rx (fieldHz) + 3, (int) L.resp.getY() + 2, 60, 12, juce::Justification::centredLeft);
    g.setColour (kText);
    if (b)
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto& f = lib.frames[(size_t) lib.anchors[(size_t) b->anchors[(size_t) i]].frame];
            g.setColour (hueOf (f.m[0]));
            g.fillEllipse (L.info.getX() + 8.0f, (float) y + 4.0f, 5.0f, 5.0f);
            g.setColour (kText);
            g.drawText (juce::String ((int) std::round (b->w[(size_t) i] * 100)) + "%  " + f.name, (int) L.info.getX() + 18, y, (int) L.info.getWidth() - 22, 13, juce::Justification::centredLeft);
            y += 13;
        }
        y += 8;
        const auto rows = geometryOf (lib.wordsOf (*b));
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
    for (int t = 0; t <= (int) tl.duration; ++t) g.drawText (juce::String (t), (int) L.tx (t) - 8, (int) L.tlAx.getBottom() + 2, 20, 12, juce::Justification::centred);
    g.drawText (juce::String (tl.playhead, 2) + " s", (int) L.tlAx.getRight() - 60, (int) L.tlAx.getY() - 12, 58, 12, juce::Justification::centredRight);
    g.setColour (kOn);
    g.drawText (status, (int) L.field.getX() + 4, (int) L.field.getBottom() - 14, (int) L.field.getWidth() - 100, 12, juce::Justification::centredLeft);
}
}
