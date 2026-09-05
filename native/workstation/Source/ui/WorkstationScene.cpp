#include "Workstation.h"
#include "Style.h"
#include <cmath>

namespace ws
{
std::vector<Batch> Workstation::scene() const
{
    std::vector<Batch> out;
    const auto ks = tl.sorted();
    const auto b = current();
    const bool haveWords = L.room == Room::sound ? ! sound.mono->empty() : (b.has_value() || (playBody && body.ready()));
    if (L.room == Room::frames)
    {
        Batch grid { Batch::lines, false, {} };
        for (int i = 1; i < 4; ++i)
        {
            const auto a = L.fromField ({ i / 4.0, 0.0 }), c = L.fromField ({ i / 4.0, 1.0 }), d = L.fromField ({ 0.0, i / 4.0 }), f = L.fromField ({ 1.0, i / 4.0 });
            grid.v.push_back (vertex (a, kLine, 1.0f)); grid.v.push_back (vertex (c, kLine, 1.0f));
            grid.v.push_back (vertex (d, kLine, 1.0f)); grid.v.push_back (vertex (f, kLine, 1.0f));
        }
        out.push_back (grid);
        Batch links { Batch::lines, false, {} };
        for (size_t i = 0; i + 1 < ks.size(); ++i) { links.v.push_back (vertex (L.fromField (ks[i].p), kDim, 1.0f)); links.v.push_back (vertex (L.fromField (ks[i + 1].p), kDim, 1.0f)); }
        if (pairLive())
        {
            const auto pa = L.fromField (pairPoint (0.0)), pb = L.fromField (pairPoint (1.0));
            links.v.push_back (vertex (pa, kChosen, 1.5f));
            links.v.push_back (vertex (pb, kChosen, 1.5f));
            const auto lo = L.fromField (pairPoint (kPushLow)), hi = L.fromField (pairPoint (kPushHigh));
            links.v.push_back (vertex (pa, kChosen, 1.0f)); links.v.push_back (vertex (lo, kChosen, 1.0f));
            links.v.push_back (vertex (pb, kChosen, 1.0f)); links.v.push_back (vertex (hi, kChosen, 1.0f));
            const float dx = pb.x - pa.x, dy = pb.y - pa.y, len = std::max (1e-3f, std::sqrt (dx * dx + dy * dy));
            const juce::Point<float> n (-dy / len * 5.0f, dx / len * 5.0f);
            for (const auto& e : { lo, hi }) { links.v.push_back (vertex (e - n, kChosen, 1.0f)); links.v.push_back (vertex (e + n, kChosen, 1.0f)); }
        }
        else if (b && ! playBody)
            for (int i = 0; i < 3; ++i) { links.v.push_back (vertex (L.fromField (*probe), kChosen, 1.0f)); links.v.push_back (vertex (L.fromField (lib.anchors[(size_t) b->anchors[(size_t) i]].p), kChosen, 1.0f)); }
        out.push_back (links);
        Batch pts { Batch::points, true, {} };
        for (int i = 0; i < (int) lib.anchors.size(); ++i)
        {
            if (! visible (i)) continue;
            const auto& a = lib.anchors[(size_t) i];
            const auto& f = lib.frames[(size_t) a.frame];
            const bool chosen = i == pairA || i == pairB || i == pickFor;
            pts.v.push_back (vertex (L.fromField (a.p), chosen ? kChosen : hueOf (f.m[0]), chosen || i == dragAnchor ? 12.0f : 8.0f));
        }
        Batch squares { Batch::points, false, {} };
        for (const auto& k : ks) squares.v.push_back (vertex (L.fromField (k.p), kData, 7.0f));
        if (probe && ! playBody) pts.v.push_back (vertex (L.fromField (*probe), kLive, 9.0f));
        if (mode == Mode::dragFrame) pts.v.push_back (vertex (dragPos, hueOf (lib.frames[(size_t) dragFrame].m[0]), 12.0f));
        if (mode == Mode::dragAnchor) pts.v.push_back (vertex (dragPos, kData, 12.0f));
        out.push_back (squares);
        out.push_back (pts);
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
            const juce::Rectangle<float> t (L.body.getX() + 8.0f + (i & 1) * 212.0f, L.body.getY() + 48.0f + (i >> 1) * 80.0f, 200.0f, 40.0f);
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
            std::vector<std::pair<int, double>> parents;
            if (L.room == Room::sound) {}
            else if (playBody && body.ready()) { const auto w = body.weights(); for (int i = 0; i < 4; ++i) parents.push_back ({ body.corner[(size_t) i], w[(size_t) i] }); }
            else if (b) for (int i = 0; i < 3; ++i) parents.push_back ({ lib.anchors[(size_t) b->anchors[(size_t) i]].frame, b->w[(size_t) i] });
            for (int s = 0; s < kRows; ++s)
            {
                const auto& r = rows[(size_t) s];
                if (! r.pole) continue;
                const auto here = L.armaXY (r.pHz, r.pR);
                for (const auto& [frameIdx, weight] : parents)
                {
                    if (weight < 0.08) continue;
                    const auto& pr = lib.frames[(size_t) frameIdx].rows[(size_t) s];
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
