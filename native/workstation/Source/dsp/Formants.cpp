#include "Formants.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace hs
{
std::vector<std::pair<double, double>> lpcResonances (const std::array<float, 13>& reflection, double sampleRateHz, int most);

std::array<double, 2> lpcFormants (const std::array<float, 13>& reflection, double sampleRateHz)
{
    const auto found = lpcResonances (reflection, sampleRateHz, 2);
    std::array<double, 2> out { 0.0, 0.0 };
    for (size_t i = 0; i < std::min<size_t> (2, found.size()); ++i) out[i] = found[i].first;
    return out;
}

std::vector<std::pair<double, double>> lpcResonances (const std::array<float, 13>& reflection, double sampleRateHz, int most)
{
    constexpr int order = 12;
    std::vector<double> a (order + 1, 0.0), next (order + 1, 0.0);
    a[0] = 1.0;
    for (int m = 1; m <= order; ++m)
    {
        const double k = (double) reflection[(size_t) m];
        next = a;
        next[(size_t) m] = -k;
        for (int i = 1; i < m; ++i) next[(size_t) i] = a[(size_t) i] - k * a[(size_t) (m - i)];
        a = next;
    }
    using Complex = std::complex<double>;
    std::vector<Complex> roots ((size_t) order);
    for (int i = 0; i < order; ++i) roots[(size_t) i] = std::polar (0.9, 2.0 * 3.141592653589793 * (i + 0.5) / order);
    auto evaluate = [&] (Complex z)
    {
        Complex acc = 1.0;
        for (int i = 1; i <= order; ++i) acc = acc * z + a[(size_t) i];
        return acc;
    };
    for (int pass = 0; pass < 200; ++pass)
    {
        double moved = 0.0;
        for (int i = 0; i < order; ++i)
        {
            Complex denominator = 1.0;
            for (int j = 0; j < order; ++j) if (j != i) denominator *= (roots[(size_t) i] - roots[(size_t) j]);
            if (std::abs (denominator) < 1e-30) continue;
            const Complex step = evaluate (roots[(size_t) i]) / denominator;
            roots[(size_t) i] -= step;
            moved = std::max (moved, std::abs (step));
        }
        if (moved < 1e-12) break;
    }
    std::vector<std::pair<double, double>> found;
    for (const auto& z : roots)
    {
        if (z.imag() <= 1e-6) continue;
        const double radius = std::abs (z), hz = std::arg (z) / (2.0 * 3.141592653589793) * sampleRateHz;
        const double bandwidth = -std::log (std::max (radius, 1e-9)) * sampleRateHz / 3.141592653589793;
        if (hz < 60.0 || hz > 12000.0 || bandwidth > 800.0) continue;
        found.push_back ({ hz, bandwidth });
    }
    std::sort (found.begin(), found.end(), [] (const auto& p, const auto& q) { return p.second < q.second; });
    if ((int) found.size() > most) found.resize ((size_t) most);
    std::sort (found.begin(), found.end());
    return found;
}
}
