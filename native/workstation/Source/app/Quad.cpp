#include "Quad.h"
#include "Library.h"
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

bool admit (const Words&) { return true; }

namespace
{
int freqCode (int f) { return ((220 * std::clamp (f, 0, kFreqCodes - 1)) >> 7) + 18; }
int radCode (int freq) { return ((freq * 0x7c) >> 8) + 0x76; }
std::uint16_t byteWord (int code) { return (std::uint16_t) (std::clamp (code, 0, 255) << 8); }
}

trench::core::PackedSection rowWords (Row row, std::uint16_t fifth)
{
    if (row.type == RowType::rest) return { 0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, fifth };
    const int freq = freqCode (row.f), rad = radCode (freq);
    int g = std::clamp (std::clamp (row.g, kGainMin, kGainMax), rad - 254, 255 - rad);
    const auto angle = byteWord (freq);
    auto make = [&] (int gain) {
        const auto zero = row.type == RowType::notch ? (std::uint16_t) 0x0000 : byteWord (rad + gain);
        return trench::core::PackedSection { angle, zero, angle, byteWord (rad - gain), fifth };
    };
    while (g < kGainMax && rowHz (make (g)) <= 0.0) ++g;
    return make (g);
}

Row rowOf (const trench::core::PackedSection& words)
{
    Row row;
    if (rowHz (words) <= 0.0) return row;
    const int angle = words[2] >> 8;
    int best = 1 << 20;
    for (int f = 0; f < kFreqCodes; ++f)
    {
        const int d = std::abs (freqCode (f) - angle);
        if (d < best) { best = d; row.f = f; }
    }
    row.g = std::clamp (radCode (freqCode (row.f)) - (int) (words[3] >> 8), kGainMin, kGainMax);
    row.type = words[1] < 0x0100 ? RowType::notch : RowType::peak;
    return row;
}

Section sectionOf (const trench::core::PackedSection& words)
{
    const auto g = trench::core::geometry_from_words (words, trench::core::kP2kDatumHz);
    Section s;
    s.scale = g.scale;
    if (const auto* p = std::get_if<trench::core::ConjugatePair> (&g.pole); p != nullptr && p->radius >= 0.05 && p->hz > 0.0) { s.pole = true; s.poleHz = p->hz; s.poleRadius = p->radius; }
    if (const auto* z = std::get_if<trench::core::ConjugatePair> (&g.zero); z != nullptr && z->radius >= 0.05 && z->hz > 0.0) { s.zero = true; s.zeroHz = z->hz; s.zeroRadius = z->radius; }
    return s;
}

trench::core::PackedSection sectionWords (const Section& section, std::uint16_t fifth, bool keepFifth)
{
    trench::core::SectionGeometry g;
    g.pole = section.pole ? trench::core::ConjugatePair { std::clamp (section.poleHz, 20.0, 20000.0), std::clamp (section.poleRadius, 0.05, 0.99999) }
                          : trench::core::ConjugatePair { 1000.0, 0.0 };
    g.zero = section.zero ? trench::core::ConjugatePair { std::clamp (section.zeroHz, 20.0, 20000.0), std::clamp (section.zeroRadius, 0.05, 1.0) }
                          : trench::core::ConjugatePair { 1000.0, 0.0 };
    g.scale = std::clamp (section.scale, 0.0, 4.0);
    auto w = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
    if (keepFifth) w[4] = fifth;
    return w;
}

double widthSt (double hz, double radius)
{
    if (hz <= 0.0) return 0.0;
    const double bw = -std::log (std::clamp (radius, 1e-9, 1.0)) * trench::core::kP2kDatumHz / 3.141592653589793;
    return 24.0 * std::asinh (bw / (2.0 * hz)) / std::log (2.0);
}

double radiusForWidth (double hz, double st)
{
    const double bw = 2.0 * std::max (hz, 1.0) * std::sinh (std::max (0.0, st) * std::log (2.0) / 24.0);
    return std::clamp (std::exp (-3.141592653589793 * bw / trench::core::kP2kDatumHz), 0.0, 1.0);
}

