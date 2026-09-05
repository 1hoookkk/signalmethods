#include "Morph.h"
#include <algorithm>
#include <cmath>

namespace ws
{
namespace
{
using trench::core::ConjugatePair;
using trench::core::PackedSection;

PackedSection sectionOf (const Words& w, int row)
{
    const auto& r = w[(size_t) row];
    return { r[0], r[1], r[2], r[3], r[4] };
}

void putSection (Words& w, int row, const PackedSection& s)
{
    for (int k = 0; k < kWords; ++k) w[(size_t) row][(size_t) k] = s[(size_t) k];
}

std::uint16_t lerpWord (std::uint16_t a, std::uint16_t b, double t)
{
    if (t >= 0.0 && t <= 1.0) return trench::core::interpolate_word (a, b, (float) t);
    return (std::uint16_t) juce::jlimit (0.0, 65535.0, std::trunc (a + (b - a) * t));
}

double logPush (double a, double b, double t)
{
    if (a <= 0.0 || b <= 0.0) return a + (b - a) * t;
    return a * std::pow (b / a, t);
}

Words plainLerp (const Words& a, const Words& b, double t)
{
    Words out {};
    for (int s = 0; s < kRows; ++s)
        for (int k = 0; k < kWords; ++k) out[(size_t) s][(size_t) k] = lerpWord (a[(size_t) s][(size_t) k], b[(size_t) s][(size_t) k], t);
    return out;
}

double poleRadius (const Words& w, int row)
{
    const auto g = trench::core::geometry_from_words (sectionOf (w, row), kDatumHz);
    if (const auto* c = std::get_if<ConjugatePair> (&g.pole)) return c->radius;
    if (const auto* r = std::get_if<trench::core::RealPair> (&g.pole)) return std::max (std::abs (r->root_a), std::abs (r->root_b));
    return 0.0;
}

bool holdRadius (Words& w, int row)
{
    bool hit = false;
    while (poleRadius (w, row) > kRadiusGuard && w[(size_t) row][3] < 0xFFFF) { ++w[(size_t) row][3]; hit = true; }
    return hit;
}

Words pushWords (const Words& a, const Words& b, double t)
{
    Words out = plainLerp (a, b, t);
    const Chord ca = decompile (a, kDatumHz), cb = decompile (b, kDatumHz);
    Chord pushed = decompile (out, kDatumHz);
    for (int s = 0; s < kRows - 1; ++s)
    {
        const auto& sa = ca[(size_t) s];
        const auto& sb = cb[(size_t) s];
        auto& sp = pushed[(size_t) s];
        if (sa.pole.on && sb.pole.on)
        {
            sp.pole.on = true;
            sp.pole.note = sa.pole.note + (sb.pole.note - sa.pole.note) * t;
            sp.pole.width = logPush (std::max (1e-3, sa.pole.width), std::max (1e-3, sb.pole.width), t);
            fitVoice (sp.pole, true, kDatumHz);
        }
        if (sa.zero.on && sb.zero.on)
        {
            sp.zero.on = true;
            sp.zero.note = sa.zero.note + (sb.zero.note - sa.zero.note) * t;
            sp.zero.width = logPush (std::max (1e-3, sa.zero.width), std::max (1e-3, sb.zero.width), t);
            fitVoice (sp.zero, false, kDatumHz);
        }
        sp.gainDb = sa.gainDb + (sb.gainDb - sa.gainDb) * t;
        sp.raw = out[(size_t) s];
    }
    pushed[kRows - 1].raw = out[kRows - 1];
    pushed[kRows - 1].pole.on = pushed[kRows - 1].zero.on = false;
    pushed[kRows - 1].gainDb = 20.0 * std::log10 (std::max (1e-6, 4.0 * trench::core::decode_word (out[kRows - 1][4])));
    Words w = compile (pushed, kDatumHz);
    for (int s = 0; s < kRows - 1; ++s)
    {
        const auto& sa = ca[(size_t) s];
        const auto& sb = cb[(size_t) s];
        if (! (sa.pole.on && sb.pole.on)) { w[(size_t) s][2] = out[(size_t) s][2]; w[(size_t) s][3] = out[(size_t) s][3]; }
        if (! (sa.zero.on && sb.zero.on)) { w[(size_t) s][0] = out[(size_t) s][0]; w[(size_t) s][1] = out[(size_t) s][1]; }
    }
    w[kRows - 1] = out[kRows - 1];
    return w;
}
}

bool guardRadius (Words& words, std::array<bool, kRows>& guarded)
{
    bool any = false;
    for (int s = 0; s < kRows; ++s)
    {
        auto geom = trench::core::geometry_from_words (sectionOf (words, s), kDatumHz);
        bool hit = false;
        if (auto* c = std::get_if<ConjugatePair> (&geom.pole))
        {
            if (c->radius > kRadiusGuard) { c->radius = kRadiusGuard; hit = true; }
        }
        else if (auto* r = std::get_if<trench::core::RealPair> (&geom.pole))
        {
            for (double* root : { &r->root_a, &r->root_b })
                if (std::abs (*root) > kRadiusGuard) { *root = std::copysign (kRadiusGuard, *root); hit = true; }
        }
        if (hit)
        {
            const auto enc = trench::core::words_from_geometry (geom, kDatumHz);
            words[(size_t) s][2] = enc[2];
            words[(size_t) s][3] = enc[3];
        }
        hit = holdRadius (words, s) || hit;
        if (! hit) continue;
        guarded[(size_t) s] = true;
        any = true;
    }
    return any;
}

Morph pairMorph (const Words& a, const Words& b, double t)
{
    Morph m;
    const double tc = juce::jlimit (0.0, 1.0, t);
    if (t == tc) { m.words = plainLerp (a, b, t); return m; }
    m.outside = true;
    m.words = pushWords (a, b, t);
    m.words[kRows - 1] = plainLerp (a, b, tc)[kRows - 1];
    guardRadius (m.words, m.guarded);
    return m;
}

Morph wheelMorph (const std::array<Words, 4>& c, double morph, double q)
{
    Morph m;
    const double mc = juce::jlimit (0.0, 1.0, morph), qc = juce::jlimit (0.0, 1.0, q);
    if (mc == morph && qc == q)
    {
        m.words = plainLerp (plainLerp (c[0], c[1], morph), plainLerp (c[2], c[3], morph), q);
        return m;
    }
    m.outside = true;
    const Words e0 = pairMorph (c[0], c[1], morph).words, e1 = pairMorph (c[2], c[3], morph).words;
    m.words = pairMorph (e0, e1, q).words;
    const Words h0 = plainLerp (c[0], c[1], mc), h1 = plainLerp (c[2], c[3], mc);
    m.words[kRows - 1] = plainLerp (h0, h1, qc)[kRows - 1];
    guardRadius (m.words, m.guarded);
    return m;
}

std::vector<Excess> excessOf (const Words& words)
{
    std::vector<Excess> out;
    const auto c = cascadeOf (words);
    bool inRun = false, above = false;
    double bestHz = 0.0, bestDb = 0.0;
    for (int i = 0; i <= kCurvePoints; ++i)
    {
        const double hz = 20.0 * std::pow (1000.0, std::min (i, kCurvePoints - 1) / double (kCurvePoints - 1));
        const double db = i < kCurvePoints ? responseDb (c, hz) : 0.0;
        const bool beyond = std::abs (db) > 30.0;
        if (beyond && (! inRun || (db > 0.0) != above))
        {
            if (inRun) out.push_back ({ bestHz, above });
            inRun = true;
            above = db > 0.0;
            bestHz = hz;
            bestDb = db;
        }
        else if (beyond && std::abs (db) > std::abs (bestDb)) { bestHz = hz; bestDb = db; }
        else if (! beyond && inRun) { out.push_back ({ bestHz, above }); inRun = false; }
    }
    return out;
}

double dissolveWidth (double note) { return widthOf (hzOf (note), 0.5, kDatumHz); }

namespace
{
double voiceCost (const Voice& a, const Voice& b, double note)
{
    if (a.on && b.on) return std::abs (a.note - b.note) + 0.5 * std::abs (std::log2 (std::max (1e-3, a.width) / std::max (1e-3, b.width)));
    if (a.on != b.on) return 18.0;
    (void) note;
    return 0.0;
}

double stageCost (const Stage& a, const Stage& b)
{
    return voiceCost (a.pole, b.pole, 0.0) + 0.5 * voiceCost (a.zero, b.zero, 0.0);
}

void dissolveInto (Stage& target, const Stage& partner)
{
    if (partner.pole.on && ! target.pole.on)
    {
        target.pole = partner.pole;
        target.zero = partner.pole;
    }
    if (partner.zero.on && ! target.zero.on) { target.zero.on = true; target.zero.note = partner.zero.note; target.zero.width = widthOf (hzOf (partner.zero.note), 0.02, kDatumHz); }
}
}

Lead leadTo (const Chord& a, const Chord& b)
{
    Lead out;
    out.a = a;
    std::array<int, kRows> perm { 0, 1, 2, 3, 4, 5 };
    double best = 1e18;
    std::array<int, kRows> bestPerm = perm;
    do
    {
        double c = 0.0;
        for (int s = 0; s < kRows - 1; ++s) c += stageCost (a[(size_t) s], b[(size_t) perm[(size_t) s]]);
        c += 0.25 * stageCost (a[kRows - 1], b[(size_t) perm[kRows - 1]]);
        if (c < best) { best = c; bestPerm = perm; }
    } while (std::next_permutation (perm.begin(), perm.end()));
    out.map = bestPerm;
    out.cost = best;
    for (int s = 0; s < kRows; ++s) out.b[(size_t) s] = b[(size_t) bestPerm[(size_t) s]];
    for (int s = 0; s < kRows - 1; ++s)
    {
        dissolveInto (out.b[(size_t) s], out.a[(size_t) s]);
        dissolveInto (out.a[(size_t) s], out.b[(size_t) s]);
    }
    return out;
}

double leadCost (const Chord& a, const Chord& b) { return leadTo (a, b).cost; }

Words leadWords (const Words& a, const Words& b) { return compile (leadTo (decompile (a, kDatumHz), decompile (b, kDatumHz)).b, kDatumHz); }

juce::String intervalsOf (const Chord& c)
{
    std::vector<double> notes;
    for (const auto& st : c) if (st.pole.on && st.pole.width <= 6.0) notes.push_back (st.pole.note);
    if (notes.empty()) for (const auto& st : c) if (st.pole.on) notes.push_back (st.pole.note);
    if (notes.empty()) return "-";
    std::sort (notes.begin(), notes.end());
    juce::String s;
    for (size_t i = 1; i < notes.size(); ++i) s += (s.isEmpty() ? "" : " ") + juce::String ((int) std::round (notes[i] - notes[0]));
    return s.isEmpty() ? "1" : s;
}

Words meanWords (const std::vector<const Words*>& parents)
{
    std::vector<double> w (parents.size(), 1.0 / (double) std::max<size_t> (1, parents.size()));
    return blend (parents, w);
}

Words schwaWords()
{
    Chord c {};
    const double bw[kRows - 1] = { 60.0, 90.0, 120.0, 150.0, 200.0 };
    for (int s = 0; s < kRows - 1; ++s)
    {
        const double hz = 500.0 * (2 * s + 1);
        c[(size_t) s].pole.on = true;
        c[(size_t) s].pole.note = noteOf (hz);
        c[(size_t) s].pole.width = widthOf (hz, std::exp (-juce::MathConstants<double>::pi * bw[s] / kDatumHz), kDatumHz);
    }
    auto& top = c[kRows - 1];
    top.pole.on = true; top.pole.note = noteOf (17000.0); top.pole.width = widthOf (17000.0, 0.414, kDatumHz);
    top.zero.on = true; top.zero.note = noteOf (20000.0); top.zero.width = widthOf (20000.0, kRadiusGuard, kDatumHz);
    Words w = compile (c, kDatumHz);
    unityDc (w);
    return w;
}
}
