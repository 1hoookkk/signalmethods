#include "Span.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace headspace {
namespace {

const float kWinTable[9][4] = {
    { 0.42659f, -0.49656f, 0.07685f, 0.0f },
    { 0.42f, -0.5f, 0.08f, 0.0f },
    { 0.42323f, -0.49755f, 0.07922f, 0.0f },
    { 0.44959f, -0.49364f, 0.05677f, 0.0f },
    { 0.35875f, -0.48829f, 0.14128f, 0.01168f },
    { 0.40217f, -0.49703f, 0.09392f, 0.00183f },
    { 0.54f, -0.46f, 0.0f, 0.0f },
    { 0.5f, -0.5f, 0.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f, 0.0f }
};

constexpr double kTurn = 6.283185307179586;
constexpr double kTwoTurn = 12.566370614359172;
constexpr double kThreeTurn = 18.84955592153876;
constexpr double kThirtyTwo = 1073741824.0;

}

const char* const Span::windowNames[Span::kWindows] = {
    "Exact Blackman", "Blackman", "Blackman-Harris 1", "Blackman-Harris 2",
    "Blackman-Harris 3", "Blackman-Harris 4", "Hamming", "Hanning", "Rectangular"
};

Span::Span() {
    setFft(order_);
    setWindow(wintype_, winsize_);
}

void Span::setFft(int order) {
    order_ = order;
    const std::size_t bins = static_cast<std::size_t>(order / 2 + 1);
    tmpc_.assign(static_cast<std::size_t>(order) * 2, 0.0f);
    twiddle_.assign(kMaxOrder, 0.0f);
    fx_.assign(static_cast<std::size_t>(order), 0.0f);
    avgstate_.assign(bins, 0.0f);
    fftinitialized_ = -1;
}

void Span::setWindow(int type, int length) {
    wintype_ = type;
    winsize_ = length;
    wintbl_.assign(static_cast<std::size_t>(length), 0.0f);
    windowed_.assign(static_cast<std::size_t>(length), 0.0f);
    samples_.assign(static_cast<std::size_t>(length), 0);
    squark_ = 0.0;
    winCalc(wintbl_.data(), type, length);
    double sum = 0.0;
    for (int i = 0; i < length; ++i) sum += static_cast<double>(wintbl_[static_cast<std::size_t>(i)]);
    fullScaleDb_ = 10.0 * std::log10(sum * sum * static_cast<double>(winsize_) / (4.0 * squark_));
    reset();
}

void Span::setAverage(bool enabled, double k) {
    average_ = enabled;
    expk_ = k;
    expg_ = 1.0 - k;
}

void Span::reset() {
    xavg(fx_.data(), order_ / 2, 1);
}

void Span::frame(const float* samples) {
    for (int i = 0; i < winsize_; ++i) {
        const float scaled = std::clamp(samples[i] * 32767.0f, -32768.0f, 32767.0f);
        samples_[static_cast<std::size_t>(i)] = static_cast<short>(std::lround(scaled));
    }
    if (removeDc_) demean(samples_.data(), static_cast<unsigned>(winsize_));
    winmult(samples_.data(), wintbl_.data(), windowed_.data(), static_cast<unsigned>(winsize_));
    spectrum(windowed_.data(), winsize_, fx_.data(), order_);
    if (average_) xavg(fx_.data(), order_ / 2, 0);
    logOf(fx_.data(), order_ / 2);
}

void Span::winCalc(float* table, int type, int n) {
    if (type < 0 || type >= kWindows) throw std::invalid_argument("Unknown Span window type");
    const float a0 = kWinTable[type][0], a1 = kWinTable[type][1];
    const float a2 = kWinTable[type][2], a3 = kWinTable[type][3];
    for (int i = 0; i < n; ++i) {
        const double c2 = std::cos((static_cast<double>(i) * kTwoTurn) / static_cast<double>(n));
        const double c1 = std::cos((static_cast<double>(i) * kTurn) / static_cast<double>(n));
        const double c3 = std::cos((static_cast<double>(i) * kThreeTurn) / static_cast<double>(n));
        const float value = static_cast<float>(c3 * static_cast<double>(a3) + static_cast<double>(a0)
            + static_cast<double>(a1) * c1 + static_cast<double>(a2) * c2);
        table[i] = value;
        squark_ += static_cast<double>(value * value);
    }
}

void Span::winmult(const short* in, const float* table, float* out, unsigned n) {
    for (unsigned i = 0; i < n; ++i) out[i] = static_cast<float>(in[i]) * table[i];
}

void Span::demean(short* data, unsigned n) {
    if (static_cast<int>(n) <= 0) return;
    int sum = 0;
    for (unsigned i = 0; i < n; ++i) sum += static_cast<int>(data[i]);
    const float mean = static_cast<float>(sum / static_cast<int>(n));
    for (unsigned i = 0; i < n; ++i)
        data[i] = static_cast<short>(static_cast<int>(std::floor(static_cast<float>(data[i]) - mean)));
}

