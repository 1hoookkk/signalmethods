#pragma once
#include <trench/core/packed_body.hpp>
#include <algorithm>
#include <cmath>
#include <complex>

namespace trench
{
struct KeySnap
{
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kGlideSeconds = 0.08;
    static constexpr double kTrackSeconds = 0.02;
    static constexpr int kAutoChoice = 25;

    static bool active (int choice) noexcept { return choice >= 1 && choice <= 24; }

    static int choiceForLabel (int label) noexcept
    {
        if (label < 0 || label >= 24)
            return 0;
        return label < 12 ? 13 + label : 1 + (label - 12);
    }

    static double referenceHz (const core::Cascade& cascade, double fs, double followHz = -1.0) noexcept
    {
        constexpr int points = 480;
        const double lo = 40.0, hi = std::min (16000.0, 0.45 * fs);
        const double step = std::log (hi / lo) / (points - 1);
        const auto powerDb = [&] (double hz)
        {
            const std::complex<double> z1 = std::polar (1.0, -2.0 * kPi * hz / fs);
            const std::complex<double> z2 = z1 * z1;
            double power = 1.0;
            for (const auto& b : cascade)
                power *= std::norm (b[0] + b[1] * z1 + b[2] * z2) / std::max (1.0e-300, std::norm (1.0 + b[3] * z1 + b[4] * z2));
            return std::isfinite (power) && power > 0.0 ? 10.0 * std::log10 (power) : -1.0e9;
        };
        double db[points];
        int best = -1;
        double bestDb = -1.0e9;
        for (int i = 0; i < points; ++i)
        {
            db[i] = powerDb (lo * std::exp (i * step));
            if (db[i] > bestDb) { bestDb = db[i]; best = i; }
        }
        if (followHz > 0.0)
        {
            const double followIndex = std::log (followHz / lo) / step;
            const double reach = 0.5 / step;
            int nearest = -1;
            for (int i = 1; i < points - 1; ++i)
                if (db[i] >= db[i - 1] && db[i] >= db[i + 1] && db[i] > bestDb - 24.0
                    && std::abs (i - followIndex) < reach
                    && (nearest < 0 || std::abs (i - followIndex) < std::abs (nearest - followIndex)))
                    nearest = i;
            if (nearest >= 0)
            {
                best = nearest;
                bestDb = db[best];
            }
        }
        if (best < 0)
            return -1.0;
        double centre = best;
        if (best > 0 && best < points - 1)
        {
            const double a = powerDb (lo * std::exp ((best - 1) * step)), c = powerDb (lo * std::exp ((best + 1) * step));
            const double curvature = a - 2.0 * bestDb + c;
            if (curvature < 0.0)
                centre += std::clamp (0.5 * (a - c) / curvature, -0.5, 0.5);
        }
        return lo * std::exp (centre * step);
    }

    static int root (int choice) noexcept { return (choice - 1) % 12; }

    static double offsetSemitones (double referenceHz, int choice) noexcept
    {
        if (! active (choice) || ! (referenceHz > 0.0))
            return 0.0;
        const double midi = 69.0 + 12.0 * std::log2 (referenceHz / 440.0);
        double best = 0.0, bestDistance = 1.0e9;
        for (int n = (int) std::floor (midi) - 7; n <= (int) std::ceil (midi) + 7; ++n)
            if (((n % 12) + 12) % 12 == root (choice) && std::abs (midi - n) < bestDistance)
            {
                bestDistance = std::abs (midi - n);
                best = n - midi;
            }
        return best;
    }

    static double glideFor (int samples, double fs) noexcept
    {
        return 1.0 - std::exp (-(double) samples / (kGlideSeconds * fs));
    }
};
}
