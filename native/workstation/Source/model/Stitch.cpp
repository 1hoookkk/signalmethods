#include "Stitch.h"
#include <algorithm>
#include <cmath>

namespace ws
{
namespace
{
const char* const kAxisNames[3] = { "MORPH", "Q", "Z" };

Vec3 mix (const Vec3& a, const Vec3& b, double t) { return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t }; }
}

bool Stitch::loadJson (const juce::File& file)
{
    const auto v = juce::JSON::parse (file);
    if (! v.isObject()) return false;
    nodes.clear(); edges.clear(); faces.clear(); stubs.clear();
    floorGap = (double) v.getProperty ("floor_gap", 1.0);
    if (const auto* fl = v.getProperty ("floors", juce::var()).getArray()) floorCount = fl->size();
    const juce::StringArray floors = [&] { juce::StringArray s; if (const auto* fl = v.getProperty ("floors", juce::var()).getArray()) for (const auto& f : *fl) s.add (f.toString()); return s; }();
    if (const auto* arr = v.getProperty ("nodes", juce::var()).getArray())
        for (const auto& n : *arr)
        {
            Node node;
            node.floor = floors.indexOf (n.getProperty ("floor", "").toString());
            node.datum = (double) n.getProperty ("datum", kDatumHz);
            node.faces = (int) n.getProperty ("faces", 0);
            if (const auto* m = n.getProperty ("members", juce::var()).getArray()) node.members = m->size();
            node.p = { (double) n.getProperty ("x", 0.0), (double) n.getProperty ("y", 0.0), (double) n.getProperty ("z", 0.0) };
            if (const auto* rows = n.getProperty ("words", juce::var()).getArray())
                for (const auto& row : *rows)
                {
                    std::array<std::uint16_t, kWords> r {};
                    if (const auto* ws = row.getArray()) for (int k = 0; k < kWords && k < ws->size(); ++k) r[(size_t) k] = (std::uint16_t) (int) (*ws)[k];
                    node.rows.push_back (r);
                }
            Words raw {};
            for (int s = 0; s < kRows; ++s)
                for (int k = 0; k < kWords; ++k) raw[(size_t) s][(size_t) k] = s < (int) node.rows.size() ? node.rows[(size_t) s][(size_t) k] : trench::core::kIdentitySection[(size_t) k];
            node.chord = decompile (raw, node.datum);
            node.words = node.datum == kDatumHz ? raw : compile (node.chord, kDatumHz);
            nodes.push_back (node);
        }
    if (const auto* arr = v.getProperty ("edges", juce::var()).getArray())
        for (const auto& e : *arr)
        {
            Edge edge;
            edge.a = (int) e.getProperty ("a", 0);
            edge.b = (int) e.getProperty ("b", 0);
            edge.body = (int) e.getProperty ("body", 0);
            edge.floor = floors.indexOf (e.getProperty ("floor", "").toString());
            const auto axis = e.getProperty ("axis", "MORPH").toString();
            edge.axis = axis == "Z" ? 2 : axis == "Q" ? 1 : 0;
            edges.push_back (edge);
        }
    if (const auto* arr = v.getProperty ("faces", juce::var()).getArray())
        for (const auto& f : *arr)
        {
            Face face;
            face.name = f.getProperty ("name", "").toString();
            face.floor = floors.indexOf (f.getProperty ("floor", "").toString());
            face.datum = (double) f.getProperty ("datum", kDatumHz);
            if (const auto* ns = f.getProperty ("nodes", juce::var()).getArray()) for (const auto& n : *ns) face.nodes.push_back ((int) n);
            faces.push_back (face);
        }
    if (const auto* arr = v.getProperty ("stubs", juce::var()).getArray())
        for (const auto& s : *arr)
        {
            Stub stub;
            stub.name = s.getProperty ("name", "").toString();
            stub.floor = floors.indexOf (s.getProperty ("floor", "").toString());
            stub.node = (int) s.getProperty ("node", 0);
            stub.p = { (double) s.getProperty ("x", 0.0), (double) s.getProperty ("y", 0.0), (double) s.getProperty ("z", 0.0) };
            if (const auto* rows = s.getProperty ("words", juce::var()).getArray())
                for (int r = 0; r < kRows && r < rows->size(); ++r)
                    if (const auto* ws = (*rows)[r].getArray()) for (int k = 0; k < kWords && k < ws->size(); ++k) stub.words[(size_t) r][(size_t) k] = (std::uint16_t) (int) (*ws)[k];
            stubs.push_back (stub);
        }
    usedFloors.clear();
    for (int f = 0; f < floorCount; ++f)
        for (const auto& n : nodes) if (n.floor == f) { usedFloors.push_back (f); break; }
    return ! nodes.empty();
}

