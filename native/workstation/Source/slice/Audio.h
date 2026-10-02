#pragma once

#include "Model.h"
#include "Span.h"
#include "trench/core/audition.hpp"
#include <atomic>
#include <span>
#include <optional>

namespace headspace {

struct Playback {
    Resolved state;
    bool playing{};
    float gain{};
    double hz{110};
    float velocity{1};
    bool strikeMode{};
    double strikes{};
};

class Audio {
public:
    static constexpr int kFftOrder = 4096;
    static constexpr int kWindowType = 1;
    static constexpr int kGlideSamples = 64;

    Audio();
    void prepare(double sampleRate);
    bool publish(const Playback& playback);
    void process(std::span<float> mono);
    const Endpoint& consumedWords() const { return current_.state.words; }
    const Endpoint& runningWords() const { return runningWords_; }
    std::optional<Endpoint> captureWords() const;
    const trench::core::Cascade& coefficients() const { return runner_.coefficients(); }
    float peak() const { return peak_.load(std::memory_order_relaxed); }
    double sampleRate() const { return rate_; }
    int spectrumBins(float* out, int capacity) const;
    int spectrumBinCount() const { return spectrumCount_.load(std::memory_order_relaxed); }
    double spectrumFullScaleDb() const { return span_.fullScaleDb(); }
private:
    void analyse(float sample);
    void publishSpectrum();
    void publishWords();
    std::array<Playback, 8> queue_{};
    std::atomic<unsigned> read_{};
    std::atomic<unsigned> write_{};
    std::atomic<float> peak_{};
    Playback current_{};
    bool primed_{};
    Endpoint runningWords_{}, glideFrom_{};
    int glideRemaining_{};
    double glideGainFrom_{}, glideGainTo_{};
    double pendingStrike_{};
    std::array<std::atomic<std::uint16_t>, 30> snapshotWords_{};
    std::atomic<unsigned> snapshotSequence_{};
    double gain_{}, gainStep_{}, gate_{}, protection_{1};
    std::size_t gainRemaining_{};
    trench::core::CascadeRunner runner_;
    double phase_{};
    double rate_{trench::core::kP2kDatumHz};
    Span span_;
    std::array<float, Span::kMaxOrder> analysis_{};
    std::array<float, Span::kMaxBins> spectrum_{};
    std::atomic<unsigned> spectrumSequence_{};
    std::atomic<int> spectrumCount_{};
    int analysisFill_{};
};

}
