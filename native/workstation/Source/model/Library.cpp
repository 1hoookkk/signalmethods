#include "Library.h"
#include <algorithm>
#include <cmath>

namespace ws
{
namespace
{
Words wordsFromVar (const juce::var& rows)
{
    Words w {};
    for (int s = 0; s < kRows && s < rows.size(); ++s)
        for (int k = 0; k < kWords && k < rows[s].size(); ++k)
            w[(size_t) s][(size_t) k] = (std::uint16_t) (int) rows[s][k];
    return w;
}
}

bool Library::loadJson (const juce::File& file)
{
    const auto parsed = juce::JSON::parse (file);
    if (! parsed.isArray()) return false;
    for (const auto& item : *parsed.getArray())
    {
        Frame f;
        f.name = item["name"].toString().replace (juce::String (juce::CharPointer_UTF8 ("\xef\xbf\xbd")), juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")));
        f.words = wordsFromVar (item["words"]);
        f.group = groupOf (f.name);
        measure (f);
        frames.push_back (f);
    }
    return ! frames.empty();
}

bool Library::loadBodies (const juce::File& dir)
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
            f.group = 0;
            measure (f);
            frames.push_back (f);
        }
    }
    return ! frames.empty();
}

void Library::computePca()
{
    const int n = (int) frames.size();
    if (n < 3) return;
    std::vector<Curve> curves;
    for (const auto& f : frames) curves.push_back (curveOf (f.words));
    Curve mean {};
    for (const auto& c : curves) for (int i = 0; i < kCurvePoints; ++i) mean[(size_t) i] += c[(size_t) i] / n;
    for (auto& c : curves) for (int i = 0; i < kCurvePoints; ++i) c[(size_t) i] -= mean[(size_t) i];
    std::vector<Curve> comps;
    for (int k = 0; k < 2; ++k)
    {
        Curve v {};
        for (int i = 0; i < kCurvePoints; ++i) v[(size_t) i] = std::sin (0.37 * (i + 1) * (k + 1)) + 0.5;
        for (int it = 0; it < 200; ++it)
        {
            Curve nv {};
            for (const auto& c : curves)
            {
                double dot = 0.0;
                for (int i = 0; i < kCurvePoints; ++i) dot += c[(size_t) i] * v[(size_t) i];
                for (int i = 0; i < kCurvePoints; ++i) nv[(size_t) i] += dot * c[(size_t) i];
            }
            for (const auto& pc : comps)
            {
                double dot = 0.0;
                for (int i = 0; i < kCurvePoints; ++i) dot += nv[(size_t) i] * pc[(size_t) i];
                for (int i = 0; i < kCurvePoints; ++i) nv[(size_t) i] -= dot * pc[(size_t) i];
            }
            double norm = 0.0;
            for (double x : nv) norm += x * x;
            norm = std::sqrt (std::max (norm, 1e-30));
            for (int i = 0; i < kCurvePoints; ++i) v[(size_t) i] = nv[(size_t) i] / norm;
        }
        comps.push_back (v);
    }
    for (int k = 0; k < 2; ++k)
    {
        std::vector<double> proj;
        for (const auto& c : curves)
        {
            double dot = 0.0;
            for (int i = 0; i < kCurvePoints; ++i) dot += c[(size_t) i] * comps[(size_t) k][(size_t) i];
            proj.push_back (dot);
        }
        const double lo = *std::min_element (proj.begin(), proj.end()), hi = *std::max_element (proj.begin(), proj.end());
        for (int i = 0; i < n; ++i) frames[(size_t) i].m[(size_t) (6 + k)] = hi > lo ? (proj[(size_t) i] - lo) / (hi - lo) : 0.5;
    }
}

std::array<double, 2> Library::coordOf (const Frame& f) const
{
    const double ax = axisX == 0 ? 1.0 - f.m[0] : f.m[(size_t) axisX];
    const double ay = axisY == 0 ? 1.0 - f.m[0] : f.m[(size_t) axisY];
    return { ax, ay };
}

void Library::sort()
{
    anchors.clear();
    for (int i = 0; i < (int) frames.size(); ++i) anchors.push_back ({ i, coordOf (frames[(size_t) i]) });
    retriangulate();
}

void Library::retriangulate()
{
    std::vector<std::array<double, 2>> pts;
    for (const auto& a : anchors) pts.push_back (a.p);
    tris = delaunay (pts);
}

std::optional<Blend> Library::blendAt (double u, double v) const
{
    for (const auto& t : tris)
    {
        const auto& a = anchors[(size_t) t.a].p;
        const auto& b = anchors[(size_t) t.b].p;
        const auto& c = anchors[(size_t) t.c].p;
        const double v0x = b[0] - a[0], v0y = b[1] - a[1], v1x = c[0] - a[0], v1y = c[1] - a[1], v2x = u - a[0], v2y = v - a[1];
        const double den = v0x * v1y - v1x * v0y;
        if (std::abs (den) < 1e-14) continue;
        const double bv = (v2x * v1y - v1x * v2y) / den, bw = (v0x * v2y - v2x * v0y) / den, bu = 1.0 - bv - bw;
        if (bu >= -1e-9 && bv >= -1e-9 && bw >= -1e-9) return Blend { { t.a, t.b, t.c }, { bu, bv, bw } };
    }
    return std::nullopt;
}

Words Library::wordsOf (const Blend& b) const
{
    std::vector<const Words*> parents;
    std::vector<double> weights;
    for (int i = 0; i < 3; ++i)
    {
        parents.push_back (&frames[(size_t) anchors[(size_t) b.anchors[(size_t) i]].frame].words);
        weights.push_back (b.w[(size_t) i]);
    }
    return blend (parents, weights);
}

int Library::addCapture (const Words& words, std::array<double, 2> at)
{
    Frame f;
    f.words = words;
    f.capture = true;
    f.name = "cap " + juce::String (at[0], 2) + "," + juce::String (at[1], 2);
    f.group = kGroups - 1;
    measure (f);
    frames.push_back (f);
    anchors.push_back ({ (int) frames.size() - 1, at });
    retriangulate();
    return (int) frames.size() - 1;
}
}