Words Stitch::wordsOf (int node) const
{
    Words w {};
    for (int s = 0; s < kRows; ++s)
        for (int k = 0; k < kWords; ++k) w[(size_t) s][(size_t) k] = trench::core::kIdentitySection[(size_t) k];
    if (node < 0 || node >= (int) nodes.size()) return w;
    return nodes[(size_t) node].words;
}

std::array<Words, 4> Stitch::cornersOf (int face) const
{
    std::array<Words, 4> c;
    const auto& f = faces[(size_t) face];
    for (int i = 0; i < 4; ++i) c[(size_t) i] = wordsOf (f.nodes[(size_t) std::min<int> (i, (int) f.nodes.size() - 1)]);
    return c;
}

std::vector<std::pair<int, double>> Stitch::nearestFaces (int floor, double x, double y, int count) const
{
    std::vector<std::pair<double, int>> d;
    for (int i = 0; i < (int) faces.size(); ++i)
        if (faces[(size_t) i].floor == floor && i < (int) centres.size())
        {
            const auto& c = centres[(size_t) i];
            d.push_back ({ std::hypot (c.x - x, c.y - y), i });
        }
    std::sort (d.begin(), d.end());
    std::vector<std::pair<int, double>> out;
    double total = 0.0;
    for (int k = 0; k < count && k < (int) d.size(); ++k) { const double w = 1.0 / std::max (1e-4, d[(size_t) k].first); out.push_back ({ d[(size_t) k].second, w }); total += w; }
    for (auto& [i, w] : out) w /= std::max (1e-12, total);
    return out;
}

Morph Stitch::soundAt (const Spot& s) const
{
    Morph m;
    if (s.floor >= 0)
    {
        const auto near = nearestFaces (s.floor, s.x, s.y, 4);
        std::vector<Words> ws;
        std::vector<double> wt;
        for (const auto& [i, w] : near) { ws.push_back (wheelMorph (cornersOf (i), 0.5, 0.5).words); wt.push_back (w); }
        std::vector<const Words*> parents;
        for (const auto& w : ws) parents.push_back (&w);
        if (! parents.empty()) m.words = blend (parents, wt);
        return m;
    }
    if (s.stub >= 0 && s.stub < (int) stubs.size()) { m.words = stubs[(size_t) s.stub].words; return m; }
    if (s.node >= 0) { m.words = wordsOf (s.node); return m; }
    if (s.edge >= 0 && s.edge < (int) edges.size()) return pairMorph (wordsOf (edges[(size_t) s.edge].a), wordsOf (edges[(size_t) s.edge].b), juce::jlimit (0.0, 1.0, s.t));
    if (s.face >= 0 && s.face < (int) faces.size()) return wheelMorph (cornersOf (s.face), juce::jlimit (0.0, 1.0, s.m), juce::jlimit (0.0, 1.0, s.q));
    return m;
}

Vec3 Stitch::positionOf (const Spot& s) const
{
    if (s.floor >= 0) return { s.x, s.y, floorZ (s.floor) };
    if (s.stub >= 0 && s.stub < (int) stubs.size()) return stubs[(size_t) s.stub].p;
    if (s.node >= 0 && s.node < (int) nodes.size()) return nodes[(size_t) s.node].p;
    if (s.edge >= 0 && s.edge < (int) edges.size()) return mix (nodes[(size_t) edges[(size_t) s.edge].a].p, nodes[(size_t) edges[(size_t) s.edge].b].p, s.t);
    if (s.face >= 0 && s.face < (int) faces.size())
    {
        const auto& f = faces[(size_t) s.face];
        const auto c = [&] (int i) { return nodes[(size_t) f.nodes[(size_t) std::min<int> (i, (int) f.nodes.size() - 1)]].p; };
        return mix (mix (c (0), c (1), s.m), mix (c (2), c (3), s.m), s.q);
    }
    return {};
}

juce::String Stitch::nameOf (const Spot& s) const
{
    if (s.floor >= 0)
    {
        juce::String out;
        for (const auto& [i, w] : nearestFaces (s.floor, s.x, s.y, 4)) out += (out.isEmpty() ? "" : "   ") + juce::String ((int) std::round (w * 100)) + "% " + faces[(size_t) i].name;
        return out;
    }
    if (s.stub >= 0 && s.stub < (int) stubs.size()) return stubs[(size_t) s.stub].name;
    if (s.node >= 0 && s.node < (int) nodes.size()) return "node " + juce::String (s.node) + "  " + juce::String (nodes[(size_t) s.node].members) + " corners";
    if (s.edge >= 0 && s.edge < (int) edges.size()) { const auto& e = edges[(size_t) s.edge]; return faces[(size_t) e.body].name + "  " + kAxisNames[e.axis] + "  " + juce::String (s.t, 2); }
    if (s.face >= 0 && s.face < (int) faces.size()) return faces[(size_t) s.face].name + "  M " + juce::String (s.m, 2) + "  Q " + juce::String (s.q, 2);
    return {};
}

