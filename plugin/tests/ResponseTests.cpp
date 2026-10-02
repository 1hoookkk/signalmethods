#include "PluginProcessor.h"
#include "TrenchBodyRoster.h"
#include "dsp/TrenchDspBridge.h"
#include <trench/core/audition.hpp>
#include <trench/core/packed_body.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
using cd = std::complex<double>;
constexpr double kBinHz = 0.75;
constexpr double kLowHz = 20.0;
constexpr double kHighHz = 20000.0;
constexpr double kRateToleranceDb = 1.0;
constexpr double kRateFloorDb = -60.0;
constexpr double kValidationDb = 1.0e-6;
constexpr double kValidationFloorDb = -100.0;
constexpr double kDynamicHeadroomDb = 6.0;
constexpr double kSettleTarget = 1.0e-10;
constexpr double kPeriodAgreement = 1.0e-9;
constexpr int kMinPeriods = 4;
constexpr int kMaxPeriods = 64;
constexpr float kStimulusPeak = 0.25f;
constexpr double kWhitenRangeDb = 100.0;
constexpr int kBlock = 4096;
constexpr std::array<double, 3> kSidecarRates { 48000.0, 96000.0, 192000.0 };

int failures = 0;
std::mutex outputLock;
void check (bool ok, const std::string& what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    std::fflush (stdout);
    if (! ok) ++failures;
}
double db (double lin) { return 20.0 * std::log10 (std::max (lin, 1.0e-300)); }

struct Dft
{
    std::size_t n;
    std::vector<std::size_t> factors;
    std::vector<cd> twiddle;
    explicit Dft (std::size_t length) : n (length), twiddle (length)
    {
        std::size_t rest = n;
        for (const std::size_t p : { (std::size_t) 2, (std::size_t) 3, (std::size_t) 5, (std::size_t) 7 })
            while (rest % p == 0) { factors.push_back (p); rest /= p; }
        jassert (rest == 1);
        for (std::size_t k = 0; k < n; ++k)
            twiddle[k] = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * (double) k / (double) n);
    }
    void transform (cd* out, const cd* in, std::size_t len, std::size_t stride, std::size_t depth, cd* scratch) const
    {
        if (len == 1) { out[0] = in[0]; return; }
        const std::size_t p = factors[depth], m = len / p, step = n / len, pstep = n / p;
        for (std::size_t r = 0; r < p; ++r) transform (out + r * m, in + r * stride, m, stride * p, depth + 1, scratch);
        if (p == 2)
        {
            for (std::size_t k = 0; k < m; ++k)
            {
                const cd t0 = out[k], t1 = out[m + k] * twiddle[k * step];
                out[k] = t0 + t1;
                out[m + k] = t0 - t1;
            }
            return;
        }
        cd t[7];
        for (std::size_t k = 0; k < m; ++k)
        {
            for (std::size_t r = 0; r < p; ++r) t[r] = out[r * m + k] * twiddle[r * k * step];
            for (std::size_t q = 0; q < p; ++q)
            {
                cd acc = t[0];
                for (std::size_t r = 1; r < p; ++r) acc += t[r] * twiddle[((r * q) % p) * pstep];
                scratch[k + q * m] = acc;
            }
        }
        std::copy (scratch, scratch + len, out);
    }
    void forward (const cd* in, cd* out, std::vector<cd>& work) const
    {
        work.resize (n);
        transform (out, in, n, 1, 0, work.data());
    }
    void inverse (const cd* in, cd* out, std::vector<cd>& work, std::vector<cd>& scratch) const
    {
        scratch.resize (n);
        for (std::size_t k = 0; k < n; ++k) scratch[k] = std::conj (in[k]);
        forward (scratch.data(), out, work);
        for (std::size_t k = 0; k < n; ++k) out[k] = std::conj (out[k]) / (double) n;
    }
};

double binPhase (std::size_t k)
{
    std::uint64_t z = (std::uint64_t) k * 0x9E3779B97F4A7C15ULL + 0x6A09E667F3BCC909ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    return 2.0 * juce::MathConstants<double>::pi * (double) (z >> 11) / 9007199254740992.0;
}

struct Work
{
    std::vector<cd> a, b, work, scratch, spectrum;
    std::vector<double> cur, prev, amplitude;
    std::vector<float> block, morph, stimulus;
    Work() : block ((std::size_t) kBlock), morph ((std::size_t) kBlock) {}
};

struct RateContext
{
    double rate;
    std::size_t n, half, binLow, binHigh;
    Dft dft;
    std::vector<float> flat;
    std::vector<cd> flatSpectrum;
    explicit RateContext (double r)
        : rate (r), n ((std::size_t) std::llround (r / kBinHz)), half (n / 2),
          binLow ((std::size_t) std::ceil (kLowHz / kBinHz)), binHigh ((std::size_t) std::floor (kHighHz / kBinHz)), dft (n) {}
};

void synthesize (const RateContext& rc, const std::vector<double>& amplitude, std::vector<float>& out, std::vector<cd>& spectrum, Work& w)
{
    w.a.assign (rc.n, cd {});
    w.b.resize (rc.n);
    for (std::size_t k = 0; k <= rc.half; ++k)
    {
        const bool real = k == 0 || k == rc.half;
        const cd v = real ? cd (amplitude[k], 0.0) : std::polar (amplitude[k], binPhase (k));
        w.a[k] = v;
        if (! real) w.a[rc.n - k] = std::conj (v);
    }
    rc.dft.inverse (w.a.data(), w.b.data(), w.work, w.scratch);
    double peak = 0.0;
    for (std::size_t i = 0; i < rc.n; ++i) peak = std::max (peak, std::abs (w.b[i].real()));
    const double scale = kStimulusPeak / peak;
    out.resize (rc.n);
    for (std::size_t i = 0; i < rc.n; ++i)
    {
        out[i] = (float) (w.b[i].real() * scale);
        w.a[i] = out[i];
    }
    spectrum.resize (rc.n);
    rc.dft.forward (w.a.data(), spectrum.data(), w.work);
}

