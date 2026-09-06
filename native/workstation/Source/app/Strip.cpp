#include "Strip.h"
#include <algorithm>
#include <cmath>

namespace hs
{
namespace
{
int clampInt (int v, int lo, int hi) { return std::max (lo, std::min (v, hi)); }
double clamp01 (double v) { return std::max (0.0, std::min (100.0, v)); }

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

juce::var originVar (const Origin& o)
{
    auto* d = new juce::DynamicObject();
    d->setProperty ("kind", o.kind); d->setProperty ("body", o.body); d->setProperty ("corner", o.corner);
    d->setProperty ("parentA", o.parentA); d->setProperty ("parentB", o.parentB); d->setProperty ("morph", o.morph);
    return juce::var (d);
}

Origin originFrom (const juce::var& v)
{
    Origin o;
    o.kind = v.getProperty ("kind", "").toString(); o.body = v.getProperty ("body", "").toString(); o.corner = v.getProperty ("corner", "").toString();
    o.parentA = v.getProperty ("parentA", "").toString(); o.parentB = v.getProperty ("parentB", "").toString(); o.morph = (double) v.getProperty ("morph", 0.0);
    return o;
}
}

bool admit (const Words& words)
{
    for (const auto& row : words) if (row == trench::core::kIdentitySection) return false;
    return true;
}

Strip insert (Strip s, int k, const Anchor& c)
{
    if (! admit (c.q0) || ! admit (c.q1)) return s;
    k = clampInt (k, 1, s.count() + 1);
    s.anchors.insert (s.anchors.begin() + (k - 1), c);
    s.selected = k;
    return s;
}

Corners cornersOf (const Strip& s, int k)
{
    const int n = s.count();
    if (k <= 0) k = s.square;
    const int a = clampInt (k, 1, std::max (1, n)), b = std::min (a + 1, std::max (1, n));
    if (n == 0) return {};
    return { s.anchors[(size_t) a - 1].q0, s.anchors[(size_t) b - 1].q0, s.anchors[(size_t) a - 1].q1, s.anchors[(size_t) b - 1].q1 };
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

Words wordsAt (const Strip& s)
{
    if (s.count() == 0) return {};
    return lerp (cornersOf (s), s.morph / 100.0, s.q / 100.0);
}

Strip keep (Strip s)
{
    const int n = s.count();
    if (n < 2) return s;
    const int k = clampInt (s.square, 1, n - 1);
    const auto c = cornersOf (s, k);
    Anchor col;
    col.q0 = lerp (c, s.morph / 100.0, 0.0);
    col.q1 = lerp (c, s.morph / 100.0, 1.0);
    col.origin.kind = "capture";
    col.origin.parentA = s.anchors[(size_t) k - 1].name;
    col.origin.parentB = s.anchors[(size_t) k].name;
    col.origin.morph = s.morph;
    s.captures += 1;
    col.name = "C" + juce::String (s.captures);
    s = insert (s, k + 1, col);
    s.square = k + 1;
    s.morph = 0.0;
    return s;
}

Strip move (Strip s, int k, int direction)
{
    const int n = s.count(), j = k + (direction > 0 ? 1 : -1);
    if (k < 1 || k > n || j < 1 || j > n) return s;
    std::swap (s.anchors[(size_t) k - 1], s.anchors[(size_t) j - 1]);
    s.selected = j;
    return s;
}

Strip remove (Strip s, int k)
{
    const int n = s.count();
    if (k < 1 || k > n) return s;
    s.anchors.erase (s.anchors.begin() + (k - 1));
    s.selected = std::min (k, s.count());
    s.square = clampInt (s.square, 1, s.squares());
    if (s.count() == 0) { s.square = 1; s.morph = 0.0; }
    return s;
}

Strip step (Strip s, double dm, double dq)
{
    const int last = s.squares();
    double m = s.morph + dm;
    int k = s.square;
    while (m > 100.0 && k < last) { m -= 100.0; ++k; }
    while (m < 0.0 && k > 1) { m += 100.0; --k; }
    s.square = k;
    s.morph = clamp01 (m);
    s.q = clamp01 (s.q + dq);
    return s;
}

Strip jumpTo (Strip s, int k)
{
    if (k < 1 || k > s.count()) return s;
    s.selected = k;
    s.square = std::max (1, k - 1);
    s.morph = k > 1 ? 100.0 : 0.0;
    return s;
}

Bytes bytesOf (const Corners& c) { return bodyOf (c).legacy_bytes(); }

bool writeBody (const Strip& s, const juce::File& file)
{
    if (s.count() == 0) return false;
    const auto b = bytesOf (cornersOf (s));
    file.getParentDirectory().createDirectory();
    return file.replaceWithData (b.data(), b.size());
}

bool save (const Strip& s, const juce::File& file)
{
    auto* d = new juce::DynamicObject();
    d->setProperty ("schema", "trench-strip-v1");
    d->setProperty ("square", s.square); d->setProperty ("morph", s.morph); d->setProperty ("q", s.q);
    d->setProperty ("selected", s.selected); d->setProperty ("captures", s.captures);
    juce::Array<juce::var> anchors;
    for (const auto& c : s.anchors)
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("name", c.name); e->setProperty ("origin", originVar (c.origin));
        e->setProperty ("q0", wordsVar (c.q0)); e->setProperty ("q1", wordsVar (c.q1));
        anchors.add (juce::var (e));
    }
    d->setProperty ("anchors", anchors);
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (juce::var (d)));
}

Strip open (const juce::File& file)
{
    Strip s;
    if (! file.existsAsFile()) return s;
    const auto v = juce::JSON::parse (file);
    if (v.getProperty ("schema", "").toString() != "trench-strip-v1") return s;
    auto list = v.getProperty ("anchors", juce::var());
    if (! list.isArray()) list = v.getProperty ("columns", juce::var());
    if (auto* anchors = list.getArray())
        for (const auto& e : *anchors)
        {
            Anchor c;
            c.name = e.getProperty ("name", "").toString();
            c.origin = originFrom (e.getProperty ("origin", juce::var()));
            c.q0 = wordsFrom (e.getProperty ("q0", juce::var())); c.q1 = wordsFrom (e.getProperty ("q1", juce::var()));
            s = insert (s, s.count() + 1, c);
        }
    s.square = clampInt ((int) v.getProperty ("square", 1), 1, s.squares());
    s.morph = clamp01 ((double) v.getProperty ("morph", 0.0)); s.q = clamp01 ((double) v.getProperty ("q", 0.0));
    s.selected = clampInt ((int) v.getProperty ("selected", 0), 0, s.count());
    s.captures = (int) v.getProperty ("captures", 0);
    return s;
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
}
