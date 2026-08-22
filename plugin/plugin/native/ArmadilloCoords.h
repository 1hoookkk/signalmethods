#pragma once

// Rossum's ARMAdillo display coordinates, Nyquist-anchored.
//
//     theta      = 2*pi*f / fs
//     thetaPrime = pi * (10 + log2(theta/pi)) / 10
//     rPrimeDb   = -20 * log10(1 - r)
//
// thetaPrime spans exactly the ten octaves below Nyquist: 0 at theta = pi/1024
// (f = fs/2048), pi at theta = pi (f = fs/2). Each doubling of f advances
// thetaPrime by pi/10, so equal angular increments are equal musical octaves,
// and equal radial increments are equal resonance in dB.
//
// This header is DISPLAY MATHEMATICS ONLY. It holds no authoring policy: no
// frequency origin, no radius ceiling, no sample rate. Every function takes fs
// from the caller, who reads it from the runtime. Whether a value may be
// written is decided by trench_validate_stage_roots_at, in Rust, next to the
// encoder that owns the answer.

#include <cmath>

namespace armadillo
{
inline constexpr double kPi  = 3.14159265358979323846;
inline constexpr double kTau = 2.0 * kPi;

// How much of the R' axis the rim shows. A viewport extent chosen by the UI,
// NOT part of Rossum's law: the law defines R', this decides how much of it
// fits on screen. Values beyond the rim remain authorable by typing.
inline constexpr double kRimDb = 96.0;

// The mathematical outcome of a transform. These are failures of the map
// itself, never authoring verdicts, and never a substituted value.
enum class Status
{
    ok,
    notFinite,
    nonPositiveAngle   // log2 is undefined at and below theta = 0
};

// ---------------------------------------------------------------- frequency

/// Lowest frequency the semicircle can show: theta = pi/1024.
inline double displayLoHz (double fs) noexcept  { return fs / 2048.0; }
/// Highest frequency the semicircle can show: theta = pi (Nyquist).
inline double displayHiHz (double fs) noexcept  { return fs * 0.5; }

/// Divide before multiplying so exact-power-of-two ratios stay exact.
inline double thetaFromHz (double hz, double fs) noexcept { return kTau * (hz / fs); }
inline double hzFromTheta (double theta, double fs) noexcept { return fs * (theta / kTau); }

inline Status thetaPrimeFromTheta (double theta, double& out) noexcept
{
    if (! std::isfinite (theta))
        return Status::notFinite;
    if (theta <= 0.0)
        return Status::nonPositiveAngle;
    // Grouped so log2 == 0 gives exactly pi and log2 == -10 gives exactly 0.
    out = kPi * ((10.0 + std::log2 (theta / kPi)) / 10.0);
    return Status::ok;
}

inline Status thetaPrimeFromHz (double hz, double fs, double& out) noexcept
{
    if (! std::isfinite (hz) || ! std::isfinite (fs) || fs <= 0.0)
        return Status::notFinite;
    return thetaPrimeFromTheta (thetaFromHz (hz, fs), out);
}

inline Status thetaFromPrime (double thetaPrime, double& out) noexcept
{
    if (! std::isfinite (thetaPrime))
        return Status::notFinite;
    out = kPi * std::exp2 (10.0 * (thetaPrime / kPi) - 10.0);
    return Status::ok;
}

inline Status hzFromPrime (double thetaPrime, double fs, double& out) noexcept
{
    if (! std::isfinite (thetaPrime) || ! std::isfinite (fs) || fs <= 0.0)
        return Status::notFinite;
    out = (fs * 0.5) * std::exp2 (10.0 * (thetaPrime / kPi) - 10.0);
    return Status::ok;
}

// ----------------------------------------------------------------- radius

inline Status rPrimeDbFromRadius (double r, double& out) noexcept
{
    if (! std::isfinite (r))
        return Status::notFinite;
    const double gap = 1.0 - r;
    if (gap <= 0.0)
        return Status::nonPositiveAngle;   // r >= 1: R' is unbounded
    out = -20.0 * std::log10 (gap);
    return Status::ok;
}

inline Status radiusFromRPrimeDb (double db, double& out) noexcept
{
    if (! std::isfinite (db))
        return Status::notFinite;
    out = 1.0 - std::pow (10.0, -db / 20.0);
    return Status::ok;
}

// ----------------------------------------------------------------- drawing

/// Screen radius for a resonance, linear in dB so equal radial steps are equal
/// dB. Rendering only — the caller clamps for the pixel, never for the model.
inline double displayRadiusFromRPrimeDb (double db, double rhoMax) noexcept
{
    return rhoMax * (db / kRimDb);
}

inline double rPrimeDbFromDisplayRadius (double rho, double rhoMax) noexcept
{
    return kRimDb * (rho / rhoMax);
}

} // namespace armadillo
