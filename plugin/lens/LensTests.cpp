#include "Locator.h"
#include <trench/core/audition.hpp>
#include <trench/core/native_body.hpp>
#include <cmath>
#include <cstdio>
#include <span>

namespace
{
int failures = 0;
void check (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++failures; }

std::vector<float> through (const trench::core::CornerWords& words, double datum, double rate, float gain, unsigned seed)
{
    trench::core::CascadeRunner runner;
    runner.set_sample_rate (rate);
    runner.reset();
    runner.set_ring_leveller (false);
    runner.set_pole_distortion (0.0);
    runner.set_immediate (trench::core::native::rewarp_cascade (words, datum, rate));
    std::vector<float> out (88200);
    for (auto& v : out) { seed = seed * 1664525u + 1013904223u; v = ((float) (seed >> 8) / 16777216.0f - 0.5f) * gain; }
    runner.process (std::span<float> (out.data(), out.size()));
    return out;
}

}

int main()
{
    const juce::File root (TRENCH_TABLE_STITCH_ROOT);
    lens::Locator locator;
    const bool loaded = locator.load (root.getChildFile ("evidence/research-results/corpus_index/corpus_index.bin"));
    std::printf ("      corpus index: %d nodes\n", (int) locator.nodes().size());
    check (loaded && locator.nodes().size() == 2444, "the corpus index loads 2444 nodes, 132 P2K and 2312 Morpheus corners");

    int ubu = -1;
    for (int i = 0; i < (int) locator.nodes().size(); ++i) if (locator.nodes()[(size_t) i].name == "Ubu Orator M0 Q1") ubu = i;
    juce::MemoryBlock raw;
    root.getChildFile ("plugin/presets/p2k/ubu_orator.body240").loadFileAsData (raw);
    bool exact = ubu >= 0 && raw.getSize() == 240;
    if (exact)
    {
        const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> ((const std::uint8_t*) raw.getData(), raw.getSize()));
        for (size_t row = 0; row < trench::core::kSectionCount; ++row) exact = exact && locator.nodes()[(size_t) ubu].words[row] == body.words[2][row];
    }
    check (exact, "a node's words are the corpus file's bytes, row for row, seventh row the identity");

    const double rate = 44100.0;
    const int factor = 4;
    const auto& node = locator.nodes()[(size_t) ubu];
    const auto loud = lens::Locator::decimate (through (node.words, node.datum, rate, 0.5f, 7u), factor);
    const auto soft = lens::Locator::decimate (through (node.words, node.datum, rate, 0.05f, 7u), factor);
    const double analysisRate = rate / factor;
    auto best = [&] (const std::vector<float>& signal)
    {
        std::vector<lens::Match> last;
        locator.resetAverage();
        for (size_t start = 0; start + lens::kFrame <= signal.size(); start += 128)
        {
            const auto d = locator.describeAveraged (signal.data() + start, analysisRate);
            last = locator.rank (d, 5);
        }
        return last;
    };
    const auto rankedLoud = best (loud), rankedSoft = best (soft);
    {
        const auto d = locator.describe (loud.data() + 4096, analysisRate);
        float lo = 1e9f, hi = -1e9f, nlo = 1e9f, nhi = -1e9f; double acc = 0.0;
        for (int k = 0; k < lens::kBins; ++k) { lo = std::min (lo, d[(size_t) k]); hi = std::max (hi, d[(size_t) k]); nlo = std::min (nlo, node.descriptor[(size_t) k]); nhi = std::max (nhi, node.descriptor[(size_t) k]); const double e = d[(size_t) k] - node.descriptor[(size_t) k]; acc += e * e; }
        std::printf ("      lpc descriptor range %.1f..%.1f dB, ubu descriptor range %.1f..%.1f dB, rms to ubu %.2f, correlation distance %.3f\n", lo, hi, nlo, nhi, std::sqrt (acc / lens::kBins), lens::Locator::distance (d, node.descriptor));
        std::printf ("      lpc bins 0 32 64 96 127: %.1f %.1f %.1f %.1f %.1f  ubu: %.1f %.1f %.1f %.1f %.1f\n", d[0], d[32], d[64], d[96], d[127], node.descriptor[0], node.descriptor[32], node.descriptor[64], node.descriptor[96], node.descriptor[127]);
    }
    std::printf ("      noise through Ubu Orator M0 Q1 locates: %s (%.3f), soft: %s (%.3f)\n",
        locator.nodes()[(size_t) rankedLoud[0].node].name.c_str(), rankedLoud[0].distance, locator.nodes()[(size_t) rankedSoft[0].node].name.c_str(), rankedSoft[0].distance);
    auto samePoles = [&] (int a, int b) { return lens::Locator::distance (locator.nodes()[(size_t) a].descriptor, locator.nodes()[(size_t) b].descriptor) < 0.01f; };
    bool inNeighbourhood = false;
    for (const auto& m : rankedLoud) inNeighbourhood = inNeighbourhood || samePoles (m.node, ubu);
    std::printf ("      ranked:");
    for (const auto& m : rankedLoud) std::printf (" [%s %.2f]", locator.nodes()[(size_t) m.node].name.c_str(), m.distance);
    std::printf ("\n");
    check (inNeighbourhood, "noise through a corpus corner puts that corner, or one with the same pole half, in the ranked neighbourhood of five");
    check (! rankedSoft.empty() && rankedSoft[0].node == rankedLoud[0].node, "twenty dB less input gain locates the same corner");

    std::vector<float> silence (lens::kFrame, 0.0f);
    check (lens::Locator::levelDb (silence.data(), lens::kFrame) < -90.0f && lens::Locator::levelDb (loud.data(), lens::kFrame) > -60.0f, "silence sits under the gate and the signal above it, so silence produces no new match");

    const auto& shown = locator.nodes()[(size_t) rankedLoud[0].node];
    const double atOneK = lens::Locator::responseDb (shown.words, shown.datum, 1000.0);
    const auto cascade = trench::core::native::rewarp_cascade (shown.words, shown.datum, rate);
    check (std::isfinite (atOneK) && cascade.size() == trench::core::kSectionCount, "the plotted response and the audio cascade come from the same node's exact words");

    std::printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
