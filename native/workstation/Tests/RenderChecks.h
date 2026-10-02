#pragma once
#include "Response.h"
#include <complex>
#include <numbers>

inline void renderChecks(const headspace::Library& library) {
    using namespace trench::core;
    using namespace headspace;
    const std::array<std::pair<std::uint16_t, double>, 9> golden{{
        {0x0000, 0.}, {0x0001, 2. / 134217728.}, {0x0ffe, 4095. / 134217728.},
        {0x0fff, 4096. / 134217728.}, {0x1000, 4097. / 134217728.},
        {0xcfff, .125}, {0xdfff, .25}, {0xefff, .5}, {0xffff, 1.}}};
    for (const auto [word, expected] : golden)
        require(decode_word(word) == expected, "Decoder rational fixture mismatch");
    for (std::uint32_t word = 0; word < 65536; ++word) {
        const std::uint32_t code = word + 1, exponent = code / 4096, mantissa = code % 4096;
        const std::uint64_t numerator = word == 0 ? 0 : exponent == 0 ? mantissa
            : (std::uint64_t(4096 + mantissa) << (exponent - 1));
        require(decode_word(static_cast<std::uint16_t>(word)) == numerator / 134217728., "Decoder differs from integer expansion oracle");
    }
    require(section_words_to_biquad({0xdfff, 0xffff, 0xcfff, 0xdfff, 0xcfff}) == Biquad{.5, 0., 0., -1.25, .75}, "Coupled coefficient golden fixture mismatch");
    require(section_words_to_biquad({0xcfff, 0xdfff, 0xcfff, 0xdfff, 0xdfff}) == Biquad{1., -1.25, .75, -1.25, .75}, "Pole/zero cancellation golden fixture mismatch");
    std::cout << "PASS independent integer decoder oracle: all 65536 codes; rational and coupled-coefficient fixtures\n";

    Model model(library.entries[0]);
    for (std::size_t c = 0; c < 4; ++c)
        model.setCorner(c, library.entries[(c * 13) % library.entries.size()].words, library.entries[(c * 13) % library.entries.size()].name);
    std::vector<Endpoint> states;
    for (float q : {0.f, .37f, .5f, 1.f}) for (float m : {0.f, .23f, .5f, 1.f})
        states.push_back(model.body(m, q));
    std::array<Pole, 6> narrow{{{301, 2}, {997, 2}, {2027, 2}, {3511, 2}, {5701, 2}, {9913, 2}}};
    states.push_back(compile(narrow));
    double worstFft = 0, worstCurve = 0;
    std::size_t checked = 0, largestCurve = 0;
    constexpr std::size_t n = 262144;
    for (const auto& words : states) {
        const auto resolved = resolve(words);
        const auto curve = responseCurve(resolved);
        largestCurve = std::max(largestCurve, curve.size());
        std::size_t segment = 1;
        for (int i = 0; i <= 100000; ++i) {
            const double hz = 20 * std::pow(1000., i / 100000.);
            while (segment + 1 < curve.size() && hz > curve[segment].hz) ++segment;
            const auto a = curve[segment - 1], b = curve[segment];
            const double fraction = std::log(hz / a.hz) / std::log(b.hz / a.hz);
            const double interpolated = a.db + fraction * (b.db - a.db);
            worstCurve = std::max(worstCurve, std::abs(interpolated - cascade_response_db(resolved.cascade, hz, kP2kDatumHz)));
        }
        CascadeRunner runner;
        runner.set_sample_rate(kP2kDatumHz); runner.set_ring_leveller(false);
        runner.set_target(encode_cascade(resolve(library.entries[1].words).cascade));
        runner.set_target(encode_cascade(resolved.cascade));
        std::array<float, 256> settle{}; runner.process(settle);
        std::vector<float> impulse(n); impulse[0] = 1e-8f;
        runner.process(impulse);
        std::vector<std::complex<double>> fft(n);
        for (std::size_t i = 0; i < n; ++i) fft[i] = impulse[i] / static_cast<double>(1e-8f);
        for (std::size_t i = 1, j = 0; i < n; ++i) {
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(fft[i], fft[j]);
        }
        for (std::size_t length = 2; length <= n; length <<= 1) {
            const auto step = std::polar(1., -2 * std::numbers::pi / length);
            for (std::size_t start = 0; start < n; start += length) {
                std::complex<double> phase = 1;
                for (std::size_t j = 0; j < length / 2; ++j) {
                    const auto a = fft[start + j], b = fft[start + j + length / 2] * phase;
                    fft[start + j] = a + b; fft[start + j + length / 2] = a - b; phase *= step;
                }
            }
        }
        double peak = -1000;
        for (const auto& point : curve) peak = std::max(peak, point.db);
        for (std::size_t bin = 119; bin < n * 20000 / 44100; bin += 7) {
            const double hz = bin * kP2kDatumHz / n;
            const double expected = cascade_response_db(resolved.cascade, hz, kP2kDatumHz);
            if (expected < peak - 80) continue;
            const double measured = 20 * std::log10(std::abs(fft[bin]));
            require(std::isfinite(measured), "Nonfinite impulse measurement");
            worstFft = std::max(worstFft, std::abs(measured - expected)); ++checked;
        }
    }
    std::cout << "MEASURE " << states.size() << " states, " << checked << " FFT bins within 80 dB of peak: max error " << worstFft
        << " dB; dense curve error " << worstCurve << " dB; max vertices " << largestCurve << '\n';
    require(checked > 10000 && worstFft < .05, "Settled impulse FFT differs from analytic curve");
    require(worstCurve < .1, "Adaptive curve misses narrow resonance");
}
