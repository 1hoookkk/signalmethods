#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "parameters/TrenchParameters.h"
#include "TrenchBodyRoster.h"
#include "dsp/TrenchDspBridge.h"
#include "dsp/TrenchRuntimePreset.h"
#include "dsp/Movement.h"
#include "dsp/KeyDetector.h"
#include "dsp/EnvFollower.h"
#include "dsp/TrenchCleanBody.h"
#include <array>
#include <atomic>
#include <vector>
class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AudioProcessorParameter::Listener,
                              private juce::AsyncUpdater,
                              private juce::Timer
{
public:
    PluginProcessor();
    ~PluginProcessor() override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    juce::AudioProcessorValueTreeState apvts;
    TrenchDspBridge dspBridge;
    const std::atomic<float>& getInputMeterLeftForUi() const noexcept  { return inputMeterL; }
    const std::atomic<float>& getInputMeterRightForUi() const noexcept { return inputMeterR; }
    float getOutClipForUi() const noexcept { return outClipForUi.load (std::memory_order_relaxed); }
    float getGritActivityForUi() const noexcept { return gritActivityForUi.load (std::memory_order_relaxed); }
    /// Positive dB the AGC is pulling down (0 = idle, ~18.4 = its table floor).
    /// Idles at DAW levels by design: the AGC table is flat below +6.02 dBFS,
    /// so PREAMP is what drives the cascade into E-mu's limiting character.
    float getAgcReductionDbForUi() const noexcept { return agcReductionDbForUi.load (std::memory_order_relaxed); }
    bool isKeyModelReady() const noexcept { return keyDetector.isModelReady(); }
    int getDetectedKeyForUi() const noexcept { return detectedKeyForUi.load (std::memory_order_relaxed); }
    int getDetectedAltKeyForUi() const noexcept { return detectedAltKeyForUi.load (std::memory_order_relaxed); }
    float getKeyConfidenceForUi() const noexcept { return keyConfidenceForUi.load (std::memory_order_relaxed); }
    /// The editor's open state gates DISPLAY TELEMETRY ONLY (meters, curve
    /// snapshot). AUTO KEY listens whenever AUTO is selected — closing and
    /// reopening the editor never changes sound or detector progress.
    void setEditorOpen (bool open) noexcept
    {
        editorOpen.store (open, std::memory_order_relaxed);
    }
    int  getLoadedBodyIndex() const noexcept { return loadedBodyIndex.load (std::memory_order_relaxed); }
    bool getLastLoadOk()      const noexcept { return lastLoadOk.load (std::memory_order_relaxed); }
    bool isCleanGroundTruthAudio() const noexcept { return trench::clean_audio::kEnabled(); }
    float getEffectiveMorphForUi() const noexcept { return effectiveMorphForUi.load (std::memory_order_relaxed); }
    float getEffectiveQForUi() const noexcept     { return effectiveQForUi.load (std::memory_order_relaxed); }
    bool isMorphModulatedForUi() const noexcept   { return morphModulatedForUi.load (std::memory_order_relaxed); }
    /// The LIVE phrase slot only means something while the Workstation is
    /// feeding one; the menu greys the entry out otherwise.
    bool hasLivePhraseForUi() const noexcept { return livePhraseValid.load (std::memory_order_acquire); }
    /// Hover-audition in the BODY menu: load a body for LISTENING only. It does
    /// NOT touch the body parameter, so hovering a menu writes no automation and
    /// leaves no undo step — only a click commits.
    /// Browsing cancelled: the body that was playing comes straight back, with
    /// no travel — nothing was chosen, so nothing should move.
    void restoreBodyForUi (int index)
    {
        if (index < 0 || index >= trench::bodyCount())
            return;
        pendingBodyIndex.store (index, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }
    void previewBodyForUi (int index)
    {
        if (index < 0 || index >= trench::bodyCount())
            return;
        pendingBodyIndex.store (index, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }
    bool seedCurrentBody();
    void exportCurrentBody();
    void forgeAuditionTyped (const std::vector<double>& cards);
    juce::File forgeSaveBody (const juce::String& name, bool overwrite = false);
    enum Axis { AxisFamily = 0, AxisMorph, AxisQ, AxisQSound, AxisSlam };
    // datumRate declares the rate the 240 bytes are Hz-anchored at; canonical
    // .body240 files keep the ROM/heritage datum default, native authoring
    // surfaces pass their host rate.
    bool installBodyBytes (const void* bytes, size_t len, double datumRate = 44'100.0);
    bool copyCurrentBodyBytes (void* out, size_t len);
    bool probeCurrentBodyForUi (float morph, float q, float outCoeffs[trench::kUiCoeffCount], float& outBoost);
    /// Bumped on every body swap/hot-reload so the editor can skip the
    /// per-frame packed probe when nothing it depends on has changed.
    std::atomic<int> bodyVersionForUi { 0 };
    static constexpr const char* processorBuildIdentifier() noexcept
    {
        return "trench-plugin-processor-v1";
    }
private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void parameterValueChanged (int, float) override {}
    /// MOVEMENT starts where you place it: the end of a MORPH wheel gesture
    /// re-anchors the pattern at step 0. This arrives on the MESSAGE thread, so
    /// it only sets an atomic — processChunk is what touches Movement.
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void forceCleanAudioUiState();
    void setParameterDenormalized (const char* parameterID, float value);
    void updateAutoKey();
    /// One prepared-size slice of a host block. processBlock walks an oversized
    /// block through this; it never sees more samples than prepareToPlay sized
    /// the buffers for.
    void processChunk (juce::AudioBuffer<float>& buffer);
    trench::Movement              movement;
    trench::EnvFollower           follower;
    std::atomic<bool>             morphRetrigger { false };
    std::vector<float>            morphBuffer;   // one authored Morph per sample
    /// What prepareToPlay sized every audio-thread buffer for. JUCE explicitly
    /// permits a later block to be LARGER than maximumExpectedSamplesPerBlock,
    /// and processBlock may not allocate, so a bigger block is walked in
    /// chunks of this size instead of being dropped to dry.
    int                           preparedBlockSize = 0;

    // LIVE PHRASE — the ear in the loop. The Workstation writes the phrase you
    // are drawing to filters/phrase_live.json; the timer picks it up and swaps
    // it in under the audio thread (write the idle slot, then flip the index).
    // Selecting "LIVE" in the MOVEMENT menu means every stroke is audible at once.
    static constexpr int kLiveCells = 64;
    struct LiveSlot
    {
        float values[kLiveCells] {};
        unsigned char trigs[kLiveCells] {};
        trench::FuncGenPattern desc { "LIVE", 16, 0, true, values, trigs };
        double stepBeats = 0.0;
    };
    LiveSlot livePhrase[2];
    std::atomic<int> livePhraseSlot { 0 };
    std::atomic<bool> livePhraseValid { false };
    juce::File livePhraseFile;
    juce::Time livePhraseMtime;
    void pollLivePhrase();
    trench::KeyDetector           keyDetector;
    /// AUTO KEY's worker. analyse() is a 131,072-point FFT plus an RTNeural
    /// forward pass; it used to run in timerCallback, which JUCE runs on the
    /// MESSAGE thread — every window stalled the whole UI, and the host's, for
    /// as long as the transform took. The capture tap in processBlock stays
    /// where it was (allocation-free, lock-free, two slots); only the analysis
    /// moved here. Results are published through the same atomics the editor
    /// already reads, so nothing else changed hands.
    ///
    /// Owned below keyDetector so it is destroyed FIRST, and stopped
    /// explicitly with a bounded timeout at every teardown — shutdown never
    /// waits on it indefinitely.
    class KeyWorker final : public juce::Thread
    {
    public:
        explicit KeyWorker (PluginProcessor& p) : juce::Thread ("TRENCH AUTO KEY"), owner (p) {}
        ~KeyWorker() override { stop(); }
        void start()
        {
            if (! isThreadRunning())
                startThread (juce::Thread::Priority::low);
        }
        /// Bounded: signal, wait 2 s, then kill. A DAW closing must not hang on
        /// an analysis window.
        void stop() { stopThread (2000); }
        void run() override
        {
            while (! threadShouldExit())
            {
                owner.updateAutoKey();
                wait (200);
            }
        }
    private:
        PluginProcessor& owner;
    };
    KeyWorker                     keyWorker { *this };
    int currentProgram = 0;
    std::atomic<int>  pendingBodyIndex { trench::kNoFilterIndex };
    std::atomic<int>  loadedBodyIndex { trench::kNoFilterIndex };
    // X3 runtime preset support.
    bool loadRuntimePresetForCurrentRate();
    void buildRuntimePresetProbeMirror (int bank);
    trench::RuntimePreset loadedRuntimePreset;
    double loadedRuntimePresetBankRate { 0.0 };
    /// UI-only mirror of the selected bank, identity-padded to six stages, so
    /// the curve has 240 words to probe. Not the engine's copy, not exportable.
    juce::MemoryBlock runtimePresetProbeBytes;
    std::atomic<bool> lastLoadOk { true };
    juce::Time        auditionSlotMtime;
    juce::String      watchedBodyPath;      // in-place reload of a disk-loaded body
    juce::Time        watchedBodyMtime;
    // Cached APVTS atomics — resolved once at construction; the audio thread
    // never does a string parameter lookup.
    std::atomic<float>* pMorph = nullptr;
    std::atomic<float>* pQ = nullptr;
    std::atomic<float>* pChew = nullptr;
    std::atomic<float>* pSlam = nullptr;
    std::atomic<float>* pPreamp = nullptr;
    std::atomic<float>* pLowKeep = nullptr;
    std::atomic<float>* pFollow = nullptr;
    std::atomic<float>* pTrack = nullptr;
    std::atomic<float>* pMovePreset = nullptr;
    std::atomic<float>* pMoveDivision = nullptr;
    std::atomic<float>* pKeySnap = nullptr;
    std::atomic<float> inputMeterL { 0.0f };
    std::atomic<float> inputMeterR { 0.0f };
    std::atomic<float> outClipForUi { 0.0f };
    std::atomic<float> gritActivityForUi { 0.0f };
    std::atomic<float> agcReductionDbForUi { 0.0f };
    std::atomic<float> effectiveMorphForUi { 0.0f };
    std::atomic<float> effectiveQForUi { 0.0f };
    std::atomic<bool> morphModulatedForUi { false };
    std::atomic<bool> editorOpen { false };
    // AUTO KEY — always listening while AUTO is selected. ~1 s analysis
    // windows; a verdict needs either one confident window plus agreement or
    // two agreeing windows (~2 s). An accepted key holds until a different
    // key clearly displaces it (hysteresis), so the snap never flaps.
    std::atomic<int> detectedKeyForUi { -1 };
    std::atomic<int> detectedAltKeyForUi { -1 };
    std::atomic<float> keyConfidenceForUi { 0.0f };
    int candidateKey = -1;
    int candidateCount = 0;
    int acceptedKey = -1;
    juce::MemoryBlock currentBodyBytes;
    double currentBodyDatumRate = 44'100.0;
    juce::MemoryBlock rosterBodyBytes;
    // The curve draws the new body instantly.
    // reloadCartridgeBytes keeps cascade states alive — no click, no crossfade.
    juce::MemoryBlock uiBodyBytes;
    void captureCurrentBodyBytes (const juce::String& cartridgeJson);
    static constexpr int kLowKeepMaxChannels = 8;
    juce::AudioBuffer<float> lowKeepBand;
    std::array<float, kLowKeepMaxChannels> lowKeepState {};
    float lowKeepCutoffHz = 0.0f;
    double lowKeepSampleRate = 44'100.0;
    bool lastPreampActive = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
