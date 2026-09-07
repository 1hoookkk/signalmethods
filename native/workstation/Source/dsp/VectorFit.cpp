#include "VectorFit.h"

#include <algorithm>
#include <cmath>

namespace hs
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
using Complex = std::complex<double>;

void transform (std::vector<Complex>& data, bool inverse)
{
    const size_t n = data.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (data[i], data[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double angle = (inverse ? 2.0 : -2.0) * kPi / (double) len;
        const Complex step (std::cos (angle), std::sin (angle));
        for (size_t i = 0; i < n; i += len)
        {
            Complex w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const Complex u = data[i + k], v = data[i + k + len / 2] * w;
                data[i + k] = u + v;
                data[i + k + len / 2] = u - v;
                w *= step;
            }
        }
    }
    if (inverse)
        for (auto& v : data) v /= (double) n;
}

struct PolePair
{
    Complex a { 0.0, 0.0 }, b { 0.0, 0.0 };
    bool conjugate = true;
};

Complex basisOf (const PolePair& p, const Complex& z, int which)
{
    const Complex x = 1.0 / (z - p.a), y = 1.0 / (z - p.b);
    if (p.conjugate) return which == 0 ? x + y : Complex (0.0, 1.0) * (x - y);
    return which == 0 ? x : y;
}

bool gauss (std::vector<double>& a, std::vector<double>& b, int n)
{
    for (int col = 0; col < n; ++col)
    {
        int pivot = col;
        for (int r = col + 1; r < n; ++r)
            if (std::abs (a[(size_t) (r * n + col)]) > std::abs (a[(size_t) (pivot * n + col)])) pivot = r;
        const double head = a[(size_t) (pivot * n + col)];
        if (! std::isfinite (head) || std::abs (head) <= 0.0) return false;
        if (pivot != col)
        {
            for (int c = 0; c < n; ++c) std::swap (a[(size_t) (col * n + c)], a[(size_t) (pivot * n + c)]);
            std::swap (b[(size_t) col], b[(size_t) pivot]);
        }
        const double d = a[(size_t) (col * n + col)];
        for (int r = col + 1; r < n; ++r)
        {
            const double f = a[(size_t) (r * n + col)] / d;
            if (f == 0.0) continue;
            for (int c = col; c < n; ++c) a[(size_t) (r * n + c)] -= f * a[(size_t) (col * n + c)];
            b[(size_t) r] -= f * b[(size_t) col];
        }
    }
    for (int r = n - 1; r >= 0; --r)
    {
        double s = b[(size_t) r];
        for (int c = r + 1; c < n; ++c) s -= a[(size_t) (r * n + c)] * b[(size_t) c];
        const double d = a[(size_t) (r * n + r)];
        if (std::abs (d) <= 0.0) return false;
        b[(size_t) r] = s / d;
    }
    for (int r = 0; r < n; ++r)
        if (! std::isfinite (b[(size_t) r])) return false;
    return true;
}

