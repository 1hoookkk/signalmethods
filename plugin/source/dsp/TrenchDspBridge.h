#pragma once
#include "DeskDrive.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <trench/core/audition.hpp>
#include <trench/core/native_body.hpp>
#include <trench/core/packed_body.hpp>
#include <trench/core/transpose.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

struct TrenchParams
{
    float morph = 0.0f;
    float q = 0.0f;
    int keySnap = 0;
    float poleDistortion = 0.0f;
};

namespace trench
{
inline constexpr int kUiStageCount = (int) core::kSectionCount;
inline constexpr int kUiCoeffsPerStage = (int) core::kCoefficientCount;
inline constexpr int kUiCoeffCount = kUiStageCount * kUiCoeffsPerStage;
}

inline int trench_engine_coeff_count() noexcept { return trench::kUiCoeffCount; }

class TrenchDspBridge
{
public:
    static constexpr double kBodyDatumRate = trench::core::kP2kDatumHz;

    struct Bypass
    {
        bool nonlinearity = false;
        bool agc = true;
        float agcDrive = 1.0f;
        bool saturate = false;
        bool dcBlock = false;
        bool x3Movement = true;
        bool operator== (const Bypass&) const noexcept = default;
    };

    TrenchDspBridge()
    {
        liveEngines().fetch_add (1, std::memory_order_relaxed);
        for (int section = 0; section < trench::kUiStageCount; ++section)
            uiCoefficients[(size_t) section * trench::kUiCoeffsPerStage] = 1.0f;
    }

    ~TrenchDspBridge()
    {
        retire();
        delete activeSnapshot.exchange (nullptr, std::memory_order_acq_rel);
    }

    class AudioScope
    {
    public:
        explicit AudioScope (TrenchDspBridge& owner) noexcept : bridge (owner)
        {
            bridge.inFlight.fetch_add (1, std::memory_order_seq_cst);
        }
        ~AudioScope() noexcept
        {
            bridge.inFlight.fetch_sub (1, std::memory_order_release);
        }
    private:
        TrenchDspBridge& bridge;
        JUCE_DECLARE_NON_COPYABLE (AudioScope)
    };

    void retire() noexcept
    {
        if (retired.exchange (true, std::memory_order_seq_cst))
            return;
        if (inFlight.load (std::memory_order_seq_cst) == 0)
            liveEngines().fetch_sub (1, std::memory_order_relaxed);
        else
            leakedEngines().fetch_add (1, std::memory_order_relaxed);
    }

    static std::atomic<int>& liveEngines() noexcept
    {
        static std::atomic<int> count { 0 };
        return count;
    }

    static std::atomic<int>& leakedEngines() noexcept
    {
        static std::atomic<int> count { 0 };
        return count;
    }

    void prepare (double sampleRate, int maxBlockSize)
    {
        sampleRateHz = sampleRate > 0.0 ? sampleRate : 48'000.0;
        deskL.prepare (sampleRateHz);
        deskR.prepare (sampleRateHz);
        monoScratch.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
        left.reset();
        right.reset();
        tickPhase = 0;
        if (! sourceBytes.empty())
            publishSnapshot (sourceBytes, sourceDatumRate);
    }

    void setBypass (const Bypass& value) noexcept { bypass = value; }
    Bypass getBypass() const noexcept { return bypass; }

    bool loadCartridge (const juce::String&) { return false; }

    bool loadCartridgeBytes (const void* bytes, size_t len, double datumRate = kBodyDatumRate)
    {
        if (bytes == nullptr || (len != trench::core::kLegacyBodyBytes
                                 && len != trench::core::kNativeBodyBytes))
            return false;
        const auto* first = static_cast<const std::uint8_t*> (bytes);
        std::vector<std::uint8_t> candidate (first, first + len);
        if (! publishSnapshot (candidate, datumRate))
            return false;
        sourceBytes.swap (candidate);
        sourceDatumRate = datumRate;
        return true;
    }

    bool loadCartridgeBytes (const juce::MemoryBlock& bytes)
    {
        return loadCartridgeBytes (bytes.getData(), bytes.getSize());
    }

    bool reloadCartridgeBytes (const void* bytes, size_t len, double datumRate = kBodyDatumRate)
    {
        return loadCartridgeBytes (bytes, len, datumRate);
    }

    bool reloadCartridgeBytes (const juce::MemoryBlock& bytes)
    {
        return reloadCartridgeBytes (bytes.getData(), bytes.getSize());
    }

    bool loadRuntimePresetBank (const juce::String&, const std::vector<unsigned short>&,
                                int, double, double = 1.0)
    {
        return false;
    }

    static bool bodyBytesFromJson (const juce::String&, juce::MemoryBlock&) { return false; }
    static bool compileTypedBody (const double*, int, juce::MemoryBlock&) { return false; }

