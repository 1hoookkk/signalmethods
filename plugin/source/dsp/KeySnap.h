#pragma once
#include <trench/core/packed_body.hpp>
#include <array>
#include <cmath>

namespace trench
{
struct KeySnap
{
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kHoldSemitones = 0.15;
    static constexpr double kGlideSeconds = 0.005;
    static constexpr double kMaxBandwidthRatio = 0.7;

    static bool active (int choice) noexcept { return choice >= 1 && choice <= 24; }

    static bool inScale (int midiNote, int choice) noexcept
    {
        static constexpr int major[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static constexpr int minor[7] = { 0, 2, 3, 5, 7, 8, 10 };
        const bool isMinor = choice <= 12;
        const int root = (choice - 1) % 12;
        const int degree = ((midiNote - root) % 12 + 12) % 12;
        for (int i = 0; i < 7; ++i)
            if ((isMinor ? minor[i] : major[i]) == degree)
                return true;
        return false;
    }

    static double targetNote (double midi, int choice, double held) noexcept
    {
        double best = std::round (midi);
        double bestDistance = 1.0e9;
        for (int n = (int) std::floor (midi) - 2; n <= (int) std::ceil (midi) + 2; ++n)
            if (inScale (n, choice) && std::abs (midi - n) < bestDistance)
            {
                bestDistance = std::abs (midi - n);
                best = n;
            }
        if (held > 0.0 && inScale ((int) held, choice) && std::abs (midi - held) < bestDistance + kHoldSemitones)
            return held;
        return best;
    }

    struct Lane
    {
        double note = -1.0;
        double shown = -1.0;
    };
    using Lanes = std::array<Lane, core::kSectionCount>;

    static bool snappable (const core::Biquad& b, double fs, double& hz, double& radius) noexcept
    {
        const double a1 = b[3], a2 = b[4];
        if (!(a2 > 0.0 && a2 < 1.0))
            return false;
        radius = std::sqrt (a2);
        const double c = -a1 / (2.0 * radius);
        if (!(c > -1.0 && c < 1.0))
            return false;
        hz = std::acos (c) * fs / (2.0 * kPi);
        const double bandwidth = -std::log (radius) * fs / kPi;
        return hz >= 20.0 && hz <= 0.45 * fs && bandwidth < kMaxBandwidthRatio * hz;
    }

    static core::Cascade apply (const core::Cascade& in, int choice, double fs, Lanes* lanes, double glide) noexcept
    {
        if (!active (choice))
            return in;
        core::Cascade out = in;
        for (std::size_t s = 0; s < in.size(); ++s)
        {
            double hz = 0.0, radius = 0.0;
            if (!snappable (in[s], fs, hz, radius))
            {
                if (lanes != nullptr)
                    (*lanes)[s] = {};
                continue;
            }
            const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
            const double held = lanes != nullptr ? (*lanes)[s].note : -1.0;
            const double note = targetNote (midi, choice, held);
            double shown = note;
            if (lanes != nullptr)
            {
                auto& lane = (*lanes)[s];
                shown = lane.shown < 0.0 ? note : lane.shown + glide * (note - lane.shown);
                lane.note = note;
                lane.shown = shown;
            }
            const double snapped = 440.0 * std::pow (2.0, (shown - 69.0) / 12.0);
            out[s][3] = -2.0 * radius * std::cos (2.0 * kPi * snapped / fs);
        }
        return out;
    }

    static double glideFor (int samples, double fs) noexcept
    {
        return 1.0 - std::exp (-(double) samples / (kGlideSeconds * fs));
    }
};
}