bool leastSquares (const std::vector<PolePair>& poles, const std::vector<Complex>& z, const std::vector<Complex>& h,
                   const std::vector<double>& weight, bool withSigma, std::vector<double>& x)
{
    const int p = (int) poles.size();
    const int n = 2 * p + 1 + (withSigma ? 2 * p : 0);
    std::vector<double> ata ((size_t) n * (size_t) n, 0.0), atb ((size_t) n, 0.0);
    std::vector<double> re ((size_t) n, 0.0), im ((size_t) n, 0.0);
    for (size_t k = 0; k < z.size(); ++k)
    {
        for (int i = 0; i < p; ++i)
        {
            const Complex b0 = basisOf (poles[(size_t) i], z[k], 0) * weight[k];
            const Complex b1 = basisOf (poles[(size_t) i], z[k], 1) * weight[k];
            re[(size_t) (2 * i)] = b0.real();
            im[(size_t) (2 * i)] = b0.imag();
            re[(size_t) (2 * i + 1)] = b1.real();
            im[(size_t) (2 * i + 1)] = b1.imag();
            if (withSigma)
            {
                const Complex s0 = -h[k] * b0, s1 = -h[k] * b1;
                re[(size_t) (2 * p + 1 + 2 * i)] = s0.real();
                im[(size_t) (2 * p + 1 + 2 * i)] = s0.imag();
                re[(size_t) (2 * p + 2 + 2 * i)] = s1.real();
                im[(size_t) (2 * p + 2 + 2 * i)] = s1.imag();
            }
        }
        re[(size_t) (2 * p)] = weight[k];
        im[(size_t) (2 * p)] = 0.0;
        const Complex rhs = h[k] * weight[k];
        for (int r = 0; r < n; ++r)
        {
            atb[(size_t) r] += re[(size_t) r] * rhs.real() + im[(size_t) r] * rhs.imag();
            for (int c = r; c < n; ++c)
                ata[(size_t) (r * n + c)] += re[(size_t) r] * re[(size_t) c] + im[(size_t) r] * im[(size_t) c];
        }
    }
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < r; ++c) ata[(size_t) (r * n + c)] = ata[(size_t) (c * n + r)];
    double top = 0.0;
    for (int r = 0; r < n; ++r) top = std::max (top, ata[(size_t) (r * n + r)]);
    if (! (top > 0.0) || ! std::isfinite (top)) return false;
    for (int r = 0; r < n; ++r) ata[(size_t) (r * n + r)] += 1.0e-12 * top;
    x.assign (atb.begin(), atb.end());
    return gauss (ata, x, n);
}

std::vector<Complex> productPoly (const std::vector<Complex>& roots)
{
    std::vector<Complex> poly { Complex (1.0, 0.0) };
    for (const auto& r : roots)
    {
        std::vector<Complex> next (poly.size() + 1, Complex (0.0, 0.0));
        for (size_t i = 0; i < poly.size(); ++i)
        {
            next[i + 1] += poly[i];
            next[i] -= r * poly[i];
        }
        poly.swap (next);
    }
    return poly;
}

std::vector<Complex> deflate (const std::vector<Complex>& poly, const Complex& root)
{
    if (poly.size() < 2) return {};
    const size_t d = poly.size() - 1;
    std::vector<Complex> q (d, Complex (0.0, 0.0));
    q[d - 1] = poly[d];
    for (int i = (int) d - 2; i >= 0; --i) q[(size_t) i] = poly[(size_t) i + 1] + root * q[(size_t) i + 1];
    return q;
}

std::vector<Complex> aberth (std::vector<double> poly)
{
    double top = 0.0;
    for (double v : poly) top = std::max (top, std::abs (v));
    if (! (top > 0.0)) return {};
    while (poly.size() > 1 && std::abs (poly.back()) <= 1.0e-13 * top) poly.pop_back();
    const int d = (int) poly.size() - 1;
    if (d < 1) return {};
    const double lead = poly[(size_t) d];
    double bound = 0.0;
    for (int i = 0; i < d; ++i) bound = std::max (bound, std::abs (poly[(size_t) i] / lead));
    double start = std::abs (poly[0] / lead) > 0.0 ? std::pow (std::abs (poly[0] / lead), 1.0 / (double) d) : 1.0;
    if (! (start > 1.0e-6) || ! std::isfinite (start)) start = 1.0;
    std::vector<Complex> x ((size_t) d);
    for (int i = 0; i < d; ++i)
    {
        const double angle = 2.0 * kPi * ((double) i + 0.31) / (double) d;
        const double radius = start * (1.0 + 0.15 * (double) (i % 3));
        x[(size_t) i] = std::polar (std::min (radius, 1.0 + bound), angle);
    }
    for (int pass = 0; pass < 60; ++pass)
    {
        double step = 0.0;
        for (int i = 0; i < d; ++i)
        {
            Complex value (0.0, 0.0), slope (0.0, 0.0);
            for (int c = d; c >= 0; --c)
            {
                slope = slope * x[(size_t) i] + value;
                value = value * x[(size_t) i] + poly[(size_t) c];
            }
            if (std::abs (slope) <= 0.0) continue;
            const Complex ratio = value / slope;
            Complex sum (0.0, 0.0);
            for (int j = 0; j < d; ++j)
            {
                if (j == i) continue;
                const Complex gap = x[(size_t) i] - x[(size_t) j];
                if (std::abs (gap) > 0.0) sum += 1.0 / gap;
            }
            const Complex denominator = 1.0 - ratio * sum;
            const Complex move = std::abs (denominator) > 0.0 ? ratio / denominator : ratio;
            if (! std::isfinite (move.real()) || ! std::isfinite (move.imag())) continue;
            x[(size_t) i] -= move;
            step = std::max (step, std::abs (move));
        }
        if (step < 1.0e-12) break;
    }
    return x;
}