Words carved (const Words& words, double amount)
{
    const double a = std::clamp (amount, 0.0, 1.0);
    std::vector<std::pair<double, size_t>> poles;
    for (size_t r = 0; r + 1 < kRows; ++r)
        if (const auto s = sectionOf (words[r]); s.pole) poles.emplace_back (s.poleHz, r);
    std::sort (poles.begin(), poles.end());
    Words out = words;
    for (size_t k = 0; k < poles.size(); ++k)
    {
        const size_t r = poles[k].second;
        Section s = sectionOf (words[r]);
        const double above = k + 1 < poles.size() ? poles[k + 1].first : k > 0 ? s.poleHz * s.poleHz / poles[k - 1].first : s.poleHz * 2.25;
        const double target = std::clamp (std::sqrt (s.poleHz * above), 20.0, 20000.0);
        const double fromHz = s.zero ? s.zeroHz : s.poleHz, fromRadius = s.zero ? s.zeroRadius : 0.5;
        s.zero = true;
        s.zeroHz = fromHz * std::pow (target / fromHz, a);
        s.zeroRadius = fromRadius + (0.97 - fromRadius) * a;
        out[r] = sectionWords (s, words[r][4]);
    }
    return out;
}

double rowHz (const trench::core::PackedSection& words)
{
    const auto g = trench::core::geometry_from_words (words, trench::core::kP2kDatumHz);
    const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
    return pole != nullptr && pole->radius >= 0.05 ? pole->hz : 0.0;
}

double rowDb (const trench::core::PackedSection& words)
{
    const double hz = rowHz (words);
    if (hz <= 0.0) return 0.0;
    const std::array<trench::core::Biquad, 1> one { trench::core::section_words_to_biquad (words) };
    return trench::core::cascade_response_db (one, hz, trench::core::kP2kDatumHz);
}

juce::String noteName (double hz)
{
    if (hz <= 0.0) return "rest";
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
    const int n = (int) std::lround (midi);
    const int cents = (int) std::lround ((midi - n) * 100.0);
    juce::String out = juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    if (cents != 0) out += (cents > 0 ? "+" : "") + juce::String (cents);
    return out;
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

trench::core::PackedBody motherBodyOf (const Explore& explore, const std::vector<Star>& stars)
{
    trench::core::PackedBody body;
    for (auto& corner : body.words) corner.fill (trench::core::kIdentitySection);
    const int anchors[2] = { explore.a, explore.b };
    const double ratio = std::pow (2.0, explore.octaves);
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
            {
                const int pin = anchors[x];
                if (pin < 0 || pin >= (int) stars.size()) continue;
                Words w = stars[(size_t) pin].words;
                if (x == 1 && anchors[0] >= 0 && anchors[0] < (int) stars.size()) w = laneLocked (stars[(size_t) anchors[0]].words, w);
                if (y == 1) w = transposed (w, ratio);
                if (z == 0) w = relaxed (w);
                std::copy (w.begin(), w.end(), body.words[(size_t) (x + 2 * y + 4 * z)].begin());
            }
    return body;
}

Words motherWordsAt (const Explore& explore, const std::vector<Star>& stars)
{
    const auto corner = motherBodyOf (explore, stars).interpolate_words ((float) explore.morph, (float) explore.frequency, (float) explore.stress);
    Words words;
    std::copy_n (corner.begin(), kRows, words.begin());
    return words;
}

Bytes bytesOf (const Corners& c) { return bodyOf (c).legacy_bytes(); }

bool writeBody (const Quad& quad, const std::vector<Star>& stars, const juce::File& file)
{
    if (! quad.complete()) return false;
    const auto b = bytesOf (cornersOf (quad, stars));
    file.getParentDirectory().createDirectory();
    return file.replaceWithData (b.data(), b.size());
}

bool save (const Quad& quad, const std::vector<Star>& stars, size_t libraryCount, const juce::File& file, const Explore* explore, const Patch* patch)
{
    auto* d = new juce::DynamicObject();
    d->setProperty ("schema", "trench-quad-v1");
    d->setProperty ("morph", quad.morph); d->setProperty ("q", quad.q); d->setProperty ("captures", quad.captures);
    juce::Array<juce::var> pins;
    for (int p : quad.pins) pins.add (p >= 0 && p < (int) stars.size() ? stars[(size_t) p].name : juce::String());
    d->setProperty ("pins", pins);
    if (explore != nullptr)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("a", explore->a >= 0 && explore->a < (int) stars.size() ? stars[(size_t) explore->a].name : juce::String());
        e->setProperty ("b", explore->b >= 0 && explore->b < (int) stars.size() ? stars[(size_t) explore->b].name : juce::String());
        e->setProperty ("morph", explore->morph);
        e->setProperty ("frequency", explore->frequency);
        e->setProperty ("stress", explore->stress);
        e->setProperty ("octaves", explore->octaves);
        d->setProperty ("explore", juce::var (e));
    }
    if (patch != nullptr)
    {
        auto* p = new juce::DynamicObject();
        p->setProperty ("fixed", patch->fixed);
        d->setProperty ("patch", juce::var (p));
    }
    juce::Array<juce::var> captures;
    for (size_t i = libraryCount; i < stars.size(); ++i)
    {
        const auto& s = stars[i];
        auto* e = new juce::DynamicObject();
        e->setProperty ("name", s.name); e->setProperty ("kind", s.kind); e->setProperty ("body", s.body);
        e->setProperty ("parentA", s.parentA); e->setProperty ("parentB", s.parentB);
        e->setProperty ("morph", s.morph); e->setProperty ("q", s.q); e->setProperty ("words", wordsVar (s.words));
        captures.add (juce::var (e));
    }
    d->setProperty ("captures", captures);
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (juce::var (d)));
}

