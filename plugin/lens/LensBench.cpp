#include "Locator.h"
#include "dsp/Formants.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <trench/core/body_from_audio.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace
{
constexpr double kRate = 44100.0;
constexpr int kFactor = 4;
constexpr double kHopSeconds = 128.0 / (kRate / kFactor);

struct Truth { lens::Descriptor shape; };

std::vector<float> resampled (const trench::core::audio::MonoClip& clip)
{
    if (std::abs (clip.sample_rate_hz - kRate) < 1.0) return clip.samples;
    std::vector<float> out;
    const double step = clip.sample_rate_hz / kRate;
    for (double at = 0.0; at + 1.0 < (double) clip.samples.size(); at += step)
    {
        const size_t i = (size_t) at;
        const double f = at - (double) i;
        out.push_back ((float) ((1.0 - f) * clip.samples[i] + f * clip.samples[i + 1]));
    }
    return out;
}

Truth groundTruth (const std::vector<float>& ir)
{
    constexpr int order = 15, n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> bins ((size_t) n * 2, 0.0f);
    for (size_t i = 0; i < std::min (ir.size(), (size_t) n); ++i) bins[i] = ir[i];
    fft.performFrequencyOnlyForwardTransform (bins.data());
    std::vector<double> db, hz;
    for (int k = 1; k < n / 2; ++k)
    {
        hz.push_back ((double) k * kRate / n);
        db.push_back (20.0 * std::log10 (std::max (1.0e-9, (double) bins[(size_t) k])));
    }
    return { lens::Locator::shape (db, hz) };
}

std::vector<float> excite (const std::vector<float>& ir, double seconds, unsigned seed)
{
    const int n = (int) (seconds * kRate);
    juce::AudioBuffer<float> noise (1, n);
    for (int i = 0; i < n; ++i) { seed = seed * 1664525u + 1013904223u; noise.setSample (0, i, ((float) (seed >> 8) / 16777216.0f - 0.5f) * 0.5f); }
    juce::dsp::Convolution convolution;
    juce::AudioBuffer<float> irBuffer (1, (int) ir.size());
    for (size_t i = 0; i < ir.size(); ++i) irBuffer.setSample (0, (int) i, ir[i]);
    convolution.loadImpulseResponse (std::move (irBuffer), kRate, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    juce::dsp::ProcessSpec spec { kRate, (juce::uint32) n, 1 };
    convolution.prepare (spec);
    juce::dsp::AudioBlock<float> block (noise);
    juce::dsp::ProcessContextReplacing<float> context (block);
    convolution.process (context);
    std::vector<float> out ((size_t) n);
    for (int i = 0; i < n; ++i) out[(size_t) i] = noise.getSample (0, i);
    return out;
}

double rmse (const lens::Descriptor& a, const lens::Descriptor& b)
{
    double acc = 0.0;
    for (int k = 0; k < lens::kBins; ++k) { const double e = a[(size_t) k] - b[(size_t) k]; acc += e * e; }
    return std::sqrt (acc / lens::kBins);
}

struct Run { double meanError = 0.0, frameJitter = 0.0, poleJitterCents = 0.0, bandwidthJitter = 0.0, converge = 0.0; };

Run measure (lens::Locator& locator, const std::vector<float>& audio, const lens::Descriptor& truth, float memory, double settleSeconds)
{
    Run r;
    locator.resetAverage();
    lens::Descriptor previous {};
    std::vector<double> errors, jitters, f1s, b1s;
    bool started = false;
    const size_t settle = (size_t) (settleSeconds / kHopSeconds);
    size_t frame = 0;
    for (size_t start = 0; start + lens::kFrame <= audio.size(); start += 128, ++frame)
    {
        const auto d = locator.describeAveraged (audio.data() + start, kRate / kFactor, memory);
        const double e = rmse (d, truth);
        if (r.converge == 0.0 && e < 3.0) r.converge = (double) frame * kHopSeconds;
        if (frame < settle) { previous = d; started = true; continue; }
        errors.push_back (e);
        if (started) jitters.push_back (rmse (d, previous));
        previous = d;
        const auto poles = hs::lpcResonances (locator.reflection(), kRate / kFactor, 6);
        if (! poles.empty()) { f1s.push_back (1200.0 * std::log2 (poles[0].first / 100.0)); b1s.push_back (poles[0].second); }
    }
    auto mean = [] (const std::vector<double>& v) { double s = 0.0; for (double x : v) s += x; return v.empty() ? 0.0 : s / (double) v.size(); };
    auto deviation = [&] (const std::vector<double>& v) { const double m = mean (v); double s = 0.0; for (double x : v) s += (x - m) * (x - m); return v.empty() ? 0.0 : std::sqrt (s / (double) v.size()); };
    r.meanError = mean (errors);
    r.frameJitter = mean (jitters);
    r.poleJitterCents = deviation (f1s);
    r.bandwidthJitter = deviation (b1s);
    if (r.converge == 0.0) r.converge = -1.0;
    return r;
}
}

int main (int argc, char** argv)
{
    const juce::File root (TRENCH_TABLE_STITCH_ROOT);
    const juce::File dir = argc > 1 ? juce::File (argv[1]) : root.getChildFile ("evidence/measured-bodies/cab_ir");
    const juce::File outDir = root.getChildFile ("evidence/research-results/lens_bench");
    outDir.createDirectory();
    auto files = dir.findChildFiles (juce::File::findFiles, true, "*.wav");
    files.sort();
    const float memories[] = { 0.0f, 0.5f, 0.8f, 0.9f, 0.95f, 0.98f };
    juce::String csv = "ir,memory,smooth_ms,mean_rmse_db,frame_jitter_db,f1_jitter_cents,b1_jitter_hz,converge_s,converge_after_switch_s\n";
    std::vector<std::vector<Run>> all (std::size (memories));
    std::vector<float> previousAudio;
    lens::Descriptor previousTruth {};
    lens::Locator locator;
    int done = 0;
    for (const auto& file : files)
    {
        const auto clip = trench::core::audio::read_wav_mono (std::filesystem::path (file.getFullPathName().toWideCharPointer()));
        if (! clip || clip->samples.size() < 64) continue;
        auto ir = resampled (*clip);
        if (ir.size() > 16384) ir.resize (16384);
        const auto truth = groundTruth (ir);
        const auto audio = excite (ir, 3.0, 99u + (unsigned) done);
        for (size_t m = 0; m < std::size (memories); ++m)
        {
            auto run = measure (locator, audio, truth.shape, memories[m], 1.0);
            double afterSwitch = -1.0;
            if (! previousAudio.empty())
            {
                std::vector<float> joined (previousAudio.begin(), previousAudio.begin() + (long) (1.5 * kRate));
                joined.insert (joined.end(), audio.begin(), audio.begin() + (long) (1.5 * kRate));
                locator.resetAverage();
                size_t frame = 0;
                const size_t switchFrame = (size_t) (1.5 / kHopSeconds);
                for (size_t start = 0; start + lens::kFrame <= joined.size(); start += 128, ++frame)
                {
                    const auto d = locator.describeAveraged (joined.data() + start, kRate / kFactor, memories[m]);
                    if (frame > switchFrame && rmse (d, truth.shape) < run.meanError + 1.5) { afterSwitch = (double) (frame - switchFrame) * kHopSeconds; break; }
                }
            }
            run.converge = afterSwitch;
            all[m].push_back (run);
            const double smoothMs = memories[m] <= 0.0f ? 0.0 : -kHopSeconds * 1000.0 / std::log ((double) memories[m]);
            csv << file.getFileNameWithoutExtension() << "," << memories[m] << "," << juce::String (smoothMs, 0) << "," << juce::String (run.meanError, 2) << "," << juce::String (run.frameJitter, 3) << "," << juce::String (run.poleJitterCents, 1) << "," << juce::String (run.bandwidthJitter, 1) << "," << juce::String (run.converge, 3) << "," << juce::String (afterSwitch, 3) << "\n";
        }
        previousAudio = audio;
        previousTruth = truth.shape;
        ++done;
    }
    outDir.getChildFile ("cab_ir_bench.csv").replaceWithText (csv);
    juce::String report = "# LENS fitter benchmark on cab impulse responses\n\n";
    report << "Ground truth: FFT magnitude of each IR on the 128 log bins 40 Hz to 5 kHz, floored 30 dB under the peak, mean removed. Estimate: white noise convolved with the IR, decimated by 4, Peevers's LPC-12 envelope per 128-sample hop, the same bins and normalisation, averaged over hops with memory m (SMOOTH). Errors are RMS dB over the bins after the first second. Jitter is the mean frame-to-frame RMS change. Pole jitter is the standard deviation of the first resonance in cents and Hz of bandwidth. Convergence is the time after a switch from the previous IR until the error is within 1.5 dB of the settled error.\n\n";
    report << "IRs: " << done << "\n\n| memory | SMOOTH ms | mean RMSE dB | frame jitter dB | F1 jitter cents | B1 jitter Hz | converge after switch s |\n|---|---|---|---|---|---|---|\n";
    for (size_t m = 0; m < std::size (memories); ++m)
    {
        double e = 0.0, j = 0.0, p = 0.0, b = 0.0, c = 0.0; int cn = 0;
        for (const auto& r : all[m]) { e += r.meanError; j += r.frameJitter; p += r.poleJitterCents; b += r.bandwidthJitter; if (r.converge >= 0.0) { c += r.converge; ++cn; } }
        const double n = std::max<size_t> (1, all[m].size());
        const double smoothMs = memories[m] <= 0.0f ? 0.0 : -kHopSeconds * 1000.0 / std::log ((double) memories[m]);
        report << "| " << memories[m] << " | " << juce::String (smoothMs, 0) << " | " << juce::String (e / n, 2) << " | " << juce::String (j / n, 3) << " | " << juce::String (p / n, 1) << " | " << juce::String (b / n, 1) << " | " << (cn > 0 ? juce::String (c / cn, 3) : juce::String ("never")) << " |\n";
    }
    std::vector<std::pair<double, juce::String>> worst;
    {
        int i = 0;
        for (const auto& file : files)
        {
            if (i >= (int) all[2].size()) break;
            worst.push_back ({ all[2][(size_t) i].meanError, file.getFileNameWithoutExtension() });
            ++i;
        }
    }
    std::sort (worst.begin(), worst.end());
    report << "\nBest five at memory 0.8:\n";
    for (size_t i = 0; i < std::min<size_t> (5, worst.size()); ++i) report << "- " << worst[i].second << " " << juce::String (worst[i].first, 2) << " dB\n";
    report << "\nWorst five at memory 0.8:\n";
    for (size_t i = worst.size(); i > 0 && i + 5 > worst.size(); --i) report << "- " << worst[i - 1].second << " " << juce::String (worst[i - 1].first, 2) << " dB\n";
    outDir.getChildFile ("REPORT.md").replaceWithText (report);
    std::printf ("%s\n", report.toRawUTF8());
    return 0;
}