    static bool probePackedBody (const void* bytes, size_t len, float morph, float q,
                                 double runtimeRate,
                                 float outCoefficients[trench::kUiCoeffCount], float& outBoost,
                                 double datumRate = kBodyDatumRate, double keyRatio = 1.0)
    {
        if (bytes == nullptr || outCoefficients == nullptr)
            return false;
        try
        {
            const auto body = trench::core::PackedBody::from_body_bytes (std::span {
                static_cast<const std::uint8_t*> (bytes), len });
            auto cascade = cascadeAt (body, datumRate, runtimeRate,
                                      juce::jlimit (0.0f, 1.0f, morph), juce::jlimit (0.0f, 1.0f, q));
            if (keyRatio > 0.0 && keyRatio != 1.0)
                cascade = trench::core::transpose_cascade (cascade, keyRatio, runtimeRate);
            int index = 0;
            for (const auto& section : cascade)
                for (const double coefficient : section)
                {
                    if (! std::isfinite (coefficient))
                        return false;
                    outCoefficients[index++] = (float) coefficient;
                }
            outBoost = 1.0f;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void process (juce::AudioBuffer<float>& buffer, const TrenchParams& params)
    {
        if (monoScratch.empty())
            return;
        const int total = buffer.getNumSamples();
        for (int start = 0; start < total; start += (int) monoScratch.size())
        {
            const int n = juce::jmin ((int) monoScratch.size(), total - start);
            std::fill (monoScratch.begin(), monoScratch.begin() + n, params.morph);
            float* chans[2] = { buffer.getWritePointer (0) + start,
                                buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) + start
                                                            : nullptr };
            juce::AudioBuffer<float> slice (chans, juce::jmin (2, buffer.getNumChannels()), n);
            processTrajectory (slice, monoScratch.data(), params);
        }
    }

    void processTrajectory (juce::AudioBuffer<float>& buffer, const float* morphPerSample,
                            const TrenchParams& params)
    {
        AudioScope scope (*this);
        if (retired.load (std::memory_order_relaxed) || morphPerSample == nullptr)
            return;
        const Snapshot* snapshot = activeSnapshot.load (std::memory_order_seq_cst);
        if (snapshot == nullptr)
            return;
        const int channels = std::min (2, buffer.getNumChannels());
        const int samples = buffer.getNumSamples();
        if (channels <= 0 || samples <= 0)
            return;
        const double keyRatio = transposeRatio (params);
        const float q = juce::jlimit (0.0f, 1.0f, params.q);
        const double bite = (double) juce::jlimit (0.0f, 1.0f, params.poleDistortion);
        left.set_pole_distortion (bite);
        right.set_pole_distortion (bite);
        const bool switched = snapshot->generation != heardGeneration;
        heardGeneration = snapshot->generation;
        float* outL = buffer.getWritePointer (0);
        float* outR = channels > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int sample = 0; sample < samples; ++sample)
        {
            const bool first = switched && sample == 0;
            const bool tick = tickPhase == 0;
            tickPhase = (tickPhase + 1) % kTickSamples;
            if (first || tick)
            {
                const float morph = juce::jlimit (0.0f, 1.0f, morphPerSample[sample]);
                if (first || morph != cachedMorph || q != cachedQ || keyRatio != cachedKeyRatio)
                {
                    cachedCascade = cascadeAt (snapshot->bank, snapshot->datumRate, snapshot->runtimeRate, morph, q);
                    if (keyRatio != 1.0)
                        cachedCascade = trench::core::transpose_cascade (cachedCascade, keyRatio, sampleRateHz);
                    cachedMorph = morph;
                    cachedQ = q;
                    cachedKeyRatio = keyRatio;
                    if (first)
                    {
                        const auto encoded = trench::core::encode_cascade (cachedCascade);
                        left.set_target (encoded);
                        right.set_target (encoded);
                    }
                    else
                    {
                        left.set_glide (cachedCascade, kTickSamples);
                        right.set_glide (cachedCascade, kTickSamples);
                    }
                }
            }
            outL[sample] = deskL.process (outL[sample], inputDrive);
            left.process (std::span<float> (outL + sample, 1));
            if (outR != nullptr)
            {
                outR[sample] = deskR.process (outR[sample], inputDrive);
                right.process (std::span<float> (outR + sample, 1));
            }
        }
        publishCascade (cachedCascade);
        if (bypass.saturate)
            for (int channel = 0; channel < channels; ++channel)
                for (int sample = 0; sample < samples; ++sample)
                    buffer.getWritePointer (channel)[sample] = std::tanh (buffer.getWritePointer (channel)[sample]);
    }