bool open (Quad& quad, std::vector<Star>& stars, size_t libraryCount, const juce::File& file, Explore* explore, Patch* patch)
{
    quad = Quad();
    if (explore != nullptr) *explore = Explore();
    if (patch != nullptr) *patch = Patch();
    stars.resize (libraryCount);
    if (! file.existsAsFile()) return false;
    const auto v = juce::JSON::parse (file);
    if (v.getProperty ("schema", "").toString() != "trench-quad-v1") return false;
    if (auto* captures = v.getProperty ("captures", juce::var()).getArray())
        for (const auto& e : *captures)
        {
            Star s;
            s.name = e.getProperty ("name", "").toString();
            s.kind = e.getProperty ("kind", "capture").toString(); s.body = e.getProperty ("body", s.name).toString();
            s.parentA = e.getProperty ("parentA", "").toString(); s.parentB = e.getProperty ("parentB", "").toString();
            s.morph = (double) e.getProperty ("morph", 0.0); s.q = (double) e.getProperty ("q", 0.0);
            s.words = wordsFrom (e.getProperty ("words", juce::var()));
            if (admit (s.words)) stars.push_back (s);
        }
    if (auto* pins = v.getProperty ("pins", juce::var()).getArray())
        for (int i = 0; i < std::min (4, pins->size()); ++i) quad.pins[(size_t) i] = indexOf (stars, (*pins)[i].toString());
    if (explore != nullptr)
    {
        const auto e = v.getProperty ("explore", juce::var());
        explore->a = indexOf (stars, e.getProperty ("a", "").toString());
        explore->b = indexOf (stars, e.getProperty ("b", "").toString());
        explore->morph = std::clamp ((double) e.getProperty ("morph", 0.5), 0.0, 1.0);
        explore->frequency = std::clamp ((double) e.getProperty ("frequency", 0.0), 0.0, 1.0);
        explore->stress = std::clamp ((double) e.getProperty ("stress", 1.0), 0.0, 1.0);
        explore->octaves = std::clamp ((double) e.getProperty ("octaves", 1.0), -3.0, 3.0);
    }
    if (patch != nullptr)
    {
        const auto p = v.getProperty ("patch", juce::var());
        if (p.isObject()) patch->fixed = (bool) p.getProperty ("fixed", patch->fixed);
    }
    quad.morph = std::clamp ((double) v.getProperty ("morph", 0.0), 0.0, 100.0);
    quad.q = std::clamp ((double) v.getProperty ("q", 0.0), 0.0, 100.0);
    quad.captures = (int) v.getProperty ("captures", 0);
    int kept = 0;
    for (size_t i = libraryCount; i < stars.size(); ++i) kept += stars[i].kind == "capture";
    if (quad.captures < kept) quad.captures = kept;
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
            if (pole->hz >= 60.0 && pole->hz <= 6000.0 && width < 6.0) peaks.push_back (pole->hz);
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
