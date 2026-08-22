#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>
extern "C"
{
    void* trench_engine_create();
    void trench_engine_destroy (void* engine);
    void trench_engine_prepare (void* engine, double sampleRate);
    int trench_engine_load_cartridge (void* engine, const char* json);
    int trench_engine_load_body_bytes (void* engine, const unsigned char* bytes, size_t len);
    // Geometry is the body authority: raw .body240 words are interchange at a
    // declared datum rate (ROM/heritage datum = 44,100). Loading recompiles
    // them at the engine's runtime rate and certifies at that rate (-5 on
    // certification failure).
    int trench_engine_load_body_bytes_at (void* engine, const unsigned char* bytes, size_t len,
                                          double datumRate);
    // Reload without resetting filter states or AGC — for body glide steps.
    int trench_engine_reload_body_bytes (void* engine, const unsigned char* bytes, size_t len,
                                         double datumRate);
    int trench_body_compile_at (const unsigned char* bytes, size_t len, double datumRate,
                                double compileRate, unsigned char* outBody);
    // X3 runtime preset: ONE pre-compiled bank of verbatim xStream words. The
    // X3 ships four distinct designs per filter (44.1/48/96/192k) and selects
    // the nearest; it never migrates coefficients, and neither do we. The
    // caller picks the bank for the host rate and passes its authored rate
    // here - which is why a host rate change must re-load (see
    // TrenchRuntimePreset::bankForRate).
    int trench_engine_load_runtime_preset (void* engine, const char* name,
                                           const unsigned short* words, size_t wordCount,
                                           size_t activeStages, double authoredRate,
                                           double boost);
    void trench_engine_reclaim (void* engine);
    void trench_engine_set_input_mode (void* engine, unsigned int mode);
    void trench_engine_set_spatial_mode (void* engine, int mode);
    void trench_engine_set_qsound_fallback_pan (void* engine, float pan);
    int  trench_engine_coeff_count (void);
    void trench_engine_set_agc_enabled (void* engine, int enabled);
    void trench_engine_set_saturation_enabled (void* engine, int enabled);
    void trench_engine_set_dc_block_enabled (void* engine, int enabled);
    void trench_engine_set_nonlinearity_enabled (void* engine, int enabled);
    void trench_engine_set_x3_movement (void* engine, int enabled);
    void trench_engine_set_agc_drive (void* engine, float drive);
    void trench_engine_set_pitch_ratio (void* engine, float ratio);
    void trench_engine_set_key_snap (void* engine, int choice);
    void trench_engine_set_growl (void* engine, float amount);
    void trench_engine_set_listener (void* engine, float amount, float speedMs);
    void trench_engine_set_env (void* engine, float amount, float releaseMs);
    void trench_engine_set_grit (void* engine, float amount);
    void trench_engine_set_input_preamp (void* engine, float amount);
    float trench_engine_grit_activity (void* engine);
    // Positive dB of AGC gain reduction right now. 0 = idle. The AGC's table
    // is flat (1.0001) below |sample| = 2.0 = +6.02 dBFS, so at DAW levels it
    // never engages; PREAMP is the door that drives the cascade into it.
    float trench_engine_agc_reduction_db (void* engine);
    void trench_engine_process_block (void* engine, float* left, float* right, int numSamples, double morph, double q);
    // Audio-rate Movement: one authored Morph position per sample, Q static.
    void trench_engine_process_trajectory (void* engine, float* left, float* right,
                                           int numSamples, const float* morphPerSample,
                                           double q);
    void trench_engine_get_coeffs (void* engine, float* outCoeffs, float* outBoost);
    int trench_packed_probe_at (const unsigned char* bytes, size_t len, double morph, double q,
                                double targetRate, double* outBiquad, double* outMaxPoleRadius,
                                uint32_t* outUnstableMask, uint32_t* outNonfiniteMask);
    // THE AUTHORING SURFACE IS NOT DECLARED HERE ANY MORE. The curve fitters
    // (trench_fit_arma_endpoint / _pinned / _refine, trench_fit_praat_endpoint,
    // trench_prepare_praat_target), the native Designer entry points, the
    // stage-root/word converters, the authoring limits and validators,
    // trench_certify_body, trench_pack_body_from_corner_words and the automatic
    // DC anchor (trench_body_dc_anchor) had ZERO callers in this build — they
    // were a header-only surface the plug-in never touched. They also carry the
    // six-section assumption the cascade left behind: the fitters still write
    // NUM_STAGES rows into buffers they declare as 30, which is a defect on the
    // AUTHORING path and is being fixed there, not here.
    //
    // The plug-in plays bodies. It does not author them, does not fit them and
    // does not re-normalise their level on load. Anything that needs the
    // authoring path calls trench-core directly.
    double trench_packed_decode (unsigned short word);
    unsigned short trench_packed_encode (double value);
    int trench_cartridge_json_to_body (const char* json, unsigned char* outBody);
    int trench_compile_body_typed (const double* cards, size_t nValues, unsigned char* outBody);
    float trench_keyframe_value (float a, float b, float legBars, double ppq,
                                 double beatsPerBar, unsigned mode);
    int trench_motion_path_value (const float* points, size_t pointCount, double phase,
                                  int closed, float baseMorph, float baseQ, float amount,
                                  float* outMorph, float* outQ);
    int trench_motion_path_value_timed (const float* points, size_t pointCount, double phase,
                                        int closed, size_t gridSteps, float baseMorph, float baseQ,
                                        float amount, float* outMorph, float* outQ);
}
struct TrenchParams
{
    float morph = 0.0f;             // static path only; the trajectory owns Morph otherwise
    float q = 0.0f;                 // the static authored second axis — never modulated
    int keySnap = 0;
    float poleDistortion  = 0.0f;   // GRIT amount — one knob, per-section pole bloom + state clip
    float envAmount = 0.0f;         // FOLLOW depth; 0 = exact bypass, detector never runs
    float growl = 0.0f;             // GROWL: pitch-locked sub-octave wheel oscillation; 0 = off
    float track = 0.0f;             // TRACK: the Hz axis — geography follows the note; 0 = off
};
/// HOW MANY COEFFICIENTS COME BACK. The cascade is SEVEN sections of five
/// (trench-core cascade.rs NUM_STAGES / NUM_COEFFS), so every FFI that hands
/// coefficients to the UI - trench_engine_get_coeffs, trench_packed_probe_at -
/// writes exactly 35 values.
///
/// It used to be 6x5 = 30, and when NUM_STAGES went to 7 these buffers were
/// not grown with it. Both calls then wrote 5 values past the end of a STACK
/// array, which trips the /GS stack cookie and fail-fasts the host with
/// 0xC0000409. Because the only callers are the editor's curve probe and the
/// editor-gated telemetry, it fired the moment a window was opened and never
/// once with the plug-in running headless - the close/open crash.
///
/// Nothing may hardcode 30 again: sizes below derive from this, and the
/// constructor asserts it against the value the library itself reports.
namespace trench
{
inline constexpr int kUiStageCount     = 7;
inline constexpr int kUiCoeffsPerStage = 5;
inline constexpr int kUiCoeffCount     = kUiStageCount * kUiCoeffsPerStage;
}
using trench::kUiCoeffCount;
using trench::kUiCoeffsPerStage;
using trench::kUiStageCount;

