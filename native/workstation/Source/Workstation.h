#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Model.h"

namespace ws
{
class Workstation : public juce::Component
{
public:
    Workstation (Library& library) : lib (library)
    {
        setOpaque (true);
        setSize (1280, 800);
    }

    Cube cube;
    std::optional<std::pair<double, double>> probe;
    double yaw = -0.55, pitch = 0.42;
    int armed = 0;
    juce::String status { "ready" };

    void paint (juce::Graphics& g) override
    {
        layout();
        g.fillAll (juce::Colours::black);
        g.setFont (mono (10.0f));
        paintPlane (g);
        paintCube (g);
        paintResponse (g);
        paintControl (g);
        paintDrag (g);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        if (keyCapture.contains (p)) { capture(); return; }
        if (keyClear.contains (p)) { clearCaptures(); return; }
        for (int i = 0; i < 3; ++i)
            if (barBox[(size_t) i].contains (p)) { mode = Mode::bar; barIdx = i; setBar (p.x); return; }
        for (int i = 0; i < 8; ++i)
            if (thumbBox[(size_t) i].contains (p)) { armed = i; repaint(); return; }
        if (const int n = nodeAt (p); n >= 0)
        {
            mode = Mode::drag;
            dragFrame = n;
            dragPos = p;
            repaint();
            return;
        }
        if (planeAx.contains (p)) { mode = Mode::probe; setProbe (p); return; }
        if (cubeBox.contains (p)) { mode = Mode::orbit; last = p; return; }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        switch (mode)
        {
            case Mode::drag: dragPos = p; hotCorner = nearestCorner (p); status = "drag " + lib.frames[(size_t) dragFrame].name + (hotCorner >= 0 ? "  ->  " + cube.poseName (hotCorner) : ""); repaint(); break;
            case Mode::probe: setProbe (p); break;
            case Mode::orbit:
                yaw += (p.x - last.x) * 0.01;
                pitch = juce::jlimit (-1.4, 1.4, pitch + (p.y - last.y) * 0.01);
                last = p;
                repaint();
                break;
            case Mode::bar: setBar (p.x); break;
            case Mode::none: break;
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (mode == Mode::drag)
        {
            const auto p = e.position;
            const int hot = nearestCorner (p);
            const auto& f = lib.frames[(size_t) dragFrame];
            const bool moved = p.getDistanceFrom (nodePos (f)) > 6.0f;
            if (hot >= 0) setCorner (hot, dragFrame);
            else if (! moved) setCorner (armed, dragFrame);
            dragFrame = -1;
            hotCorner = -1;
        }
        mode = Mode::none;
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (const int n = nodeAt (e.position); n >= 0)
        {
            const auto& f = lib.frames[(size_t) n];
            status = f.name + "  " + juce::String (f.f1) + " / " + juce::String (f.f2) + " Hz";
            repaint (planeBox.toNearestInt());
        }
    }

    void setCorner (int i, int frame)
    {
        cube.corner[(size_t) i] = frame;
        armed = (i + 1) % 8;
        status = cube.poseName (i) + " = " + lib.frames[(size_t) frame].name;
        repaint();
    }

    void capture()
    {
        if (! cube.ready()) { status = "fill all eight corners first"; repaint(); return; }
        Frame f;
        f.words = cube.wheelWords (lib);
        f.capture = true;
        f.name = "cap" + juce::String ((int) captures + 1) + " m" + juce::String (cube.morph, 2) + " q" + juce::String (cube.q, 2) + " z" + juce::String (cube.z, 2);
        const auto [x, y] = cube.planeXY (lib, cube.morph, cube.q, cube.z);
        place (f);
        f.u = x;
        f.v = y;
        lib.frames.push_back (f);
        ++captures;
        lib.triangulate();
        status = "captured " + f.name;
        repaint();
    }

    void clearCaptures()
    {
        lib.frames.erase (std::remove_if (lib.frames.begin(), lib.frames.end(), [] (const Frame& f) { return f.capture; }), lib.frames.end());
        for (auto& c : cube.corner)
            if (c >= (int) lib.frames.size()) c = -1;
        captures = 0;
        lib.triangulate();
        repaint();
    }

private:
    enum class Mode { none, drag, probe, orbit, bar };
    Library& lib;
    Mode mode = Mode::none;
    int dragFrame = -1, hotCorner = -1, barIdx = -1;
    size_t captures = 0;
    juce::Point<float> dragPos, last;

    juce::Rectangle<float> planeBox, cubeBox, respBox, ctrlBox, planeAx, respAx, keyCapture, keyClear;
    std::array<juce::Rectangle<float>, 3> barBox;
    std::array<juce::Rectangle<float>, 8> thumbBox;
    static constexpr float kBar = 14.0f;

    static juce::Font mono (float h) { return juce::Font (juce::FontOptions ("Courier New", h, juce::Font::plain)); }

    void layout()
    {
        const float W = (float) getWidth(), H = (float) getHeight();
        const float topH = std::round (H * 0.5f), cubeW = std::round (W * 0.42f);
        planeBox = { 0.0f, 0.0f, W, topH };
        cubeBox = { 0.0f, topH, cubeW, H - topH };
        const float respH = std::round ((H - topH) * 0.62f);
        respBox = { cubeW, topH, W - cubeW, respH };
        ctrlBox = { cubeW, topH + respH, W - cubeW, H - topH - respH };
        planeAx = { 44.0f, kBar + 10.0f, W - 60.0f, topH - kBar - 34.0f };
        respAx = { cubeW + 44.0f, topH + kBar + 10.0f, W - cubeW - 60.0f, respH - kBar - 34.0f };
    }

    float pu (double u) const { return planeAx.getX() + (float) u * planeAx.getWidth(); }
    float pv (double v) const { return planeAx.getY() + (1.0f - (float) v) * planeAx.getHeight(); }
    float rx (double hz) const { return respAx.getX() + (float) (std::log10 (hz / 20.0) / 3.0) * respAx.getWidth(); }
    float ry (double db) const { return respAx.getY() + (float) ((30.0 - db) / 60.0) * respAx.getHeight(); }
    juce::Point<float> nodePos (const Frame& f) const { return { pu (f.u), pv (f.v) }; }

    struct P3 { float x, y, depth; };
    P3 proj (double m, double qq, double zz) const
    {
        const double x = m - 0.5, y = qq - 0.5, d = zz - 0.5;
        const double cy = std::cos (yaw), sy = std::sin (yaw), cp = std::cos (pitch), sp = std::sin (pitch);
        const double x1 = x * cy + d * sy, d1 = -x * sy + d * cy;
        const double y2 = y * cp - d1 * sp, d2 = y * sp + d1 * cp;
        const float s = std::min (cubeBox.getWidth(), cubeBox.getHeight() - kBar) * 0.5f;
        return { cubeBox.getCentreX() + (float) x1 * s, cubeBox.getY() + kBar + (cubeBox.getHeight() - kBar) * 0.5f - (float) y2 * s, (float) d2 };
    }
    P3 cornerPos (int i) const { return proj ((i & 1) ? 1.0 : 0.0, (i & 2) ? 1.0 : 0.0, (i & 4) ? 1.0 : 0.0); }

    int nodeAt (juce::Point<float> p) const
    {
        for (int i = (int) lib.frames.size() - 1; i >= 0; --i)
            if (nodePos (lib.frames[(size_t) i]).getDistanceFrom (p) < 5.0f) return i;
        return -1;
    }

    int nearestCorner (juce::Point<float> p) const
    {
        int best = -1;
        float bd = 1e9f;
        for (int i = 0; i < 8; ++i)
        {
            const auto c = cornerPos (i);
            const float d = juce::Point<float> (c.x, c.y).getDistanceFrom (p);
            if (d < bd) { bd = d; best = i; }
        }
        return bd < 40.0f ? best : -1;
    }

    void setProbe (juce::Point<float> p)
    {
        const double u = juce::jlimit (0.0, 1.0, (double) (p.x - planeAx.getX()) / planeAx.getWidth());
        const double v = juce::jlimit (0.0, 1.0, 1.0 - (double) (p.y - planeAx.getY()) / planeAx.getHeight());
        probe = { u, v };
        if (cube.ready()) cube.solveFromPlane (lib, u, v);
        repaint();
    }

    void setBar (float x)
    {
        const auto& b = barBox[(size_t) barIdx];
        const double v = juce::jlimit (0.0, 1.0, (double) (x - b.getX()) / b.getWidth());
        if (barIdx == 0) cube.morph = v; else if (barIdx == 1) cube.q = v; else cube.z = v;
        repaint();
    }

    void panel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
    {
        g.setColour (juce::Colour (0xff666666));
        g.drawRect (r, 1.0f);
        g.setColour (juce::Colour (0xffbbbbbb));
        g.fillRect (r.getX() + 1.0f, r.getY() + 1.0f, r.getWidth() - 2.0f, kBar - 1.0f);
        g.setColour (juce::Colours::black);
        g.setFont (mono (10.0f).boldened());
        g.drawText (title, (int) r.getX() + 6, (int) r.getY() + 1, 400, (int) kBar - 2, juce::Justification::centredLeft);
        g.setFont (mono (10.0f));
    }

    void axes (juce::Graphics& g, juce::Rectangle<float> a, const std::vector<std::pair<float, juce::String>>& xt, const std::vector<std::tuple<float, juce::String, bool>>& yt)
    {
        g.setColour (juce::Colour (0xffbbbbbb));
        g.drawRect (a, 1.0f);
        const float dash[] = { 1.0f, 3.0f };
        for (const auto& [x, label] : xt)
        {
            g.setColour (juce::Colour (0xff444444));
            g.drawDashedLine ({ x, a.getY(), x, a.getBottom() }, dash, 2, 1.0f);
            g.setColour (juce::Colour (0xffbbbbbb));
            g.drawLine (x, a.getBottom(), x, a.getBottom() + 4.0f);
            g.drawText (label, (int) x - 20, (int) a.getBottom() + 5, 40, 12, juce::Justification::centred);
        }
        for (const auto& [y, label, solid] : yt)
        {
            g.setColour (solid ? juce::Colour (0xff888888) : juce::Colour (0xff444444));
            if (solid) g.drawLine (a.getX(), y, a.getRight(), y);
            else g.drawDashedLine ({ a.getX(), y, a.getRight(), y }, dash, 2, 1.0f);
            g.setColour (juce::Colour (0xffbbbbbb));
            g.drawLine (a.getX() - 4.0f, y, a.getX(), y);
            g.drawText (label, (int) a.getX() - 40, (int) y - 6, 32, 12, juce::Justification::centredRight);
        }
    }

    static juce::String octLabel (int i) { return "C" + juce::String (i); }

    void paintPlane (juce::Graphics& g)
    {
        panel (g, planeBox, "FRAME SPACE");
        std::vector<std::pair<float, juce::String>> xt;
        std::vector<std::tuple<float, juce::String, bool>> yt;
        for (int i = 1; i <= 9; ++i)
        {
            const double hz = 32.703 * std::pow (2.0, i - 1);
            xt.push_back ({ pu (gridPos (hz)), octLabel (i) });
            yt.push_back ({ pv (gridPos (hz)), octLabel (i), false });
        }
        axes (g, planeAx, xt, yt);
        g.setColour (juce::Colour (0xffbbbbbb));
        g.drawText ("2nd resonance", (int) planeAx.getX() + 4, (int) planeAx.getY() + 2, 120, 12, juce::Justification::centredLeft);
        g.drawText ("1st resonance", (int) planeAx.getRight() - 96, (int) planeAx.getBottom() - 14, 90, 12, juce::Justification::centredRight);
        if (probe && cube.ready())
        {
            const auto [x, y] = cube.planeXY (lib, cube.morph, cube.q, cube.z);
            g.setColour (juce::Colour (0xff00ffff));
            for (int i = 0; i < 8; ++i)
                g.drawLine (pu (x), pv (y), nodePos (lib.frames[(size_t) cube.corner[(size_t) i]]).x, nodePos (lib.frames[(size_t) cube.corner[(size_t) i]]).y, 1.0f);
            g.setColour (juce::Colours::white);
            g.drawRect (pu (x) - 4.0f, pv (y) - 4.0f, 8.0f, 8.0f, 1.0f);
        }
        else if (probe)
        {
            const auto blendInfo = planeBlend (probe->first, probe->second);
            juce::Path tri;
            for (size_t i = 0; i < blendInfo.parents.size(); ++i)
            {
                const auto p = nodePos (lib.frames[(size_t) blendInfo.parents[i]]);
                if (i == 0) tri.startNewSubPath (p); else tri.lineTo (p);
            }
            tri.closeSubPath();
            g.setColour (juce::Colour (0x1400ffff));
            g.fillPath (tri);
            g.setColour (juce::Colour (0xff00ffff));
            g.strokePath (tri, juce::PathStrokeType (1.0f));
            for (auto idx : blendInfo.parents)
                g.drawLine (pu (probe->first), pv (probe->second), nodePos (lib.frames[(size_t) idx]).x, nodePos (lib.frames[(size_t) idx]).y, 1.0f);
            g.setColour (juce::Colours::white);
            g.drawRect (pu (probe->first) - 4.0f, pv (probe->second) - 4.0f, 8.0f, 8.0f, 1.0f);
        }
        for (int i = 0; i < 8; ++i)
            if (cube.corner[(size_t) i] >= 0)
            {
                const auto n = nodePos (lib.frames[(size_t) cube.corner[(size_t) i]]);
                const auto c = cornerPos (i);
                g.setColour (juce::Colour (0x8000ffff));
                const float dash[] = { 2.0f, 3.0f };
                g.drawDashedLine ({ n.x, n.y, c.x, c.y }, dash, 2, 1.0f);
            }
        for (size_t i = 0; i < lib.frames.size(); ++i)
        {
            const auto& f = lib.frames[i];
            const auto p = nodePos (f);
            g.setColour ((int) i == dragFrame ? juce::Colours::white : f.capture ? juce::Colours::yellow : juce::Colour (0xffaaaaaa));
            g.fillRect (p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f);
        }
        g.setColour (juce::Colour (0xff00ffff));
        g.drawText (status, (int) (planeAx.getX() + planeAx.getWidth() * 0.55f), (int) planeBox.getBottom() - 14, 560, 12, juce::Justification::centredLeft);
    }

    struct Blend { std::vector<int> parents; std::vector<double> weights; };
    Blend planeBlend (double u, double v) const
    {
        for (const auto& t : lib.tris)
            if (const auto b = barycentric (u, v, lib.frames[(size_t) t.a], lib.frames[(size_t) t.b], lib.frames[(size_t) t.c]))
                if ((*b)[0] >= -1e-9 && (*b)[1] >= -1e-9 && (*b)[2] >= -1e-9)
                    return { { t.a, t.b, t.c }, { (*b)[0], (*b)[1], (*b)[2] } };
        std::vector<std::pair<double, int>> near;
        for (size_t i = 0; i < lib.frames.size(); ++i)
            near.push_back ({ std::hypot (lib.frames[i].u - u, lib.frames[i].v - v), (int) i });
        std::partial_sort (near.begin(), near.begin() + std::min<size_t> (3, near.size()), near.end());
        Blend out;
        double sum = 0.0;
        for (size_t i = 0; i < std::min<size_t> (3, near.size()); ++i) { out.parents.push_back (near[i].second); out.weights.push_back (1.0 / (near[i].first + 1e-6)); sum += out.weights.back(); }
        for (auto& w : out.weights) w /= sum;
        return out;
    }

    void thumb (juce::Graphics& g, const Words& w, juce::Rectangle<float> r)
    {
        const auto c = cascadeOf (w);
        juce::Path p;
        for (int i = 0; i <= 28; ++i)
        {
            const double hz = 20.0 * std::pow (1000.0, i / 28.0);
            const double d = juce::jlimit (-30.0, 30.0, responseDb (c, hz));
            const float x = r.getX() + i / 28.0f * r.getWidth(), y = r.getBottom() - (float) ((d + 30.0) / 60.0) * r.getHeight();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }

    void paintCube (juce::Graphics& g)
    {
        panel (g, cubeBox, "CUBE");
        std::array<P3, 8> P;
        for (int i = 0; i < 8; ++i) P[(size_t) i] = cornerPos (i);
        std::vector<std::pair<int, int>> edges;
        for (int i = 0; i < 8; ++i)
            for (int b : { 1, 2, 4 })
                if (! (i & b)) edges.push_back ({ i, i | b });
        std::sort (edges.begin(), edges.end(), [&] (auto& e1, auto& e2) { return P[(size_t) e1.first].depth + P[(size_t) e1.second].depth < P[(size_t) e2.first].depth + P[(size_t) e2.second].depth; });
        for (const auto& [i, j] : edges)
        {
            g.setColour (P[(size_t) i].depth + P[(size_t) j].depth < 0.0f ? juce::Colour (0xff555555) : juce::Colour (0xffbbbbbb));
            g.drawLine (P[(size_t) i].x, P[(size_t) i].y, P[(size_t) j].x, P[(size_t) j].y, 1.0f);
        }
        if (cube.ready())
        {
            const auto w = proj (cube.morph, cube.q, cube.z);
            const float dash[] = { 2.0f, 2.0f };
            g.setColour (juce::Colour (0xff00ffff));
            for (const auto f : { proj (cube.morph, cube.q, 0.0), proj (cube.morph, 0.0, cube.z), proj (0.0, cube.q, cube.z) })
                g.drawDashedLine ({ w.x, w.y, f.x, f.y }, dash, 2, 1.0f);
            g.setColour (juce::Colours::yellow);
            g.fillRect (w.x - 3.0f, w.y - 3.0f, 6.0f, 6.0f);
        }
        const float tw = 54.0f, th = 22.0f;
        const float ccx = cubeBox.getCentreX(), ccy = cubeBox.getY() + kBar + (cubeBox.getHeight() - kBar) * 0.5f;
        for (int i = 0; i < 8; ++i)
        {
            const auto c = P[(size_t) i];
            const float dx = c.x - ccx, dy = c.y - ccy, len = std::max (1.0f, std::hypot (dx, dy));
            const juce::Rectangle<float> box (c.x + dx / len * 46.0f - tw / 2.0f, c.y + dy / len * 30.0f - th / 2.0f, tw, th);
            thumbBox[(size_t) i] = box;
            g.setColour (juce::Colour (0xff555555));
            g.drawLine (c.x, c.y, box.getCentreX(), box.getCentreY(), 1.0f);
            g.setColour (juce::Colours::black);
            g.fillRect (box);
            g.setColour (i == hotCorner ? juce::Colours::white : i == armed ? juce::Colour (0xff00ffff) : juce::Colour (0xff777777));
            g.drawRect (box, i == hotCorner ? 2.0f : 1.0f);
            if (cube.corner[(size_t) i] >= 0)
            {
                g.setColour (juce::Colour (0xffdddddd));
                thumb (g, lib.frames[(size_t) cube.corner[(size_t) i]].words, box.reduced (2.0f));
            }
            else
            {
                g.setColour (juce::Colour (0xff444444));
                g.drawLine (box.getX() + 2.0f, box.getCentreY(), box.getRight() - 2.0f, box.getCentreY(), 1.0f);
            }
            g.setColour (juce::Colour (0xffbbbbbb));
            g.drawText (cube.poseName (i), (int) box.getX(), (int) box.getY() - 13, 160, 12, juce::Justification::centredLeft);
        }
    }

    void curve (juce::Graphics& g, const Words& w, int points)
    {
        const auto c = cascadeOf (w);
        juce::Path p;
        for (int i = 0; i <= points; ++i)
        {
            const double hz = 20.0 * std::pow (1000.0, (double) i / points);
            const float x = rx (hz), y = ry (juce::jlimit (-30.0, 30.0, responseDb (c, hz)));
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }

    void paintResponse (juce::Graphics& g)
    {
        panel (g, respBox, "RESPONSE");
        std::vector<std::pair<float, juce::String>> xt;
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            xt.push_back ({ rx (f), f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f, 0) });
        std::vector<std::tuple<float, juce::String, bool>> yt { { ry (20), "+20", false }, { ry (10), "+10", false }, { ry (0), "0", true }, { ry (-10), "-10", false }, { ry (-20), "-20", false } };
        axes (g, respAx, xt, yt);
        if (probe && ! cube.ready())
        {
            const auto b = planeBlend (probe->first, probe->second);
            std::vector<const Words*> parents;
            for (auto idx : b.parents) parents.push_back (&lib.frames[(size_t) idx].words);
            g.setColour (juce::Colour (0xff00ffff));
            curve (g, blend (parents, b.weights), 200);
        }
        if (cube.ready())
        {
            g.setColour (juce::Colours::white);
            curve (g, cube.wheelWords (lib), 240);
        }
    }

    void paintControl (juce::Graphics& g)
    {
        panel (g, ctrlBox, "CONTROL");
        const float x0 = ctrlBox.getX() + 34.0f, w = std::round (ctrlBox.getWidth() * 0.3f), h = 10.0f, y0 = ctrlBox.getY() + kBar + 10.0f;
        const juce::Colour cols[] = { juce::Colour (0xff00ccff), juce::Colour (0xffff00ff), juce::Colour (0xffff4444) };
        const char* names[] = { "M", "Q", "Z" };
        const double vals[] = { cube.morph, cube.q, cube.z };
        for (int i = 0; i < 3; ++i)
        {
            const float y = y0 + i * (h + 8.0f);
            barBox[(size_t) i] = { x0, y, w, h };
            g.setColour (juce::Colour (0xffbbbbbb));
            g.drawText (names[i], (int) ctrlBox.getX() + 10, (int) y - 1, 20, 12, juce::Justification::centredLeft);
            g.setColour (juce::Colours::black);
            g.fillRect (barBox[(size_t) i]);
            g.setColour (juce::Colour (0xff777777));
            g.drawRect (barBox[(size_t) i], 1.0f);
            g.setColour (cols[i]);
            g.fillRect (x0 + 1.0f, y + 1.0f, std::max (0.0f, (w - 2.0f) * (float) vals[i]), h - 2.0f);
            g.setColour (juce::Colour (0xffbbbbbb));
            g.drawText (juce::String (vals[i], 3) + "  " + cube.axisNames[(size_t) i][0] + " > " + cube.axisNames[(size_t) i][1], (int) (x0 + w + 8.0f), (int) y - 1, 200, 12, juce::Justification::centredLeft);
        }
        const float kx = x0 + w + 180.0f;
        keyCapture = { kx, y0, 96.0f, 16.0f };
        keyClear = { kx + 104.0f, y0, 96.0f, 16.0f };
        for (const auto& [box, label] : { std::pair { keyCapture, "CAPTURE" }, std::pair { keyClear, "CLEAR CAPS" } })
        {
            g.setColour (juce::Colour (0xff222222));
            g.fillRect (box);
            g.setColour (juce::Colour (0xff999999));
            g.drawRect (box, 1.0f);
            g.setColour (juce::Colour (0xffdddddd));
            g.drawText (label, box.toNearestInt().reduced (6, 0), juce::Justification::centredLeft);
        }
    }

    void paintDrag (juce::Graphics& g)
    {
        if (dragFrame < 0) return;
        const auto& f = lib.frames[(size_t) dragFrame];
        const auto n = nodePos (f);
        const float dash[] = { 2.0f, 3.0f };
        g.setColour (juce::Colour (0x8000ffff));
        g.drawDashedLine ({ n.x, n.y, dragPos.x, dragPos.y }, dash, 2, 1.0f);
        g.setColour (juce::Colours::white);
        thumb (g, f.words, { dragPos.x - 27.0f, dragPos.y - 11.0f, 54.0f, 22.0f });
    }
};
}
