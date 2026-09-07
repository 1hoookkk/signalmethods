#include "Peevers.h"

#include <cmath>

namespace hs
{
namespace
{
const float kWinTbl[9][4] = {
    { 0.42659f, -0.49656f, 0.07685f, 0.0f },
    { 0.42f, -0.5f, 0.08f, 0.0f },
    { 0.42323f, -0.49755f, 0.07922f, 0.0f },
    { 0.44959f, -0.49364f, 0.05677f, 0.0f },
    { 0.35875f, -0.48829f, 0.14128f, 0.01168f },
    { 0.40217f, -0.49703f, 0.09392f, 0.00183f },
    { 0.54f, -0.46f, 0.0f, 0.0f },
    { 0.5f, -0.5f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 0.0f }
};

const int pwr_two[16] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 };

int power_of (int order) { return order >= 0 && order < 16 ? pwr_two[order] : 0; }
}

const char* const Peevers::windowNames[Peevers::kWindows] = {
    "Exact Blackman", "Blackman", "Blackman-Harris 1", "Blackman-Harris 2",
    "Blackman-Harris 3", "Blackman-Harris 4", "Hamming", "Hanning", "None"
};

Peevers::Peevers()
{
    wintbl.assign ((size_t) kMax, 0.0f);
    tmpc.assign ((size_t) kMax * 2, 0.0f);
    twiddle.assign ((size_t) kMax, 0.0f);
    arry.assign ((size_t) kMax + 2, 0.0f);
    fx.assign ((size_t) kMax + 2, 0.0f);
    zlogpos.assign ((size_t) kMax / 2 + 2, 0.0f);
    synth.assign ((size_t) kMax, 0.0f);
    levels (gain, floorlevel);
    reset();
    setParms (nfft, winsize, stride, wintype);
}

void Peevers::reset()
{
    lpc.k.fill (0.0f);
    lpc.f.fill (0.0f);
    lpc.b.fill (0.0f);
    lpc.g.fill (0.0f);
    lpc.power.fill (1.0f);
}

void Peevers::levels (float gainValue, float floorValue)
{
    gain = gainValue;
    floorlevel = floorValue;
    const float hi = std::log10 ((float) largest);
    const float lo = std::log10 ((float) smallest);
    m = ((double) gain * 12.75 - (double) floorlevel) / (double) (hi - lo);
    b = (double) floorlevel + m * 6.0;
}

void Peevers::setParms (int fftSize, int windowSize, int hop, int type)
{
    nfft = fftSize;
    nfft2 = fftSize / 2;
    winsize = windowSize;
    stride = hop;
    wintype = type;
    win_calc (wintbl.data(), wintype, winsize);
    init_logftbl();
}

void Peevers::win_calc (float* tbl, int type, int n)
{
    float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    if (type < 9)
    {
        a0 = kWinTbl[type][0];
        a1 = kWinTbl[type][1];
        a2 = kWinTbl[type][2];
        a3 = kWinTbl[type][3];
    }
    if (synthwin == 0)
    {
        for (int i = 0; i < n; ++i)
        {
            const double c2 = std::cos (((double) i * 12.566370614359172) / (double) n);
            const double c1 = std::cos (((double) i * 6.283185307179586) / (double) n);
            const double c3 = std::cos (((double) i * 18.84955592153876) / (double) n);
            tbl[i] = (float) (c3 * (double) a3 + (double) a0 + (double) a1 * c1 + (double) a2 * c2);
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const double c2 = std::cos (((double) i * 12.566370614359172) / (double) n);
            const double c1 = std::cos (((double) i * 6.283185307179586) / (double) n);
            const double c3 = std::cos (((double) i * 18.84955592153876) / (double) n);
            tbl[i] = std::sqrt ((float) (c3 * (double) a3 + (double) a0 + (double) a1 * c1 + (double) a2 * c2));
        }
    }
}

void Peevers::winmult (const float* tbl, float* data, int n)
{
    for (int i = 0; i < n; ++i) data[i] = data[i] * tbl[i];
    if (nfft != winsize)
    {
        int hi = nfft + winsize;
        if (hi < 0) hi = hi + 1;
        hi = hi >> 1;
        int span = nfft - winsize;
        if (span < 0) span = span + 1;
        int j = n;
        if (n < nfft)
        {
            do { data[j] = 0.0f; j = j + 1; } while (j < nfft);
        }
        for (; (span >> 1) <= hi; hi = hi - 1)
        {
            n = n - 1;
            data[hi] = data[n];
        }
        int t = 0;
        int low = nfft - winsize;
        if (low < 0) low = low + 1;
        if (0 < (low >> 1))
        {
            do
            {
                data[t] = 0.0f;
                int top = nfft + winsize;
                if (top < 0) top = top + 1;
                data[t + (top >> 1)] = 0.0f;
                t = t + 1;
                low = nfft - winsize;
                if (low < 0) low = low + 1;
            } while (t < (low >> 1));
        }
    }
}