class TrenchDspBridge
{
public:
    TrenchDspBridge()
    {
        engine = trench_engine_create();
        if (engine.load (std::memory_order_relaxed) != nullptr)
            liveEngines().fetch_add (1, std::memory_order_relaxed);
        // The tripwire: if the cascade grows another section, this fires in a
        // debug build instead of corrupting a stack in a release one.
        jassert (trench_engine_coeff_count() == kUiCoeffCount);
        for (int s = 0; s < kUiStageCount; ++s)
            uiSnapshot.coeffs[s * kUiCoeffsPerStage] = 1.0f;
        uiSnapshot.boost = 1.0f;
    }
    ~TrenchDspBridge()
    {
        retire();
    }
    /// THE OWNERSHIP LAW, in one object.
    ///
    /// Hosts (FL above all) can destroy the plugin while the audio thread is
    /// still inside process() on the engine, and may call releaseResources
    /// WITHOUT stopping that thread. An atomic POINTER only makes the pointer
    /// safe to read; it does nothing about a thread that already read it and is
    /// inside the engine now. A flag set by releaseResources ("audio stopped")
    /// is a CLAIM about the host, not a proof, and FL is exactly the host that
    /// breaks it.
    ///
    /// So the audio thread states its presence. Every audio-thread touch of the
    /// engine happens inside an AudioScope, which counts. retire() nulls the
    /// pointer and then reads the count:
    ///
    ///   - count 0  -> no callback is inside the engine, and any callback that
    ///                 starts later reads a null pointer and touches nothing.
    ///                 Free it.
    ///   - count >0 -> a callback is inside the engine RIGHT NOW. The host is
    ///                 tearing us down anyway, so LEAK one engine rather than
    ///                 free it underneath. A few hundred KB at process death
    ///                 beats a use-after-free, and beats blocking the DAW on a
    ///                 shutdown wait or a deferred timer (the earlier fix
    ///                 deferred the free 2 s via Timer::callAfterDelay and
    ///                 turned the race into an exit hang).
    ///
    /// Why the two cases are exhaustive: the increment in AudioScope and the
    /// exchange here are both seq_cst, so they sit in ONE total order. Either
    /// the increment comes first — this load sees it, and we leak — or the
    /// exchange comes first, and the audio thread's own seq_cst load of
    /// `engine` (after its increment) sees null. There is no interleaving in
    /// which both threads miss each other. Exactly one free, never on the audio
    /// thread, never on a timer.
    class AudioScope
    {
    public:
        explicit AudioScope (TrenchDspBridge& b) noexcept : bridge (b)
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
        if (void* e = engine.exchange (nullptr, std::memory_order_seq_cst))
        {
            if (inFlight.load (std::memory_order_seq_cst) == 0)
            {
                trench_engine_destroy (e);
                liveEngines().fetch_sub (1, std::memory_order_relaxed);
            }
            else
            {
                leakedEngines().fetch_add (1, std::memory_order_relaxed);
            }
        }
    }
    /// Engines created but not yet freed, and engines deliberately abandoned
    /// because a callback was inside one at retire(). The lifecycle test drives
    /// create/process/close cycles and asserts both land where the law says.
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
        monoScratch.assign ((size_t) juce::jmax (1, maxBlockSize), 0.0f);
        // prepare() resets the engine's control state; forget what was sent.
        lastKeySnapSent = -1;
        lastGritSent = -1.0f;
        lastEnvAmountSent = -1.0f;
        lastGrowlSent = -1.0f;
        lastTrackSent = -1.0f;
        if (auto* e = engine.load (std::memory_order_relaxed))
        {
            trench_engine_prepare (e, sampleRate);
            // Re-send the dev bypass state, not a hardcoded "everything on":
            // prepare() resets the engine's control state, so a stage switched
            // off in the panel used to come back every time the host changed
            // rate or block size, mid-listen. prepareToPlay is never concurrent
            // with processBlock, so this install is the audio thread's own.
            appliedBypassBits = ~(juce::uint64) 0;
        }
    }
    /// THE DEV BYPASS DESK. Which stages of the chain are running. Not preset
    /// state, not automatable, never saved: it exists so a stage can be taken
    /// out and put back BY EAR while the audio runs.
    /// THE DEFAULT STATE IS THE X3's STATE. A preset's reference state has to
    /// null against the X3 with nothing fitted, so nothing that is OURS may sit
    /// in the default path. Three of these were: the section clamp and
    /// pole-radius bend are character, the output tanh and the DC blocker are
    /// safety. None of them came from the X3, all three were on by default, and
    /// together with AUTO KEY they are what made a capture of the same preset
    /// sound different. They stay in the build and stay switchable from the dev
    /// desk — they are just no longer what you get for free.
    ///
    /// The AGC stays on: the X3 has one. Its DRIVE is a separate question —
    /// 2.0 is ours, and the DLL's own is 1.0.
    struct Bypass
    {
        bool nonlinearity = false;  // section state clamp + pole-radius modulator
        bool agc = true;            // the leveller — the X3 has this one
        // UNITY, because the DLL has no pre-scale. ref/ghidra_extracts/
        // runtime_hacks.md, verbatim: "The repo's agc_drive pre-scale is an
        // authoring and audition control. It is not part of the observed DLL
        // path." trench-core/src/engine.rs says the same in its own words and
        // calls unity the shipped value — but AGC_DRIVE there is still 2.0, and
        // this desk was independently sending 2.0 over the top of it every
        // block. At 2.0 the leveller engages in 85 of the 360 X3 preset states;
        // at unity, 28. It is still the audition hook: drag the bar.
        float agcDrive = 1.0f;
        bool saturate = false;      // the output tanh
        bool dcBlock = false;
        // X3 movement parity (X3_MOVEMENT_SPEC.md): block-rate rebuild +
        // kernel ramp + morph one-pole. ON is the SHIPPING DEFAULT (Tyson
        // 2026-08-15 "do what they did verbatim in voice processing"); off =
        // our old per-sample path, kept for A/B.
        bool x3Movement = true;
        bool operator== (const Bypass& o) const noexcept
        {
            return nonlinearity == o.nonlinearity && agc == o.agc
                && juce::approximatelyEqual (agcDrive, o.agcDrive)
                && saturate == o.saturate && dcBlock == o.dcBlock
                && x3Movement == o.x3Movement;
        }
    };
    /// Called from the MESSAGE thread (the dev panel). It must not touch the
    /// engine: the Rust setters take `&mut FilterEngine`, and the audio thread
    /// is inside process() holding exactly that. An atomic pointer makes the
    /// POINTER safe to read, not the object safe to mutate from two threads.
    /// So the desk publishes its state as one 64-bit word and the audio thread
    /// installs it at the top of its own block, where it already owns the
    /// engine. Same path every other control on the face takes.
    void setBypass (const Bypass& b)
    {
        bypass = b;
        bypassBits.store (packBypass (b), std::memory_order_release);
    }
    Bypass getBypass() const noexcept { return bypass; }
    bool loadCartridge (const juce::String& json)
    {
        auto* e = engine.load (std::memory_order_relaxed);
        if (e == nullptr)
            return false;
        return trench_engine_load_cartridge (e, json.toRawUTF8()) == 0;
    }
    // The factory law (E-mu rate-bank audit, 2026-07-29): body words are
    // Hz-anchored at their datum rate and recompile to the runtime rate on
    // load. ROM/heritage .body240 datum = 44,100; native authoring surfaces
    // pass their host rate. datumRate 0 would load a verbatim theta-space
    // carrier - no shipping path uses it.
    static constexpr double kBodyDatumRate = 44'100.0;
    bool loadCartridgeBytes (const void* bytes, size_t len, double datumRate = kBodyDatumRate)
    {
        auto* e = engine.load (std::memory_order_relaxed);
        if (e == nullptr || bytes == nullptr)
            return false;
        const auto* data = static_cast<const unsigned char*> (bytes);
        if (datumRate > 0.0)
            return trench_engine_load_body_bytes_at (e, data, len, datumRate) == 0;
        return trench_engine_load_body_bytes (e, data, len) == 0;
    }
    bool loadCartridgeBytes (const juce::MemoryBlock& block)
    {
        return loadCartridgeBytes (block.getData(), block.getSize());
    }
    /// Reload corner words without resetting states — for body glide steps.
    bool reloadCartridgeBytes (const void* bytes, size_t len, double datumRate = kBodyDatumRate)
    {
        auto* e = engine.load (std::memory_order_relaxed);
        if (e == nullptr || bytes == nullptr)
            return false;
        const auto* data = static_cast<const unsigned char*> (bytes);
        return trench_engine_reload_body_bytes (e, data, len, datumRate) == 0;
    }
    bool reloadCartridgeBytes (const juce::MemoryBlock& block)
    {
        return reloadCartridgeBytes (block.getData(), block.getSize());
    }
    /// Load one xStream bank verbatim. `authoredRate` must be exactly one of
    /// 44100/48000/96000/192000 — the FFI rejects anything else, because a
    /// bank that was not authored at a known rate has no meaning.
    bool loadRuntimePresetBank (const juce::String& name,
                                const std::vector<unsigned short>& words,
                                int activeStages, double authoredRate,
                                double boost = 1.0)
    {
        auto* e = engine.load (std::memory_order_relaxed);
        if (e == nullptr || words.empty() || activeStages < 1 || activeStages > 3)
            return false;
        if (words.size() != (size_t) (4 * activeStages * 5))
            return false;
        return trench_engine_load_runtime_preset (
                   e, name.toRawUTF8(), words.data(), words.size(),
                   (size_t) activeStages, authoredRate, boost) == 0;
    }
    static bool bodyBytesFromJson (const juce::String& cartridgeJson, juce::MemoryBlock& out)
    {
        out.setSize (240);
        return trench_cartridge_json_to_body (cartridgeJson.toRawUTF8(),
                                              static_cast<unsigned char*> (out.getData())) == 0;
    }
    static bool compileTypedBody (const double* cards, int nValues, juce::MemoryBlock& out)
    {
        out.setSize (240);
        return trench_compile_body_typed (cards, (size_t) nValues,
                                          static_cast<unsigned char*> (out.getData())) == 0;
    }
    static bool probePackedBody (const void* bytes, size_t len, float morph, float q,
                                 double runtimeRate,
                                 float outCoeffs[kUiCoeffCount], float& outBoost,
                                 double datumRate = kBodyDatumRate)
    {
        if (bytes == nullptr || outCoeffs == nullptr || len != 240)
            return false;
        // Show what plays: verbatim carriers probe as stored; Hz-anchored
        // bodies probe the words compiled at the runtime rate, the same
        // transform the engine applies on load.
        unsigned char compiled[240];
        std::memcpy (compiled, bytes, 240);
        if (datumRate > 0.0
            && trench_body_compile_at (static_cast<const unsigned char*> (bytes), len,
                                       datumRate, runtimeRate, compiled) != 0)
            return false;
        double biquad[kUiCoeffCount] {};
        double maxPoleRadius = 0.0;
        uint32_t unstableMask = 0, nonfiniteMask = 0;
        const int rc = trench_packed_probe_at (compiled, len,
                                               (double) juce::jlimit (0.0f, 1.0f, morph),
                                               (double) juce::jlimit (0.0f, 1.0f, q),
                                               runtimeRate,
                                               biquad, &maxPoleRadius,
                                               &unstableMask, &nonfiniteMask);
        juce::ignoreUnused (maxPoleRadius);
        if (rc != 0 || unstableMask != 0 || nonfiniteMask != 0)
            return false;
        for (int i = 0; i < kUiCoeffCount; ++i)
            outCoeffs[i] = (float) biquad[i];
        outBoost = 1.0f;
        return true;
    }
    void process (juce::AudioBuffer<float>& buffer, const TrenchParams& params)
    {
        float* left = nullptr;
        float* right = nullptr;
        void* e = resolveChannels (buffer, left, right);
        if (e == nullptr)
            return;
        updateStaticControls (e, params);
        trench_engine_process_block (e, left, right, buffer.getNumSamples(),
                                     params.morph, params.q);
    }
    /// Audio-rate Movement: `morphPerSample` holds one authored wheel
    /// position per sample. Mono buffers traverse the real wet engine via the
    /// preallocated scratch, exactly like process().
    void processTrajectory (juce::AudioBuffer<float>& buffer, const float* morphPerSample,
                            const TrenchParams& params)
    {
        float* left = nullptr;
        float* right = nullptr;
        void* e = resolveChannels (buffer, left, right);
        if (e == nullptr || morphPerSample == nullptr)
            return;
        updateStaticControls (e, params);
        trench_engine_process_trajectory (e, left, right, buffer.getNumSamples(),
                                          morphPerSample, params.q);
    }
    void reclaim()
    {
        if (auto* e = engine.load (std::memory_order_relaxed))
            trench_engine_reclaim (e);
    }
    void setInputPreamp (float amount) noexcept
    {
        if (auto* e = engine.load (std::memory_order_relaxed))
            trench_engine_set_input_preamp (e, amount);
    }
    float gritActivity() const noexcept
    {
        auto* e = engine.load (std::memory_order_relaxed);
        return e != nullptr ? trench_engine_grit_activity (e) : 0.0f;
    }
    /// Positive dB the AGC is pulling down right now (0 = idle).
    float agcReductionDb() const noexcept
    {
        auto* e = engine.load (std::memory_order_relaxed);
        return e != nullptr ? trench_engine_agc_reduction_db (e) : 0.0f;
    }
    void publishUiSnapshot()
    {
        float c[kUiCoeffCount];
        float b = 1.0f;
        if (auto* e = engine.load (std::memory_order_relaxed))
            trench_engine_get_coeffs (e, c, &b);
        else
            std::memset (c, 0, sizeof (c));
        const uint32_t s = uiSnapshot.seq.load (std::memory_order_relaxed);
        uiSnapshot.seq.store (s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_release);
        std::memcpy (uiSnapshot.coeffs, c, sizeof (c));
        uiSnapshot.boost = b;
        std::atomic_thread_fence (std::memory_order_release);
        uiSnapshot.seq.store (s + 2, std::memory_order_release);
    }
    bool readUiSnapshot (float* outCoeffs, float& outBoost)
    {
        if (outCoeffs == nullptr)
            return false;
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const uint32_t s1 = uiSnapshot.seq.load (std::memory_order_acquire);
            if (s1 & 1u)
                continue;
            float c[kUiCoeffCount];
            float b;
            std::atomic_thread_fence (std::memory_order_acquire);
            std::memcpy (c, uiSnapshot.coeffs, sizeof (c));
            b = uiSnapshot.boost;
            std::atomic_thread_fence (std::memory_order_acquire);
            if (uiSnapshot.seq.load (std::memory_order_acquire) == s1)
            {
                std::memcpy (outCoeffs, c, sizeof (c));
                outBoost = b;
                return true;
            }
        }
        return false;
    }
    void setInputMode (int mode)
    {
        if (auto* e = engine.load (std::memory_order_relaxed))
            trench_engine_set_input_mode (e, static_cast<unsigned int> (juce::jlimit (0, 2, mode)));
    }
    void setSpatialMode (int mode)
    {
        if (auto* e = engine.load (std::memory_order_relaxed))
            trench_engine_set_spatial_mode (e, juce::jlimit (0, 2, mode));
    }
    void setQSoundFallbackPan (float pan)
    {
        if (auto* e = engine.load (std::memory_order_relaxed))
            trench_engine_set_qsound_fallback_pan (e, juce::jlimit (-1.0f, 1.0f, pan));
    }
    void setAgcEnabled (bool enabled)
    {
        if (auto* e = engine.load (std::memory_order_relaxed))
            trench_engine_set_agc_enabled (e, enabled ? 1 : 0);
    }
    void setAgcDrive (float drive)
    {
        if (auto* e = engine.load (std::memory_order_relaxed))
            trench_engine_set_agc_drive (e, juce::jmax (1.0f, drive));
    }
    void setSaturationEnabled (bool enabled)
    {
        if (auto* e = engine.load (std::memory_order_relaxed))
            trench_engine_set_saturation_enabled (e, enabled ? 1 : 0);
    }
