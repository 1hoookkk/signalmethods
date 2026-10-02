#pragma once
#include "Model.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace headspace {
struct ResponsePoint { double hz, db; };

inline double listeningGain(const Resolved& state, double rmsDb) {
    double power = 0, flat = 0;
    for (int harmonic = 1; harmonic * 110 < state.rate / 2; ++harmonic) {
        const double amplitude = .2 / (std::acos(-1.) * harmonic);
        const double energy = amplitude * amplitude * .5;
        flat += energy;
        const double db = trench::core::cascade_response_db(state.cascade, harmonic * 110., state.rate);
        power += energy * std::pow(10., db / 10);
    }
    return std::pow(10., rmsDb / 20) / std::sqrt(std::max(power, flat / 16));
}

inline std::vector<ResponsePoint> responseCurve(const Resolved& state) {
    std::vector<double> frequencies;
    for (int i = 0; i <= 128; ++i)
        frequencies.push_back(20 * std::pow(1000., i / 128.));
    for (std::size_t s = 0; s < state.words.size(); ++s) {
        const auto p = pole(state.words, s);
        for (double offset : {-16., -8., -4., -2., -1., -.5, -.25, -.125, 0., .125, .25, .5, 1., 2., 4., 8., 16.})
            frequencies.push_back(std::clamp(p.hz + offset * p.bandwidth, 20., 20000.));
        const auto z = zeroOf(state.words, s);
        if (z.parked || !(z.hz > 0) || !(z.bandwidth > 0)) continue;
        for (double offset : {-16., -8., -4., -2., -1., -.5, -.25, -.125, 0., .125, .25, .5, 1., 2., 4., 8., 16.})
            frequencies.push_back(std::clamp(z.hz + offset * z.bandwidth, 20., 20000.));
    }
    std::sort(frequencies.begin(), frequencies.end());
    frequencies.erase(std::unique(frequencies.begin(), frequencies.end()), frequencies.end());
    const auto at = [&](double hz) {
        return ResponsePoint{hz, trench::core::cascade_response_db(state.cascade, hz, state.rate)};
    };
    std::vector<ResponsePoint> curve{at(frequencies.front())};
    std::function<void(ResponsePoint, ResponsePoint, int)> refine;
    refine = [&](ResponsePoint a, ResponsePoint b, int depth) {
        const auto mid = at(std::sqrt(a.hz * b.hz));
        if (depth < 12 && std::abs(mid.db - (a.db + b.db) * .5) > .04) {
            refine(a, mid, depth + 1); refine(mid, b, depth + 1);
        } else curve.push_back(b);
    };
    for (std::size_t i = 1; i < frequencies.size(); ++i)
        refine(curve.back(), at(frequencies[i]), 0);
    return curve;
}
}