double maxPoleRadius (const trench::core::Cascade& cascade)
{
    double worst = 0.0;
    for (const auto& c : cascade)
    {
        const double a1 = c[3], a2 = c[4];
        const double disc = a1 * a1 - 4.0 * a2;
        if (disc < 0.0)
            worst = std::max (worst, std::sqrt (a2));
        else
        {
            const double root = std::sqrt (disc);
            worst = std::max ({ worst, std::abs ((-a1 + root) / 2.0), std::abs ((-a1 - root) / 2.0) });
        }
    }
    return worst;
}

std::vector<double> formulaDb (const trench::core::Cascade& cascade, const RateContext& rc)
{
    std::vector<double> out (rc.half + 1);
    for (std::size_t k = 0; k <= rc.half; ++k)
    {
        const cd z = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * (double) k / (double) rc.n);
        const cd z2 = z * z;
        cd h = 1.0;
        for (const auto& c : cascade)
            h *= (c[0] + c[1] * z + c[2] * z2) / (1.0 + c[3] * z + c[4] * z2);
        out[k] = db (std::abs (h));
    }
    return out;
}

struct Measurement
{
    std::vector<double> dbs;
    trench::core::Cascade heard {};
    int periods = 0;
    double delta = 0.0;
    std::size_t ulpOver = 0;
    double outPeak = 0.0;
    double maxPoleRadius = 0.0;
    bool finite = true;
};

void runBlocks (TrenchDspBridge& bridge, const float* x, std::size_t n, const TrenchParams& params, double* out, Work& w)
{
    for (std::size_t start = 0; start < n; start += (std::size_t) kBlock)
    {
        const int len = (int) std::min<std::size_t> ((std::size_t) kBlock, n - start);
        std::copy_n (x + start, len, w.block.data());
        float* chans[1] = { w.block.data() };
        juce::AudioBuffer<float> slice (chans, 1, len);
        bridge.process (slice, params);
        if (out != nullptr)
            for (int i = 0; i < len; ++i) out[start + (std::size_t) i] = w.block[(std::size_t) i];
    }
}

Measurement measure (TrenchDspBridge& bridge, const RateContext& rc, const std::vector<float>& x, const std::vector<cd>& spectrum, float morph, float q, Work& w)
{
    Measurement m;
    bridge.prepare (rc.rate, kBlock);
    TrenchParams params;
    params.morph = morph;
    params.q = q;
    w.cur.resize (rc.n);
    w.prev.resize (rc.n);
    int required = kMinPeriods;
    for (int p = 0; p < required; ++p)
    {
        std::swap (w.cur, w.prev);
        runBlocks (bridge, x.data(), rc.n, params, w.cur.data(), w);
        if (p == 0)
        {
            m.heard = bridge.heardCascadeForTests();
            m.maxPoleRadius = maxPoleRadius (m.heard);
            if (m.maxPoleRadius > 0.0 && m.maxPoleRadius < 1.0)
            {
                const double needed = std::ceil (-std::log (kSettleTarget) / (-std::log (m.maxPoleRadius) * (double) rc.n));
                required = (int) std::clamp (needed, (double) kMinPeriods, (double) kMaxPeriods);
            }
        }
    }
    m.periods = required;
    double peak = 0.0, maxDiff = 0.0;
    for (std::size_t i = 0; i < rc.n; ++i)
    {
        const double v = w.cur[i];
        m.finite = m.finite && std::isfinite (v);
        peak = std::max (peak, std::abs (v));
        const double d = std::abs (v - w.prev[i]);
        maxDiff = std::max (maxDiff, d);
        const float f = std::abs ((float) v);
        if (d > (double) (std::nextafter (f, INFINITY) - f)) ++m.ulpOver;
    }
    m.outPeak = peak;
    m.delta = peak > 0.0 ? maxDiff / peak : 0.0;
    w.a.resize (rc.n);
    w.b.resize (rc.n);
    for (std::size_t i = 0; i < rc.n; ++i) w.a[i] = w.cur[i];
    rc.dft.forward (w.a.data(), w.b.data(), w.work);
    m.dbs.resize (rc.half + 1);
    for (std::size_t k = 0; k <= rc.half; ++k) m.dbs[k] = db (std::abs (w.b[k] / spectrum[k]));
    return m;
}

struct Body
{
    std::string name, source;
    juce::String path;
    juce::MemoryBlock base;
    std::array<juce::MemoryBlock, 3> sidecar {};
    std::array<bool, 3> hasSidecar { false, false, false };
    int rosterIndex = -1;
};

int sidecarSlot (double rate)
{
    for (int i = 0; i < 3; ++i)
        if (std::abs (kSidecarRates[(size_t) i] - rate) < 0.5) return i;
    return -1;
}

std::string safeName (const std::string& s)
{
    std::string out;
    for (const char c : s) out += std::isalnum ((unsigned char) c) ? c : '_';
    return out;
}

