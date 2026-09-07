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
    return polynomialResonances (a, sampleRateHz, most);
}

std::vector<std::pair<double, double>> spectralLpcPoles (const std::vector<float>& mono, double sampleRateHz, int most)
{
    constexpr int n = 512, hop = 128, order = 12;
    constexpr double alpha = 0.85, pi = 3.141592653589793;
    const int decimation = std::max (1, (int) std::lround (sampleRateHz / 11025.0));
    const double fs = sampleRateHz / decimation;
    std::vector<float> low;
    low.reserve (mono.size() / (size_t) decimation + 1);
    const int taps = 16 * decimation + 1, half = taps / 2;
    std::vector<double> fir ((size_t) taps);
    const double cutoff = 0.45 / decimation;
    double sum = 0.0;
    for (int i = 0; i < taps; ++i)
    {
        const double t = i - half;
        const double sinc = t == 0.0 ? 2.0 * cutoff : std::sin (2.0 * pi * cutoff * t) / (pi * t);
        const double w = 0.42 - 0.5 * std::cos (2.0 * pi * i / (taps - 1)) + 0.08 * std::cos (4.0 * pi * i / (taps - 1));
        fir[(size_t) i] = sinc * w;
        sum += fir[(size_t) i];
    }
    for (auto& v : fir) v /= sum;
    for (size_t i = (size_t) half; i + (size_t) half < mono.size(); i += (size_t) decimation)
    {
        double acc = 0.0;
        for (int j = 0; j < taps; ++j) acc += fir[(size_t) j] * mono[i + (size_t) j - (size_t) half];
        low.push_back ((float) acc);
    }
    if (low.size() < n) return {};
    std::vector<double> window (n), power (n / 2 + 1, 0.0), frame (n / 2 + 1);
    for (int i = 0; i < n; ++i) window[(size_t) i] = 0.54 - 0.46 * std::cos (2.0 * pi * i / (n - 1));
    std::vector<double> cosTable ((size_t) n * (n / 2 + 1)), sinTable ((size_t) n * (n / 2 + 1));
    for (int m = 0; m <= n / 2; ++m)
        for (int i = 0; i < n; ++i) { cosTable[(size_t) m * n + (size_t) i] = std::cos (2.0 * pi * m * i / n); sinTable[(size_t) m * n + (size_t) i] = std::sin (2.0 * pi * m * i / n); }
    bool primed = false;
    for (size_t start = 0; start + n <= low.size(); start += hop)
    {
        for (int m = 0; m <= n / 2; ++m)
        {
            double re = 0.0, im = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double x = (double) low[start + (size_t) i] * window[(size_t) i];
                re += x * cosTable[(size_t) m * n + (size_t) i];
                im -= x * sinTable[(size_t) m * n + (size_t) i];
            }
            frame[(size_t) m] = re * re + im * im;
        }
        for (int m = 0; m <= n / 2; ++m) power[(size_t) m] = primed ? alpha * power[(size_t) m] + (1.0 - alpha) * frame[(size_t) m] : frame[(size_t) m];
        primed = true;
    }
    {
        const int width = std::max (1, (int) std::lround (150.0 / (fs / n)));
        std::vector<double> smooth (power.size(), 0.0);
        for (int m = 0; m <= n / 2; ++m)
        {
            double acc = 0.0; int count = 0;
            for (int j = std::max (0, m - width); j <= std::min (n / 2, m + width); ++j) { acc += power[(size_t) j]; ++count; }
            smooth[(size_t) m] = acc / count;
        }
        power = smooth;
    }
    std::vector<double> r (order + 1, 0.0);
    for (int lag = 0; lag <= order; ++lag)
    {
        double acc = power[0] + power[(size_t) (n / 2)] * std::cos (pi * lag);
        for (int m = 1; m < n / 2; ++m) acc += 2.0 * power[(size_t) m] * std::cos (2.0 * pi * m * lag / n);
        r[(size_t) lag] = acc / n;
    }
    if (r[0] <= 1.0e-12) return {};
    r[0] *= 1.0001;
    std::vector<double> a (order + 1, 0.0), scratch (order + 1, 0.0);
    a[0] = 1.0;
    double error = r[0];
    for (int m = 1; m <= order; ++m)
    {
        double acc = r[(size_t) m];
        for (int i = 1; i < m; ++i) acc += a[(size_t) i] * r[(size_t) (m - i)];
        const double kk = -acc / error;
        scratch = a;
        for (int i = 1; i < m; ++i) a[(size_t) i] = scratch[(size_t) i] + kk * scratch[(size_t) (m - i)];
        a[(size_t) m] = kk;
        error *= (1.0 - kk * kk);
        if (error <= 0.0) break;
    }
    return polynomialResonances (a, fs, most);
}

std::vector<std::pair<double, double>> polynomialResonances (const std::vector<double>& a, double sampleRateHz, int most)
{
    const int order = (int) a.size() - 1;
    if (order < 2) return {};
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
        if (hz < 60.0 || hz > 12000.0 || bandwidth > 1500.0) continue;
        found.push_back ({ hz, bandwidth });
    }
    std::sort (found.begin(), found.end(), [] (const auto& p, const auto& q) { return p.second < q.second; });
    if ((int) found.size() > most) found.resize ((size_t) most);
    std::sort (found.begin(), found.end());
    return found;
}
}
