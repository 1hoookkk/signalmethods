// Deterministic tests for the ARMAdillo display coordinates. No JUCE, no
// runtime, no audio: this proves the map alone, before any UI is built on it.
// Non-zero exit on any failure.

#include "ArmadilloCoords.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace
{
int failures = 0;
int checks   = 0;

void check (bool ok, const std::string& what)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf ("  FAIL  %s\n", what.c_str());
    }
}

void checkClose (double got, double want, double tol, const std::string& what)
{
    ++checks;
    const double delta = std::abs (got - want);
    if (! (delta <= tol))
    {
        ++failures;
        std::printf ("  FAIL  %s\n        got %.17g want %.17g (delta %.3g > %.3g)\n",
                     what.c_str(), got, want, delta, tol);
    }
}

void checkCloseRel (double got, double want, double rel, const std::string& what)
{
    ++checks;
    const double delta = std::abs (got - want);
    const double bound = rel * std::abs (want);
    if (! (delta <= bound))
    {
        ++failures;
        std::printf ("  FAIL  %s\n        got %.17g want %.17g (rel %.3g > %.3g)\n",
                     what.c_str(), got, want, delta / std::abs (want), rel);
    }
}

std::string at (double fs) { char b[64]; std::snprintf (b, sizeof b, " @ fs=%.1f", fs); return b; }

using namespace armadillo;

// 1 & 2 — the two anchors of the Nyquist-anchored map.
void anchors (double fs)
{
    double tp = -1.0;
    const double lo = displayLoHz (fs);
    checkCloseRel (thetaFromHz (lo, fs), kPi / 1024.0, 1e-15, "fs/2048 -> theta = pi/1024" + at (fs));
    check (thetaPrimeFromHz (lo, fs, tp) == Status::ok, "fs/2048 transforms" + at (fs));
    checkClose (tp, 0.0, 1e-12, "fs/2048 -> thetaPrime = 0" + at (fs));

    const double hi = displayHiHz (fs);
    checkCloseRel (thetaFromHz (hi, fs), kPi, 1e-15, "fs/2 -> theta = pi" + at (fs));
    check (thetaPrimeFromHz (hi, fs, tp) == Status::ok, "fs/2 transforms" + at (fs));
    checkClose (tp, kPi, 1e-12, "fs/2 -> thetaPrime = pi" + at (fs));
}

// 3 — a doubling is exactly one octave, everywhere in the domain.
void octaveStep (double fs)
{
    const double step = kPi / 10.0;
    for (double f = displayLoHz (fs); f * 2.0 <= displayHiHz (fs) * 1.0000001; f *= 1.37)
    {
        double a = 0.0, b = 0.0;
        if (thetaPrimeFromHz (f, fs, a) != Status::ok || thetaPrimeFromHz (f * 2.0, fs, b) != Status::ok)
        {
            check (false, "octave pair transforms" + at (fs));
            continue;
        }
        checkClose (b - a, step, 1e-12, "doubling advances thetaPrime by pi/10" + at (fs));
    }
}

// 4 — frequency round-trips across the whole domain.
void freqRoundTrip (double fs)
{
    const double lo = displayLoHz (fs), hi = displayHiHz (fs);
    const int n = 401;
    for (int i = 0; i < n; ++i)
    {
        const double f = lo * std::pow (hi / lo, (double) i / (double) (n - 1));
        double tp = 0.0, back = 0.0;
        check (thetaPrimeFromHz (f, fs, tp) == Status::ok, "forward transform" + at (fs));
        check (hzFromPrime (tp, fs, back) == Status::ok, "inverse transform" + at (fs));
        checkCloseRel (back, f, 1e-9, "f -> thetaPrime -> f round-trips" + at (fs));
        check (tp >= -1e-12 && tp <= kPi + 1e-12, "thetaPrime stays inside the semicircle" + at (fs));
    }
}

