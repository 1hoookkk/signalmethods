#include "Frame.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace ws
{
const char* const kGroupNames[kGroups] = { "P2K", "MORPHEUS", "X3", "VOWELS", "HEADS", "XL-1", "INSTRUMENTS" };
const char* const kMeasureNames[kMeasures] = { "pitch", "spread", "resonance", "stages", "zeros", "ceiling", "martens 1", "martens 2" };
const char* const kPoseEnds[kMeasures][2] = { { "high", "low" }, { "closed", "open" }, { "relaxed", "stressed" }, { "few", "many" }, { "plain", "carved" }, { "dark", "bright" }, { "-", "+" }, { "-", "+" } };

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
        const auto& r = w[(size_t) s];
        const auto geom = trench::core::geometry_from_words ({ r[0], r[1], r[2], r[3], r[4] }, kDatumHz);
        if (const auto* c = std::get_if<trench::core::ConjugatePair> (&geom.pole)) { out[(size_t) s].pole = true; out[(size_t) s].pHz = c->hz; out[(size_t) s].pR = c->radius; }
        if (const auto* c = std::get_if<trench::core::ConjugatePair> (&geom.zero)) { out[(size_t) s].zero = true; out[(size_t) s].zHz = c->hz; out[(size_t) s].zR = c->radius; }
    }
    return out;
}

trench::core::Cascade cascadeOf (const Words& w)
{
    trench::core::Cascade c {};
    for (int i = 0; i < kRows; ++i)
        c[(size_t) i] = trench::core::section_words_to_biquad ({ w[(size_t) i][0], w[(size_t) i][1], w[(size_t) i][2], w[(size_t) i][3], w[(size_t) i][4] });
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

juce::Colour hueOf (double t)
{
    return juce::Colour::fromHSV ((float) (juce::jlimit (0.0, 1.0, t) * 0.75), 0.9f, 0.9f, 1.0f);
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
