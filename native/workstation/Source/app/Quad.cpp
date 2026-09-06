#include "Quad.h"
#include <cmath>

namespace hs
{
const char* const kPinNames[4] = { "M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1" };

namespace
{
juce::var wordsVar (const Words& w)
{
    juce::Array<juce::var> a;
    for (const auto& row : w) for (auto x : row) a.add ((int) x);
    return a;
}

Words wordsFrom (const juce::var& v)
{
    Words w {};
    if (auto* a = v.getArray())
        for (int i = 0; i < std::min (30, a->size()); ++i)
            w[(size_t) i / kWords][(size_t) i % kWords] = (std::uint16_t) (int) (*a)[i];
    return w;
}

int indexOf (const std::vector<Star>& stars, const juce::String& name)
{
    for (int i = 0; i < (int) stars.size(); ++i) if (stars[(size_t) i].name == name) return i;
    return -1;
}
}

bool admit (const Words& words)
{
    for (const auto& row : words) if (row == trench::core::kIdentitySection) return false;
    return true;
}

Corners cornersOf (const Quad& quad, const std::vector<Star>& stars)
{
    Corners c {};
    for (size_t i = 0; i < 4; ++i)
        if (quad.pins[i] >= 0 && quad.pins[i] < (int) stars.size()) c[i] = stars[(size_t) quad.pins[i]].words;
    return c;
}

trench::core::PackedBody bodyOf (const Corners& c)
{
    trench::core::PackedBody body;
    for (size_t i = 0; i < trench::core::kCornerCount; ++i)
    {
        body.words[i].fill (trench::core::kIdentitySection);
        for (size_t s = 0; s < kRows; ++s) body.words[i][s] = c[i % 4][s];
    }
    return body;
}

Words lerp (const Corners& c, double morph, double q)
{
    const auto cw = bodyOf (c).interpolate_words ((float) morph, (float) q, 0.0f);
    Words w {};
    for (size_t s = 0; s < kRows; ++s) w[s] = cw[s];
    return w;
}

Words wordsAt (const Quad& quad, const std::vector<Star>& stars)
{
    if (! quad.complete()) return {};
    return lerp (cornersOf (quad, stars), quad.morph / 100.0, quad.q / 100.0);
}

Bytes bytesOf (const Corners& c) { return bodyOf (c).legacy_bytes(); }

bool writeBody (const Quad& quad, const std::vector<Star>& stars, const juce::File& file)
{
    if (! quad.complete()) return false;
    const auto b = bytesOf (cornersOf (quad, stars));
    file.getParentDirectory().createDirectory();
    return file.replaceWithData (b.data(), b.size());
}

bool save (const Quad& quad, const std::vector<Star>& stars, size_t libraryCount, const juce::File& file)
{
    auto* d = new juce::DynamicObject();
    d->setProperty ("schema", "trench-quad-v1");
    d->setProperty ("morph", quad.morph); d->setProperty ("q", quad.q); d->setProperty ("captures", quad.captures);
    juce::Array<juce::var> pins;
    for (int p : quad.pins) pins.add (p >= 0 && p < (int) stars.size() ? stars[(size_t) p].name : juce::String());
    d->setProperty ("pins", pins);
    juce::Array<juce::var> captures;
    for (size_t i = libraryCount; i < stars.size(); ++i)
    {
        const auto& s = stars[i];
        auto* e = new juce::DynamicObject();
        e->setProperty ("name", s.name); e->setProperty ("parentA", s.parentA); e->setProperty ("parentB", s.parentB);
        e->setProperty ("morph", s.morph); e->setProperty ("q", s.q); e->setProperty ("words", wordsVar (s.words));
        captures.add (juce::var (e));
    }
    d->setProperty ("captures", captures);
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (juce::var (d)));
}

bool open (Quad& quad, std::vector<Star>& stars, size_t libraryCount, const juce::File& file)
{
    quad = Quad();
    stars.resize (libraryCount);
    if (! file.existsAsFile()) return false;
    const auto v = juce::JSON::parse (file);
    if (v.getProperty ("schema", "").toString() != "trench-quad-v1") return false;
    if (auto* captures = v.getProperty ("captures", juce::var()).getArray())
        for (const auto& e : *captures)
        {
            Star s;
            s.name = e.getProperty ("name", "").toString(); s.kind = "capture";
            s.parentA = e.getProperty ("parentA", "").toString(); s.parentB = e.getProperty ("parentB", "").toString();
            s.morph = (double) e.getProperty ("morph", 0.0); s.q = (double) e.getProperty ("q", 0.0);
            s.words = wordsFrom (e.getProperty ("words", juce::var()));
            if (admit (s.words)) stars.push_back (s);
        }
    if (auto* pins = v.getProperty ("pins", juce::var()).getArray())
        for (int i = 0; i < std::min (4, pins->size()); ++i) quad.pins[(size_t) i] = indexOf (stars, (*pins)[i].toString());
    quad.morph = std::clamp ((double) v.getProperty ("morph", 0.0), 0.0, 100.0);
    quad.q = std::clamp ((double) v.getProperty ("q", 0.0), 0.0, 100.0);
    quad.captures = (int) v.getProperty ("captures", 0);
    if (quad.captures < (int) (stars.size() - libraryCount)) quad.captures = (int) (stars.size() - libraryCount);
    return true;
}

std::vector<double> curveHz() { return trench::core::logarithmic_frequency_grid (20.0, 20000.0, 160); }

std::vector<double> responseDb (const Words& words, const std::vector<double>& hz)
{
    std::array<trench::core::Biquad, kRows> cascade;
    for (size_t s = 0; s < kRows; ++s) cascade[s] = trench::core::section_words_to_biquad (words[s]);
    std::vector<double> out (hz.size());
    for (size_t i = 0; i < hz.size(); ++i) out[i] = trench::core::cascade_response_db (cascade, hz[i], trench::core::kP2kDatumHz);
    return out;
}

double peakDb (const Words& words, const std::vector<double>& hz)
{
    const auto db = responseDb (words, hz);
    return *std::max_element (db.begin(), db.end());
}

std::array<double, 4> formantsOf (const Words& words)
{
    std::vector<double> peaks;
    for (const auto& row : words)
    {
        const auto g = trench::core::geometry_from_words (row, trench::core::kP2kDatumHz);
        if (const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole))
        {
            const double width = 12.0 * std::log2 (1.0 + (-std::log (std::max (pole->radius, 1e-9)) * trench::core::kP2kDatumHz / 3.141592653589793) / std::max (pole->hz, 1.0));
            if (pole->hz >= 60.0 && pole->hz <= 6000.0 && width < 12.0) peaks.push_back (pole->hz);
        }
    }
    std::sort (peaks.begin(), peaks.end());
    std::array<double, 4> out { 0.0, 0.0, 0.0, 0.0 };
    for (size_t i = 0; i < std::min<size_t> (4, peaks.size()); ++i) out[i] = peaks[i];
    return out;
}

std::vector<double> hotCells (const Corners& c, int n, const std::vector<double>& hz)
{
    double top = -1e9;
    for (const auto& corner : c) top = std::max (top, peakDb (corner, hz));
    std::vector<double> out ((size_t) (n * n));
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
            out[(size_t) (j * n + i)] = peakDb (lerp (c, (i + 0.5) / n, (j + 0.5) / n), hz) - top;
    return out;
}
}
