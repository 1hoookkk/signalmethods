#include "Audio.h"
#include <algorithm>
#include <cmath>

namespace headspace {

Audio::Audio() {
    runner_.set_sample_rate(trench::core::kP2kDatumHz);
    runner_.set_ring_leveller(false);
    runner_.set_stage_saturation(false, 1.0);
}

void Audio::prepare(double sampleRate) {
    rate_ = sampleRate > 0 ? sampleRate : trench::core::kP2kDatumHz;
    runner_.set_sample_rate(rate_);
    phase_ = 0;
    span_.setFft(kFftOrder);
    span_.setWindow(kWindowType, kFftOrder);
    span_.setRemoveDc(true);
    span_.setAverage(true, .99);
    analysisFill_ = 0;
    publishSpectrum();
}

bool Audio::publish(const Playback& playback) {
    const auto write = write_.load(std::memory_order_relaxed);
    const auto next = (write + 1) % queue_.size();
    if (next == read_.load(std::memory_order_acquire)) return false;
    queue_[write] = playback;
    write_.store(static_cast<unsigned>(next), std::memory_order_release);
    return true;
}

void Audio::publishWords() {
    snapshotSequence_.fetch_add(1);
    for (std::size_t s = 0; s < 6; ++s)
        for (std::size_t w = 0; w < 5; ++w) snapshotWords_[s * 5 + w].store(runningWords_[s][w]);
    snapshotSequence_.fetch_add(1);
}

std::optional<Endpoint> Audio::captureWords() const {
    for (int attempt = 0; attempt < 4; ++attempt) {
        const auto before = snapshotSequence_.load();
        if (!before || (before & 1U)) continue;
        Endpoint words;
        for (std::size_t s = 0; s < 6; ++s)
            for (std::size_t w = 0; w < 5; ++w) words[s][w] = snapshotWords_[s * 5 + w].load();
        if (snapshotSequence_.load() == before) return words;
    }
    return std::nullopt;
}

void Audio::analyse(float sample) {
    analysis_[static_cast<std::size_t>(analysisFill_++)] = sample;
    if (analysisFill_ < span_.windowLength()) return;
    span_.frame(analysis_.data());
    publishSpectrum();
    const int hop = span_.windowLength() / 2;
    std::copy(analysis_.begin() + hop, analysis_.begin() + span_.windowLength(), analysis_.begin());
    analysisFill_ = span_.windowLength() - hop;
}

void Audio::publishSpectrum() {
    const int count = span_.bins();
    const unsigned sequence = spectrumSequence_.load(std::memory_order_relaxed);
    spectrumSequence_.store(sequence + 1, std::memory_order_release);
    std::copy_n(span_.spectrum(), static_cast<std::size_t>(count), spectrum_.begin());
    spectrumCount_.store(count, std::memory_order_relaxed);
    spectrumSequence_.store(sequence + 2, std::memory_order_release);
}

int Audio::spectrumBins(float* out, int capacity) const {
    const unsigned before = spectrumSequence_.load(std::memory_order_acquire);
    if (before & 1U) return 0;
    const int count = std::min(capacity, spectrumCount_.load(std::memory_order_relaxed));
    std::copy_n(spectrum_.begin(), static_cast<std::size_t>(count), out);
    if (spectrumSequence_.load(std::memory_order_acquire) != before) return 0;
    return count;
}

void Audio::process(std::span<float> mono) {
    auto read = read_.load(std::memory_order_relaxed);
    const auto write = write_.load(std::memory_order_acquire);
    if (read != write) {
        const auto previous = current_.state.words;
        const auto previousGain = current_.gain;
        const auto previousStrikes = current_.strikes;
        while (read != write) {
            current_ = queue_[read];
            read = static_cast<unsigned>((read + 1) % queue_.size());
        }
        read_.store(read, std::memory_order_release);
        if (current_.strikeMode) pendingStrike_ += std::max(0., current_.strikes - previousStrikes);
        else pendingStrike_ = 0;
        if (!primed_ || previous != current_.state.words) {
            if (!primed_) {
                runningWords_ = current_.state.words;
                runner_.set_immediate(current_.state.cascade);
            } else {
                glideFrom_ = runningWords_;
                glideRemaining_ = kGlideSamples;
                glideGainFrom_ = std::log(std::max(1e-30, runner_.coefficients()[6][0]));
                glideGainTo_ = std::log(std::max(1e-30, current_.state.cascade[6][0]));
            }
        }
        if (!primed_) gain_ = std::max(1e-30f, current_.gain);
        else if (previousGain != current_.gain) {
            gainStep_ = (std::log(std::max(1e-30f, current_.gain)) - std::log(std::max(1e-30, gain_))) / 256;
            gainRemaining_ = 256;
        }
        primed_ = true;
    }
    if (!primed_) {
        std::fill(mono.begin(), mono.end(), 0.0f);
        return;
    }
    for (auto& sample : mono) {
        const auto value = static_cast<float>((2 * phase_ - 1) * .1);
        phase_ += std::clamp(current_.hz, 1., rate_ * .45) / rate_;
        phase_ -= std::floor(phase_);
        gate_ = std::clamp(gate_ + (current_.playing ? 1. / 256 : -1. / 256), 0., 1.);
        sample = static_cast<float>(value * gate_ * std::clamp(current_.velocity, 0.f, 1.f));
        if (current_.strikeMode) {
            sample = static_cast<float>(.1 * pendingStrike_);
            pendingStrike_ = 0;
        }
        if (glideRemaining_) {
            const int elapsed = kGlideSamples - --glideRemaining_;
            for (std::size_t s = 0; s < runningWords_.size(); ++s)
                for (std::size_t w = 0; w < runningWords_[s].size(); ++w) {
                    const int from = glideFrom_[s][w];
                    const int to = current_.state.words[s][w];
                    runningWords_[s][w] = static_cast<std::uint16_t>(from + (to - from) * elapsed / kGlideSamples);
                }
            auto cascade = glideRemaining_ ? resolve(runningWords_, rate_, false).cascade : current_.state.cascade;
            if (glideRemaining_) cascade[6][0] = std::exp(std::lerp(glideGainFrom_, glideGainTo_, static_cast<double>(elapsed) / kGlideSamples));
            runner_.set_immediate(cascade);
        }
        runner_.process(std::span<float>(&sample, 1));
    }
    float peak = 0;
    for (auto& sample : mono) {
        analyse(sample);
        if (gainRemaining_) {
            gain_ *= std::exp(gainStep_);
            if (--gainRemaining_ == 0) gain_ = current_.gain;
        }
        const double unprotected = sample * gain_;
        if (!std::isfinite(unprotected)) { sample = 0; protection_ = 0; continue; }
        const double ceiling = 1.0;
        const double needed = std::min(1., ceiling / std::max(ceiling, std::abs(unprotected)));
        protection_ = std::min(needed, protection_ + (1 - protection_) * .0005);
        sample = static_cast<float>(unprotected * protection_);
        peak = std::max(peak, std::abs(sample));
        sample = std::clamp(sample, -1.0f, 1.0f);
    }
    peak_.store(peak, std::memory_order_relaxed);
    if (!mono.empty()) publishWords();
}

}