// 5, 6, 7 — the radial law.
void radialLaw()
{
    double db = -1.0;
    check (rPrimeDbFromRadius (0.0, db) == Status::ok, "r = 0 transforms");
    checkClose (db, 0.0, 1e-15, "r = 0 -> R' = 0 dB");

    double prev = -1.0;
    for (int i = 0; i <= 400; ++i)
    {
        const double r = (double) i / 401.0;   // 0 .. just under 1
        double d = 0.0, back = 0.0;
        check (rPrimeDbFromRadius (r, d) == Status::ok, "radius transforms");
        check (radiusFromRPrimeDb (d, back) == Status::ok, "radius inverse transforms");
        checkClose (back, r, 1e-12, "R -> R' -> R round-trips");
        check (d > prev, "R' is strictly monotonic in R");
        prev = d;
    }

    // Over the range the encoder can actually hold. The rim (96 dB) sits
    // essentially exactly on the encoder's contiguous ceiling, so 120 dB is
    // already past anything authorable.
    for (double d = 0.0; d <= 120.0; d += 3.0)
    {
        double r = 0.0, back = 0.0;
        check (radiusFromRPrimeDb (d, r) == Status::ok, "dB transforms");
        check (rPrimeDbFromRadius (r, back) == Status::ok, "dB inverse transforms");
        checkClose (back, d, 1e-9, "R' -> R -> R' round-trips");
    }
}

// Where the radius representation itself runs out. Storing a resonance as r
// rather than as (1 - r) means 1 - r is a cancelling subtraction: past roughly
// 140 dB the low bits of the gap are already gone, so R' can only be recovered
// to about 1e-6. Recorded here rather than hidden, because it bounds how far
// numeric entry can be trusted.
void radiusPrecisionCeilingIsWhereWeThinkItIs()
{
    double r = 0.0, back = 0.0;
    check (radiusFromRPrimeDb (133.0, r) == Status::ok, "133 dB transforms");
    check (rPrimeDbFromRadius (r, back) == Status::ok, "133 dB inverse transforms");
    checkClose (back, 133.0, 1e-9, "R' round-trips exactly up to 133 dB");

    check (radiusFromRPrimeDb (196.0, r) == Status::ok, "196 dB transforms");
    check (rPrimeDbFromRadius (r, back) == Status::ok, "196 dB inverse transforms");
    check (std::abs (back - 196.0) > 1e-9, "past ~140 dB the gap has lost its low bits");
    checkClose (back, 196.0, 1e-5, "and degrades gracefully, not catastrophically");
}

// 8 — equal dB steps are equal screen steps.
void equalStepsAreEqualDb()
{
    const double rhoMax = 480.0;
    const double firstGap = displayRadiusFromRPrimeDb (12.0, rhoMax) - displayRadiusFromRPrimeDb (0.0, rhoMax);
    for (double d = 0.0; d + 12.0 <= kRimDb; d += 12.0)
    {
        const double gap = displayRadiusFromRPrimeDb (d + 12.0, rhoMax) - displayRadiusFromRPrimeDb (d, rhoMax);
        checkClose (gap, firstGap, 1e-12, "equal dB increments are equal display increments");
    }
    for (double d = 0.0; d <= kRimDb; d += 6.0)
        checkClose (rPrimeDbFromDisplayRadius (displayRadiusFromRPrimeDb (d, rhoMax), rhoMax), d, 1e-12,
                    "display radius round-trips");
}

// 9 — the authoring ceiling reported by Rust sits strictly inside the arc.
// Rust owns the number; this only proves the display can hold it.
void authoringCeilingFits (double fs, double authoringFreqMaxHz)
{
    double tp = 0.0;
    check (thetaPrimeFromHz (authoringFreqMaxHz, fs, tp) == Status::ok, "ceiling transforms" + at (fs));
    check (tp < kPi, "the authoring frequency ceiling is strictly inside the semicircle" + at (fs));
    check (tp > 0.0, "the authoring frequency ceiling is above the domain floor" + at (fs));
}

// 10 — the plot range follows fs, it is not pinned to a fixed origin.
void domainTracksFs()
{
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    for (int i = 1; i < 3; ++i)
    {
        check (displayLoHz (rates[i]) > displayLoHz (rates[i - 1]), "display floor rises with fs");
        check (displayHiHz (rates[i]) > displayHiHz (rates[i - 1]), "display ceiling rises with fs");
    }
    checkCloseRel (displayLoHz (48000.0), 23.4375, 1e-15, "48000 floor is 23.4375 Hz");
    checkCloseRel (displayHiHz (48000.0), 24000.0, 1e-15, "48000 ceiling is 24000 Hz");
    for (double fs : rates)
        checkCloseRel (displayHiHz (fs) / displayLoHz (fs), 1024.0, 1e-12, "the domain is always ten octaves");
}