bool addFile (std::vector<Body>& bodies, const juce::File& file, const std::string& name, const std::string& source)
{
    Body b;
    if (! file.loadFileAsData (b.base) || b.base.getSize() != 240) return false;
    b.name = name;
    b.source = source;
    b.path = file.getFullPathName();
    const auto stem = file.getFileNameWithoutExtension();
    for (int i = 0; i < 3; ++i)
    {
        const auto side = file.getParentDirectory().getChildFile ("_" + stem + "." + juce::String ((juce::int64) kSidecarRates[(size_t) i]) + ".body240");
        b.hasSidecar[(size_t) i] = side.existsAsFile() && side.loadFileAsData (b.sidecar[(size_t) i]) && b.sidecar[(size_t) i].getSize() == 240;
    }
    bodies.push_back (std::move (b));
    return true;
}

void addShipped (std::vector<Body>& bodies)
{
    int count = 0;
    trench::bakedRoster (count);
    for (int i = 1; i < count; ++i)
    {
        Body b;
        if (! trench::bodyRawBytes (i, b.base) || b.base.getSize() != 240) continue;
        b.name = trench::bodyDisplayName (i).toStdString();
        b.source = "shipped";
        b.rosterIndex = i;
        for (int r = 0; r < 3; ++r)
            b.hasSidecar[(size_t) r] = trench::bodySidecarBytes (trench::bodyBaseForIndex (i), kSidecarRates[(size_t) r], b.sidecar[(size_t) r]);
        bodies.push_back (std::move (b));
    }
}

void addLibrary (std::vector<Body>& bodies)
{
    int baked = 0, count = 0;
    trench::bakedRoster (baked);
    const auto* roster = trench::bodyRoster (count);
    for (int i = baked; i < count; ++i)
        if (addFile (bodies, juce::File (roster[i].base), trench::bodyDisplayName (i).toStdString(), "library"))
            bodies.back().rosterIndex = i;
}

