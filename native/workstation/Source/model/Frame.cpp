#include "Frame.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace ws
{
const char* const kGroupNames[kGroups] = { "P2K", "MORPHEUS", "X3", "VOWELS", "HEADS", "XL-1", "INSTRUMENTS" };
const char* const kMeasureNames[kMeasures] = { "pitch", "spread", "res dB", "stages", "zeros", "ceiling", "martens1", "martens2" };
const char* const kPoseEnds[kMeasures][2] = { { "high", "low" }, { "closed", "open" }, { "relaxed", "stressed" }, { "few", "many" }, { "plain", "carved" }, { "dark", "bright" }, { "-", "+" }, { "-", "+" } };

namespace
{
const char* const kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

trench::core::PackedSection sectionOf (const Words& w, int row)
{
    const auto& r = w[(size_t) row];
    return { r[0], r[1], r[2], r[3], r[4] };
}

double clampHz (double hz, double datum) { return juce::jlimit (20.0, datum * 0.495, hz); }
}

double noteOf (double hz) { return 69.0 + 12.0 * std::log2 (std::max (1e-3, hz) / 440.0); }
double hzOf (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

double widthOf (double hz, double radius, double datum)
{
    const double r = juce::jlimit (1e-6, 1.0, radius);
    const double bw = -std::log (r) * datum / juce::MathConstants<double>::pi;
    return juce::jlimit (0.0, kMaxWidth, 12.0 * std::log2 (1.0 + bw / std::max (1.0, hz)));
}

double radiusOf (double hz, double width, double datum)
{
    const double bw = std::max (1.0, hz) * (std::pow (2.0, juce::jlimit (0.0, kMaxWidth, width) / 12.0) - 1.0);
    return juce::jlimit (0.0, 1.0, std::exp (-juce::MathConstants<double>::pi * bw / datum));
}

juce::String noteName (double note)
{
    const int n = (int) std::round (note);
    const int cents = (int) std::round ((note - n) * 100.0);
    juce::String s = juce::String (kNoteNames[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    if (cents != 0) s += (cents > 0 ? " +" : " ") + juce::String (cents);
    return s;
}

Chord decompile (const Words& words, double datum)
{
    Chord c {};
    for (int s = 0; s < kRows; ++s)
    {
        auto& st = c[(size_t) s];
        st.raw = words[(size_t) s];
        const auto g = trench::core::geometry_from_words (sectionOf (words, s), datum);
        if (const auto* p = std::get_if<trench::core::ConjugatePair> (&g.pole)) { st.pole.on = true; st.pole.note = noteOf (p->hz); st.pole.width = widthOf (p->hz, p->radius, datum); }
        if (const auto* z = std::get_if<trench::core::ConjugatePair> (&g.zero)) { st.zero.on = true; st.zero.note = noteOf (z->hz); st.zero.width = widthOf (z->hz, z->radius, datum); }
        st.gainDb = 20.0 * std::log10 (std::max (1e-6, g.scale));
    }
    return c;
}

Words compile (const Chord& chord, double datum)
{
    Words w {};
    for (int s = 0; s < kRows; ++s)
    {
        const auto& st = chord[(size_t) s];
        w[(size_t) s] = st.raw;
        if (! st.pole.on && ! st.zero.on)
        {
            w[(size_t) s][4] = trench::core::encode_word (juce::jlimit (0.0, 1.0, std::pow (10.0, st.gainDb / 20.0) / 4.0));
            continue;
        }
        auto geom = trench::core::geometry_from_words (sectionOf (w, s), datum);
        const bool writePole = st.pole.on || std::holds_alternative<trench::core::ConjugatePair> (geom.pole);
        const bool writeZero = st.zero.on || std::holds_alternative<trench::core::ConjugatePair> (geom.zero);
        if (st.pole.on) { const double hz = clampHz (hzOf (st.pole.note), datum); geom.pole = trench::core::ConjugatePair { hz, radiusOf (hz, st.pole.width, datum) }; }
        else if (writePole) geom.pole = trench::core::DegeneratePair {};
        if (st.zero.on) { const double hz = clampHz (hzOf (st.zero.note), datum); geom.zero = trench::core::ConjugatePair { hz, radiusOf (hz, st.zero.width, datum) }; }
        else if (writeZero) geom.zero = trench::core::DegeneratePair {};
        geom.scale = std::pow (10.0, st.gainDb / 20.0);
        const auto enc = trench::core::words_from_geometry (geom, datum);
        if (writePole) { w[(size_t) s][2] = enc[2]; w[(size_t) s][3] = enc[3]; }
        if (writeZero) { w[(size_t) s][0] = enc[0]; w[(size_t) s][1] = enc[1]; }
        w[(size_t) s][4] = enc[4];
    }
    return w;
}

void fitVoice (Voice& v, bool pole, double datum)
{
    if (! v.on) return;
    const double hz = clampHz (hzOf (v.note), datum);
    for (int i = 0; i < 24; ++i)
    {
        trench::core::SectionGeometry geom {};
        const trench::core::ConjugatePair pair { hz, radiusOf (hz, v.width, datum) };
        if (pole) geom.pole = pair; else geom.zero = pair;
        const auto enc = trench::core::words_from_geometry (geom, datum);
        const auto back = trench::core::geometry_from_words (enc, datum);
        const auto* got = std::get_if<trench::core::ConjugatePair> (pole ? &back.pole : &back.zero);
        if (got != nullptr && std::abs (noteOf (got->hz) - noteOf (hz)) < 0.1) return;
        v.width *= 0.7;
    }
}

Chord chordFrom (double rootNote, const std::array<double, kRows>& intervals, double width, double gainDb)
{
    Chord c {};
    for (int s = 0; s < kRows; ++s)
    {
        auto& st = c[(size_t) s];
        st.pole.on = true;
        st.pole.note = rootNote + intervals[(size_t) s];
        st.pole.width = width;
        st.gainDb = gainDb;
    }
    return c;
}

void setChord (Frame& f, const Chord& chord)
{
    f.chord = chord;
    f.words = compile (chord, kDatumHz);
    measure (f);
}

void setWords (Frame& f, const Words& words, double datum)
{
    f.chord = decompile (words, datum);
    f.words = datum == kDatumHz ? words : compile (f.chord, kDatumHz);
    measure (f);
}

int groupOf (const juce::String& n)
{
    if (n.matchesWildcard ("* · M? Q?", true)) return 0;
    if (n.startsWith ("MORPHEUS")) return 1;
    if (n.startsWith ("X3") || n.startsWith ("LADDER")) return 2;
    if (n.startsWithIgnoreCase ("sung") || n.containsIgnoreCase ("hedz") || n.containsIgnoreCase ("vowel")) return 3;
    if (n.containsIgnoreCase ("ear az")) return 4;
    if (n.startsWith ("Aud ")) return 5;
    return 6;
}

double resDb (double r) { return std::min (60.0, 20.0 * std::log10 (1.0 / std::max (1e-3, 1.0 - r))); }
double octOf (double hz) { return std::log2 (juce::jlimit (30.0, 16000.0, hz) / 30.0); }

std::array<RowGeom, kRows> geometryOf (const Words& w)
{
    std::array<RowGeom, kRows> out {};
    for (int s = 0; s < kRows; ++s)
    {
        const auto geom = trench::core::geometry_from_words (sectionOf (w, s), kDatumHz);
        if (const auto* c = std::get_if<trench::core::ConjugatePair> (&geom.pole)) { out[(size_t) s].pole = true; out[(size_t) s].pHz = c->hz; out[(size_t) s].pR = c->radius; }
        if (const auto* c = std::get_if<trench::core::ConjugatePair> (&geom.zero)) { out[(size_t) s].zero = true; out[(size_t) s].zHz = c->hz; out[(size_t) s].zR = c->radius; }
    }
    return out;
}

trench::core::Cascade cascadeOf (const Words& w)
{
    trench::core::Cascade c {};
    for (int i = 0; i < kRows; ++i) c[(size_t) i] = trench::core::section_words_to_biquad (sectionOf (w, i));
    c[6] = trench::core::section_words_to_biquad (trench::core::kIdentitySection);
    return c;
}

double responseDb (const trench::core::Cascade& c, double hz)
{
    return trench::core::cascade_response_db (std::span<const trench::core::Biquad> (c.data(), kRows), hz, kDatumHz);
}

Curve curveOf (const Words& w)
{
    const auto c = cascadeOf (w);
    Curve out {};
    for (int i = 0; i < kCurvePoints; ++i)
        out[(size_t) i] = juce::jlimit (-60.0, 30.0, responseDb (c, 20.0 * std::pow (1000.0, i / double (kCurvePoints - 1))));
    return out;
}

void measure (Frame& f)
{
    f.rows = geometryOf (f.words);
    std::vector<double> octs, res;
    int stages = 0, zeros = 0;
    bool anyZero = false;
    double maxZ = 0.0;
    for (const auto& r : f.rows)
    {
        if (r.pole)
        {
            res.push_back (resDb (r.pR));
            if (r.pR > 0.85) octs.push_back (octOf (r.pHz));
            if (r.pR > 0.5) ++stages;
        }
        if (r.zero)
        {
            if (r.zR > 0.5 && r.zHz < 15000.0) ++zeros;
            anyZero = true;
            maxZ = std::max (maxZ, r.zHz);
        }
    }
    if (octs.empty())
        for (const auto& r : f.rows) if (r.pole) octs.push_back (octOf (r.pHz));
    double pitch = 4.5, spread = 0.0, meanRes = 0.0;
    if (! octs.empty())
    {
        pitch = std::accumulate (octs.begin(), octs.end(), 0.0) / (double) octs.size();
        spread = *std::max_element (octs.begin(), octs.end()) - *std::min_element (octs.begin(), octs.end());
    }
    if (! res.empty()) meanRes = std::accumulate (res.begin(), res.end(), 0.0) / (double) res.size();
    const double ceiling = anyZero ? octOf (maxZ) / 9.0 : 1.0;
    f.m = { pitch / 9.0, std::min (1.0, spread / 6.0), std::min (1.0, meanRes / 40.0), stages / 6.0, zeros / 6.0, ceiling, 0.5, 0.5 };
}

namespace
{
void setPair (Words& words, int row, bool pole, double hz, double radius)
{
    auto& r = words[(size_t) row];
    auto geom = trench::core::geometry_from_words ({ r[0], r[1], r[2], r[3], r[4] }, kDatumHz);
    const trench::core::ConjugatePair pair { juce::jlimit (20.0, kDatumHz * 0.495, hz), juce::jlimit (0.0, 0.9995, radius) };
    if (pole) geom.pole = pair; else geom.zero = pair;
    const auto out = trench::core::words_from_geometry (geom, kDatumHz);
    for (int k = 0; k < kWords; ++k) r[(size_t) k] = out[(size_t) k];
}
}

void setPole (Words& words, int row, double hz, double radius) { setPair (words, row, true, hz, radius); }
void setZero (Words& words, int row, double hz, double radius) { setPair (words, row, false, hz, radius); }

double sectionDb (const Words& words, int row, double hz)
{
    return trench::core::section_response_db (trench::core::section_words_to_biquad (sectionOf (words, row)), hz, kDatumHz);
}

void unityDc (Words& words)
{
    double product = 1.0;
    for (int s = 0; s < kRows; ++s)
    {
        const auto& r = words[(size_t) s];
        const double d0 = trench::core::decode_word (r[0]), d1 = trench::core::decode_word (r[1]), d2 = trench::core::decode_word (r[2]), d3 = trench::core::decode_word (r[3]);
        const double num = 4.0 * d0 + d1 - d1, den = 4.0 * d2 + d3 - d3;
        if (std::abs (den) > 1e-12 && std::abs (num) > 1e-12) product *= num / den;
    }
    const double gain = std::pow (1.0 / std::max (1e-9, std::abs (product)), 1.0 / kRows);
    const auto word = trench::core::encode_word (juce::jlimit (0.0, 1.0, gain / 4.0));
    for (int s = 0; s < kRows; ++s) words[(size_t) s][4] = word;
}

void sharpen (Chord& chord, double keep)
{
    for (auto& st : chord) if (st.pole.on) st.pole.width *= keep;
}

juce::Colour hueOf (double t)
{
    return juce::Colour::fromHSV ((float) (juce::jlimit (0.0, 1.0, t) * 0.75), 0.85f, 0.72f, 1.0f);
}

Words blend (const std::vector<const Words*>& parents, const std::vector<double>& weights)
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
}