std::vector<PolePair> pairUp (std::vector<Complex> roots, int pairs)
{
    for (auto& r : roots)
        if (std::abs (r) > 1.0) r = 1.0 / std::conj (r);
    std::vector<PolePair> out;
    std::vector<Complex> reals;
    std::vector<bool> used (roots.size(), false);
    for (size_t i = 0; i < roots.size(); ++i)
    {
        if (used[i]) continue;
        used[i] = true;
        if (std::abs (roots[i].imag()) < 1.0e-9) { reals.push_back (Complex (roots[i].real(), 0.0)); continue; }
        size_t best = roots.size();
        double gap = 1.0e300;
        for (size_t j = i + 1; j < roots.size(); ++j)
        {
            if (used[j]) continue;
            const double d = std::abs (roots[j] - std::conj (roots[i]));
            if (d < gap) { gap = d; best = j; }
        }
        if (best >= roots.size()) return {};
        used[best] = true;
        Complex p = roots[i];
        if (p.imag() < 0.0) p = std::conj (p);
        const double radius = std::clamp (std::abs (p), 1.0e-6, 0.99999);
        p = std::polar (radius, std::abs (std::arg (p)));
        out.push_back ({ p, std::conj (p), true });
    }
    for (size_t i = 0; i + 1 < reals.size(); i += 2)
    {
        const double a = std::clamp (reals[i].real(), -0.99999, 0.99999);
        const double b = std::clamp (reals[i + 1].real(), -0.99999, 0.99999);
        if (std::abs (a - b) < 1.0e-9) return {};
        out.push_back ({ Complex (a, 0.0), Complex (b, 0.0), false });
    }
    if ((int) out.size() != pairs) return {};
    return out;
}

void spread (const std::vector<PolePair>& poles, const std::vector<double>& x, bool sigma,
             std::vector<Complex>& roots, std::vector<Complex>& residues)
{
    const int p = (int) poles.size();
    const int base = sigma ? 2 * p + 1 : 0;
    roots.clear();
    residues.clear();
    for (int i = 0; i < p; ++i)
    {
        const double u = x[(size_t) (base + 2 * i)], v = x[(size_t) (base + 2 * i + 1)];
        roots.push_back (poles[(size_t) i].a);
        roots.push_back (poles[(size_t) i].b);
        if (poles[(size_t) i].conjugate)
        {
            residues.push_back (Complex (u, v));
            residues.push_back (Complex (u, -v));
        }
        else
        {
            residues.push_back (Complex (u, 0.0));
            residues.push_back (Complex (v, 0.0));
        }
    }
}

std::vector<double> numeratorPoly (const std::vector<Complex>& roots, const std::vector<Complex>& residues, double lead)
{
    const auto full = productPoly (roots);
    std::vector<Complex> num (full.size(), Complex (0.0, 0.0));
    for (size_t i = 0; i < full.size(); ++i) num[i] = lead * full[i];
    for (size_t n = 0; n < roots.size(); ++n)
    {
        const auto q = deflate (full, roots[n]);
        for (size_t i = 0; i < q.size(); ++i) num[i] += residues[n] * q[i];
    }
    std::vector<double> real (num.size(), 0.0);
    for (size_t i = 0; i < num.size(); ++i) real[i] = num[i].real();
    return real;
}

Complex evaluateFit (const std::vector<PolePair>& poles, const std::vector<double>& x, const Complex& z)
{
    const int p = (int) poles.size();
    Complex sum (x[(size_t) (2 * p)], 0.0);
    for (int i = 0; i < p; ++i)
        sum += x[(size_t) (2 * i)] * basisOf (poles[(size_t) i], z, 0) + x[(size_t) (2 * i + 1)] * basisOf (poles[(size_t) i], z, 1);
    return sum;
}
}