void Span::buildtable(int order) {
    const int n = kPwrTwo[order];
    const int count = kPwrTwo[order - 1];
    for (int i = 0; i < count; ++i) {
        const float angle = static_cast<float>((static_cast<double>(i) * kTurn) / static_cast<double>(n));
        twiddle_[static_cast<std::size_t>(i) * 2] = static_cast<float>(std::cos(static_cast<double>(angle)));
        twiddle_[static_cast<std::size_t>(i) * 2 + 1] = static_cast<float>(-std::sin(static_cast<double>(angle)));
    }
}

void Span::bitreverse(float* data, int order) {
    const int n = kPwrTwo[order];
    unsigned j = 0;
    int i = 0;
    while (true) {
        if (i < static_cast<int>(j)) {
            const std::size_t a = static_cast<std::size_t>(i) * 2, b = static_cast<std::size_t>(j) * 2;
            const float re = data[a], im = data[a + 1];
            data[a] = data[b]; data[a + 1] = data[b + 1];
            data[b] = re; data[b + 1] = im;
        }
        ++i;
        if (i == n) return;
        int t = order - 1;
        while (t > -1 && (static_cast<unsigned>(kPwrTwo[t]) & j) != 0) {
            j -= static_cast<unsigned>(kPwrTwo[t]);
            --t;
        }
        j += static_cast<unsigned>(kPwrTwo[t]);
    }
}

void Span::fft(float* data, unsigned order) {
    const int n = kPwrTwo[order];
    const int half = kPwrTwo[order - 1];
    unsigned span = 1;
    unsigned shift = order;
    while (order != 0) {
        shift -= 1;
        const unsigned next = order - 1;
        unsigned i = 0;
        int count = half;
        while (count != 0) {
            --count;
            const unsigned twiddle = (i << (shift & 0x1f)) & static_cast<unsigned>(n - 1);
            const unsigned j = i + span;
            const float ar = data[static_cast<std::size_t>(j) * 2], ai = data[static_cast<std::size_t>(j) * 2 + 1];
            const float wr = twiddle_[static_cast<std::size_t>(twiddle) * 2];
            const float wi = twiddle_[static_cast<std::size_t>(twiddle) * 2 + 1];
            const float pr = ar * wr - ai * wi;
            const float pi = ar * wi + ai * wr;
            const float br = data[static_cast<std::size_t>(i) * 2], bi = data[static_cast<std::size_t>(i) * 2 + 1];
            data[static_cast<std::size_t>(j) * 2] = br - pr;
            data[static_cast<std::size_t>(j) * 2 + 1] = bi - pi;
            data[static_cast<std::size_t>(i) * 2] = br + pr;
            data[static_cast<std::size_t>(i) * 2 + 1] = bi + pi;
            i = (j + 1U) & ~span;
        }
        span <<= 1;
        order = next;
    }
}

void Span::spectrum(const float* in, int nin, float* out, int nout) {
    int size = 1, order = 0;
    if (1 < nout) {
        do { size <<= 1; ++order; } while (size < nout);
    }
    if (order != fftinitialized_) {
        buildtable(order);
        fftinitialized_ = order;
    }
    float* c = tmpc_.data();
    for (int i = 0; i < nin; ++i) { c[0] = in[i]; c[1] = 0.0f; c += 2; }
    for (int i = nin; i < size; ++i) { c[0] = 0.0f; c[1] = 0.0f; c += 2; }
    bitreverse(tmpc_.data(), order);
    fft(tmpc_.data(), static_cast<unsigned>(order));
    c = tmpc_.data();
    for (int i = 0; i < nout; ++i) {
        out[i] = c[0] * c[0];
        out[i] = out[i] + c[1] * c[1];
        c += 2;
    }
}

void Span::xavg(float* data, int n, int clear) {
    if (clear != 0 && n >= 0)
        for (int i = 0; i <= n; ++i) avgstate_[static_cast<std::size_t>(i)] = 0.0f;
    if (n < 0) return;
    for (int i = 0; i <= n; ++i) {
        data[i] = static_cast<float>(static_cast<double>(avgstate_[static_cast<std::size_t>(i)]) * expk_
            + static_cast<double>(static_cast<float>(static_cast<double>(data[i]) * expg_)));
        avgstate_[static_cast<std::size_t>(i)] = data[i];
    }
}

void Span::logOf(float* data, int n) {
    const double ratio = static_cast<double>(winsize_) / squark_;
    const float orderLog = std::log10(static_cast<float>(order_));
    const float scale = static_cast<float>(ratio);
    const float unit = static_cast<float>(static_cast<double>(scale) / kThirtyTwo);
    const float floorValue = static_cast<float>((powerMode_ == 0 ? 1e-10 : 1e-4) * static_cast<double>(1.0f / unit));
    const float orderTerm = 20.0f * orderLog;
    for (int i = 0; i <= n; ++i) {
        float value = data[i];
        if (value < floorValue) { data[i] = floorValue; value = data[i]; }
        data[i] = 10.0f * std::log10(value * unit) - orderTerm * static_cast<float>(powerMode_);
    }
}

}