int Stitch::addStub (const juce::String& name, const Words& words, const Spot& near, int floor)
{
    Stub s;
    s.name = name;
    s.floor = floor;
    s.words = words;
    int anchor = 0;
    if (near.node >= 0) anchor = near.node;
    else if (near.edge >= 0) anchor = edges[(size_t) near.edge].a;
    else if (near.face >= 0) anchor = faces[(size_t) near.face].nodes[0];
    else if (near.stub >= 0) anchor = stubs[(size_t) near.stub].node;
    else if (near.floor >= 0) { const auto nf = nearestFaces (near.floor, near.x, near.y, 1); if (! nf.empty()) anchor = faces[(size_t) nf[0].first].nodes[0]; }
    s.node = anchor;
    const auto at = positionOf (near);
    s.p = gridPlace (words, at.z);
    stubs.push_back (s);
    return (int) stubs.size() - 1;
}

Vec3 Stitch::gridPlace (const Words& words, double z)
{
    const auto cv = curveOf (words);
    int best = 0;
    for (int i = 1; i < kCurvePoints; ++i) if (cv[(size_t) i] > cv[(size_t) best]) best = i;
    const double hz = 20.0 * std::pow (1000.0, best / double (kCurvePoints - 1));
    double res = 0.0;
    int count = 0;
    for (const auto& g : geometryOf (words)) if (g.pole && g.pR > 0.5) { res += resDb (g.pR); ++count; }
    const double lift = count > 0 ? juce::jlimit (0.0, 1.0, res / count / 60.0) * 0.3 : 0.0;
    return { std::log10 (hz / 20.0) / 3.0 - 0.5, juce::jlimit (-0.5, 0.5, cv[(size_t) best] / 60.0), z + lift };
}

bool Stitch::sharesNode (int a, int b) const
{
    if (a < 0 || b < 0 || a == b) return false;
    for (const int n : faces[(size_t) a].nodes)
        for (const int m : faces[(size_t) b].nodes) if (n == m) return true;
    return false;
}

std::vector<int> Stitch::neighbours (int face) const
{
    std::vector<int> out;
    if (face < 0) return out;
    for (int i = 0; i < (int) faces.size(); ++i) if (sharesNode (face, i)) out.push_back (i);
    return out;
}

void Stitch::placeOnGrid()
{
    for (auto& n : nodes) n.p = gridPlace (wordsOf ((int) (&n - nodes.data())), floorZ (n.floor));
    centres.clear();
    for (int i = 0; i < (int) faces.size(); ++i) centres.push_back (gridPlace (wheelMorph (cornersOf (i), 0.5, 0.5).words, floorZ (faces[(size_t) i].floor)));
    for (auto& s : stubs) s.p = gridPlace (s.words, floorZ (s.floor >= 0 && std::find (usedFloors.begin(), usedFloors.end(), s.floor) != usedFloors.end() ? s.floor : nodes[(size_t) s.node].floor));
}

int Stitch::nearestNode (const Words& words) const
{
    const auto g = geometryOf (words);
    int best = 0;
    double bestD = 1e18;
    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        if (nodes[(size_t) i].floor == 1) continue;
        const auto h = geometryOf (wordsOf (i));
        double d = 0.0;
        for (int s = 0; s < kRows; ++s)
        {
            if (! g[(size_t) s].pole || ! h[(size_t) s].pole) { d += 4.0; continue; }
            const double dl = std::log2 (g[(size_t) s].pHz / h[(size_t) s].pHz), dr = 25.0 * (g[(size_t) s].pR - h[(size_t) s].pR);
            d += dl * dl + dr * dr;
        }
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

Spot Stitch::lerp (const Spot& a, const Spot& b, double f) const
{
    if (a.floor >= 0 && a.floor == b.floor) { Spot s = a; s.x = a.x + (b.x - a.x) * f; s.y = a.y + (b.y - a.y) * f; return s; }
    if (a.face >= 0 && a.face == b.face) { Spot s = a; s.m = a.m + (b.m - a.m) * f; s.q = a.q + (b.q - a.q) * f; return s; }
    if (a.edge >= 0 && a.edge == b.edge) { Spot s = a; s.t = a.t + (b.t - a.t) * f; return s; }
    return f < 0.5 ? a : b;
}
}
