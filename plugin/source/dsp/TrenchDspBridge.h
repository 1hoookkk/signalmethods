#pragma once
#include "DeskDrive.h"
#include "DriveLaw.h"
#include "KeySnap.h"

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
#include "SlamStage.h"
#if TRENCH_DEV_PANEL
#include "../parameters/DevCalibration.h"
#endif

struct TrenchParams
{
    float morph = 0.0f;
    float q = 0.0f;
    int keySnap = 0;
    float poleDistortion = 0.0f;
    float biteAxis = 0.0f;
    bool noteLatched = false;
    float noteTrackRatio = 1.0f;
    float noteBite = 0.0f;
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
        bool saturate = false;
        bool dcBlock = false;
        bool x3Movement = true;
        bool mackity = true;
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
#if TRENCH_DEV_PANEL
        calibrationValid = false;
#endif
        inputGain.reset (sampleRateHz, 0.005);
        inputGain.setCurrentAndTargetValue (1.0f);
        outputGain.reset (sampleRateHz, 0.005);
        outputGain.setCurrentAndTargetValue (1.0f);
        preDeskL.prepare (sampleRateHz);
        preDeskR.prepare (sampleRateHz);
        for (auto* desk : { &preDeskL, &preDeskR })
            desk->setTrims (kSlamInTrim, kSlamOutPad);
        postDeskL.prepare (sampleRateHz);
        postDeskR.prepare (sampleRateHz);
        postDeskL.setEnabled (outputStageOn);
        postDeskR.setEnabled (outputStageOn);
        monoScratch.assign ((size_t) std::max (1, maxBlockSize), 0.0f);
        left = trench::core::CascadeRunner {};
        right = trench::core::CascadeRunner {};
        left.set_sample_rate (sampleRateHz);
        right.set_sample_rate (sampleRateHz);
        left.reset();
        right.reset();
        smoothedMorph = -1.0f;
        smoothedQ = -1.0f;
        controlTick = juce::jmax (1, (int) std::lround (sampleRateHz * 88.0 / 44100.0));
        fadeLeft.resize ((size_t) std::max (128, controlTick));
        fadeRight.resize (fadeLeft.size());
        bodyFadeLength = std::max (1, (int) std::lround (sampleRateHz * 0.010));
        bodyFadeRemaining = 0;
        heardGeneration = 0;
        left.set_feedback_ceiling (kStateCeiling);
        right.set_feedback_ceiling (kStateCeiling);
        if (! sourceBytes.empty())
            publishSnapshot (sourceBytes, sourceDatumRate);
    }

    void setRingLeveller (bool enabled) noexcept
    {
        ringEnabled = enabled;
        left.set_ring_leveller (enabled);
        right.set_ring_leveller (enabled);
    }

#if TRENCH_DEV_PANEL
    void applyCalibration (const trench::calibration::Values& v)
    {
        if (calibrationValid && calibrationValues == v) return;
        if (! calibrationValid || v[0] != calibrationValues[0] || v[1] != calibrationValues[1] || v[2] != calibrationValues[2])
            heardGeneration = 0;
        calibrationValues = v;
        calibrationValid = true;
        for (auto* runner : { &left, &right })
        {
            runner->set_feedback_ceiling (std::pow (10.0, v[6] / 20.0));
            runner->set_ring_leveller (v[7] > 0.5f);
            runner->set_ring_calibration (v[8], v[9], v[10], v[11]);
        }
        outputStageOn = v[12] > 0.5f;
        postDeskL.setEnabled (outputStageOn);
        postDeskR.setEnabled (outputStageOn);
        postDeskL.setBypassSaturation (v[13] < 0.5f);
        postDeskR.setBypassSaturation (v[13] < 0.5f);
        setDeskCoupling (v[15]);
        postClipOn = v[19] > 0.5f;
        postClipKnee = v[17];
        postClipCeiling = std::pow (10.0f, v[18] / 20.0f);
    }
    double declaredDatumForCalibration() const noexcept { return reportedDatum.load(); }
    float preDeskPeakForCalibration() const noexcept { return reportedPreDesk.load(); }
    float postDeskPeakForCalibration() const noexcept { return reportedPostDesk.load(); }
