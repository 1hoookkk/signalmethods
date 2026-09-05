#include "model/Library.h"
#include "model/Body.h"
#include "model/Morph.h"
#include <cmath>
#include <cstdio>

namespace
{
int failures = 0;

void check (bool ok, const char* what, double a = 0.0, double b = 0.0)
{
    std::printf ("%s  %s  (%.4f  %.4f)\n", ok ? "ok  " : "FAIL", what, a, b);
    if (! ok) ++failures;
}

double semis (double a, double b) { return 12.0 * std::log2 (b / a); }

double maxRadius (const ws::Words& w)
{
    double r = 0.0;
    for (const auto& g : ws::geometryOf (w)) if (g.pole) r = std::max (r, g.pR);
    return r;
}

bool leavesFrame (const ws::Words& w)
{
    const auto c = ws::cascadeOf (w);
    for (int i = 0; i < ws::kCurvePoints; ++i)
        if (std::abs (ws::responseDb (c, 20.0 * std::pow (1000.0, i / double (ws::kCurvePoints - 1)))) > 30.0) return true;
    return false;
}
}

int main()
{
    ws::Library lib;
    const juce::File root (TRENCH_TABLE_STITCH_ROOT);
    if (! lib.loadJson (root.getChildFile ("native/python/workstation/frames_3d.json"))) lib.loadBodies (root.getChildFile ("plugin/presets/p2k"));
    std::vector<std::pair<int, int>> pairs;
    for (int i = 0; i + 1 < (int) lib.frames.size(); ++i)
        if (lib.frames[(size_t) i].name.endsWith ("M0 Q0") && lib.frames[(size_t) i + 1].name.endsWith ("M1 Q0")) pairs.push_back ({ i, i + 1 });
    std::printf ("frames %d  pairs %d\n", (int) lib.frames.size(), (int) pairs.size());

    double worstRatio = 0.0, worstSemis = 0.0;
    int measured = 0;
    for (const auto& [ia, ib] : pairs)
    {
        const auto& a = lib.frames[(size_t) ia].words;
        const auto& b = lib.frames[(size_t) ib].words;
        const auto ga = ws::geometryOf (a), g1 = ws::geometryOf (ws::pairMorph (a, b, 1.0).words), g2 = ws::geometryOf (ws::pairMorph (a, b, 2.0).words);
        for (int s = 0; s < ws::kRows - 1; ++s)
        {
            if (! (ga[(size_t) s].pole && g1[(size_t) s].pole && g2[(size_t) s].pole)) continue;
            const double d1 = semis (ga[(size_t) s].pHz, g1[(size_t) s].pHz), d2 = semis (ga[(size_t) s].pHz, g2[(size_t) s].pHz);
            if (std::abs (d1) < 1.0) continue;
            const double expectHz = ga[(size_t) s].pHz * std::pow (2.0, 2.0 * d1 / 12.0);
            if (expectHz > ws::kDatumHz * 0.49 || expectHz < 21.0) continue;
            ++measured;
            const double err = std::abs (d2 - 2.0 * d1);
            if (err > worstSemis)
            {
                worstSemis = err;
                worstRatio = d2 / d1;
                std::printf ("  %s row %d  d1 %.2f st  d2 %.2f st  a %.0f Hz r %.4f\n", lib.frames[(size_t) ia].name.toRawUTF8(), s + 1, d1, d2, ga[(size_t) s].pHz, ga[(size_t) s].pR);
            }
        }
    }
    check (measured > 0 && worstSemis < 1.0, "t = 2 doubles every pole's semitone distance from a, worst error in semitones and ratio", worstSemis, worstRatio);

    double worstR = 0.0, worstT = 0.0, nativeR = 0.0;
    for (const auto& [ia, ib] : pairs)
    {
        nativeR = std::max ({ nativeR, maxRadius (lib.frames[(size_t) ia].words), maxRadius (lib.frames[(size_t) ib].words) });
        for (double t = ws::kPushLow; t <= ws::kPushHigh + 1e-9; t += 0.05)
        {
            if (t >= 0.0 && t <= 1.0) continue;
            const double r = maxRadius (ws::pairMorph (lib.frames[(size_t) ia].words, lib.frames[(size_t) ib].words, t).words);
            if (r > worstR) { worstR = r; worstT = t; }
        }
    }
    std::printf ("  largest native radius in the pairs %.5f\n", nativeR);
    check (worstR <= ws::kRadiusGuard + 1e-9, "no pole radius exceeds the guard at any t outside 0 .. 1, worst radius and its t", worstR, worstT);

    int mismatches = 0;
    for (const auto& [ia, ib] : pairs)
    {
        const auto& a = lib.frames[(size_t) ia].words;
        const auto& b = lib.frames[(size_t) ib].words;
        trench::core::PackedBody body;
        for (auto& c : body.words) c.fill (trench::core::kIdentitySection);
        for (int c = 0; c < 8; ++c)
            for (int s = 0; s < ws::kRows; ++s)
                for (int k = 0; k < ws::kWords; ++k) body.words[(size_t) c][(size_t) s][(size_t) k] = ((c & 1) ? b : a)[(size_t) s][(size_t) k];
        const auto plugin = body.interpolate_words (0.5f, 0.0f, 0.0f);
        const auto ours = ws::pairMorph (a, b, 0.5).words;
        for (int s = 0; s < ws::kRows; ++s)
            for (int k = 0; k < ws::kWords; ++k) if (plugin[(size_t) s][(size_t) k] != ours[(size_t) s][(size_t) k]) ++mismatches;
    }
    check (mismatches == 0, "t = 0.5 equals the plugin's interpolate_words word for word, mismatched words", mismatches);

    {
        const auto& a = lib.frames[(size_t) pairs[0].first].words;
        const auto& b = lib.frames[(size_t) pairs[0].second].words;
        ws::Library scratch;
        for (double t : { 1.5, 0.0, 1.5, 0.0 }) scratch.addCapture (ws::pairMorph (a, b, t).words, { 0.5, 0.5 });
        ws::Body body;
        body.corner = { 0, 1, 2, 3 };
        body.unity = false;
        const juce::File out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ws_caricature_test.body240");
        check (body.exportTo (scratch.frames, out), "body exported at t = 1.5");
        juce::MemoryBlock bytes;
        out.loadFileAsData (bytes);
        check (bytes.getSize() == trench::core::kLegacyBodyBytes, "export is 240 bytes", (double) bytes.getSize());
        const auto loaded = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> ((const std::uint8_t*) bytes.getData(), bytes.getSize()));
        check (loaded.is_legacy_representable(), "loaded body is legacy representable");
        double r = 0.0;
        for (int i = 0; i <= 10; ++i)
            for (int j = 0; j <= 10; ++j)
            {
                const auto w = loaded.interpolate_words (i / 10.0f, j / 10.0f, 0.0f);
                ws::Words ww;
                for (int s = 0; s < ws::kRows; ++s) for (int k = 0; k < ws::kWords; ++k) ww[(size_t) s][(size_t) k] = w[(size_t) s][(size_t) k];
                r = std::max (r, maxRadius (ww));
            }
        check (r <= ws::kRadiusGuard + 1e-9, "every wheel position of the exported body stays inside the guard", r);
    }

    {
        int ticksWhenOut = 0, outFrames = 0, ticksWhenIn = 0, inFrames = 0;
        for (const auto& [ia, ib] : pairs)
            for (double t : { 0.5, 2.0, 3.0, -2.0 })
            {
                const auto w = ws::pairMorph (lib.frames[(size_t) ia].words, lib.frames[(size_t) ib].words, t).words;
                const auto ex = ws::excessOf (w);
                if (leavesFrame (w)) { ++outFrames; if (! ex.empty()) ++ticksWhenOut; }
                else { ++inFrames; if (! ex.empty()) ++ticksWhenIn; }
            }
        check (ticksWhenOut == outFrames && ticksWhenIn == 0, "excess ticks appear when and only when the curve leaves the frame, frames out and in", outFrames, inFrames);
    }

    {
        const auto w = ws::schwaWords();
        const auto g = ws::geometryOf (w);
        check (g[0].pole && std::abs (semis (g[0].pHz, 500.0)) < 0.2 && g[4].pole && std::abs (semis (g[4].pHz, 4500.0)) < 0.2, "schwa sits on the uniform tube", g[0].pHz, g[4].pHz);
    }

    std::printf ("%s  %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