void addDirectory (std::vector<Body>& bodies, const juce::File& dir)
{
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.body240");
    std::sort (files.begin(), files.end(), [] (const auto& a, const auto& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    for (const auto& f : files)
        if (! f.getFileName().startsWithChar ('_'))
            addFile (bodies, f, (dir.getFileName() + "/" + f.getFileNameWithoutExtension()).toStdString(), dir.getFullPathName().toStdString());
}

struct Options
{
    juce::File out;
    int grid = 41;
    std::vector<double> rates { 44100.0, 48000.0, 96000.0, 192000.0 };
    unsigned threads = std::max (1u, std::thread::hardware_concurrency());
    bool validate = true, dynamic = true, nonlinear = true, quick = false, sidecarOnly = false;
    std::vector<std::string> sources;
};

struct PathState
{
    double worstErr = -1.0;
    int worstState = -1;
    double loudest = -1.0e300;
    int loudestState = -1;
    double staticPeak = 0.0;
    double valWorst = 0.0;
    int valFail = 0, rateFail = 0, unstable = 0, unsettled = 0;
};

const char* pathName (bool sidecar, double rate) { return sidecar ? "sidecar" : (rate == 44100.0 ? "datum" : "rewarp"); }

void loadFor (TrenchDspBridge& bridge, const Body& body, double rate, bool sidecar)
{
    if (sidecar)
        bridge.loadCartridgeBytes (body.sidecar[(size_t) sidecarSlot (rate)].getData(), 240, rate);
    else
        bridge.loadCartridgeBytes (body.base.getData(), 240, TrenchDspBridge::kBodyDatumRate);
}

void dumpCurve (const juce::File& dir, const std::string& file, const RateContext& rc, const std::vector<double>& ref, const std::vector<double>& test)
{
    std::vector<float> packed;
    packed.push_back ((float) (rc.binHigh - rc.binLow + 1));
    for (std::size_t k = rc.binLow; k <= rc.binHigh; ++k) packed.push_back ((float) ref[k]);
    for (std::size_t k = rc.binLow; k <= rc.binHigh; ++k) packed.push_back ((float) test[k]);
    dir.getChildFile (file).replaceWithData (packed.data(), packed.size() * sizeof (float));
}

std::string stateTag (int grid, int state)
{
    return "m" + std::to_string (state % grid) + "q" + std::to_string (state / grid);
}

void stateValues (int grid, int state, float& morph, float& q)
{
    morph = (float) (state % grid) / (float) (grid - 1);
    q = (float) (state / grid) / (float) (grid - 1);
}

void writeCriteria (const Options& o)
{
    juce::String text;
    text << "# TRENCH definitive response test\n\n"
         << "Written before the run. Criteria are fixed; results do not change them.\n\n"
         << "## Method\n"
         << "- Engine: TrenchDspBridge::process -> PackedBody::interpolate_words -> cascadeAt (rewarp_cascade or section_words_to_biquad) -> CascadeRunner::process, plain branch, ring leveller off as in PluginProcessor::prepareToPlay, BITE/grit 0, KEY off, desks and saturation bypassed.\n"
         << "- Stimulus: periodic flat-spectrum random-phase signal, period = rate / 0.75 Hz samples (bins exactly 0.75 Hz apart, identical grid at every rate), peak " << kStimulusPeak << ".\n"
         << "- Warm-up: at least " << kMinPeriods << " periods, extended until the slowest pole has decayed to " << kSettleTarget << " (cap " << kMaxPeriods << " periods); the last two periods are compared (relative max difference and count of samples differing by more than one float ulp). H = Y/X at every bin of the last period.\n"
         << "- Validation: for every state and rate the plain branch is also measured with the stimulus shaped so the output spectrum is flat (range cap " << kWhitenRangeDb << " dB), and compared with the transfer function of the heard coefficients.\n\n"
         << "## Pass criteria\n"
         << "- Validation: |measured - formula| <= " << kValidationDb << " dB wherever the formula is above " << kValidationFloorDb << " dB.\n"
         << "- Stability: no state has a pole on or outside the unit circle at any rate.\n"
         << "- Rate fidelity: every state at 48, 96 and 192 kHz is within " << kRateToleranceDb << " dB of the same state at 44.1 kHz over " << kLowHz << " Hz to " << kHighHz << " Hz wherever the 44.1 kHz response is above " << kRateFloorDb << " dB.\n"
         << "- Level: each body's loudest point over the whole surface is reported; no ceiling.\n"
         << "- Dynamic: Morph sweeps at 0.5, 2 and 8 Hz through the real process call produce no non-finite sample and no peak more than " << kDynamicHeadroomDb << " dB above the static surface peak at that rate.\n"
         << "- Nonlinear branches (grit, ring leveller, stage saturation, pole distortion): finite output only; levels reported.\n\n"
         << "## Run\n"
         << "- grid " << o.grid << " x " << o.grid << ", rates";
    for (const double r : o.rates) text << " " << r;
    text << ", threads " << (int) o.threads << ", sources";
    for (const auto& s : o.sources) text << " " << s;
    text << "\n";
    o.out.getChildFile ("run.md").replaceWithText (text);
}

struct Shared
{
    const Options& options;
    std::vector<RateContext>& contexts;
    FILE* states;
    FILE* curves;
    juce::File curveDir;
};

void measureBody (Shared& sh, const Body& body, std::vector<std::vector<PathState>>& pathStates)
{
    const int grid = sh.options.grid;
    const int stateCount = grid * grid;
    const auto& contexts = sh.contexts;
    pathStates.assign (contexts.size(), std::vector<PathState> (2));
    std::mutex agg;
    std::atomic<int> next { 0 };
    auto worker = [&]
    {
        Work w;
        TrenchDspBridge bridge;
        bridge.setRingLeveller (false);
        std::vector<double> reference;
        for (int state = next.fetch_add (1); state < stateCount; state = next.fetch_add (1))
        {
            float morph, q;
            stateValues (grid, state, morph, q);
            reference.clear();
            for (std::size_t ri = 0; ri < contexts.size(); ++ri)
            {
                const auto& rc = contexts[ri];
                const int slot = sidecarSlot (rc.rate);
                for (int pathIndex = 0; pathIndex < 2; ++pathIndex)
                {
                    const bool sidecar = pathIndex == 1;
                    const bool hasSidecar = slot >= 0 && body.hasSidecar[(size_t) slot];
                    if (sidecar && ! hasSidecar) continue;
                    if (! sidecar && hasSidecar && sh.options.sidecarOnly) continue;
                    loadFor (bridge, body, rc.rate, sidecar);
                    const auto m = measure (bridge, rc, rc.flat, rc.flatSpectrum, morph, q, w);
                    if (rc.rate == 44100.0 && ! sidecar) reference = m.dbs;
                    double levelDb = -1.0e300, levelHz = 0.0;
                    for (std::size_t k = 0; k <= rc.half; ++k)
                        if (m.dbs[k] > levelDb) { levelDb = m.dbs[k]; levelHz = (double) k * kBinHz; }
                    double rateErr = 0.0, rateHz = 0.0;
                    int binsFail = 0, binsChecked = 0;
                    if (rc.rate != 44100.0 && ! reference.empty())
                        for (std::size_t k = rc.binLow; k <= rc.binHigh; ++k)
                        {
                            if (reference[k] <= kRateFloorDb) continue;
                            ++binsChecked;
                            const double e = std::abs (m.dbs[k] - reference[k]);
                            if (e > kRateToleranceDb) ++binsFail;
                            if (e > rateErr) { rateErr = e; rateHz = (double) k * kBinHz; }
                        }
                    double valMax = 0.0, valAt = 0.0;
                    int valFail = 0, valChecked = 0;
                    if (sh.options.validate)
                    {
                        const auto formula = formulaDb (m.heard, rc);
                        double top = -1.0e300;
                        for (const double v : formula) top = std::max (top, v);
                        w.amplitude.resize (rc.half + 1);
                        for (std::size_t k = 0; k <= rc.half; ++k)
                            w.amplitude[k] = std::pow (10.0, -std::max (formula[k], top - kWhitenRangeDb) / 20.0);
                        synthesize (rc, w.amplitude, w.stimulus, w.spectrum, w);
                        const auto shaped = measure (bridge, rc, w.stimulus, w.spectrum, morph, q, w);
                        for (std::size_t k = 0; k <= rc.half; ++k)
                        {
                            if (formula[k] <= kValidationFloorDb) continue;
                            ++valChecked;
                            const double e = std::abs (shaped.dbs[k] - formula[k]);
                            if (e > valMax) { valMax = e; valAt = formula[k]; }
                            if (e > kValidationDb) ++valFail;
                        }
                    }
                    const bool unstable = ! (m.maxPoleRadius < 1.0);
                    const bool unsettled = m.periods >= kMaxPeriods && m.delta > kPeriodAgreement && m.ulpOver > 0;
                    {
                        const std::lock_guard<std::mutex> lock (outputLock);
                        std::fprintf (sh.states, "%s,%s,%s,%.0f,%d,%d,%.6f,%.6f,%d,%.3e,%zu,%.9f,%.4f,%.2f,%.4f,%.6f,%.2f,%d,%d,%.3e,%.2f,%d,%d\n",
                                      body.name.c_str(), body.source.c_str(), pathName (sidecar, rc.rate), rc.rate, state % grid, state / grid, morph, q,
                                      m.periods, m.delta, m.ulpOver, m.maxPoleRadius, levelDb, levelHz, db (m.outPeak), rateErr, rateHz, binsFail, binsChecked,
                                      valMax, valAt, valFail, valChecked);
                    }
                    if (binsFail > 0)
                    {
                        const std::string file = safeName (body.name) + "__" + std::to_string ((int) rc.rate) + "_" + pathName (sidecar, rc.rate) + "__" + stateTag (grid, state) + ".f32";
                        dumpCurve (sh.curveDir, file, rc, reference, m.dbs);
                        const std::lock_guard<std::mutex> lock (outputLock);
                        std::fprintf (sh.curves, "%s,%s,%s,%s,%.0f,%d,%d,%.6f,%.6f,fail,%.4f,%.2f\n", file.c_str(), body.name.c_str(), body.source.c_str(), pathName (sidecar, rc.rate), rc.rate,
                                      state % grid, state / grid, morph, q, rateErr, rateHz);
                    }
                    {
                        const std::lock_guard<std::mutex> lock (agg);
                        auto& ps = pathStates[ri][(size_t) pathIndex];
                        ps.staticPeak = std::max (ps.staticPeak, m.outPeak);
                        ps.valWorst = std::max (ps.valWorst, valMax);
                        ps.valFail += valFail > 0 ? 1 : 0;
                        ps.rateFail += binsFail > 0 ? 1 : 0;
                        ps.unstable += unstable ? 1 : 0;
                        ps.unsettled += unsettled ? 1 : 0;
                        if (rateErr > ps.worstErr) { ps.worstErr = rateErr; ps.worstState = state; }
                        if (levelDb > ps.loudest) { ps.loudest = levelDb; ps.loudestState = state; }
                    }
                }
            }
        }
    };
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < sh.options.threads; ++t) pool.emplace_back (worker);
    for (auto& t : pool) t.join();

    Work w;
    TrenchDspBridge bridge;
    bridge.setRingLeveller (false);
    for (std::size_t ri = 0; ri < contexts.size(); ++ri)
    {
        const auto& rc = contexts[ri];
        for (int pathIndex = 0; pathIndex < 2; ++pathIndex)
        {
            const auto& ps = pathStates[ri][(size_t) pathIndex];
            const bool sidecar = pathIndex == 1;
            if (ps.loudestState < 0) continue;
            for (const auto [state, kind] : { std::pair { ps.worstState, "worst" }, std::pair { ps.loudestState, "loud" } })
            {
                if (state < 0 || (rc.rate == 44100.0 && std::strcmp (kind, "worst") == 0)) continue;
                float morph, q;
                stateValues (grid, state, morph, q);
                loadFor (bridge, body, 44100.0, false);
                const auto ref = measure (bridge, contexts[0], contexts[0].flat, contexts[0].flatSpectrum, morph, q, w);
                loadFor (bridge, body, rc.rate, sidecar);
                const auto m = measure (bridge, rc, rc.flat, rc.flatSpectrum, morph, q, w);
                const std::string file = safeName (body.name) + "__" + std::to_string ((int) rc.rate) + "_" + pathName (sidecar, rc.rate) + "__" + stateTag (grid, state) + "_" + kind + ".f32";
                dumpCurve (sh.curveDir, file, rc, ref.dbs, m.dbs);
                std::fprintf (sh.curves, "%s,%s,%s,%s,%.0f,%d,%d,%.6f,%.6f,%s,%.4f,%.4f\n", file.c_str(), body.name.c_str(), body.source.c_str(), pathName (sidecar, rc.rate), rc.rate,
                              state % grid, state / grid, morph, q, kind, ps.worstErr, ps.loudest);
            }
        }
    }
    std::fflush (sh.states);
    std::fflush (sh.curves);
}

double sweepPeak (TrenchDspBridge& bridge, const RateContext& rc, float q, double sweepHz, Work& w, bool& finite)
{
    bridge.prepare (rc.rate, kBlock);
    TrenchParams params;
    params.q = q;
    const std::size_t warm = (std::size_t) (2.0 * rc.rate);
    const std::size_t total = warm + (std::size_t) (std::max (2.0, 2.0 / sweepHz) * rc.rate);
    double peak = 0.0;
    for (std::size_t start = 0; start < total; start += (std::size_t) kBlock)
    {
        const int len = (int) std::min<std::size_t> ((std::size_t) kBlock, total - start);
        for (int i = 0; i < len; ++i)
        {
            const std::size_t index = start + (std::size_t) i;
            w.block[(size_t) i] = rc.flat[index % rc.n];
            const double phase = std::fmod ((double) index / rc.rate * sweepHz, 1.0);
            w.morph[(size_t) i] = (float) (phase < 0.5 ? 2.0 * phase : 2.0 - 2.0 * phase);
        }
        float* chans[1] = { w.block.data() };
        juce::AudioBuffer<float> slice (chans, 1, len);
        bridge.processTrajectory (slice, w.morph.data(), params);
        for (int i = 0; i < len; ++i)
            if (start + (std::size_t) i >= warm)
            {
                finite = finite && std::isfinite (w.block[(size_t) i]);
                peak = std::max (peak, (double) std::abs (w.block[(size_t) i]));
            }
    }
    return peak;
}

template <typename Job>
void parallelFor (unsigned threads, int count, Job job)
{
    std::atomic<int> next { 0 };
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < threads; ++t)
        pool.emplace_back ([&]
        {
            Work w;
            for (int i = next.fetch_add (1); i < count; i = next.fetch_add (1)) job (i, w);
        });
    for (auto& t : pool) t.join();
}

