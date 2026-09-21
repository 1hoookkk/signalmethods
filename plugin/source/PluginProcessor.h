#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "parameters/TrenchParameters.h"
#include "TrenchBodyRoster.h"
#include "dsp/TrenchDspBridge.h"
#include "dsp/TrenchRuntimePreset.h"
#include "dsp/Movement.h"
#include "dsp/UserMotion.h"
#include "dsp/KeyDetector.h"
#include "dsp/TransientDetector.h"
#include "dsp/DeskDrive.h"
#include "dsp/TrenchCleanBody.h"
#include "dsp/WheelLoop.h"
#include <array>
#include <atomic>
#include <vector>
class PluginProcessor final : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener,
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
    void restartMovement() noexcept { movementRestart.fetch_add (1, std::memory_order_relaxed); }
    trench::UserMotionState userMotion;
    bool usingUserMotion() const { return pMoveCustom->load() > 0.5f; }
    trench::UserMotion motionForEditing() const;
    void applyUserMotion (const trench::UserMotion&, bool restart = false);
    juce::AudioProcessorValueTreeState apvts;
    TrenchDspBridge dspBridge;
#if TRENCH_DEV_PANEL
    juce::ValueTree calibrationArchive { "DevCalibrationSession" };
    juce::CriticalSection calibrationArchiveLock;
    juce::MemoryBlock bodyBytesForCalibration() const { return currentBodyBytes; }
    std::atomic<float> calibrationInputRms { 0 }, calibrationOutputRms { 0 }, calibrationCeilingFraction { 0 };
    std::array<std::atomic<float>, trench::calibration::count> calibrationReceived {};
    std::atomic<std::uint64_t> calibrationBlocks { 0 };
#endif
    const std::atomic<float>& getInputMeterLeftForUi() const noexcept  { return inputMeterL; }
    const std::atomic<float>& getInputMeterRightForUi() const noexcept { return inputMeterR; }
    float getOutClipForUi() const noexcept { return outClipForUi.load (std::memory_order_relaxed); }
    float getGritActivityForUi() const noexcept { return gritActivityForUi.load (std::memory_order_relaxed); }
    bool isKeyModelReady() const noexcept { return keyDetector.isModelReady(); }
    bool isNoteLatched() const noexcept { return noteLatched.load (std::memory_order_relaxed); }
    float getNoteTrackRatio() const noexcept { return noteTrackRatio.load (std::memory_order_relaxed); }
    float getNoteBite() const noexcept { return noteBite.load (std::memory_order_relaxed); }
    int getDetectedKeyForUi() const noexcept { return detectedKeyForUi.load (std::memory_order_relaxed); }
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
    trench::WheelLoop& wheelLoop() noexcept { return wheelLoopSource; }
    void clearCalibrationNoteLatch() noexcept { noteLatched.store (false, std::memory_order_relaxed); }
    bool isCleanGroundTruthAudio() const noexcept { return trench::clean_audio::kEnabled(); }
    float getEffectiveMorphForUi() const noexcept { return effectiveMorphForUi.load (std::memory_order_relaxed); }
    std::uint32_t getMorphUpdatesForUi() const noexcept { return morphUpdatesForUi.load (std::memory_order_relaxed); }
    float getEffectiveBiteForUi() const noexcept  { return effectiveBiteForUi.load (std::memory_order_relaxed); }
    float getEffectiveQForUi() const noexcept     { return effectiveQForUi.load (std::memory_order_relaxed); }
    bool isMorphModulatedForUi() const noexcept   { return morphModulatedForUi.load (std::memory_order_relaxed); }
    /// Hover-audition in the BODY menu: load a body for LISTENING only. It does
    /// NOT touch the body parameter, so hovering a menu writes no automation and
    /// leaves no undo step — only a click commits.
    /// Browsing cancelled: the body that was playing comes straight back, with
    /// no travel — nothing was chosen, so nothing should move.
    bool hasLivePhraseForUi() const noexcept { return false; }
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
    // Serializes body producers and UI readers. Never acquired by processBlock.
    juce::CriticalSection bodyStateLock;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void forceCleanAudioUiState();
    void setParameterDenormalized (const char* parameterID, float value);
    void updateAutoKey();
    /// One prepared-size slice of a host block. processBlock walks an oversized
    /// block through this; it never sees more samples than prepareToPlay sized
    /// the buffers for.
    void processChunk (juce::AudioBuffer<float>& buffer, int sampleOffset = 0);
    trench::Movement              movement;
    trench::UserMotionState::Audio audioMotion;
    std::atomic<std::uint64_t> movementRestart { 0 };
    trench::TransientDetector     transientDetector;
    float previousDriveGain = 1.0f;