std::vector<double> minimumPhaseResponse (const std::vector<double>& magnitudeDb, std::vector<Complex>& out)
{
    out.clear();
    const size_t m = magnitudeDb.size();
    if (m < 3) return {};
    const size_t n = 2 * (m - 1);
    if ((n & (n - 1)) != 0) return {};
    double top = -1.0e300;
    for (double v : magnitudeDb)
        if (std::isfinite (v)) top = std::max (top, v);
    if (! std::isfinite (top)) return {};
    const double bottom = top - 140.0, scale = std::log (10.0) / 20.0;
    std::vector<Complex> buffer (n, Complex (0.0, 0.0));
    for (size_t k = 0; k < m; ++k)
    {
        const double db = std::isfinite (magnitudeDb[k]) ? std::max (magnitudeDb[k], bottom) : bottom;
        buffer[k] = Complex (db * scale, 0.0);
    }
    for (size_t k = 1; k + 1 < m; ++k) buffer[n - k] = buffer[k];
    transform (buffer, true);
    for (size_t i = 0; i < n; ++i)
    {
        if (i == 0 || i == n / 2) continue;
        if (i < n / 2) buffer[i] *= 2.0;
        else buffer[i] = Complex (0.0, 0.0);
    }
    transform (buffer, false);
    std::vector<double> phase (m, 0.0);
    out.resize (m);
    for (size_t k = 0; k < m; ++k)
    {
        out[k] = std::exp (buffer[k]);
        phase[k] = buffer[k].imag();
    }
    return phase;
}

Fitted vectorFit (const std::vector<Complex>& response, double sampleRateHz, int pairs, int passes)
{
    Fitted out;
    const int p = std::max (1, pairs);
    const size_t m = response.size();
    if (m < 8 || ! (sampleRateHz > 0.0)) return out;
    std::vector<Complex> z, h;
    std::vector<double> weight;
    for (size_t k = 0; k < m; ++k)
    {
        const Complex v = response[k];
        if (! std::isfinite (v.real()) || ! std::isfinite (v.imag())) continue;
        const double magnitude = std::abs (v);
        if (! (magnitude > 0.0)) continue;
        z.push_back (std::polar (1.0, kPi * (double) k / (double) (m - 1)));
        h.push_back (v);
        weight.push_back (1.0 / std::max (magnitude, 1.0e-4));
    }
    if (z.size() < (size_t) (4 * p + 2)) return out;
    std::vector<PolePair> poles ((size_t) p);
    const double low = 120.0, high = std::max (240.0, 0.4 * sampleRateHz / 2.0);
    for (int i = 0; i < p; ++i)
    {
        const double t = p > 1 ? (double) i / (double) (p - 1) : 0.0;
        const double hz = low * std::pow (high / low, t);
        const Complex pole = std::polar (0.9, 2.0 * kPi * hz / sampleRateHz);
        poles[(size_t) i] = { pole, std::conj (pole), true };
    }
    std::vector<double> x;
    for (int pass = 0; pass < passes; ++pass)
    {
        std::vector<double> sigma;
        if (! leastSquares (poles, z, h, weight, true, sigma)) break;
        std::vector<Complex> roots, residues;
        spread (poles, sigma, true, roots, residues);
        const auto next = pairUp (aberth (numeratorPoly (roots, residues, 1.0)), p);
        if (next.empty()) break;
        poles = next;
    }
    if (! leastSquares (poles, z, h, weight, false, x)) return out;
    std::vector<Complex> roots, residues;
    spread (poles, x, false, roots, residues);
    out.gain = x[(size_t) (2 * p)];
    out.poles = roots;
    out.zeros = aberth (numeratorPoly (roots, residues, out.gain));
    double sum = 0.0;
    size_t count = 0;
    for (size_t k = 0; k < z.size(); ++k)
    {
        const Complex fit = evaluateFit (poles, x, z[k]);
        const double magnitude = std::abs (fit);
        if (! (magnitude > 0.0) || ! std::isfinite (magnitude)) continue;
        const double e = 20.0 * std::log10 (magnitude / std::abs (h[k]));
        if (! std::isfinite (e)) continue;
        sum += e * e;
        ++count;
    }
    out.errorDb = count > 0 ? std::sqrt (sum / (double) count) : 0.0;
    return out;
}
}