void dynamicBody (Shared& sh, const Body& body, const std::vector<std::vector<PathState>>& pathStates, FILE* out)
{
    const std::array<double, 3> sweeps { 0.5, 2.0, 8.0 };
    const std::array<float, 3> qs { 0.0f, 0.5f, 1.0f };
    struct Row { double rate; bool sidecar; double hz; float q; double peak, staticPeak; bool finite; };
    std::vector<Row> rows (sh.contexts.size() * 9);
    parallelFor (sh.options.threads, (int) rows.size(), [&] (int i, Work& w)
    {
        const auto& rc = sh.contexts[(size_t) i / 9];
        const int slot = sidecarSlot (rc.rate);
        const bool sidecar = slot >= 0 && body.hasSidecar[(size_t) slot];
        TrenchDspBridge bridge;
        bridge.setRingLeveller (false);
        loadFor (bridge, body, rc.rate, sidecar);
        Row r { rc.rate, sidecar, sweeps[(size_t) (i % 9) / 3], qs[(size_t) i % 3], 0.0, pathStates[(size_t) i / 9][sidecar ? 1 : 0].staticPeak, true };
        r.peak = sweepPeak (bridge, rc, r.q, r.hz, w, r.finite);
        rows[(size_t) i] = r;
    });
    bool allFinite = true, within = true;
    double worstOver = -1.0e300;
    std::string worstTag;
    for (const auto& r : rows)
    {
        const double over = db (r.peak) - db (r.staticPeak);
        allFinite = allFinite && r.finite;
        if (over > kDynamicHeadroomDb) within = false;
        if (over > worstOver) { worstOver = over; worstTag = std::to_string ((int) r.rate) + " Hz " + juce::String (r.hz, 1).toStdString() + " Hz q " + juce::String (r.q, 1).toStdString(); }
        std::fprintf (out, "%s,%s,%s,%.0f,%.1f,%.2f,%.4f,%.4f,%.4f,%d\n", body.name.c_str(), body.source.c_str(), pathName (r.sidecar, r.rate), r.rate, r.hz, r.q,
                      db (r.peak), db (r.staticPeak), over, r.finite ? 1 : 0);
    }
    std::fflush (out);
    check (allFinite && within, body.name + " morph sweeps finite and within +" + std::to_string ((int) kDynamicHeadroomDb) + " dB of the static peak (worst "
                                     + juce::String (worstOver, 2).toStdString() + " dB at " + worstTag + ")");
}