#if TRENCH_DEV_PANEL
    std::array<std::atomic<float>*, trench::calibration::count> calibrationParameters {};
    float calibrationMorph = -1.0f;
    float calibrationOutputGain = 1.0f;
#endif
    std::atomic<bool>             noteLatched { false };
    std::atomic<float>            noteTrackRatio { 1.0f };
    std::atomic<float>            noteBite { 0.0f };
    float                         wheelRampFrom = -1.0f;
    std::vector<float>            morphBuffer;   // one authored Morph per sample
    /// What prepareToPlay sized every audio-thread buffer for. JUCE explicitly
    /// permits a later block to be LARGER than maximumExpectedSamplesPerBlock,
    /// and processBlock may not allocate, so a bigger block is walked in
    /// chunks of this size instead of being dropped to dry.
    int                           preparedBlockSize = 0;

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
    std::atomic<bool> bodyInjected { false };
    // X3 runtime preset support.
    bool loadRuntimePresetForCurrentRate();
    void buildRuntimePresetProbeMirror (int bank);
    trench::RuntimePreset loadedRuntimePreset;
    double loadedRuntimePresetBankRate { 0.0 };
    /// UI-only mirror of the selected bank, identity-padded to six stages, so
    /// the curve has 240 words to probe. Not the engine's copy, not exportable.
    juce::MemoryBlock runtimePresetProbeBytes;
    std::atomic<bool> lastLoadOk { true };
    trench::WheelLoop wheelLoopSource;
    juce::Time        auditionSlotMtime;
    juce::String      watchedBodyPath;      // in-place reload of a disk-loaded body
    juce::Time        watchedBodyMtime;
    // Cached APVTS atomics — resolved once at construction; the audio thread
    // never does a string parameter lookup.
    std::atomic<float>* pMorph = nullptr;
    std::atomic<float>* pQ = nullptr;
    std::atomic<float>* pPreamp = nullptr;
    std::atomic<float>* pDistortion = nullptr;
    std::atomic<float>* pMovePreset = nullptr;
    std::atomic<float>* pMoveTransition = nullptr;
    std::atomic<float>* pMoveLength = nullptr;
    std::atomic<float>* pMovePlayback = nullptr;
    std::atomic<float>* pMoveCustom = nullptr;
    std::atomic<float>* pKeySnap = nullptr;
    std::atomic<float> inputMeterL { 0.0f };
    std::atomic<float> inputMeterR { 0.0f };
    std::atomic<float> outClipForUi { 0.0f };
    std::atomic<float> gritActivityForUi { 0.0f };
    std::atomic<float> effectiveMorphForUi { 0.0f };
    std::atomic<std::uint32_t> morphUpdatesForUi { 0 };
    std::atomic<float> effectiveBiteForUi { 0.0f };
    std::atomic<float> effectiveQForUi { 0.0f };
    std::atomic<bool> morphModulatedForUi { false };
    std::atomic<bool> editorOpen { false };
    // AUTO KEY — always listening while AUTO is selected. ~1 s analysis
    // windows; a verdict needs either one confident window plus agreement or
    // two agreeing windows (~2 s). An accepted key holds until a different
    // key clearly displaces it (hysteresis), so the snap never flaps.
    std::atomic<int> detectedKeyForUi { -1 };
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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
