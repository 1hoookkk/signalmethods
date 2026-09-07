#pragma once

#include <array>
#include <vector>

namespace hs
{
struct Peevers
{
    static constexpr int kMax = 4096;
    static constexpr int kOrder = 12;
    static constexpr int kWindows = 9;
    static const char* const windowNames[kWindows];

    Peevers();

    int nfft = 256, nfft2 = 128, winsize = 256, stride = 128, wintype = 7;
    int synthwin = 0, lpcenv = 0;
    int fftinitialized = -1;
    float gain = 20.0f, floorlevel = -20.0f;
    double smallest = 1e-06, largest = 268225000.0;
    double m = 0.0, b = 0.0;

    struct Lpc
    {
        std::array<float, 13> k {}, f {}, b {}, power {}, g {};
    } lpc;

    std::vector<float> wintbl, tmpc, twiddle, arry, fx, zlogpos, synth;

    void levels (float gainValue, float floorValue);
    void setParms (int fftSize, int windowSize, int hop, int type);
    void reset();

    void win_calc (float* tbl, int type, int n);
    void winmult (const float* tbl, float* data, int n);
    void winmult2 (const float* tbl, float* data, int n);
    void buildtable (int order);
    void bitreverse (float* data, int order);
    void fft (float* data, unsigned order);
    void ifft (float* data, unsigned order);
    void spectrum (const float* in, int nin, float* out, int nout);
    static float mag2 (float re, float im);
    double magl (float re, float im) const;
    void log_of (float* data, int n);
    float findmax() const;
    static void normalize (double* data, unsigned n, float scale);
    static float getmax (const double* data, unsigned n);
    void gal (float x);
    float lattice (float x);
    void init_logftbl();
    static void floatit (const short* in, float* out, int n);
    static float lutlimit (float v);
    void frame (const float* samples);
};
}
