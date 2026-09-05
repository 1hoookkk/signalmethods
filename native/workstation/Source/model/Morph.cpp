#include "Morph.h"
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
    for (int s = 0; s < kRows - 1; ++s)
    {
        const auto ga = trench::core::geometry_from_words (sectionOf (a, s), kDatumHz);
        const auto gb = trench::core::geometry_from_words (sectionOf (b, s), kDatumHz);
        auto geom = trench::core::geometry_from_words (sectionOf (out, s), kDatumHz);
        const auto* pa = std::get_if<ConjugatePair> (&ga.pole);
        const auto* pb = std::get_if<ConjugatePair> (&gb.pole);
        if (pa != nullptr && pb != nullptr)
        {
            const double hz = juce::jlimit (20.0, kDatumHz * 0.495, logPush (pa->hz, pb->hz, t));
            const double r = 1.0 - logPush (1.0 - pa->radius, 1.0 - pb->radius, t);
            geom.pole = ConjugatePair { hz, juce::jlimit (0.0, kRadiusGuard, r) };
        }
        const auto* za = std::get_if<ConjugatePair> (&ga.zero);
        const auto* zb = std::get_if<ConjugatePair> (&gb.zero);
        if (za != nullptr && zb != nullptr)
        {
            const double hz = juce::jlimit (20.0, kDatumHz * 0.495, logPush (za->hz, zb->hz, t));
            geom.zero = ConjugatePair { hz, juce::jlimit (0.0, 1.0, za->radius + (zb->radius - za->radius) * t) };
        }
        geom.scale = juce::jlimit (0.0, 4.0, logPush (ga.scale, gb.scale, t));
        const auto enc = trench::core::words_from_geometry (geom, kDatumHz);
        if (pa != nullptr && pb != nullptr) { out[(size_t) s][2] = enc[2]; out[(size_t) s][3] = enc[3]; }
        if (za != nullptr && zb != nullptr) { out[(size_t) s][0] = enc[0]; out[(size_t) s][1] = enc[1]; }
        out[(size_t) s][4] = enc[4];
    }
    return out;
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

Words meanWords (const std::vector<const Words*>& parents)
{
    std::vector<double> w (parents.size(), 1.0 / (double) std::max<size_t> (1, parents.size()));
    return blend (parents, w);
}

Words schwaWords()
{
    Words w;
    for (int s = 0; s < kRows; ++s) putSection (w, s, trench::core::kIdentitySection);
    const double bw[kRows - 1] = { 60.0, 90.0, 120.0, 150.0, 200.0 };
    for (int s = 0; s < kRows - 1; ++s)
        setPole (w, s, 500.0 * (2 * s + 1), std::exp (-juce::MathConstants<double>::pi * bw[s] / kDatumHz));
    setPole (w, kRows - 1, 17000.0, 0.414);
    setZero (w, kRows - 1, 20000.0, kRadiusGuard);
    unityDc (w);
    return w;
}
}
