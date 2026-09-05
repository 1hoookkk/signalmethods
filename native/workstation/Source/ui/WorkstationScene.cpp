#include "Workstation.h"
#include "Style.h"
#include <algorithm>
#include <cmath>

namespace ws
{
namespace
{
juce::Colour parula (double t)
{
    static const float stops[5][3] = { { 0.24f, 0.15f, 0.66f }, { 0.10f, 0.54f, 0.83f }, { 0.16f, 0.73f, 0.62f }, { 0.65f, 0.79f, 0.28f }, { 0.98f, 0.80f, 0.15f } };
    const double u = juce::jlimit (0.0, 0.9999, t) * 4.0;
    const int i = (int) u;
    const float f = (float) (u - i);
    return juce::Colour::fromFloatRGBA (stops[i][0] + (stops[i + 1][0] - stops[i][0]) * f, stops[i][1] + (stops[i + 1][1] - stops[i][1]) * f, stops[i][2] + (stops[i + 1][2] - stops[i][2]) * f, 1.0f);
}

void dotted (Batch& b, juce::Point<float> a, juce::Point<float> c, juce::Colour colour)
{
    const float dx = c.x - a.x, dy = c.y - a.y, len = std::sqrt (dx * dx + dy * dy);
    if (len < 1.0f) return;
    const int n = std::max (1, (int) (len / 5.0f));
    for (int i = 0; i < n; ++i)
    {
        const float t0 = i / (float) n, t1 = t0 + 1.5f / len;
        b.v.push_back (vertex ({ a.x + dx * t0, a.y + dy * t0 }, colour, 1.0f));
        b.v.push_back (vertex ({ a.x + dx * std::min (1.0f, t1), a.y + dy * std::min (1.0f, t1) }, colour, 1.0f));
    }
}
}

void Workstation::sceneStitch (std::vector<Batch>& out) const
{
    const auto& v = view;
    const auto fade = [&] (juce::Colour c, const Vec3& p) { return c.interpolatedWith (kPanel, (float) (0.3 * v.depth01 (p))); };
    Batch grid { Batch::lines, false, {} };
    const bool farX = v.farPlane (0), farY = v.farPlane (1);
    const double fx = farX ? v.hi.x : v.lo.x, fy = farY ? v.hi.y : v.lo.y, fz = v.lo.z;
    for (double hz : { 100.0, 1000.0, 10000.0 })
    {
        const double x = std::log10 (hz / 20.0) / 3.0 - 0.5;
        dotted (grid, v.project ({ x, v.lo.y, fz }), v.project ({ x, v.hi.y, fz }), kLine);
        dotted (grid, v.project ({ x, fy, v.lo.z }), v.project ({ x, fy, v.hi.z }), kLine);
    }
    for (double db : { -20.0, 0.0, 20.0 })
    {
        const double y = db / 60.0;
        dotted (grid, v.project ({ v.lo.x, y, fz }), v.project ({ v.hi.x, y, fz }), db == 0.0 ? kRule : kLine);
        dotted (grid, v.project ({ fx, y, v.lo.z }), v.project ({ fx, y, v.hi.z }), db == 0.0 ? kRule : kLine);
    }
    for (double db : { 20.0, 40.0, 60.0 })
    {
        const double z = db / 60.0;
        dotted (grid, v.project ({ fx, v.lo.y, z }), v.project ({ fx, v.hi.y, z }), kLine);
        dotted (grid, v.project ({ v.lo.x, fy, z }), v.project ({ v.hi.x, fy, z }), kLine);
    }
    out.push_back (grid);
    Batch box { Batch::lines, false, {} };
    for (int i = 0; i < 12; ++i)
    {
        const int axis = i / 4, k = i % 4;
        Vec3 a, c;
        if (axis == 0) { a = { v.lo.x, (k & 1) ? v.hi.y : v.lo.y, (k & 2) ? v.hi.z : v.lo.z }; c = a; c.x = v.hi.x; }
        else if (axis == 1) { a = { (k & 1) ? v.hi.x : v.lo.x, v.lo.y, (k & 2) ? v.hi.z : v.lo.z }; c = a; c.y = v.hi.y; }
        else { a = { (k & 1) ? v.hi.x : v.lo.x, (k & 2) ? v.hi.y : v.lo.y, v.lo.z }; c = a; c.z = v.hi.z; }
        box.v.push_back (vertex (v.project (a), kFrame, 1.0f));
        box.v.push_back (vertex (v.project (c), kFrame, 1.0f));
    }
    out.push_back (box);
    const auto near = st.neighbours (openFace);
    const auto isNear = [&] (int i) { return std::find (near.begin(), near.end(), i) != near.end(); };
    std::vector<int> activeNodes;
    if (picked && picked->node >= 0) activeNodes.push_back (picked->node);
    if (pairLive()) { activeNodes.push_back (pairA); activeNodes.push_back (pairB); }
    if (spot && spot->node >= 0) activeNodes.push_back (spot->node);
    static const juce::Colour groupColour[kGroups] = { juce::Colour (0xff0072bd), juce::Colour (0xff1f9e89), juce::Colour (0xff5c5c5c), juce::Colour (0xffd95319), juce::Colour (0xff7e2f8e), juce::Colour (0xffedb120), juce::Colour (0xffa2142f) };
    Batch stems { Batch::lines, false, {} };
    Batch shelf { Batch::points, true, {} };
    for (const auto& it : st.items)
    {
        if (! floorOpen (it.group) || it.face == openFace) continue;
        const auto& c = it.p;
        const bool lit = it.face >= 0 && isNear (it.face);
        const float dimness = openFace >= 0 && ! lit ? 0.22f : 1.0f;
        const auto stemColour = fade (kDim, c).withMultipliedAlpha (dimness);
        stems.v.push_back (vertex (v.project ({ c.x, c.y, 0.0 }), stemColour, 1.0f));
        stems.v.push_back (vertex (v.project (c), stemColour, 1.0f));
        const auto base = groupColour[juce::jlimit (0, kGroups - 1, it.group)];
        shelf.v.push_back (vertex (v.project (c), lit ? kChosen : fade (base, c).withMultipliedAlpha (dimness), lit ? 8.0f : it.stub >= 0 ? 5.0f : 6.0f));
    }
    Batch fill { Batch::tris, false, {} };
    Batch wire { Batch::lines, false, {} };
    static const juce::Colour axisColour[3] = { juce::Colour (0xff303030), juce::Colour (0xff808080), juce::Colour (0xffb4b4b4) };
    if (openFace >= 0 && st.faces[(size_t) openFace].nodes.size() >= 4)
    {
        const auto& f = st.faces[(size_t) openFace];
        double res = 0.0;
        int count = 0;
        for (int k = 0; k < 4; ++k)
            for (const auto& g : geometryOf (st.wordsOf (f.nodes[(size_t) k]))) if (g.pole && g.pR > 0.5) { res += resDb (g.pR); ++count; }
        const auto colour = parula (count > 0 ? res / count / 45.0 : 0.0).withAlpha (0.35f);
        const juce::Point<float> c[4] = { v.project (st.nodes[(size_t) f.nodes[0]].p), v.project (st.nodes[(size_t) f.nodes[1]].p), v.project (st.nodes[(size_t) f.nodes[3]].p), v.project (st.nodes[(size_t) f.nodes[2]].p) };
        for (int k : { 0, 1, 2, 0, 2, 3 }) fill.v.push_back (vertex (c[k], colour, 0.0f));
        for (const auto& e : st.edges)
        {
            if (e.body != openFace && ! isNear (e.body)) continue;
            const auto& a = st.nodes[(size_t) e.a].p;
            const auto& b = st.nodes[(size_t) e.b].p;
            const auto col = e.body == openFace ? axisColour[e.axis] : kLine;
            wire.v.push_back (vertex (v.project (a), col, 1.0f));
            wire.v.push_back (vertex (v.project (b), col, 1.0f));
        }
        for (const int n : f.nodes)
        {
            const auto& p = st.nodes[(size_t) n].p;
            stems.v.push_back (vertex (v.project ({ p.x, p.y, 0.0 }), kRule, 1.0f));
            stems.v.push_back (vertex (v.project (p), kRule, 1.0f));
        }
    }
    if (spot && spot->stub >= 0)
    {
        const auto& s = st.stubs[(size_t) spot->stub];
        wire.v.push_back (vertex (v.project (st.nodes[(size_t) s.node].p), kChosen, 1.0f));
        wire.v.push_back (vertex (v.project (s.p), kChosen, 1.0f));
    }
    out.push_back (fill);
    out.push_back (stems);
    out.push_back (wire);
    out.push_back (shelf);
    Batch marks { Batch::points, true, {} };
    if (openFace >= 0)
        for (const int n : st.faces[(size_t) openFace].nodes)
        {
            const bool lit = std::find (activeNodes.begin(), activeNodes.end(), n) != activeNodes.end();
            marks.v.push_back (vertex (v.project (st.nodes[(size_t) n].p), lit ? kChosen : kData, lit ? 7.0f : 5.0f));
        }
    out.push_back (marks);
    Batch links { Batch::lines, false, {} };
    const auto ks = tl.sorted();
    for (size_t i = 0; i + 1 < ks.size(); ++i) { links.v.push_back (vertex (v.project (st.positionOf (ks[i].spot)), kDim, 1.0f)); links.v.push_back (vertex (v.project (st.positionOf (ks[i + 1].spot)), kDim, 1.0f)); }
    if (spot && spot->free && ! playBody)
    {
        const auto at = st.positionOf (*spot);
        for (const auto& n : st.nearest (at, 4, open))
        {
            const auto colour = kChosen.withAlpha ((float) juce::jlimit (0.15, 1.0, n.weight * 2.0));
            links.v.push_back (vertex (v.project (at), colour, 1.0f));
            links.v.push_back (vertex (v.project (n.item.p), colour, 1.0f));
        }
        links.v.push_back (vertex (v.project ({ at.x, at.y, 0.0 }), kLive, 1.0f));
        links.v.push_back (vertex (v.project (at), kLive, 1.0f));
    }
    if (pairLive())
    {
        const auto pa = v.project (pairPoint (0.0)), pb = v.project (pairPoint (1.0));
        links.v.push_back (vertex (pa, kChosen, 1.5f));
        links.v.push_back (vertex (pb, kChosen, 1.5f));
        const auto lo = v.project (pairPoint (kPushLow)), hi = v.project (pairPoint (kPushHigh));
        links.v.push_back (vertex (pa, kChosen, 1.0f)); links.v.push_back (vertex (lo, kChosen, 1.0f));
        links.v.push_back (vertex (pb, kChosen, 1.0f)); links.v.push_back (vertex (hi, kChosen, 1.0f));
        const float dx = pb.x - pa.x, dy = pb.y - pa.y, len = std::max (1e-3f, std::sqrt (dx * dx + dy * dy));
        const juce::Point<float> n (-dy / len * 5.0f, dx / len * 5.0f);
        for (const auto& e : { lo, hi }) { links.v.push_back (vertex (e - n, kChosen, 1.0f)); links.v.push_back (vertex (e + n, kChosen, 1.0f)); }
    }
    out.push_back (links);
    Batch pts { Batch::points, true, {} };
    Batch squares { Batch::points, false, {} };
    for (const auto& k : ks) squares.v.push_back (vertex (v.project (st.positionOf (k.spot)), kData, 7.0f));
    for (int a : { pairA, pairB }) if (pairLive() && a >= 0) pts.v.push_back (vertex (v.project (st.nodes[(size_t) a].p), kChosen, 10.0f));
    if (picked) squares.v.push_back (vertex (v.project (st.positionOf (*picked)), kChosen, 11.0f));
    if (pairLive()) pts.v.push_back (vertex (v.project (pairPoint (pairT)), kLive, 9.0f));
    else if (spot && ! playBody) pts.v.push_back (vertex (v.project (st.positionOf (*spot)), kLive, 9.0f));
    if (mode == Mode::dragFrame) pts.v.push_back (vertex (dragPos, hueOf (lib.frames[(size_t) dragFrame].m[0]), 12.0f));
    if (mode == Mode::dragSpot) pts.v.push_back (vertex (dragPos, kData, 12.0f));
    out.push_back (squares);
    out.push_back (pts);
}

std::vector<Batch> Workstation::scene() const
{
    std::vector<Batch> out;
    const bool haveWords = haveSound();
    if (L.room == Room::frames)
    {
        sceneStitch (out);
        if (playBody && body.ready())
        {
            const auto sq = L.square;
            Batch sqLines { Batch::lines, false, {} };
            sqLines.v.push_back (vertex ({ sq.getCentreX(), sq.getY() }, kLine, 1.0f)); sqLines.v.push_back (vertex ({ sq.getCentreX(), sq.getBottom() }, kLine, 1.0f));
            sqLines.v.push_back (vertex ({ sq.getX(), sq.getCentreY() }, kLine, 1.0f)); sqLines.v.push_back (vertex ({ sq.getRight(), sq.getCentreY() }, kLine, 1.0f));
            out.push_back (sqLines);
        }
        if (body.ready())
        {
            const auto sq = L.square;
            Batch wheel { Batch::points, true, {} };
            for (int i = 0; i < 4; ++i)
                wheel.v.push_back (vertex ({ i & 1 ? sq.getRight() : sq.getX(), i & 2 ? sq.getY() : sq.getBottom() }, hueOf (lib.frames[(size_t) body.corner[(size_t) i]].m[0]), 8.0f));
            wheel.v.push_back (vertex (L.wheelXY (body.morph, body.q), kLive, 9.0f));
            out.push_back (wheel);
        }
    }
    if (L.room == Room::edit)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (body.corner[(size_t) i] < 0) continue;
            const auto t = L.thumbRect (i);
            Batch thumb { Batch::strip, false, {} };
            const auto cv = curveOf (lib.frames[(size_t) body.corner[(size_t) i]].words);
            for (int k = 0; k < kCurvePoints; ++k)
                thumb.v.push_back (vertex ({ t.getX() + k / float (kCurvePoints - 1) * t.getWidth(), t.getY() + (float) ((30.0 - juce::jlimit (-30.0, 30.0, cv[(size_t) k])) / 60.0) * t.getHeight() }, i == editing ? kChosen : kDim, 1.0f));
            out.push_back (thumb);
        }
    }
    if (L.room == Room::edit && editing >= 0 && body.corner[(size_t) editing] >= 0)
    {
        const auto& f = lib.frames[(size_t) body.corner[(size_t) editing]];
        Batch curves { Batch::strip, false, {} };
        const auto cr = L.cascade;
        const auto cv = curveOf (f.words);
        for (int i = 0; i < kCurvePoints; ++i) curves.v.push_back (vertex ({ L.sx (cr, 20.0 * std::pow (1000.0, i / double (kCurvePoints - 1))), L.sy (cr, juce::jlimit (-30.0, 30.0, cv[(size_t) i])) }, kData, 1.5f));
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
                sc.v.push_back (vertex ({ L.sx (r, hz), L.sy (r, juce::jlimit (-30.0, 30.0, sectionDb (f.words, s, hz))) }, on ? kData : kDim, 1.2f));
            }
            out.push_back (sc);
            if (g.pole) handles.v.push_back (vertex ({ L.sx (r, g.pHz), L.sy (r, juce::jlimit (-30.0, 30.0, sectionDb (f.words, s, g.pHz))) }, kData, 7.0f));
            if (g.zero) zeroHandles.v.push_back (vertex ({ L.sx (r, g.zHz), L.sy (r, juce::jlimit (-30.0, 30.0, sectionDb (f.words, s, g.zHz))) }, kChosen, 6.0f));
        }
        out.push_back (handles);
        out.push_back (zeroHandles);
    }
    if (L.room == Room::sound && ! sound.mono->empty())
    {
        Batch spec { Batch::strip, false, {} };
        const auto mag = sound.sliceMagnitude (sound.slice);
        for (int i = 0; i < kCurvePoints; ++i) spec.v.push_back (vertex ({ L.rx (20.0 * std::pow (1000.0, i / double (kCurvePoints - 1))), L.ry (juce::jlimit (-30.0, 30.0, (double) mag[(size_t) i] + 30.0)) }, kDim, 1.0f));
        out.push_back (spec);
    }
    Batch marker { Batch::lines, false, {} };
    marker.v.push_back (vertex ({ L.rx (fieldHz), L.ry (30.0) }, kLine, 1.0f));
    marker.v.push_back (vertex ({ L.rx (fieldHz), L.ry (-30.0) }, kLine, 1.0f));
    out.push_back (marker);
    const auto now = live();
    if (haveWords)
    {
        const auto colour = L.room == Room::sound ? kChosen : kData;
        const auto cascade = cascadeOf (now.words);
        Batch curve { Batch::strip, false, {} };
        double prevDb = 0.0, prevHz = 20.0;
        for (int i = 0; i < kCurvePoints; ++i)
        {
            const double hz = 20.0 * std::pow (1000.0, i / double (kCurvePoints - 1));
            const double db = responseDb (cascade, hz);
            const bool in = std::abs (db) <= 30.0, wasIn = i == 0 ? in : std::abs (prevDb) <= 30.0;
            if (in != wasIn && i > 0)
            {
                const double edge = db > 30.0 || prevDb > 30.0 ? 30.0 : -30.0;
                const double f = (edge - prevDb) / (db - prevDb);
                const double hzX = prevHz * std::pow (hz / prevHz, f);
                curve.v.push_back (vertex ({ L.rx (hzX), L.ry (edge) }, colour, 1.5f));
                if (! in) { out.push_back (curve); curve.v.clear(); }
            }
            if (in) curve.v.push_back (vertex ({ L.rx (hz), L.ry (db) }, colour, 1.5f));
            prevDb = db;
            prevHz = hz;
        }
        if (curve.v.size() > 1) out.push_back (curve);
        Batch ticks { Batch::lines, false, {} };
        for (const auto& e : excessOf (now.words))
        {
            const float y = L.ry (e.above ? 30.0 : -30.0), d = e.above ? -6.0f : 6.0f;
            ticks.v.push_back (vertex ({ L.rx (e.hz), y }, kChosen, 1.0f));
            ticks.v.push_back (vertex ({ L.rx (e.hz), y + d }, kChosen, 1.0f));
        }
        out.push_back (ticks);
    }
    {
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
        if (haveWords)
        {
            const auto rows = geometryOf (now.words);
            Batch glides { Batch::lines, false, {} };
            Batch poles { Batch::points, false, {} };
            Batch zeros { Batch::points, true, {} };
            std::vector<std::pair<Words, double>> parents;
            if (L.room == Room::sound) {}
            else if (playBody && body.ready()) { const auto w = body.weights(); for (int i = 0; i < 4; ++i) parents.push_back ({ lib.frames[(size_t) body.corner[(size_t) i]].words, w[(size_t) i] }); }
            else if (pairLive()) { parents.push_back ({ st.wordsOf (pairA), 1.0 - pairT }); parents.push_back ({ st.wordsOf (pairB), pairT }); }
            else if (spot && spot->edge >= 0) { const auto& e = st.edges[(size_t) spot->edge]; parents.push_back ({ st.wordsOf (e.a), 1.0 - spot->t }); parents.push_back ({ st.wordsOf (e.b), spot->t }); }
            else if (spot && spot->face >= 0) { const auto c = st.cornersOf (spot->face); const double m = spot->m, q = spot->q; const double w[4] = { (1 - m) * (1 - q), m * (1 - q), (1 - m) * q, m * q }; for (int i = 0; i < 4; ++i) parents.push_back ({ c[(size_t) i], w[i] }); }
            for (int s = 0; s < kRows; ++s)
            {
                const auto& r = rows[(size_t) s];
                if (! r.pole) continue;
                const auto here = L.armaXY (r.pHz, r.pR);
                for (const auto& [parentWords, weight] : parents)
                {
                    if (weight < 0.3) continue;
                    const auto pr = geometryOf (parentWords)[(size_t) s];
                    if (! pr.pole) continue;
                    glides.v.push_back (vertex (here, kChosen, 1.0f));
                    glides.v.push_back (vertex (L.armaXY (pr.pHz, pr.pR), kChosen, 1.0f));
                }
                poles.v.push_back (vertex (here, now.guarded[(size_t) s] ? kChosen : kData, 8.0f));
                if (r.zero && r.zR > 0.01) zeros.v.push_back (vertex (L.armaXY (r.zHz, r.zR), kChosen, 7.0f));
            }
            out.push_back (glides);
            out.push_back (poles);
            out.push_back (zeros);
        }
    }
    if (L.timelineOpen)
    {
        const auto ks = tl.sorted();
        Batch tline { Batch::lines, false, {} };
        for (size_t i = 0; i + 1 < ks.size(); ++i) { tline.v.push_back (vertex ({ L.tx (ks[i].t), L.tlAx.getCentreY() }, kDim, 1.0f)); tline.v.push_back (vertex ({ L.tx (ks[i + 1].t), L.tlAx.getCentreY() }, kDim, 1.0f)); }
        tline.v.push_back (vertex ({ L.tx (tl.playhead), L.tlAx.getY() }, kLive, 1.0f));
        tline.v.push_back (vertex ({ L.tx (tl.playhead), L.tlAx.getBottom() }, kLive, 1.0f));
        out.push_back (tline);
        Batch tpts { Batch::points, false, {} };
        for (const auto& k : ks) tpts.v.push_back (vertex ({ L.tx (k.t), L.tlAx.getCentreY() }, kData, 8.0f));
        out.push_back (tpts);
    }
    return out;
}
}
