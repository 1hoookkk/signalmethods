#pragma once

#include <juce_core/juce_core.h>
#include <trench/core/packed_body.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ws
{
constexpr double kDatumHz = trench::core::kP2kDatumHz;
constexpr int kRows = 6;
constexpr int kWords = 5;
constexpr double kGridLo = 30.0;
constexpr double kGridHi = 16000.0;

using Words = std::array<std::array<std::uint16_t, kWords>, kRows>;

struct Frame
{
    juce::String name;
    Words words {};
    double u = 0.0, v = 0.0;
    int f1 = 0, f2 = 0;
    bool capture = false;
};

struct Pole { double hz; double r; };

inline std::vector<Pole> strongPoles (const Words& w)
{
    std::vector<Pole> out;
    for (const auto& row : w)
    {
        const auto geom = trench::core::geometry_from_words ({ row[0], row[1], row[2], row[3], row[4] }, kDatumHz);
        if (const auto* c = std::get_if<trench::core::ConjugatePair> (&geom.pole))
            if (c->radius > 0.85 && c->hz >= kGridLo && c->hz <= kGridHi)
                out.push_back ({ c->hz, c->radius });
    }
    std::sort (out.begin(), out.end(), [] (const Pole& a, const Pole& b) { return a.hz < b.hz; });
    return out;
}

inline trench::core::Cascade cascadeOf (const Words& w)
{
    trench::core::Cascade c {};
    for (int i = 0; i < kRows; ++i)
        c[(size_t) i] = trench::core::section_words_to_biquad ({ w[(size_t) i][0], w[(size_t) i][1], w[(size_t) i][2], w[(size_t) i][3], w[(size_t) i][4] });
    c[6] = trench::core::section_words_to_biquad (trench::core::kIdentitySection);
    return c;
}

inline double responseDb (const trench::core::Cascade& c, double hz)
{
    return trench::core::cascade_response_db (std::span<const trench::core::Biquad> (c.data(), kRows), hz, kDatumHz);
}

inline double gridPos (double hz)
{
    return std::log2 (juce::jlimit (kGridLo, kGridHi, hz) / kGridLo) / std::log2 (kGridHi / kGridLo);
}

inline void place (Frame& f)
{
    const auto poles = strongPoles (f.words);
    double a = 0.0, b = 0.0;
    if (poles.size() >= 2) { a = poles[0].hz; b = poles[1].hz; }
    else if (poles.size() == 1) { a = b = poles[0].hz; }
    else
    {
        const auto c = cascadeOf (f.words);
        double best = -1e9;
        for (int i = 0; i < 96; ++i)
        {
            const double hz = 20.0 * std::pow (1000.0, i / 95.0);
            const double d = responseDb (c, hz);
            if (d > best) { best = d; a = b = hz; }
        }
    }
    f.f1 = (int) std::lround (a);
    f.f2 = (int) std::lround (b);
    f.u = gridPos (a);
    f.v = gridPos (b);
}

inline Words blend (const std::vector<const Words*>& parents, const std::vector<double>& weights)
{
    Words out {};
    for (int s = 0; s < kRows; ++s)
        for (int k = 0; k < kWords; ++k)
        {
            double acc = 0.0;
            for (size_t i = 0; i < parents.size(); ++i)
                acc += (*parents[i])[(size_t) s][(size_t) k] * weights[i];
            out[(size_t) s][(size_t) k] = (std::uint16_t) juce::jlimit (0.0, 65535.0, std::trunc (acc));
        }
    return out;
}

struct Triangle { int a, b, c; };

inline std::vector<Triangle> delaunay (const std::vector<Frame>& pts)
{
    struct P { double x, y; };
    std::vector<P> p;
    juce::Random rng (7);
    for (const auto& f : pts)
        p.push_back ({ f.u + (rng.nextDouble() - 0.5) * 2e-5, f.v + (rng.nextDouble() - 0.5) * 2e-5 });
    const int n = (int) p.size();
    p.push_back ({ -10.0, -10.0 });
    p.push_back ({ 10.0, -10.0 });
    p.push_back ({ 0.0, 10.0 });
    struct T { int a, b, c; double cx, cy, r2; bool bad; };
    const auto circum = [&] (int a, int b, int c) -> T
    {
        const double ax = p[(size_t) a].x, ay = p[(size_t) a].y, bx = p[(size_t) b].x, by = p[(size_t) b].y, cx = p[(size_t) c].x, cy = p[(size_t) c].y;
        const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
        if (std::abs (d) < 1e-18) return { a, b, c, 0.0, 0.0, -1.0, false };
        const double ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d;
        const double uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d;
        return { a, b, c, ux, uy, (ax - ux) * (ax - ux) + (ay - uy) * (ay - uy), false };
    };
    std::vector<T> tris { circum (n, n + 1, n + 2) };
    for (int i = 0; i < n; ++i)
    {
        std::vector<std::pair<int, int>> edges;
        for (auto& t : tris)
        {
            const double dx = p[(size_t) i].x - t.cx, dy = p[(size_t) i].y - t.cy;
            t.bad = t.r2 >= 0.0 && dx * dx + dy * dy < t.r2;
            if (t.bad)
            {
                edges.push_back ({ t.a, t.b });
                edges.push_back ({ t.b, t.c });
                edges.push_back ({ t.c, t.a });
            }
        }
        tris.erase (std::remove_if (tris.begin(), tris.end(), [] (const T& t) { return t.bad; }), tris.end());
        for (size_t e = 0; e < edges.size(); ++e)
        {
            bool shared = false;
            for (size_t f = 0; f < edges.size(); ++f)
                if (e != f && ((edges[e].first == edges[f].first && edges[e].second == edges[f].second) || (edges[e].first == edges[f].second && edges[e].second == edges[f].first)))
                { shared = true; break; }
            if (! shared)
                tris.push_back (circum (edges[e].first, edges[e].second, i));
        }
    }
    std::vector<Triangle> out;
    for (const auto& t : tris)
        if (t.a < n && t.b < n && t.c < n)
            out.push_back ({ t.a, t.b, t.c });
    return out;
}

inline std::optional<std::array<double, 3>> barycentric (double px, double py, const Frame& a, const Frame& b, const Frame& c)
{
    const double v0x = b.u - a.u, v0y = b.v - a.v, v1x = c.u - a.u, v1y = c.v - a.v, v2x = px - a.u, v2y = py - a.v;
    const double den = v0x * v1y - v1x * v0y;
    if (std::abs (den) < 1e-14) return std::nullopt;
    const double v = (v2x * v1y - v1x * v2y) / den, w = (v0x * v2y - v2x * v0y) / den;
    return std::array<double, 3> { 1.0 - v - w, v, w };
}

struct Library
{
    std::vector<Frame> frames;
    std::vector<Triangle> tris;

    static Words wordsFromVar (const juce::var& rows)
    {
        Words w {};
        for (int s = 0; s < kRows && s < rows.size(); ++s)
            for (int k = 0; k < kWords && k < rows[s].size(); ++k)
                w[(size_t) s][(size_t) k] = (std::uint16_t) (int) rows[s][k];
        return w;
    }

    bool loadJson (const juce::File& file)
    {
        const auto parsed = juce::JSON::parse (file);
        if (! parsed.isArray()) return false;
        for (const auto& item : *parsed.getArray())
        {
            Frame f;
            f.name = item["name"].toString().replace (juce::String (juce::CharPointer_UTF8 ("\xef\xbf\xbd")), juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")));
            f.words = wordsFromVar (item["words"]);
            place (f);
            frames.push_back (f);
        }
        return ! frames.empty();
    }

    bool loadBodies (const juce::File& dir)
    {
        static const char* tags[] = { "M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1" };
        for (const auto& file : dir.findChildFiles (juce::File::findFiles, false, "*.body240"))
        {
            juce::MemoryBlock mb;
            if (! file.loadFileAsData (mb) || mb.getSize() != trench::core::kLegacyBodyBytes) continue;
            const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> ((const std::uint8_t*) mb.getData(), mb.getSize()));
            for (int c = 0; c < 4; ++c)
            {
                Frame f;
                f.name = file.getFileNameWithoutExtension() + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + tags[c];
                for (int s = 0; s < kRows; ++s)
                    for (int k = 0; k < kWords; ++k)
                        f.words[(size_t) s][(size_t) k] = body.words[(size_t) c][(size_t) s][(size_t) k];
                place (f);
                frames.push_back (f);
            }
        }
        return ! frames.empty();
    }

    void triangulate() { tris = delaunay (frames); }
};

struct Cube
{
    std::array<int, 8> corner { -1, -1, -1, -1, -1, -1, -1, -1 };
    double morph = 0.5, q = 0.5, z = 0.5;
    std::array<std::array<juce::String, 2>, 3> axisNames { { { "high", "low" }, { "closed", "open" }, { "relaxed", "stressed" } } };

    bool ready() const { return std::all_of (corner.begin(), corner.end(), [] (int i) { return i >= 0; }); }

    std::array<double, 8> weightsAt (double m, double qq, double zz) const
    {
        std::array<double, 8> w {};
        for (int i = 0; i < 8; ++i)
            w[(size_t) i] = ((i & 1) ? m : 1.0 - m) * ((i & 2) ? qq : 1.0 - qq) * ((i & 4) ? zz : 1.0 - zz);
        return w;
    }

    juce::String poseName (int i) const
    {
        return axisNames[0][(size_t) (i & 1)] + " " + axisNames[1][(size_t) ((i & 2) ? 1 : 0)] + " " + axisNames[2][(size_t) ((i & 4) ? 1 : 0)];
    }

    Words wheelWords (const Library& lib) const
    {
        std::vector<const Words*> parents;
        std::vector<double> weights;
        const auto w = weightsAt (morph, q, z);
        for (int i = 0; i < 8; ++i) { parents.push_back (&lib.frames[(size_t) corner[(size_t) i]].words); weights.push_back (w[(size_t) i]); }
        return blend (parents, weights);
    }

    std::pair<double, double> planeXY (const Library& lib, double m, double qq, double zz) const
    {
        const auto w = weightsAt (m, qq, zz);
        double x = 0.0, y = 0.0;
        for (int i = 0; i < 8; ++i)
        {
            const auto& f = lib.frames[(size_t) corner[(size_t) i]];
            x += f.u * w[(size_t) i];
            y += f.v * w[(size_t) i];
        }
        return { x, y };
    }

    void solveFromPlane (const Library& lib, double u, double v)
    {
        const auto err = [&] (double m, double qq) { const auto [x, y] = planeXY (lib, m, qq, z); return std::hypot (x - u, y - v); };
        double bm = morph, bq = q, bd = 1e9;
        for (int a = 0; a <= 16; ++a)
            for (int b = 0; b <= 16; ++b)
            {
                const double d = err (a / 16.0, b / 16.0);
                if (d < bd) { bd = d; bm = a / 16.0; bq = b / 16.0; }
            }
        double step = 1.0 / 32.0;
        for (int it = 0; it < 30; ++it)
        {
            bool improved = false;
            const double dm[4] = { step, -step, 0.0, 0.0 }, dq[4] = { 0.0, 0.0, step, -step };
            for (int k = 0; k < 4; ++k)
            {
                const double m = juce::jlimit (0.0, 1.0, bm + dm[k]), qq = juce::jlimit (0.0, 1.0, bq + dq[k]);
                const double d = err (m, qq);
                if (d < bd) { bd = d; bm = m; bq = qq; improved = true; }
            }
            if (! improved) step *= 0.5;
        }
        morph = bm;
        q = bq;
    }
};
}