private:
    static juce::uint64 packBypass (const Bypass& b) noexcept
    {
        juce::uint32 flags = (b.nonlinearity ? 1u : 0u) | (b.agc       ? 2u : 0u)
                           | (b.saturate     ? 4u : 0u) | (b.dcBlock   ? 8u : 0u)
                           | (b.x3Movement   ? 16u : 0u);
        juce::uint32 drive = 0;
        const float d = juce::jmax (1.0f, b.agcDrive);
        std::memcpy (&drive, &d, sizeof (drive));
        return (juce::uint64) flags | ((juce::uint64) drive << 32);
    }
    /// AUDIO THREAD ONLY. Called from updateStaticControls, which already runs
    /// with the engine to itself.
    void applyBypass (void* e, juce::uint64 bits) noexcept
    {
        const juce::uint32 flags = (juce::uint32) (bits & 0xffffffffull);
        const juce::uint32 driveBits = (juce::uint32) (bits >> 32);
        float drive = 1.0f;
        std::memcpy (&drive, &driveBits, sizeof (drive));
        trench_engine_set_nonlinearity_enabled (e, (flags & 1u) ? 1 : 0);
        trench_engine_set_agc_enabled          (e, (flags & 2u) ? 1 : 0);
        trench_engine_set_saturation_enabled   (e, (flags & 4u) ? 1 : 0);
        trench_engine_set_dc_block_enabled     (e, (flags & 8u) ? 1 : 0);
        trench_engine_set_x3_movement          (e, (flags & 16u) ? 1 : 0);
        trench_engine_set_agc_drive            (e, juce::jmax (1.0f, drive));
    }
    /// Message-thread copy, for the panel to read back what it set.
    Bypass bypass;
    /// The published word. Written by the message thread, read by audio.
    std::atomic<juce::uint64> bypassBits { packBypass (Bypass {}) };
    /// Audio-thread only: what has actually been installed. The sentinel forces
    /// one install on the first block after every prepare().
    juce::uint64 appliedBypassBits = ~(juce::uint64) 0;
    // Shared across the audio and message threads. All accesses are atomic;
    // retire() exchanges the pointer away (the audio thread immediately
    // no-ops on a null engine) and frees it only once the in-flight count
    // proves no callback is inside it. See AudioScope for why that is a proof
    // and not a hope.
    std::atomic<void*> engine { nullptr };
    std::atomic<int> inFlight { 0 };
    std::vector<float> monoScratch;
    int lastKeySnapSent = -1;
    float lastGritSent = -1.0f;
    float lastEnvAmountSent = -1.0f;
    float lastGrowlSent = -1.0f;
    float lastTrackSent = -1.0f;
    // Engine + channel plumbing shared by the static and trajectory paths.
    // Returns nullptr while the plugin is tearing down (never touch the
    // engine then) or when the mono scratch cannot cover the block.
    //
    // The load is seq_cst, not relaxed: it is one half of the ordering that
    // makes retire()'s free provable (see AudioScope). Callers must already
    // hold an AudioScope. The mono-scratch bound is the last line of defence —
    // the processor walks an oversized host block in prepared-size chunks, so
    // a block bigger than the scratch never reaches here.
    void* resolveChannels (juce::AudioBuffer<float>& buffer, float*& left, float*& right)
    {
        void* e = engine.load (std::memory_order_seq_cst);
        const int channels = buffer.getNumChannels();
        const int n = buffer.getNumSamples();
        if (e == nullptr || channels < 1)
            return nullptr;
        left = buffer.getWritePointer (0);
        if (channels >= 2)
        {
            right = buffer.getWritePointer (1);
        }
        else
        {
            if ((size_t) n > monoScratch.size())
                return nullptr;
            std::copy_n (left, n, monoScratch.data());
            right = monoScratch.data();
        }
        return e;
    }
    // Change-gated static controls: one FFI call per control per CHANGE, no
    // string lookups, no per-block chatter.
    void updateStaticControls (void* e, const TrenchParams& params)
    {
        // The dev bypass desk, installed here and nowhere else: this is the one
        // place the engine is provably ours (see setBypass).
        if (const auto bits = bypassBits.load (std::memory_order_acquire);
            bits != appliedBypassBits)
        {
            applyBypass (e, bits);
            appliedBypassBits = bits;
        }
        if (params.keySnap != lastKeySnapSent)
        {
            trench_engine_set_key_snap (e, params.keySnap);
            lastKeySnapSent = params.keySnap;
        }
        if (! juce::approximatelyEqual (params.poleDistortion, lastGritSent))
        {
            trench_engine_set_grit (e, params.poleDistortion);
            lastGritSent = params.poleDistortion;
        }
        // FOLLOW rides the core's own detector on the dry input tap
        // (trench-core/src/env.rs). The release is the authored 200 ms; only
        // depth is on the surface. Its attack/release is the detector's
        // authored behaviour — never a licence to smooth coefficients again.
        if (! juce::approximatelyEqual (params.envAmount, lastEnvAmountSent))
        {
            trench_engine_set_env (e, params.envAmount, 200.0f);
            lastEnvAmountSent = params.envAmount;
        }
        if (! juce::approximatelyEqual (params.growl, lastGrowlSent))
        {
            trench_engine_set_growl (e, params.growl);
            lastGrowlSent = params.growl;
        }
        // TRACK rides the core's pitch listener (trench-core/src/listener.rs):
        // the body's whole geography slides with the note, glide 80 ms.
        if (! juce::approximatelyEqual (params.track, lastTrackSent))
        {
            trench_engine_set_listener (e, params.track, 80.0f);
            lastTrackSent = params.track;
        }
    }
    struct UiCoeffSnapshot
    {
        std::atomic<uint32_t> seq { 0 };
        float coeffs[kUiCoeffCount] {};
        float boost { 1.0f };
    };
    UiCoeffSnapshot uiSnapshot;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrenchDspBridge)
};