    void reclaim() noexcept
    {
        if (inFlight.load (std::memory_order_seq_cst) == 0)
            graveyard.clear();
    }
    void setInputPreamp (float drive) noexcept
    {
        inputDrive = std::clamp (drive, 0.0f, 1.0f);
        const bool on = inputDrive > 0.001f;
        deskL.setEnabled (on);
        deskR.setEnabled (on);
    }
    float gritActivity() const noexcept
    {
        return (float) std::max (left.grit_activity(), right.grit_activity());
    }
    float agcReductionDb() const noexcept { return 0.0f; }
    double tailSeconds() const noexcept
    {
        double radius = 0.0;
        for (int section = 0; section < trench::kUiStageCount; ++section)
            radius = std::max (radius, std::sqrt (std::max (0.0, (double) uiCoefficients[(size_t) (section * trench::kUiCoeffsPerStage + 4)])));
        if (radius <= 0.0 || radius >= 1.0)
            return 0.0;
        return std::clamp (std::log (1000.0) / (-std::log (radius)) / sampleRateHz, 0.0, 4.0);
    }
    void publishUiSnapshot() noexcept {}

    bool readUiSnapshot (float* outCoefficients, float& outBoost) const noexcept
    {
        if (outCoefficients == nullptr)
            return false;
        std::copy (uiCoefficients.begin(), uiCoefficients.end(), outCoefficients);
        outBoost = 1.0f;
        return activeSnapshot.load (std::memory_order_acquire) != nullptr;
    }

    void setInputMode (int) noexcept {}
    void setSpatialMode (int) noexcept {}
    void setQSoundFallbackPan (float) noexcept {}
    void setAgcEnabled (bool enabled) noexcept { bypass.agc = enabled; }
    void setAgcDrive (float drive) noexcept { bypass.agcDrive = std::max (1.0f, drive); }
    void setSaturationEnabled (bool enabled) noexcept { bypass.saturate = enabled; }

public:
    static double keySnapRatio (int choice) noexcept
    {
        if (choice <= 0 || choice > 24)
            return 1.0;
        const int root = (choice - 1) % 12;
        const int semitones = root <= 6 ? root : root - 12;
        return trench::core::ratio_of_semitones ((double) semitones);
    }

    static double transposeRatio (const TrenchParams& params) noexcept
    {
        if (params.keySnap > 0)
            return keySnapRatio (params.keySnap);
        return 1.0;
    }
private:
    struct Snapshot
    {
        trench::core::PackedBody bank {};
        double datumRate = 0.0;
        double runtimeRate = 0.0;
        std::uint64_t generation = 0;
    };

    static trench::core::Cascade cascadeAt (const trench::core::PackedBody& bank, double datumRate,
                                            double runtimeRate, float morph, float q)
    {
        if (datumRate > 0.0 && runtimeRate > 0.0 && ! juce::approximatelyEqual (datumRate, runtimeRate))
            return trench::core::native::rewarp_cascade (bank.interpolate_words (morph, q, 0.0f),
                                                         datumRate, runtimeRate);
        return bank.interpolate_biquads (morph, q, 0.0f);
    }

    bool publishSnapshot (const std::vector<std::uint8_t>& bytes, double datumRate)
    {
        try
        {
            const auto packed = trench::core::PackedBody::from_body_bytes (bytes);
            auto next = std::make_unique<Snapshot>();
            next->bank = packed;
            next->datumRate = datumRate;
            next->runtimeRate = sampleRateHz;
            next->generation = ++publishedGeneration;
            publishCascade (cascadeAt (next->bank, datumRate, sampleRateHz, 0.0f, 0.0f));
            retireSnapshot (next.release());
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void retireSnapshot (Snapshot* next)
    {
        Snapshot* old = activeSnapshot.exchange (next, std::memory_order_seq_cst);
        if (old != nullptr)
            graveyard.emplace_back (old);
        reclaim();
    }

    void publishCascade (const trench::core::Cascade& cascade) noexcept
    {
        size_t index = 0;
        for (const auto& section : cascade)
            for (const auto coefficient : section)
                uiCoefficients[index++] = (float) coefficient;
    }

    std::atomic<int> inFlight { 0 };
    std::atomic<bool> retired { false };
    std::vector<std::uint8_t> sourceBytes;
    std::vector<float> monoScratch;
    std::atomic<Snapshot*> activeSnapshot { nullptr };
    std::vector<std::unique_ptr<Snapshot>> graveyard;
    trench::core::CascadeRunner left;
    trench::core::CascadeRunner right;
    static constexpr int kTickSamples = 32;
    int tickPhase = 0;
    std::uint64_t publishedGeneration = 0;
    std::uint64_t heardGeneration = 0;
    float cachedMorph = -1.0f;
    float cachedQ = -1.0f;
    double cachedKeyRatio = 0.0;
    trench::core::Cascade cachedCascade {};
    std::array<float, trench::kUiCoeffCount> uiCoefficients {};
    double sampleRateHz = 48'000.0;
    double sourceDatumRate = kBodyDatumRate;
    float inputDrive = 0.0f;
    trench::DeskDrive deskL, deskR;
    Bypass bypass;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrenchDspBridge)
};