// 11 — bad input is reported, never substituted.
void badInputIsReported()
{
    double out = 12345.0;
    const double sentinel = out;
    const double nan = std::nan ("");
    const double inf = std::numeric_limits<double>::infinity();

    check (thetaPrimeFromTheta (nan, out) == Status::notFinite, "NaN theta is reported");
    check (out == sentinel, "NaN theta leaves the output untouched");
    check (thetaPrimeFromTheta (inf, out) == Status::notFinite, "infinite theta is reported");
    check (thetaPrimeFromTheta (0.0, out) == Status::nonPositiveAngle, "theta = 0 is reported");
    check (thetaPrimeFromTheta (-1.0, out) == Status::nonPositiveAngle, "negative theta is reported");
    check (out == sentinel, "a rejected theta leaves the output untouched");

    check (thetaPrimeFromHz (nan, 48000.0, out) == Status::notFinite, "NaN Hz is reported");
    check (thetaPrimeFromHz (1000.0, 0.0, out) == Status::notFinite, "fs = 0 is reported");
    check (thetaPrimeFromHz (1000.0, nan, out) == Status::notFinite, "NaN fs is reported");
    check (hzFromPrime (nan, 48000.0, out) == Status::notFinite, "NaN thetaPrime is reported");
    check (thetaFromPrime (inf, out) == Status::notFinite, "infinite thetaPrime is reported");

    check (rPrimeDbFromRadius (nan, out) == Status::notFinite, "NaN radius is reported");
    check (rPrimeDbFromRadius (1.0, out) == Status::nonPositiveAngle, "r = 1 has unbounded R' and is reported");
    check (rPrimeDbFromRadius (1.5, out) == Status::nonPositiveAngle, "r > 1 is reported");
    check (out == sentinel, "a rejected radius leaves the output untouched");
    check (radiusFromRPrimeDb (nan, out) == Status::notFinite, "NaN dB is reported");
    check (out == sentinel, "a rejected dB leaves the output untouched");
}

// 12 — guard: the header must carry no authoring policy and no fixed origin.
void headerCarriesNoPolicy (const char* path)
{
    std::ifstream in (path);
    if (! in)
    {
        check (false, std::string ("cannot open ") + path + " for the policy scan");
        return;
    }
    const std::string src ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
    check (! src.empty(), "the header is readable");

    // A frequency origin, a sample rate, or a radius/frequency ceiling in this
    // file would mean the UI had started deciding what the encoder may hold.
    // (20.0 on its own is legitimate here - it is the decibel constant.)
    const char* forbidden[] = {
        "log2 (hz / 20", "log2(hz/20", "log2 (f / 20", "log2(f/20",   // the 20 Hz-anchored law
        "44100", "48000", "96000",                                     // a mirrored sample rate
        "0.99997", "0.49", "0.499",                                    // radius / frequency ceilings
        "FREQ_MAX", "kEvalSampleRate", "emuInternalRate"
    };
    for (const char* token : forbidden)
        check (src.find (token) == std::string::npos,
               std::string ("the header must not contain policy token \"") + token + "\"");

    // Positively: the domain must be derived from fs, ten octaves below Nyquist.
    check (src.find ("2048") != std::string::npos, "the header derives its floor as fs/2048");
    check (src.find ("double fs") != std::string::npos, "the header takes fs as a parameter");
}
} // namespace

int main (int argc, char** argv)
{
    const char* headerPath = argc > 1 ? argv[1] : ARMADILLO_COORDS_HEADER;

    std::printf ("ARMAdillo coordinate law\n");

    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    for (double fs : rates)
    {
        anchors (fs);
        octaveStep (fs);
        freqRoundTrip (fs);
        // 0.49*fs is the crate's authoring ceiling; mirrored here only as a
        // test input, never as a rule the header enforces.
        authoringCeilingFits (fs, 0.49 * fs);
    }
    radialLaw();
    radiusPrecisionCeilingIsWhereWeThinkItIs();
    equalStepsAreEqualDb();
    domainTracksFs();
    badInputIsReported();
    headerCarriesNoPolicy (headerPath);

    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