void Peevers::winmult2 (const float* tbl, float* data, int n)
{
    for (int i = 0; i < n; ++i) data[i] = data[i] * tbl[i];
}

void Peevers::buildtable (int order)
{
    const int n = power_of (order);
    const int count = power_of (order - 1);
    for (int i = 0; i < count; ++i)
    {
        const float a = ((float) i * 6.2831855f) / (float) n;
        twiddle[(size_t) i * 2] = (float) std::cos ((double) a);
        twiddle[(size_t) i * 2 + 1] = (float) -std::sin ((double) a);
    }
}

void Peevers::bitreverse (float* data, int order)
{
    const int n = power_of (order);
    unsigned j = 0;
    int i = 0;
    while (true)
    {
        if (i < (int) j)
        {
            const float re = data[(size_t) i * 2], im = data[(size_t) i * 2 + 1];
            data[(size_t) i * 2] = data[(size_t) j * 2];
            data[(size_t) i * 2 + 1] = data[(size_t) j * 2 + 1];
            data[(size_t) j * 2] = re;
            data[(size_t) j * 2 + 1] = im;
        }
        i = i + 1;
        int t = order;
        if (i == n) break;
        for (--t; t > -1 && ((unsigned) power_of (t) & j) != 0; --t) j = j - (unsigned) power_of (t);
        j = (unsigned) power_of (t) + j;
    }
}

void Peevers::fft (float* data, unsigned order)
{
    const int n = power_of ((int) order);
    const int half = power_of ((int) order - 1);
    unsigned span = 1;
    unsigned shift = order;
    while (order != 0)
    {
        shift = shift - 1;
        const unsigned next = order - 1;
        unsigned i = 0;
        int c = half;
        while (c != 0)
        {
            c = c - 1;
            const unsigned tw = (i << (shift & 0x1f)) & (unsigned) (n - 1);
            const unsigned j = i + span;
            const float ar = data[(size_t) j * 2], ai = data[(size_t) j * 2 + 1];
            const float wr = twiddle[(size_t) tw * 2], wi = twiddle[(size_t) tw * 2 + 1];
            const float pr = ar * wr - ai * wi;
            const float pi = ar * wi + ai * wr;
            const float br = data[(size_t) i * 2], bi = data[(size_t) i * 2 + 1];
            data[(size_t) j * 2] = br - pr;
            data[(size_t) j * 2 + 1] = bi - pi;
            data[(size_t) i * 2] = br + pr;
            data[(size_t) i * 2 + 1] = bi + pi;
            i = (j + 1U) & ~span;
        }
        span = span << 1;
        order = next;
    }
}

void Peevers::ifft (float* data, unsigned order)
{
    const int n = power_of ((int) order);
    const int half = power_of ((int) order - 1);
    unsigned span = 1;
    unsigned shift = order;
    while (order != 0)
    {
        shift = shift - 1;
        const unsigned next = order - 1;
        unsigned i = 0;
        int c = half;
        while (c != 0)
        {
            c = c - 1;
            const unsigned j = i + span;
            const unsigned tw = (i << (shift & 0x1f)) & (unsigned) (n - 1);
            const float wr = twiddle[(size_t) tw * 2], wi = twiddle[(size_t) tw * 2 + 1];
            const float pr = wi * data[(size_t) j * 2 + 1] + data[(size_t) j * 2] * wr;
            const float pi = wr * data[(size_t) j * 2 + 1] - data[(size_t) j * 2] * wi;
            data[(size_t) j * 2] = data[(size_t) i * 2] - pr;
            data[(size_t) j * 2 + 1] = data[(size_t) i * 2 + 1] - pi;
            data[(size_t) i * 2] = data[(size_t) i * 2] + pr;
            data[(size_t) i * 2 + 1] = data[(size_t) i * 2 + 1] + pi;
            i = (j + 1U) & ~span;
        }
        span = span << 1;
        order = next;
    }
}

void Peevers::spectrum (const float* in, int nin, float* out, int nout)
{
    int size = 1, order = 0;
    if (1 < nout)
    {
        do { size = size << 1; order = order + 1; } while (size < nout);
    }
    if (fftinitialized != order)
    {
        buildtable (order);
        fftinitialized = order;
    }
    float* c = tmpc.data();
    const float* p = in;
    for (int i = 0; i < nin; ++i)
    {
        c[0] = *p;
        p = p + 1;
        c[1] = 0.0f;
        c = c + 2;
    }
    if (nin < size)
    {
        for (int i = 0; i < size - nin; ++i)
        {
            c[0] = 0.0f;
            c[1] = 0.0f;
            c = c + 2;
        }
    }
    bitreverse (tmpc.data(), order);
    fft (tmpc.data(), (unsigned) order);
    c = tmpc.data();
    float* o = out;
    for (int i = 0; i < nout; ++i)
    {
        *o = c[0] * c[0];
        *o = *o + c[1] * c[1];
        *o = *o / (float) (nout * nout);
        o = o + 1;
        c = c + 2;
    }
}

