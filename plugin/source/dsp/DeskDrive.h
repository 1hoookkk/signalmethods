#pragma once
#include <algorithm>
#include <cmath>
namespace trench
{
class DeskDrive
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (8000.0, sampleRate);
        const double scale = sr / 44100.0;
        iirAmountA = kIirA / scale;
        updateCoupling();
        biquadA.setLowpass (sr, kUltrasonicHz, kBiquadAQ);
        biquadB.setLowpass (sr, kUltrasonicHz, kBiquadBQ);
        reset();
    }
    void setOutputCoupling (double hz)
    {
        if (couplingHz == hz)
            return;
        couplingHz = hz;
        updateCoupling();
    }
    void setEnabled (bool on)
    {
        if (enabled != on) { enabled = on; reset(); }
    }
    bool isActive() const noexcept { return enabled; }
    void setBypassSaturation (bool bypass) noexcept { bypassSaturate = bypass; }
    bool isBypassSaturation() const noexcept { return bypassSaturate; }
    void reset()
    {
        iirA = 0.0; iirB = 0.0;
        biquadA.reset(); biquadB.reset();
    }
    float process (float input, float drive) noexcept
    {
        if (! enabled)
            return input;
        double s = (double) input;
        iirA = guard (iirA * (1.0 - iirAmountA) + s * iirAmountA);
        s -= iirA;
        s *= inTrim (drive);
        s = biquadA.process (s);
        if (! bypassSaturate)
            s = saturate (s);
        s = biquadB.process (s);
        iirB = guard (iirB * (1.0 - iirAmountB) + s * iirAmountB);
        s -= iirB;
        s *= outPad (drive);
        return std::isfinite (s) ? (float) s : 0.0f;
    }
    static double inTrim (float knob) noexcept
    {
        return std::pow (10.0, kInTrimTopDb * std::clamp ((double) knob, 0.0, 1.0) / 20.0);
    }
    static double outPad (float knob) noexcept
    {
        return std::pow (inTrim (knob), -kOutPadShare);
    }
    static double saturate (double sample) noexcept
    {
        const double x = std::clamp (sample, -1.0, 1.0);
        return x - std::pow (x, 5.0) * 0.1768;
    }
private:
    static constexpr double kUltrasonicHz = 19160.0;
    static constexpr double kBiquadAQ = 0.431684981684982;
    static constexpr double kBiquadBQ = 1.1582298;
    static constexpr double kIirA = 0.001860867;
    static constexpr double kIirB = 0.000287496;
    static constexpr double kInTrimTopDb = 18.0;
    static constexpr double kOutPadShare = 0.75;
    static double guard (double x) noexcept { return std::abs (x) < 1.18e-37 ? 0.0 : x; }
    struct Biquad
    {
        double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void setLowpass (double sampleRate, double hz, double q)
        {
            const double n = std::clamp (hz / sampleRate, 1.0e-6, 0.49);
            const double k = std::tan (3.14159265358979323846 * n);
            const double norm = 1.0 / (1.0 + k / q + k * k);
            b0 = k * k * norm; b1 = 2.0 * b0; b2 = b0;
            a1 = 2.0 * (k * k - 1.0) * norm;
            a2 = (1.0 - k / q + k * k) * norm;
        }
        double process (double in) noexcept
        {
            const double out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = in; y2 = y1; y1 = guard (out);
            return y1;
        }
        void reset() { x1 = x2 = y1 = y2 = 0.0; }
    };
    void updateCoupling()
    {
        iirAmountB = couplingHz > 0.0 ? 2.0 * 3.14159265358979323846 * couplingHz / sr : kIirB / (sr / 44100.0);
    }
    bool enabled = false;
    bool bypassSaturate = false;
    double sr = 44100.0;
    double couplingHz = 0.0;
    double iirAmountA = kIirA, iirAmountB = kIirB, iirA = 0.0, iirB = 0.0;
    Biquad biquadA, biquadB;
};
}