void nonlinearBody (Shared& sh, const Body& body, FILE* out)
{
    const int grid = sh.options.grid;
    const std::array<int, 5> states { 0, grid - 1, grid * (grid - 1), grid * grid - 1, (grid / 2) * grid + grid / 2 };
    const std::array<float, 2> scales { 1.0f, 4.0f };
    const std::array<const char*, 5> modes { "grit0.5", "grit1.0", "ring", "stagesat0.5", "poledist0.02" };
    struct Row { double rate; bool sidecar; int state; float scale; const char* mode; double peak; bool finite; };
    std::vector<Row> rows (sh.contexts.size() * states.size() * scales.size() * modes.size());
    parallelFor (sh.options.threads, (int) rows.size(), [&] (int i, Work& w)
    {
        int index = i;
        const char* mode = modes[(size_t) index % modes.size()];
        index /= (int) modes.size();
        const float scale = scales[(size_t) index % scales.size()];
        index /= (int) scales.size();
        const int state = states[(size_t) index % states.size()];
        index /= (int) states.size();
        const auto& rc = sh.contexts[(size_t) index];
        const int slot = sidecarSlot (rc.rate);
        const bool sidecar = slot >= 0 && body.hasSidecar[(size_t) slot];
        float morph, q;
        stateValues (grid, state, morph, q);
        w.stimulus.resize (rc.n);
        for (std::size_t k = 0; k < rc.n; ++k) w.stimulus[k] = rc.flat[k] * scale;
        w.cur.resize (rc.n);
        TrenchParams params;
        params.morph = morph;
        params.q = q;
        if (std::strcmp (mode, "poledist0.02") == 0)
        {
            TrenchDspBridge probe;
            probe.setRingLeveller (false);
            loadFor (probe, body, rc.rate, sidecar);
            probe.prepare (rc.rate, kBlock);
            runBlocks (probe, w.stimulus.data(), (std::size_t) kBlock, params, nullptr, w);
            trench::core::CascadeRunner runner;
            runner.set_sample_rate (rc.rate);
            runner.set_ring_leveller (false);
            runner.set_radius_distortion (0.02);
            runner.set_immediate (probe.heardCascadeForTests());
            std::vector<float> span (rc.n);
            for (int p = 0; p < kMinPeriods; ++p)
            {
                std::copy (w.stimulus.begin(), w.stimulus.end(), span.begin());
                runner.process (std::span<float> (span.data(), span.size()));
            }
            for (std::size_t k = 0; k < rc.n; ++k) w.cur[k] = span[k];
        }
        else
        {
            TrenchDspBridge bridge;
            bridge.setRingLeveller (std::strcmp (mode, "ring") == 0);
            if (std::strcmp (mode, "stagesat0.5") == 0) bridge.setStageSaturation (0.5);
            loadFor (bridge, body, rc.rate, sidecar);
            bridge.prepare (rc.rate, kBlock);
            params.poleDistortion = std::strcmp (mode, "grit0.5") == 0 ? 0.5f : std::strcmp (mode, "grit1.0") == 0 ? 1.0f : 0.0f;
            for (int p = 0; p < kMinPeriods; ++p) runBlocks (bridge, w.stimulus.data(), rc.n, params, w.cur.data(), w);
        }
        Row r { rc.rate, sidecar, state, scale, mode, 0.0, true };
        for (const double v : w.cur)
        {
            r.finite = r.finite && std::isfinite (v);
            r.peak = std::max (r.peak, std::abs (v));
        }
        rows[(size_t) i] = r;
    });
    bool allFinite = true;
    for (const auto& r : rows)
    {
        float morph, q;
        stateValues (grid, r.state, morph, q);
        allFinite = allFinite && r.finite;
        std::fprintf (out, "%s,%s,%s,%.0f,%d,%d,%.4f,%.4f,%s,%.2f,%.4f,%d\n", body.name.c_str(), body.source.c_str(), pathName (r.sidecar, r.rate), r.rate,
                      r.state % grid, r.state / grid, morph, q, r.mode, db (kStimulusPeak * r.scale), db (r.peak), r.finite ? 1 : 0);
    }
    std::fflush (out);
    check (allFinite, body.name + " nonlinear branches produce finite output");
}

