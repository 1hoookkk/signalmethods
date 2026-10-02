#pragma once

#include <array>
#include <vector>

namespace headspace {

class Span {
public:
    static constexpr int kWindows = 9;
    static constexpr int kMaxOrder = 4096;
    static constexpr int kMaxBins = kMaxOrder / 2 + 1;
    static const char* const windowNames[kWindows];

    Span();

    void setFft(int order);
    void setWindow(int type, int length);
    void setAverage(bool enabled, double k = .99);
    void setPowerMode(int mode) { powerMode_ = mode; }
    void setRemoveDc(bool enabled) { removeDc_ = enabled; }
    void reset();
    void frame(const float* samples);
    static void demean(short* data, unsigned n);

    int order() const { return order_; }
    int windowLength() const { return winsize_; }
    int windowType() const { return wintype_; }
    int bins() const { return order_ / 2 + 1; }
    double windowEnergy() const { return squark_; }
    double fullScaleDb() const { return fullScaleDb_; }
    const float* spectrum() const { return fx_.data(); }

private:
    static constexpr int kPwrTwo[16] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};

    void winCalc(float* table, int type, int n);
    static void winmult(const short* in, const float* table, float* out, unsigned n);
    void buildtable(int order);
    static void bitreverse(float* data, int order);
    void fft(float* data, unsigned order);
    void spectrum(const float* in, int nin, float* out, int nout);
    void xavg(float* data, int n, int clear);
    void logOf(float* data, int n);

    int order_{1024}, winsize_{1024}, wintype_{1}, powerMode_{};
    int fftinitialized_{-1};
    bool removeDc_{}, average_{};
    double squark_{}, fullScaleDb_{};
    double expk_{.99}, expg_{.01};
    std::vector<float> wintbl_, twiddle_, tmpc_, windowed_, fx_, avgstate_;
    std::vector<short> samples_;
};

}