float Peevers::mag2 (float re, float im) { return re * re + im * im; }

double Peevers::magl (float re, float im) const
{
    float x = (re * re + im * im) / (float) (nfft * nfft);
    if (x < 1e-05f) x = 1e-05f;
    return (double) std::log10 (x) * m + b;
}

void Peevers::log_of (float* data, int n)
{
    for (int i = 0; i <= n; ++i)
    {
        if (data[i] < 1e-05f) data[i] = 1e-05f;
        const float l = std::log10 (data[i]);
        data[i] = (float) ((double) l * m + b);
    }
}

float Peevers::findmax() const
{
    float top = -100000.0f;
    for (int i = 0; i < nfft2; ++i) if (top < fx[(size_t) i]) top = fx[(size_t) i];
    return top;
}

void Peevers::normalize (double* data, unsigned n, float scale)
{
    for (unsigned i = 0; i < n; ++i) data[i] = data[i] * (double) scale;
}

float Peevers::getmax (const double* data, unsigned n)
{
    float top = (float) data[0];
    for (unsigned i = 0; i < n; ++i) if ((double) top < data[i]) top = (float) data[i];
    return top;
}

void Peevers::gal (float x)
{
    lpc.f[0] = x;
    for (int i = 1; i < 13; ++i)
        lpc.f[(size_t) i] = lpc.f[(size_t) i - 1] - lpc.b[(size_t) i - 1] * lpc.k[(size_t) i];
    for (int i = 12; 0 < i; --i)
    {
        lpc.b[(size_t) i] = lpc.b[(size_t) i - 1] - lpc.f[(size_t) i - 1] * lpc.k[(size_t) i];
        lpc.power[(size_t) i] = lpc.power[(size_t) i] * 0.998f;
        lpc.power[(size_t) i] = lpc.power[(size_t) i]
            + (lpc.f[(size_t) i - 1] * lpc.f[(size_t) i - 1] + lpc.b[(size_t) i - 1] * lpc.b[(size_t) i - 1]) * 0.002f;
        lpc.k[(size_t) i] = lpc.k[(size_t) i]
            + ((lpc.f[(size_t) i] * lpc.b[(size_t) i - 1] + lpc.b[(size_t) i] * lpc.f[(size_t) i - 1]) * 0.002f) / lpc.power[(size_t) i];
    }
    lpc.b[0] = x;
    lpc.k[0] = lpc.f[12];
}

float Peevers::lattice (float x)
{
    float s = x;
    for (int i = 11; -1 < i; --i)
    {
        s = s + lpc.k[(size_t) i + 1] * lpc.g[(size_t) i];
        lpc.g[(size_t) i + 1] = lpc.g[(size_t) i] - lpc.k[(size_t) i + 1] * s;
    }
    lpc.g[0] = s;
    return s;
}

void Peevers::init_logftbl()
{
    const float l = std::log ((float) nfft2);
    const float span = (float) nfft2;
    for (int i = 0; i <= nfft2; ++i) zlogpos[(size_t) i] = std::log ((float) (i + 1)) * (span / l);
}

void Peevers::floatit (const short* in, float* out, int n)
{
    for (int i = 0; i < n; ++i) out[i] = (float) (int) in[i];
}

float Peevers::lutlimit (float v)
{
    if (v < 1.0f) v = 1.0f;
    else if (254.0f < v) v = 254.0f;
    return v;
}

void Peevers::frame (const float* samples)
{
    float* a = arry.data() + 1;
    for (int i = 0; i < winsize; ++i) a[i] = samples[i];
    for (int i = winsize; i < nfft; ++i) a[i] = 0.0f;
    if (lpcenv == 0)
    {
        if (wintype != 8) winmult (wintbl.data(), a, winsize);
        spectrum (a, nfft, fx.data(), nfft);
        log_of (fx.data(), nfft2);
    }
    else
    {
        for (int i = 0; i < nfft; ++i)
        {
            gal (a[i]);
            synth[(size_t) i] = lattice (i == 0 ? 32000.0f : 0.0f);
        }
        spectrum (synth.data(), nfft, fx.data(), nfft);
        log_of (fx.data(), nfft2);
    }
}
}