void selectBody (PluginProcessor& processor, int index)
{
    auto* parameter = processor.apvts.getParameter (ParamID::body);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

void processorPaths (Shared& sh, const std::vector<Body>& bodies, FILE* out)
{
    Work w;
    TrenchDspBridge bare;
    bare.setRingLeveller (false);
    bool shippedShown = false;
    for (const auto& body : bodies)
    {
        if (body.rosterIndex < 0) continue;
        const bool anySidecar = body.hasSidecar[0] || body.hasSidecar[1] || body.hasSidecar[2];
        if (! anySidecar && (body.source != "shipped" || shippedShown)) continue;
        if (body.source == "shipped") shippedShown = true;
        for (const auto& rc : sh.contexts)
        {
            PluginProcessor processor;
            processor.setPlayConfigDetails (2, 2, rc.rate, 512);
            processor.prepareToPlay (rc.rate, 512);
            selectBody (processor, body.rosterIndex);
            for (const char* id : { ParamID::morph, ParamID::q })
                if (auto* p = processor.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
            juce::AudioBuffer<float> audio (2, 512);
            juce::MidiBuffer midi;
            for (int block = 0; block < 40; ++block)
            {
                audio.clear();
                processor.processBlock (audio, midi);
            }
            const auto heard = processor.dspBridge.heardCascadeForTests();
            const int slot = sidecarSlot (rc.rate);
            std::string verdict = "neither";
            double nearest = 1.0e300;
            for (int pathIndex = 0; pathIndex < 2; ++pathIndex)
            {
                const bool sidecar = pathIndex == 1;
                if (sidecar && (slot < 0 || ! body.hasSidecar[(size_t) slot])) continue;
                loadFor (bare, body, rc.rate, sidecar);
                bare.prepare (rc.rate, kBlock);
                TrenchParams params;
                runBlocks (bare, rc.flat.data(), (std::size_t) kBlock, params, nullptr, w);
                const auto candidate = bare.heardCascadeForTests();
                double diff = 0.0;
                for (std::size_t s = 0; s < candidate.size(); ++s)
                    for (std::size_t c = 0; c < candidate[s].size(); ++c) diff = std::max (diff, std::abs (candidate[s][c] - heard[s][c]));
                if (diff < nearest) nearest = diff;
                if (diff == 0.0) verdict = pathName (sidecar, rc.rate);
            }
            const bool expected = verdict == std::string (slot >= 0 && body.hasSidecar[(size_t) slot] ? "sidecar" : pathName (false, rc.rate));
            std::fprintf (out, "%s,%s,%.0f,%d,%s,%.3e\n", body.name.c_str(), body.source.c_str(), rc.rate,
                          slot >= 0 && body.hasSidecar[(size_t) slot] ? 1 : 0, verdict.c_str(), nearest);
            check (expected, body.name + " at " + std::to_string ((int) rc.rate) + " Hz: PluginProcessor takes the " + verdict + " path");
        }
    }
    std::fflush (out);
}
}

int main (int argc, char** argv)
{
    _putenv_s ("TRENCH_HEADLESS", "1");
    juce::ScopedJuceInitialiser_GUI init;
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto value = [&] { return i + 1 < argc ? std::string (argv[++i]) : std::string(); };
        if (arg == "--out") o.out = juce::File (juce::String (value()));
        else if (arg == "--grid") o.grid = std::max (2, std::atoi (value().c_str()));
        else if (arg == "--threads") o.threads = (unsigned) std::max (1, std::atoi (value().c_str()));
        else if (arg == "--rates")
        {
            o.rates.clear();
            for (const auto& r : juce::StringArray::fromTokens (juce::String (value()), ",", {})) o.rates.push_back (r.getDoubleValue());
        }
        else if (arg == "--no-validate") o.validate = false;
        else if (arg == "--no-dynamic") o.dynamic = false;
        else if (arg == "--no-nonlinear") o.nonlinear = false;
        else if (arg == "--sidecar-only") o.sidecarOnly = true;
        else if (arg == "--quick") { o.quick = true; o.grid = 5; o.rates = { 44100.0, 96000.0 }; }
        else o.sources.push_back (arg);
    }
    if (o.sources.empty()) o.sources.push_back ("shipped");
    if (o.rates.empty() || o.rates[0] != 44100.0) o.rates.insert (o.rates.begin(), 44100.0);
    if (o.out == juce::File())
        o.out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("trench-response-test");
    o.out.createDirectory();
    const auto curveDir = o.out.getChildFile ("curves");
    curveDir.createDirectory();
    writeCriteria (o);

    std::vector<Body> bodies;
    for (const auto& s : o.sources)
    {
        if (s == "shipped") addShipped (bodies);
        else if (s == "library") addLibrary (bodies);
        else if (s.rfind ("dir:", 0) == 0) addDirectory (bodies, juce::File (juce::String (s.substr (4))));
        else if (s.rfind ("file:", 0) == 0) addFile (bodies, juce::File (juce::String (s.substr (5))), juce::File (juce::String (s.substr (5))).getFileNameWithoutExtension().toStdString(), "file");
    }
    check (! bodies.empty(), "bodies found: " + std::to_string (bodies.size()));

    std::vector<RateContext> contexts;
    for (const double r : o.rates) contexts.emplace_back (r);
    {
        Work w;
        for (auto& rc : contexts)
        {
            std::vector<double> amplitude (rc.half + 1, 1.0);
            synthesize (rc, amplitude, rc.flat, rc.flatSpectrum, w);
        }
    }

    FILE* states = std::fopen (o.out.getChildFile ("states.csv").getFullPathName().toRawUTF8(), "w");
    FILE* curves = std::fopen (o.out.getChildFile ("curves.csv").getFullPathName().toRawUTF8(), "w");
    FILE* dynamic = std::fopen (o.out.getChildFile ("dynamic.csv").getFullPathName().toRawUTF8(), "w");
    FILE* nonlinear = std::fopen (o.out.getChildFile ("nonlinear.csv").getFullPathName().toRawUTF8(), "w");
    FILE* paths = std::fopen (o.out.getChildFile ("paths.csv").getFullPathName().toRawUTF8(), "w");
    check (states && curves && dynamic && nonlinear && paths, "output files open in " + o.out.getFullPathName().toStdString());
    if (! (states && curves && dynamic && nonlinear && paths)) return 1;
    std::fprintf (states, "body,source,path,rate,mi,qi,morph,q,periods,delta,ulp_over,max_pole_r,level_db,level_hz,out_peak_db,rate_err_db,rate_err_hz,rate_bins_fail,rate_bins_checked,val_max_db,val_at_db,val_bins_fail,val_bins_checked\n");
    std::fprintf (curves, "file,body,source,path,rate,mi,qi,morph,q,kind,err_db,at\n");
    std::fprintf (dynamic, "body,source,path,rate,sweep_hz,q,peak_db,static_peak_db,over_db,finite\n");
    std::fprintf (nonlinear, "body,source,path,rate,mi,qi,morph,q,mode,in_peak_db,out_peak_db,finite\n");
    std::fprintf (paths, "body,source,rate,has_sidecar,processor_path,nearest_coefficient_diff\n");

    Shared sh { o, contexts, states, curves, curveDir };
    processorPaths (sh, bodies, paths);

    for (const auto& body : bodies)
    {
        std::vector<std::vector<PathState>> pathStates;
        measureBody (sh, body, pathStates);
        int unstable = 0, unsettled = 0, valFail = 0;
        double valWorst = 0.0, loudest = -1.0e300;
        std::string loudTag;
        for (std::size_t ri = 0; ri < contexts.size(); ++ri)
            for (int pathIndex = 0; pathIndex < 2; ++pathIndex)
            {
                const auto& ps = pathStates[ri][(size_t) pathIndex];
                if (ps.loudestState < 0) continue;
                unstable += ps.unstable;
                unsettled += ps.unsettled;
                valFail += ps.valFail;
                valWorst = std::max (valWorst, ps.valWorst);
                if (ps.loudest > loudest) { loudest = ps.loudest; loudTag = std::to_string ((int) contexts[ri].rate) + " Hz " + pathName (pathIndex == 1, contexts[ri].rate) + " " + stateTag (o.grid, ps.loudestState); }
                if (contexts[ri].rate != 44100.0)
                    check (ps.rateFail == 0, body.name + " " + std::to_string ((int) contexts[ri].rate) + " Hz " + pathName (pathIndex == 1, contexts[ri].rate) + " within 1 dB of 44.1 kHz above -60 dB (worst "
                                                 + juce::String (ps.worstErr, 3).toStdString() + " dB at " + stateTag (o.grid, ps.worstState) + ", " + std::to_string (ps.rateFail) + " states fail)");
            }
        check (unstable == 0, body.name + " stable at every rate (" + std::to_string (unstable) + " unstable states)");
        if (o.validate)
            check (valFail == 0, body.name + " plain branch matches the transfer function within 1e-6 dB above -100 dB (worst " + juce::String (valWorst, 3, true).toStdString() + " dB, " + std::to_string (valFail) + " states fail)");
        std::printf ("INFO  %s settled: %d states hit the %d-period cap without agreeing; loudest %+.2f dB at %s\n", body.name.c_str(), unsettled, kMaxPeriods, loudest, loudTag.c_str());
        if (o.dynamic) dynamicBody (sh, body, pathStates, dynamic);
        if (o.nonlinear) nonlinearBody (sh, body, nonlinear);
    }
    for (FILE* f : { states, curves, dynamic, nonlinear, paths }) std::fclose (f);
    std::printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
