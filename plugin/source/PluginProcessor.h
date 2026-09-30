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
    float getKeySemitonesForUi() const noexcept { return dspBridge.keySemitonesForUi(); }
    float getKeyConfidenceForUi() const noexcept { return keyConfidenceForUi.load (std::memory_order_relaxed); }
    void setEditorOpen (bool open) noexcept
    {
        editorOpen.store (open, std::memory_order_relaxed);
        if (! open)
            morphHeld.store (false, std::memory_order_release);
    }
    void holdMorph (bool hold);
    void holdQ (bool hold);
    bool isQModulatedForUi() const noexcept { return qModulatedForUi.load (std::memory_order_relaxed); }
    void setEchoArmed (bool on);
    bool isEchoArmed() const noexcept { return echoArmed.load (std::memory_order_relaxed); }
    static constexpr const char* kEchoName = "Echo";
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
    bool installBodyBytes (const void* bytes, size_t len, double datumRate = 44'100.0);
    bool copyCurrentBodyBytes (void* out, size_t len);
    bool probeCurrentBodyForUi (float morph, float q, float outCoeffs[trench::kUiCoeffCount], float& outBoost);
    std::atomic<int> bodyVersionForUi { 0 };
    static constexpr const char* processorBuildIdentifier() noexcept
    {
        return "trench-plugin-processor-v1";
    }
private:
    juce::CriticalSection bodyStateLock;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void forceCleanAudioUiState();
    void setParameterDenormalized (const char* parameterID, float value);
    int recoverBodyFromState (const juce::String& bodyId, const juce::String& bodyBytesText, juce::MemoryBlock& unlisted);
public:
    juce::File bodyRecoveryDirectory = trench::userBodyDirectory();
private:
    void updateAutoKey();
    void processChunk (juce::AudioBuffer<float>& buffer, int sampleOffset = 0);
    trench::Movement              movement;
    trench::UserMotionState::Audio audioMotion;
    std::atomic<std::uint64_t> movementRestart { 0 };
    trench::TransientDetector     transientDetector;
    float previousDriveGain = 1.0f;
#if TRENCH_DEV_PANEL
    std::array<std::atomic<float>*, trench::calibration::count> calibrationParameters {};
    float calibrationMorph = -1.0f;
    float followEnvelope = 0.0f;
    float followPush = 0.0f;
    float calibrationOutputGain = 1.0f;
#endif
    std::atomic<bool>             noteLatched { false };
    std::atomic<float>            noteTrackRatio { 1.0f };
    std::atomic<float>            noteBite { 0.0f };
    float                         wheelRampFrom = -1.0f;
    std::atomic<bool>             morphHeld { false };
    bool                          morphWasHeld = false;
    trench::Movement              movementQ;
    std::atomic<bool>             qHeld { false };
    bool                          qWasHeld = false;
    float                         movementDepthQ = 1.0f;
    std::atomic<bool>             qModulatedForUi { false };
    std::atomic<bool>             echoArmed { false };
    struct EchoTake
    {
        static constexpr int kMaxPoints = 4096;
        std::array<float, kMaxPoints> values {};
        std::array<int, kMaxPoints> at {};
        int count = 0, samples = 0;
        double bpm = 120.0, beatsPerBar = 4.0, sampleRate = 48000.0;
        float release = 0.0f;
    };
    EchoTake                      echoTakes[2];
    int                           echoWriting = 0;
    bool                          echoRecording = false;
    std::atomic<int>              echoReady { -1 };
    struct EchoFinalizer final : juce::AsyncUpdater
    {
        explicit EchoFinalizer (PluginProcessor& p) : owner (p) {}
        void handleAsyncUpdate() override { owner.finishEcho(); }
        PluginProcessor& owner;
    } echoFinalizer { *this };
    void finishEcho();
    float                         movementDepth = 1.0f;
    float                         movementReturnStep = 0.0f;
    std::vector<float>            morphBuffer;
    std::vector<float>            qBuffer;
    int                           preparedBlockSize = 0;

    trench::KeyDetector           keyDetector;
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
    bool loadRuntimePresetForCurrentRate();
    void buildRuntimePresetProbeMirror (int bank);
    trench::RuntimePreset loadedRuntimePreset;
    double loadedRuntimePresetBankRate { 0.0 };
    juce::MemoryBlock runtimePresetProbeBytes;
    std::atomic<bool> lastLoadOk { true };
    trench::WheelLoop wheelLoopSource;
    juce::Time        auditionSlotMtime;
    juce::String      watchedBodyPath;
    juce::String      publishedAxisNames { "MORPH/Q" };
    juce::Time        watchedBodyMtime;
    std::atomic<float>* pMorph = nullptr;
    std::atomic<float>* pQ = nullptr;
    std::atomic<float>* pPreamp = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pMovePreset = nullptr;
    std::atomic<float>* pMoveTransition = nullptr;
    std::atomic<float>* pMoveLength = nullptr;
    std::atomic<float>* pMovePlayback = nullptr;
    std::atomic<float>* pMoveCustom = nullptr;
    std::atomic<float>* pKeySnap = nullptr;
    std::atomic<float>* pSlam = nullptr;
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
    std::atomic<int> detectedKeyForUi { -1 };
    std::atomic<float> keyConfidenceForUi { 0.0f };
    static constexpr double kKeyForget = 0.97;
    static constexpr int kKeyMinWindows = 3;
    std::array<double, 24> keyEvidence {};
    int keyWindows = 0;
    juce::MemoryBlock currentBodyBytes;
    double currentBodyDatumRate = 44'100.0;
    juce::MemoryBlock rosterBodyBytes;
    juce::MemoryBlock uiBodyBytes;
    void captureCurrentBodyBytes (const juce::String& cartridgeJson);
    struct BodyRateBank
    {
        double rate = 0.0;
        juce::MemoryBlock bytes;
    };
    static constexpr int kBodyRateBankCount = 3;
    int loadSidecarBanksForPath (const juce::String& absoluteBodyPath,
                                std::array<BodyRateBank, kBodyRateBankCount>& out) const;
    const BodyRateBank* bankForRate (double rate) const noexcept;
    bool installBodyForSelection (const juce::MemoryBlock& baseBytes, const juce::String& sourcePath);
    std::array<BodyRateBank, kBodyRateBankCount> bodySidecarBanks {};
    int bodySidecarBankCount = 0;
    double installedBankRate = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