#endif
    static constexpr double kStateCeiling = 1.9952623;

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
                                 double datumRate = kBodyDatumRate, int keyChoice = 0)
    {
        if (bytes == nullptr || outCoefficients == nullptr)
            return false;
        try
        {
            const auto body = trench::core::PackedBody::from_body_bytes (std::span {
                static_cast<const std::uint8_t*> (bytes), len });
            auto cascade = cascadeAt (body.interpolate_words (juce::jlimit (0.0f, 1.0f, morph), juce::jlimit (0.0f, 1.0f, q), 0.0f),
                                      datumRate, runtimeRate);
            cascade = trench::KeySnap::apply (cascade, keyChoice, runtimeRate, nullptr, 1.0);
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
        Snapshot* snapshot = activeSnapshot.load (std::memory_order_seq_cst);
        if (snapshot == nullptr)
            return;
        const int channels = std::min (2, buffer.getNumChannels());
        const int samples = buffer.getNumSamples();
        if (channels <= 0 || samples <= 0)
            return;
        const double keyRatio = 1.0;
        const int keyChoice = trench::KeySnap::active (params.keySnap) ? params.keySnap : 0;
        if (keyChoice != heardKeyChoice)
        {
            keyLanes = {};
            heardKeyChoice = keyChoice;
        }
        const double bite = (double) juce::jlimit (0.0f, 1.0f, params.poleDistortion);
        left.set_pole_distortion (bite);
        right.set_pole_distortion (bite);
        left.set_stage_saturation (false, 1.0);
        right.set_stage_saturation (false, 1.0);
        int caught = 0;
#if TRENCH_DEV_PANEL
        float preDeskPeak = 0.0f, postDeskPeak = 0.0f;
#endif
        const bool switched = snapshot->generation != heardGeneration && bodyFadeRemaining == 0;
        if (switched)
        {
            if (heardGeneration != 0)
            {
                outgoingLeft = left;
                outgoingRight = right;
                outgoingLeft.zero_kernel_deltas();
                outgoingRight.zero_kernel_deltas();
                bodyFadeRemaining = bodyFadeLength;
            }
            audioSnapshot = *snapshot;
            heardGeneration = snapshot->generation;
            keyLanes = {};
            for (auto* runner : { &left, &right })
            {
                *runner = trench::core::CascadeRunner {};
                runner->set_sample_rate (sampleRateHz);
                runner->set_feedback_ceiling (kStateCeiling);
                runner->set_ring_leveller (ringEnabled);
                runner->set_pole_distortion (bite);
#if TRENCH_DEV_PANEL
                runner->set_feedback_ceiling (std::pow (10.0, calibrationValues[6] / 20.0));
                runner->set_ring_leveller (calibrationValues[7] > 0.5f);
                runner->set_ring_calibration (calibrationValues[8], calibrationValues[9], calibrationValues[10], calibrationValues[11]);
#endif
            }
        }
        snapshot = &audioSnapshot;
        float* outL = buffer.getWritePointer (0);
        float* outR = channels > 1 ? buffer.getWritePointer (1) : nullptr;
#if TRENCH_DEV_PANEL
        const int kBlockSize = juce::jlimit (1, 128, (int) calibrationValues[1]);
#else
        const int kBlockSize = controlTick;
#endif
        for (int blockStart = 0; blockStart < samples; blockStart += kBlockSize)
        {
            const int blockLen = std::min (kBlockSize, samples - blockStart);
            const int targetIdx = blockStart + blockLen - 1;
            const bool first = switched && blockStart == 0;
            const float morphTarget = juce::jlimit (0.0f, 1.0f, morphPerSample[targetIdx]);
            const float qTarget = juce::jlimit (0.0f, 1.0f, params.q);
            const float glide = 1.0f - (float) std::pow (1.0 - kControlGlide, (double) blockLen / (double) controlTick);
            smoothedMorph = first || smoothedMorph < 0.0f ? morphTarget : smoothedMorph + glide * (morphTarget - smoothedMorph);
            smoothedQ = first || smoothedQ < 0.0f ? qTarget : smoothedQ + glide * (qTarget - smoothedQ);
            if (std::abs (morphTarget - smoothedMorph) < 1.0e-6f) smoothedMorph = morphTarget;
            if (std::abs (qTarget - smoothedQ) < 1.0e-6f) smoothedQ = qTarget;
            const float morph = smoothedMorph;
            const float q = smoothedQ;
            if (first || morph != cachedMorph || q != cachedQ || keyRatio != cachedKeyRatio || keyChoice != 0 || keyWasActive)
            {
                bool changed = first || keyRatio != cachedKeyRatio;
                if (first || morph != cachedMorph || q != cachedQ)
                {
                    const auto words = snapshot->bank.interpolate_words (morph, q, 0.0f);
                    if (first || words != cachedWords)
                    {
                        cachedWords = words;
                        cachedBase = cascadeAt (words, snapshot->datumRate, sampleRateHz);
                        changed = true;
                    }
                }
                if (changed)
                    cachedCascade = keyRatio != 1.0 ? trench::core::transpose_cascade (cachedBase, keyRatio, sampleRateHz) : cachedBase;
                cachedMorph = morph;
                cachedQ = q;
                cachedKeyRatio = keyRatio;
                if (keyChoice != 0 || keyWasActive)
                    changed = true;
                keyWasActive = keyChoice != 0;
                heardCascade = keyChoice != 0
                    ? trench::KeySnap::apply (cachedCascade, keyChoice, sampleRateHz, &keyLanes,
                                              first ? 1.0 : trench::KeySnap::glideFor (blockLen, sampleRateHz))
                    : cachedCascade;
                if (changed)
                {
#if TRENCH_DEV_PANEL
                    if (first || calibrationValues[2] < 0.5f)
                    {
                        left.set_immediate (heardCascade);
                        right.set_immediate (heardCascade);
                    }
                    else
#else
                    if (first)
                    {
                        left.set_immediate (heardCascade);
                        right.set_immediate (heardCascade);
                    }
                    else
#endif
                    {
                        const auto encoded = trench::core::encode_cascade (heardCascade);
                        left.set_target (encoded, (size_t) blockLen);
                        right.set_target (encoded, (size_t) blockLen);
                    }
                }
                else
                {
                    left.zero_kernel_deltas();
                    right.zero_kernel_deltas();
                }
            }
            else
            {
                left.zero_kernel_deltas();
                right.zero_kernel_deltas();
            }
            for (int s = 0; s < blockLen; ++s)
            {
                const int sample = blockStart + s;
                const float gain = inputGain.getNextValue();
                outL[sample] = preDeskL.process (outL[sample] * gain);
                if (outR != nullptr)
                    outR[sample] = preDeskR.process (outR[sample] * gain);
            }
            const int fadeSamples = std::min (blockLen, bodyFadeRemaining);
            if (fadeSamples > 0)
            {
                std::copy_n (outL + blockStart, fadeSamples, fadeLeft.data());
                outgoingLeft.process (std::span<float> (fadeLeft.data(), (size_t) fadeSamples));
                if (outR != nullptr)
                {
                    std::copy_n (outR + blockStart, fadeSamples, fadeRight.data());
                    outgoingRight.process (std::span<float> (fadeRight.data(), (size_t) fadeSamples));
                }
            }
            left.process (std::span<float> (outL + blockStart, (size_t) blockLen));
            if (outR != nullptr)
                right.process (std::span<float> (outR + blockStart, (size_t) blockLen));
            for (int s = 0; s < blockLen; ++s)
            {
                const int sample = blockStart + s;
                if (s < fadeSamples)
                {
                    const float mix = (float) (bodyFadeLength - bodyFadeRemaining + 1) / (float) bodyFadeLength;
                    outL[sample] = fadeLeft[(size_t) s] * (1.0f - mix) + outL[sample] * mix;
                    if (outR != nullptr)
                        outR[sample] = fadeRight[(size_t) s] * (1.0f - mix) + outR[sample] * mix;
                    --bodyFadeRemaining;
                }
#if TRENCH_DEV_PANEL
                preDeskPeak = std::max (preDeskPeak, outR != nullptr ? std::max (std::abs (outL[sample]), std::abs (outR[sample])) : std::abs (outL[sample]));
#endif
                const float level = outputGain.getNextValue();
                outL[sample] = outputStage (postDeskL, postClip (outL[sample])) * level;
                if (outR != nullptr)
                    outR[sample] = outputStage (postDeskR, postClip (outR[sample])) * level;
                caught += clipEngaged (outL[sample]) || (outR != nullptr && clipEngaged (outR[sample])) ? 1 : 0;
#if TRENCH_DEV_PANEL
                    postDeskPeak = std::max (postDeskPeak, outR != nullptr ? std::max (std::abs (outL[sample]), std::abs (outR[sample])) : std::abs (outL[sample]));
#endif
            }
        }
        publishCascade (keyWasActive ? heardCascade : cachedCascade);
        caughtFraction = (float) caught / (float) samples;
#if TRENCH_DEV_PANEL
        reportedDatum.store (snapshot->datumRate);
        reportedPreDesk.store (preDeskPeak);
        reportedPostDesk.store (postDeskPeak);
#endif
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
    void setInputDrive (float gain) noexcept
    {
        inputGain.setTargetValue (std::clamp (gain, 0.0f, 16.0f));
    }
    bool inputDriveIsUnity() const noexcept
    {
        return inputGain.getCurrentValue() == 1.0f && inputGain.getTargetValue() == 1.0f;
    }
    void setInputSlam (bool on) noexcept
    {
        preDeskL.setEnabled (on);
        preDeskR.setEnabled (on);
    }
    void setOutputLevel (float gain) noexcept
    {
        outputGain.setTargetValue (std::clamp (gain, 0.0f, 16.0f));
    }
    void setDeskCoupling (float hz)
    {
        for (auto* desk : { &postDeskL, &postDeskR })
            desk->setOutputCoupling ((double) hz);
    }
    float caughtFractionForUi() const noexcept { return caughtFraction; }
    float gritActivity() const noexcept
    {
        return (float) std::max (left.grit_activity(), right.grit_activity());
    }
    double tailSeconds() const noexcept
    {
        return reportedTailSeconds.load (std::memory_order_relaxed);
    }
    void publishUiSnapshot() noexcept {}

    bool readUiSnapshot (float* outCoefficients, float& outBoost) const noexcept
    {
        if (outCoefficients == nullptr || uiSnapshotBusy.test_and_set (std::memory_order_acquire))
            return false;
        std::copy (uiCoefficients.begin(), uiCoefficients.end(), outCoefficients);
        uiSnapshotBusy.clear (std::memory_order_release);
        outBoost = 1.0f;
        return activeSnapshot.load (std::memory_order_acquire) != nullptr;
    }

    void setInputMode (int) noexcept {}
    void setSpatialMode (int) noexcept {}
    void setQSoundFallbackPan (float) noexcept {}
    double takePeakState() noexcept { return std::max (left.take_peak_state(), right.take_peak_state()); }
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

    static double transposeRatio (bool noteLatched, float noteTrackRatio, int keySnap) noexcept
    {
        if (noteLatched)
            return (double) noteTrackRatio;
        if (keySnap > 0)
            return keySnapRatio (keySnap);
        return 1.0;
    }

    static double transposeRatio (const TrenchParams& params) noexcept
    {
        return transposeRatio (params.noteLatched, params.noteTrackRatio, params.keySnap);
    }
private:
    struct Snapshot
    {
        trench::core::PackedBody bank {};
        double datumRate = 0.0;
        std::uint64_t generation = 0;
    };

    static trench::core::Cascade cascadeAt (const trench::core::CornerWords& words, double datumRate, double runtimeRate)
    {
        if (datumRate > 0.0 && runtimeRate > 0.0 && ! juce::approximatelyEqual (datumRate, runtimeRate))
            return trench::core::native::rewarp_cascade (words, datumRate, runtimeRate);
        trench::core::Cascade out {};
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = trench::core::section_words_to_biquad (words[i]);
        return out;
    }

    bool publishSnapshot (const std::vector<std::uint8_t>& bytes, double datumRate)
    {
        try
        {
            auto next = std::make_unique<Snapshot>();
            next->bank = trench::core::PackedBody::from_body_bytes (bytes);
            next->datumRate = datumRate;
            next->generation = ++publishedGeneration;
            publishCascade (cascadeAt (next->bank.interpolate_words (0.0f, 0.0f, 0.0f), datumRate, sampleRateHz));
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

#if TRENCH_DEV_PANEL
    float postClip (float x) const noexcept { return postClipOn ? trench::calibration::guard (x, postClipKnee, postClipCeiling) : x; }
    bool clipEngaged (float x) const noexcept { return postClipOn && (! std::isfinite (x) || std::abs (x) > postClipKnee * postClipCeiling); }
#else
    static float postClip (float x) noexcept { return x; }
    static bool clipEngaged (float x) noexcept { return ! std::isfinite (x) || std::abs (x) > 1.0f; }
#endif
    float outputStage (trench::DeskDrive& desk, float x) const noexcept
    {
        if (! bypass.mackity || ! desk.isActive())
            return x;
        return desk.process (x);
    }

    void publishCascade (const trench::core::Cascade& cascade) noexcept
    {
        if (uiSnapshotBusy.test_and_set (std::memory_order_acquire))
            return;
        size_t index = 0;
        for (const auto& section : cascade)
            for (const auto coefficient : section)
                uiCoefficients[index++] = (float) coefficient;
        double radius = 0.0;
        for (const auto& section : cascade)
            radius = std::max (radius, std::sqrt (std::max (0.0, section[4])));
        const double tail = radius > 0.0 && radius < 1.0
            ? std::clamp (std::log (1000.0) / (-std::log (radius)) / sampleRateHz, 0.0, 4.0)
            : 0.0;
        reportedTailSeconds.store ((float) tail, std::memory_order_relaxed);
        uiSnapshotBusy.clear (std::memory_order_release);
    }

    std::atomic<int> inFlight { 0 };
    std::atomic<bool> retired { false };
    std::vector<std::uint8_t> sourceBytes;
    std::vector<float> monoScratch;
    std::atomic<Snapshot*> activeSnapshot { nullptr };
    std::vector<std::unique_ptr<Snapshot>> graveyard;
    trench::core::CascadeRunner left;
    trench::core::CascadeRunner right;
    trench::core::CascadeRunner outgoingLeft, outgoingRight;
    Snapshot audioSnapshot;
    std::vector<float> fadeLeft, fadeRight;
    int bodyFadeRemaining = 0, bodyFadeLength = 1;
    bool ringEnabled = true;
    std::uint64_t publishedGeneration = 0;
    std::uint64_t heardGeneration = 0;
    float cachedMorph = -1.0f;
    float smoothedMorph = -1.0f, smoothedQ = -1.0f;
    static constexpr double kControlGlide = 0.4509729743;
    int controlTick = 88;
    float cachedQ = -1.0f;
    double cachedKeyRatio = 0.0;
    trench::core::Cascade cachedCascade {};
    trench::core::Cascade heardCascade {};
    trench::KeySnap::Lanes keyLanes {};
    int heardKeyChoice = 0;
    bool keyWasActive = false;
    trench::core::Cascade cachedBase {};
    trench::core::CornerWords cachedWords {};
    std::array<float, trench::kUiCoeffCount> uiCoefficients {};
    mutable std::atomic_flag uiSnapshotBusy = ATOMIC_FLAG_INIT;
    static_assert (std::atomic<float>::is_always_lock_free);
    std::atomic<float> reportedTailSeconds { 0.0f };
    double sampleRateHz = 48'000.0;
    double sourceDatumRate = kBodyDatumRate;
    juce::SmoothedValue<float> inputGain { 1.0f };
    juce::SmoothedValue<float> outputGain { 1.0f };
    trench::DeskDrive preDeskL, preDeskR;
    static constexpr double kSlamInTrim = 0.4;
    static constexpr double kSlamOutPad = 0.5;
    float caughtFraction = 0.0f;
    bool outputStageOn = false;
    trench::DeskDrive postDeskL, postDeskR;
    Bypass bypass;
#if TRENCH_DEV_PANEL
    trench::calibration::Values calibrationValues = trench::calibration::defaults();
    bool calibrationValid = false;
    bool postClipOn = true;
    float postClipKnee = trench::kGuardLinearZone;
    float postClipCeiling = trench::kFinalSafetyCeiling;
    std::atomic<double> reportedDatum { kBodyDatumRate };
    std::atomic<float> reportedPreDesk { 0 }, reportedPostDesk { 0 };
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrenchDspBridge)
};
