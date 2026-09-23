#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TrenchBodyRoster.h"
#include "dsp/PreampLaw.h"
#include "dsp/DeskCompensation.h"
#include "dsp/SlamStage.h"
#include "parameters/CurveMap.h"
#include "BinaryData.h"
#include <cmath>
#include <cstring>
#include <limits>
namespace
{
constexpr int kCleanInputMode = 0;
constexpr int kMackieDeskSlam = 1;
constexpr int kSpatialOff = 2;
// KEY AUTO label mapping (same as KeySnapBox::snapChoiceForSuggestion):
// detector labels are 0..11 major, 12..23 minor; the parameter is
// 1..12 minor, 13..24 major, 0 = AUTO.
int snapChoiceForDetection (int label) noexcept
{
    if (label < 0 || label >= 24)
        return 0;   // unknown: NO snap — never a fabricated C
    return label < 12 ? 13 + label : 1 + (label - 12);
}
}
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                      #if ! JucePlugin_IsMidiEffect
                       #if ! JucePlugin_IsSynth
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       #endif
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                      #endif
                        ),
       apvts (*this, nullptr, "TRENCH_STATE", TrenchParameters::createParameterLayout())
{
    // Every audio-thread parameter read goes through these — resolved ONCE,
    // never a string lookup in the callback.
    pMorph      = apvts.getRawParameterValue (ParamID::morph);
#if TRENCH_DEV_PANEL
    for (size_t i = 0; i < trench::calibration::count; ++i)
        calibrationParameters[i] = apvts.getRawParameterValue (trench::calibration::variables[i].id);
#endif
    pQ          = apvts.getRawParameterValue (ParamID::q);
    pPreamp     = apvts.getRawParameterValue (ParamID::preamp);
    pOutput     = apvts.getRawParameterValue (ParamID::output);
    pMovePreset = apvts.getRawParameterValue (ParamID::movePreset);
    pMoveTransition = apvts.getRawParameterValue (ParamID::moveTransition);
    pMoveLength = apvts.getRawParameterValue (ParamID::moveLength);
    pMovePlayback = apvts.getRawParameterValue (ParamID::movePlayback);
    pMoveCustom = apvts.getRawParameterValue (ParamID::moveCustom);
    userMotion.readAudio (audioMotion);
    pKeySnap    = apvts.getRawParameterValue (ParamID::keySnap);
    if (trench::clean_audio::kEnabled())
        forceCleanAudioUiState();
    const int startIndex = juce::jlimit (0, juce::jmax (0, trench::bodyCount() - 1),
                                         (int) apvts.getRawParameterValue (ParamID::body)->load());
    // Load the initial body synchronously so the curve is ready for first paint.
    juce::MemoryBlock startRaw;
    if (trench::bodyRawBytes (startIndex, startRaw))
    {
        dspBridge.loadCartridgeBytes (startRaw);
        currentBodyBytes = startRaw;
        currentBodyDatumRate = TrenchDspBridge::kBodyDatumRate;
        rosterBodyBytes = currentBodyBytes;
    }
    else
    {
        const auto startJson = trench::bodyCartridgeJson (startIndex);
        if (startJson.isNotEmpty() && trench::isRuntimePresetJson (startJson))
        {
            loadedRuntimePreset = trench::parseRuntimePreset (startJson);
            loadRuntimePresetForCurrentRate();
            currentBodyBytes.reset();
            rosterBodyBytes.reset();
            currentBodyDatumRate = 0.0;
        }
        else if (startJson.isNotEmpty())
        {
            dspBridge.loadCartridge (startJson);
            captureCurrentBodyBytes (startJson);
        }
    }
    pendingBodyIndex.store (startIndex, std::memory_order_relaxed);
    loadedBodyIndex.store (startIndex, std::memory_order_relaxed);
    dspBridge.setSpatialMode (kSpatialOff);
    dspBridge.setQSoundFallbackPan (1.0f);
    {
        int modelBytes = 0;
        const auto* modelJson = BinaryData::getNamedResource ("key_model_rtneural_json", modelBytes);
        const bool modelReady = keyDetector.loadModel (modelJson, (size_t) juce::jmax (0, modelBytes));
        juce::Logger::writeToLog (juce::String ("key model -> ") + (modelReady ? "ready" : "FAILED"));
    }
    apvts.addParameterListener (ParamID::body, this);
    startTimer (250);
}
PluginProcessor::~PluginProcessor()
{
    // Order matters. The worker reads keyDetector, so it stops (bounded, 2 s)
    // before anything it touches goes away.
    keyWorker.stop();
    stopTimer();
    apvts.removeParameterListener (ParamID::body, this);
    cancelPendingUpdate();
}
const juce::String PluginProcessor::getName() const { return "TRENCH"; }
bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}
bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}
bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}
double PluginProcessor::getTailLengthSeconds() const { return dspBridge.tailSeconds(); }
int PluginProcessor::getNumPrograms() { return 1; }
int PluginProcessor::getCurrentProgram() { return 0; }
void PluginProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return "TRENCH";
}
void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}
bool PluginProcessor::loadRuntimePresetForCurrentRate()
{
    if (! loadedRuntimePreset.isValid())
        return false;
    const double rate = getSampleRate() > 0.0 ? getSampleRate()
                                              : TrenchDspBridge::kBodyDatumRate;
    const int bank = loadedRuntimePreset.bankForRate (rate);
    if (bank < 0)
        return false;
    loadedRuntimePresetBankRate = loadedRuntimePreset.bankRates[(size_t) bank];
    buildRuntimePresetProbeMirror (bank);
    return dspBridge.loadRuntimePresetBank (
        loadedRuntimePreset.name,
        loadedRuntimePreset.bankWords[(size_t) bank],
        loadedRuntimePreset.activeStages,
        loadedRuntimePresetBankRate);
}
// UI-only: pad the selected bank's 1-3 active stages out to six with the
// identity sentinel so the curve display has 240 words to probe. Never fed to
// the engine (the engine took the unpadded bank) and never exported.
void PluginProcessor::buildRuntimePresetProbeMirror (int bank)
{
    runtimePresetProbeBytes.reset();
    if (! loadedRuntimePreset.isValid()
        || bank < 0 || bank >= (int) loadedRuntimePreset.bankWords.size())
        return;
    const auto& src = loadedRuntimePreset.bankWords[(size_t) bank];
    const int stages = loadedRuntimePreset.activeStages;
    if (src.size() != (size_t) (4 * stages * 5))
        return;
    // The pad sentinel — same row Morph Designer and body240 use; decodes to
    // the exact unity biquad, so the padded stages are electrically invisible.
    static constexpr unsigned short kIdentity[5] =
        { 0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000 };
    std::vector<unsigned short> padded;
    padded.reserve (120);
    for (int corner = 0; corner < 4; ++corner)
        for (int stage = 0; stage < 6; ++stage)
            if (stage < stages)
            {
                const size_t base = (size_t) (corner * stages * 5 + stage * 5);
                padded.insert (padded.end(), src.begin() + (long) base,
                               src.begin() + (long) base + 5);
            }
            else
                padded.insert (padded.end(), kIdentity, kIdentity + 5);
    runtimePresetProbeBytes.setSize (240);
    auto* out = static_cast<unsigned char*> (runtimePresetProbeBytes.getData());
    for (size_t i = 0; i < padded.size(); ++i)
    {
        out[i * 2]     = (unsigned char) (padded[i] & 0xff);
        out[i * 2 + 1] = (unsigned char) ((padded[i] >> 8) & 0xff);
    }
}
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock bodyLock (bodyStateLock);
    dspBridge.prepare (sampleRate, samplesPerBlock);
#if TRENCH_DEV_PANEL
    calibrationMorph = -1.0f;
    calibrationOutputGain = 1.0f;
#endif
    previousDriveGain = 1.0f;
    dspBridge.setRingLeveller (false);
    transientDetector.prepare (sampleRate);
    // The selected bank is a function of the host rate: a preset chosen at
    // 44.1k is an octave out once the host moves to 96k. prepare() has just
    // reset the engine, so re-select and re-load whenever the rate moved.
    if (loadedRuntimePreset.isValid())
    {
        const int bank = loadedRuntimePreset.bankForRate (sampleRate);
        if (bank >= 0
            && loadedRuntimePreset.bankRates[(size_t) bank] != loadedRuntimePresetBankRate)
        {
            loadedRuntimePresetBankRate = loadedRuntimePreset.bankRates[(size_t) bank];
            dspBridge.loadRuntimePresetBank (
                loadedRuntimePreset.name,
                loadedRuntimePreset.bankWords[(size_t) bank],
                loadedRuntimePreset.activeStages,
                loadedRuntimePresetBankRate);
        }
    }
    setLatencySamples (0);
    dspBridge.setInputMode (kCleanInputMode);
    dspBridge.setSpatialMode (kSpatialOff);
    dspBridge.setQSoundFallbackPan (1.0f);
    movement.prepare (sampleRate);
    wheelRampFrom = trench::curves::curveMap (trench::curves::Axis::morph, juce::jlimit (0.0f, 1.0f, pMorph->load()));
    // The trajectory buffer is the audio thread's — sized here, never touched
    // by the allocator again.
    morphBuffer.assign ((size_t) juce::jmax (samplesPerBlock, 1), 0.0f);
    effectiveMorphForUi.store (pMorph->load(), std::memory_order_relaxed);
    effectiveQForUi.store (pQ->load(), std::memory_order_relaxed);
    morphModulatedForUi.store (false, std::memory_order_relaxed);
    preparedBlockSize = juce::jmax (1, samplesPerBlock);
    // ~1 s analysis windows: the first useful AUTO KEY verdict lands at
    // loop-creation speed instead of the heritage 17.8 s.
    // The worker owns analyse(); stop it before re-sizing the capture slots
    // underneath it, and start it again once they are the new size.
    keyWorker.stop();
    keyDetector.prepare (sampleRate, 1.0);
    keyWorker.start();
}
void PluginProcessor::releaseResources()
{
    keyWorker.stop();
    keyDetector.reset();
}
bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif
    return true;
  #endif
}
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        if (! message.isNoteOn())
            continue;
        noteTrackRatio.store ((float) std::pow (2.0, (message.getNoteNumber() - 60) / 12.0),
                              std::memory_order_relaxed);
        noteBite.store (message.getFloatVelocity(), std::memory_order_relaxed);
        noteLatched.store (true, std::memory_order_relaxed);
        break;
    }
    // 1. Surplus output channels carry silence, not garbage.
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    const int numSamples = buffer.getNumSamples();
    // 2. A body that failed to load returns DRY audio — never mute the track.
    if (! lastLoadOk.load (std::memory_order_acquire) || numSamples <= 0 || morphBuffer.empty())
        return;
    // 3. The engine cannot be freed while this scope is alive. One acquisition
    //    covers every engine touch below, telemetry included.
    TrenchDspBridge::AudioScope engineScope (dspBridge);
    // 4. OVERSIZED HOST BLOCK. JUCE: "the host may well pass a larger block".
    //    morphBuffer and monoScratch are sized from the PREPARED size, and
    //    processBlock may not allocate. Dropping to dry
    //    (what this did before) makes a whole block silent-of-effect at exactly
    //    the moment the host changes its buffer — audible, and nondeterministic.
    //    So walk the block in prepared-size chunks. The channel pointer array is
    //    a fixed stack array; nothing here allocates.
    const int maxChunk = juce::jmax (1, juce::jmin (preparedBlockSize, (int) morphBuffer.size()));
    if (numSamples > maxChunk)
    {
        static constexpr int kMaxChannels = 8;
        const int channels = juce::jmin (kMaxChannels, buffer.getNumChannels());
        if (channels <= 0)
            return;
        for (int start = 0; start < numSamples; start += maxChunk)
        {
            float* chans[kMaxChannels];
            const int n = juce::jmin (maxChunk, numSamples - start);
            for (int c = 0; c < channels; ++c)
                chans[c] = buffer.getWritePointer (c) + start;
            juce::AudioBuffer<float> slice (chans, channels, n);
            processChunk (slice, start);
        }
        return;
    }
    processChunk (buffer);
}
void PluginProcessor::processChunk (juce::AudioBuffer<float>& buffer, int sampleOffset)
{
    const int numSamples = buffer.getNumSamples();
#if TRENCH_DEV_PANEL
    trench::calibration::Values calibration {};
    for (size_t i = 0; i < calibration.size(); ++i)
    {
        const auto& d = trench::calibration::variables[i];
        const float v = calibrationParameters[i]->load (std::memory_order_relaxed);
        calibration[i] = std::isfinite (v) ? juce::jlimit (d.low, d.high, v) : d.initial;
    }
    dspBridge.applyCalibration (calibration);
    const auto rms = [&buffer, numSamples]
    {
        double power = 0.0;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            for (int i = 0; i < numSamples; ++i) power += (double) buffer.getSample (c, i) * buffer.getSample (c, i);
        return (float) std::sqrt (power / (double) juce::jmax (1, numSamples * buffer.getNumChannels()));
    };
    calibrationInputRms.store (rms(), std::memory_order_relaxed);
#endif
    // 3. Every parameter, read once from the cached atomics.
    // EVERY MACRO READS THROUGH ITS TABLE (plugin/tools/gen_curves.py). The
    // tables are identity until a bisection session has measured that axis.
    using trench::curves::Axis;
    using trench::curves::curveMap;
    const float baseMorph = curveMap (Axis::morph,  juce::jlimit (0.0f, 1.0f, pMorph->load()));
    const float q         = curveMap (Axis::q,      juce::jlimit (0.0f, 1.0f, pQ->load()));
    const int movePreset  = (int) pMovePreset->load();
    const int moveTransition = (int) pMoveTransition->load();
    const int keyChoice   = juce::jlimit (0, 24, (int) pKeySnap->load());
    // 4. THE INPUT METER IS THE INPUT. Taken here from the untouched buffer,
    //    not after the cascade and output stage, where it becomes an output
    //    meter wearing an input label.
    float dryPeakL = 0.0f, dryPeakR = 0.0f;
    if (editorOpen.load (std::memory_order_relaxed))
    {
        auto dryPeakOf = [&buffer, numSamples] (int channel)
        {
            if (channel >= buffer.getNumChannels())
                return 0.0f;
            const auto* d = buffer.getReadPointer (channel);
            float peak = 0.0f;
            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax (peak, std::abs (d[i]));
            return juce::jlimit (0.0f, 1.0f, peak);
        };
        dryPeakL = dryPeakOf (0);
        dryPeakR = dryPeakOf (1);
    }
    // 5. AUTO KEY hears the dry input whenever AUTO is selected — editor or
    //    no editor. A manual key bypasses detection immediately.
    if (keyChoice == 0)
        keyDetector.pushAudio (buffer);
    // 6. Host timing, read once.
    trench::MovementTransport transport;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) transport.bpm = *b;
            if (auto p = pos->getPpqPosition()) transport.ppq = *p;
            transport.playing = pos->getIsPlaying();
            if (auto signature = pos->getTimeSignature())
                if (signature->numerator > 0 && signature->denominator > 0)
                    transport.beatsPerBar = signature->numerator * 4.0 / signature->denominator;
        }
    if (sampleOffset > 0 && transport.ppq >= 0.0 && getSampleRate() > 0.0)
        transport.ppq += (double) sampleOffset * transport.bpm / 60.0 / getSampleRate();
    // 7. The per-sample Morph trajectory — the ONLY movement law.
    const bool customMotion = pMoveCustom->load() > 0.5f;
    if (customMotion) userMotion.readAudio (audioMotion);
    const auto customPattern = audioMotion.pattern();
    movement.render (morphBuffer.data(), numSamples, baseMorph, transport,
                     movePreset, customMotion ? 0 : moveTransition, (int) pMoveLength->load(),
                     (int) pMovePlayback->load(), movementRestart.load (std::memory_order_relaxed),
                     customMotion ? &customPattern : nullptr, customMotion ? audioMotion.loopSteps : 0);
    const float wheel = baseMorph;
    const float from = wheelRampFrom < 0.0f ? wheel : wheelRampFrom;
    const float step = (wheel - from) / (float) juce::jmax (1, numSamples);
    for (int i = 0; i < numSamples; ++i)
        morphBuffer[(size_t) i] = juce::jlimit (0.0f, 1.0f, from + step * (float) (i + 1) + morphBuffer[(size_t) i]);
    wheelRampFrom = wheel;
#if TRENCH_DEV_PANEL
    wheelLoopSource.process (morphBuffer.data(), numSamples, transport.ppq, transport.playing, transport.bpm, getSampleRate());
    const float alpha = calibration[3] > 0.0f ? (float) std::exp (-1.0 / (0.001 * calibration[3] * getSampleRate())) : 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float target = morphBuffer[(size_t) i];
        calibrationMorph = calibrationMorph < 0.0f ? target : target + alpha * (calibrationMorph - target);
        morphBuffer[(size_t) i] = calibrationMorph;
    }
#endif
    // 8. Static controls that changed since last block.
    const float outputAmount = juce::jlimit (0.0f, 1.0f, pOutput->load());
    const float inputDrive = trench::preampGain (juce::jlimit (0.0f, 1.0f, pPreamp->load()));
    dspBridge.setInputDrive (inputDrive);
    dspBridge.setOutputDrive (outputAmount, trench::deskCompensationGain (outputAmount));
    TrenchParams params;
    params.q = q;                       // the static authored second axis
    params.poleDistortion = 0.0f;
#if TRENCH_DEV_PANEL
    params.poleDistortion = calibration[5];
#endif
    // AUTO DETECTS. IT DOES NOT RETUNE.
    //
    // This line used to substitute the DETECTOR'S guess for the player's
    // choice, so the default state — AUTO — handed the filter's tuning to
    // whatever the plug-in happened to be hearing. Fed pink noise it heard G
    // minor and moved the whole geometry about a semitone, which is most of why
    // a capture of an X3 preset did not sound like the X3. The X3 has no key
    // tracking at all; nothing that retunes the filter belongs in the state a
    // preset is judged in.
    //
    // Nothing is lost. The detector still runs, and KeySnapBox still shows what
    // it heard as a primary and secondary suggestion you can click to commit
    // (applySuggestedChoice). Snapping is now something the player asks for.
    // 0 = no snap.
    params.keySnap = keyChoice;
    params.noteLatched = noteLatched.load (std::memory_order_relaxed);
    params.noteTrackRatio = noteTrackRatio.load (std::memory_order_relaxed);
    params.noteBite = noteBite.load (std::memory_order_relaxed);
    const float driveGain = 1.0f
#if TRENCH_DEV_PANEL
        * std::pow (10.0f, calibration[4] / 20.0f)
#endif
        ;
    buffer.applyGainRamp (0, numSamples, previousDriveGain, driveGain);
    previousDriveGain = driveGain;
    dspBridge.processTrajectory (buffer, morphBuffer.data(), params);
#if TRENCH_DEV_PANEL
    const float monitorGain = std::pow (10.0f, calibration[16] / 20.0f);
    buffer.applyGainRamp (0, numSamples, calibrationOutputGain, monitorGain);
    calibrationOutputGain = monitorGain;
#endif
#if TRENCH_DEV_PANEL
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < numSamples; ++i)
            d[i] = std::isfinite (d[i]) ? d[i] : 0.0f;
    }
    const float limitFrac = dspBridge.caughtFractionForUi();
#else
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < numSamples; ++i)
            d[i] = std::isfinite (d[i]) ? juce::jlimit (-trench::kFinalSafetyCeiling, trench::kFinalSafetyCeiling, d[i]) : 0.0f;
    }
    const float limitFrac = dspBridge.caughtFractionForUi();
#endif
#if TRENCH_DEV_PANEL
    calibrationOutputRms.store (rms(), std::memory_order_relaxed);
    calibrationCeilingFraction.store (limitFrac, std::memory_order_relaxed);
    for (size_t i = 0; i < calibration.size(); ++i)
        calibrationReceived[i].store (calibration[i], std::memory_order_relaxed);
    calibrationBlocks.fetch_add (1, std::memory_order_release);
#endif
    // 12. Telemetry only while the editor is looking.
    if (editorOpen.load (std::memory_order_relaxed))
    {
        auto smoothMeter = [] (std::atomic<float>& target, float next)
        {
            const float prev = target.load (std::memory_order_relaxed);
            target.store (next > prev ? next : prev * 0.86f + next * 0.14f,
                          std::memory_order_relaxed);
        };
        smoothMeter (inputMeterL, dryPeakL);
        smoothMeter (inputMeterR, dryPeakR);
        const float effective = morphBuffer[(size_t) numSamples - 1];
        effectiveMorphForUi.store (effective, std::memory_order_relaxed);
        morphUpdatesForUi.fetch_add (1, std::memory_order_relaxed);
        effectiveQForUi.store (q, std::memory_order_relaxed);
        morphModulatedForUi.store (customMotion || (movePreset >= 1 && movePreset <= trench::kNumFuncGenPatterns)
#if TRENCH_DEV_PANEL
                                   || (wheelLoopSource.currentMode() == trench::WheelLoop::Mode::Playing
                                       && transport.playing && transport.ppq >= 0.0 && transport.bpm > 0.0)
                                   || std::abs (effective - baseMorph) > 0.00001f
#endif
                                   ,
                                   std::memory_order_relaxed);
        const float prevClip = outClipForUi.load (std::memory_order_relaxed);
        outClipForUi.store (juce::jmax (limitFrac, prevClip * 0.90f), std::memory_order_relaxed);
        gritActivityForUi.store (dspBridge.gritActivity(), std::memory_order_relaxed);
        effectiveBiteForUi.store (dspBridge.gritActivity(), std::memory_order_relaxed);
        dspBridge.publishUiSnapshot();
    }
}
void PluginProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamID::body)
    {
        const int raw = juce::roundToInt (newValue);
        // An out-of-range index must never resolve to a DIFFERENT preset. The
        // old modulo wrap silently loaded some other body when a saved project
        // named a slot this roster no longer has; land on NO FILTER instead.
        const int wanted = (raw >= 0 && raw < trench::bodyCount()) ? raw : trench::kNoFilterIndex;
        pendingBodyIndex.store (wanted, std::memory_order_relaxed);
        triggerAsyncUpdate();
    }
}
void PluginProcessor::handleAsyncUpdate()
{
    const juce::ScopedLock bodyLock (bodyStateLock);
    const int want = pendingBodyIndex.load (std::memory_order_relaxed);
    if (want == loadedBodyIndex.load (std::memory_order_relaxed))
        return;
    const bool wasLive = lastLoadOk.load (std::memory_order_acquire);
    juce::MemoryBlock raw;
    juce::String json;
    if (! trench::bodyRawBytes (want, raw))
    {
        json = trench::bodyCartridgeJson (want);
        if (json.isNotEmpty() && trench::isRuntimePresetJson (json))
        {
            loadedRuntimePreset = trench::parseRuntimePreset (json);
            const bool ok = loadRuntimePresetForCurrentRate();
            if (ok)
            {
                currentBodyBytes.reset();
                rosterBodyBytes.reset();
                currentBodyDatumRate = 0.0;
            }
            else { loadedRuntimePreset = {}; }
            lastLoadOk.store (ok || wasLive, std::memory_order_release);
            loadedBodyIndex.store (want, std::memory_order_relaxed);
            bodyInjected.store (false, std::memory_order_relaxed);
            bodyVersionForUi.fetch_add (1, std::memory_order_relaxed);
            return;
        }
        if (json.isNotEmpty())
            TrenchDspBridge::bodyBytesFromJson (json, raw);
    }
    loadedRuntimePreset = {};
    // UI updates first — curve draws the new body immediately.
    bodyVersionForUi.fetch_add (1, std::memory_order_relaxed);
    bool ok;
    if (raw.getSize() == 240)
    {
        // reloadCartridgeBytes keeps cascade states alive — no click
        ok = dspBridge.reloadCartridgeBytes (raw);
        currentBodyBytes = raw;
        uiBodyBytes = raw;
        currentBodyDatumRate = TrenchDspBridge::kBodyDatumRate;
        rosterBodyBytes = currentBodyBytes;
    }
    else
    {
        ok = json.isNotEmpty() && dspBridge.loadCartridge (json);
        captureCurrentBodyBytes (json);
    }
    lastLoadOk.store (ok || wasLive, std::memory_order_release);
    loadedBodyIndex.store (want, std::memory_order_relaxed);
    bodyInjected.store (false, std::memory_order_relaxed);
    if (trench::bodyIsAudition (want))
        auditionSlotMtime = trench::auditionSlotFile().getLastModificationTime();
    juce::Logger::writeToLog (juce::String ("body switch -> ")
                               + trench::bodyDisplayName (want)
                               + (ok ? " ok" : " FAIL"));
}
void PluginProcessor::captureCurrentBodyBytes (const juce::String& cartridgeJson)
{
    const juce::ScopedLock bodyLock (bodyStateLock);
    if (cartridgeJson.isEmpty() || ! TrenchDspBridge::bodyBytesFromJson (cartridgeJson, currentBodyBytes))
        currentBodyBytes.setSize (0);
    currentBodyDatumRate = TrenchDspBridge::kBodyDatumRate;
    rosterBodyBytes = currentBodyBytes;
    bodyVersionForUi.fetch_add (1, std::memory_order_relaxed);
}
bool PluginProcessor::seedCurrentBody()
{
    // The sibling-seeding experiment retired with trench_seed_body; the
    // editor affordance stays until the five-point cleanup rules on it.
    return false;
}
void PluginProcessor::exportCurrentBody()
{
    const juce::ScopedLock bodyLock (bodyStateLock);
    if (currentBodyBytes.getSize() != 240)
        captureCurrentBodyBytes (trench::bodyCartridgeJson (loadedBodyIndex.load (std::memory_order_relaxed)));
    if (currentBodyBytes.getSize() != 240)
        return;
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("TRENCH")
                   .getChildFile ("exports");
    dir.createDirectory();
    auto base = trench::bodyDisplayName (loadedBodyIndex.load (std::memory_order_relaxed))
                    .retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ")
                    .replaceCharacter (' ', '_');
    if (base.isEmpty())
        base = "body";
    juce::File f;
    for (int i = 1; i < 10000; ++i)
    {
        f = dir.getChildFile (base + "_" + juce::String (i).paddedLeft ('0', 3) + ".body240");
        if (! f.existsAsFile())
            break;
    }
    if (f.replaceWithData (currentBodyBytes.getData(), currentBodyBytes.getSize()))
        juce::Logger::writeToLog ("EXPORT body -> " + f.getFullPathName());
}
void PluginProcessor::forgeAuditionTyped (const std::vector<double>& cards)
{
    const juce::ScopedLock bodyLock (bodyStateLock);
    juce::MemoryBlock body;
    if (! TrenchDspBridge::compileTypedBody (cards.data(), (int) cards.size(), body)
        || body.getSize() != 240)
    {
        juce::Logger::writeToLog ("FORGE compile FAILED");
        return;
    }
    if (installBodyBytes (body.getData(), body.getSize()))
    {
        rosterBodyBytes = currentBodyBytes;
        juce::Logger::writeToLog ("FORGE -> typed body compiled + auditioned");
    }
}
juce::File PluginProcessor::forgeSaveBody (const juce::String& name, bool overwrite)
{
    juce::ignoreUnused (name, overwrite);
    return {};
}
bool PluginProcessor::installBodyBytes (const void* bytes, size_t len, double datumRate)
{
    const juce::ScopedLock bodyLock (bodyStateLock);
    if (bytes == nullptr || len != 240)
        return false;
    if (! dspBridge.loadCartridgeBytes (bytes, len, datumRate))
        return false;
    currentBodyBytes = juce::MemoryBlock (bytes, len);
    bodyInjected.store (true, std::memory_order_relaxed);
    currentBodyDatumRate = datumRate;
    bodyVersionForUi.fetch_add (1, std::memory_order_relaxed);
    lastLoadOk.store (true, std::memory_order_release);
    return true;
}
bool PluginProcessor::copyCurrentBodyBytes (void* out, size_t len)
{
    const juce::ScopedLock bodyLock (bodyStateLock);
    if (out == nullptr || len != 240)
        return false;
    if (currentBodyBytes.getSize() != 240)
        captureCurrentBodyBytes (trench::bodyCartridgeJson (loadedBodyIndex.load (std::memory_order_relaxed)));
    if (currentBodyBytes.getSize() != 240)
        return false;
    std::memcpy (out, currentBodyBytes.getData(), 240);
    return true;
}
bool PluginProcessor::probeCurrentBodyForUi (float morph, float q, float outCoeffs[trench::kUiCoeffCount], float& outBoost)
{
    const juce::ScopedLock bodyLock (bodyStateLock);
    if (outCoeffs == nullptr)
        return false;
    // A runtime preset has no 240-word interchange form — it is a bank
    // selection, and currentBodyBytes is deliberately empty so export
    // and the datum path cannot treat it as an Hz-anchored body. The curve
    // still needs something to probe, so mirror the SELECTED bank (padded to
    // six stages) and probe it verbatim, datum 0.
    if (loadedRuntimePreset.isValid() && runtimePresetProbeBytes.getSize() == 240)
        return TrenchDspBridge::probePackedBody (
            runtimePresetProbeBytes.getData(), runtimePresetProbeBytes.getSize(),
            morph, q, getSampleRate() > 0.0 ? getSampleRate() : 48'000.0,
            outCoeffs, outBoost, 0.0);
    if (currentBodyBytes.getSize() != 240)
        captureCurrentBodyBytes (trench::bodyCartridgeJson (loadedBodyIndex.load (std::memory_order_relaxed)));
    if (currentBodyBytes.getSize() != 240)
        return false;
    const int keyChoice = juce::jlimit (0, 24, (int) pKeySnap->load());
    const double ratio = TrenchDspBridge::transposeRatio (
        noteLatched.load (std::memory_order_relaxed),
        noteTrackRatio.load (std::memory_order_relaxed),
        keyChoice);
    return TrenchDspBridge::probePackedBody (currentBodyBytes.getData(), currentBodyBytes.getSize(),
                                             morph, q,
                                             getSampleRate() > 0.0 ? getSampleRate() : 48'000.0,
                                             outCoeffs, outBoost,
                                             currentBodyDatumRate,
                                             ratio);
}
// AUTO KEY hysteresis. Runs on the WORKER (never the message thread), editor
// open or not — a 131,072-point FFT and an RTNeural pass do not belong on the
// thread that draws the host. candidateKey/candidateCount/acceptedKey are the
// worker's own state; everything the UI reads leaves through atomics. Windows are
// ~1 s; the first useful verdict is one confident window (~1.3 s worst case)
// or two agreeing windows (~2.3 s). An accepted key holds until a DIFFERENT
// key wins three consecutive useful windows — no flapping. Unknown stays
// unknown; the engine snaps to nothing rather than a fabricated C.
void PluginProcessor::updateAutoKey()
{
    if (juce::jlimit (0, 24, (int) pKeySnap->load()) != 0)
        return;   // manual key: detection is bypassed at the processBlock tap
    trench::KeyDetector::Result window;
    while (keyDetector.analyse (window))
    {
        keyConfidenceForUi.store (window.confidence, std::memory_order_relaxed);
        const bool useful = window.confidence >= 0.20f && window.margin >= 0.03f;
        if (! useful)
            continue;   // a weak window neither builds nor tears down a verdict
        if (window.labelIndex == candidateKey)
            ++candidateCount;
        else
        {
            candidateKey = window.labelIndex;
            candidateCount = 1;
        }
        const bool confident = window.confidence >= 0.40f && window.margin >= 0.10f;
        if (acceptedKey < 0)
        {
            if (confident || candidateCount >= 2)
                acceptedKey = candidateKey;
        }
        else if (candidateKey != acceptedKey && candidateCount >= 3)
        {
            acceptedKey = candidateKey;   // clearly displaced
        }
        detectedKeyForUi.store (acceptedKey, std::memory_order_relaxed);
    }
}

void PluginProcessor::timerCallback()
{
    const juce::ScopedLock bodyLock (bodyStateLock);
    dspBridge.reclaim();
    // user bodies hot-reload in place: the Workstation saves, the plugin
    // follows - no TYPE menu round trip. Only disk-loaded .body240 bodies.
    {
        const auto base = trench::bodyBaseForIndex (loadedBodyIndex.load (std::memory_order_relaxed));
        if (juce::File::isAbsolutePath (base) && base.endsWithIgnoreCase (".body240"))
        {
            const juce::File f (base);
            const auto t = f.existsAsFile() ? f.getLastModificationTime() : juce::Time();
            if (base != watchedBodyPath)
            {
                watchedBodyPath = base;   // new selection: arm, don't reload
                watchedBodyMtime = t;
            }
            else if (t != watchedBodyMtime)
            {
                watchedBodyMtime = t;
                juce::MemoryBlock raw;
                if (f.loadFileAsData (raw) && raw.getSize() == 240
                    && dspBridge.loadCartridgeBytes (raw))
                {
                    currentBodyBytes = raw;
                    uiBodyBytes = raw;
                    currentBodyDatumRate = TrenchDspBridge::kBodyDatumRate;
                    rosterBodyBytes = currentBodyBytes;
                    bodyVersionForUi.fetch_add (1, std::memory_order_relaxed);
                    lastLoadOk.store (true, std::memory_order_release);
                    juce::Logger::writeToLog ("body hot-reload <- " + f.getFullPathName());
                }
            }
        }
        else if (watchedBodyPath.isNotEmpty())
            watchedBodyPath.clear();
    }
#ifdef TRENCH_PLAYER_DIAGNOSTICS
    if (! trench::bodyIsAudition (loadedBodyIndex.load (std::memory_order_relaxed)))
        return;
    auto slot = trench::auditionSlotFile();
    if (! slot.existsAsFile())
        return;
    const auto t = slot.getLastModificationTime();
    if (t == auditionSlotMtime)
        return;
    auditionSlotMtime = t;
    const auto json = slot.loadFileAsString();
    if (json.isEmpty())
        return;
    const bool ok = dspBridge.loadCartridge (json);
    if (ok)
        lastLoadOk.store (true, std::memory_order_release);
    captureCurrentBodyBytes (json);
#endif
}
bool PluginProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("userMotion", juce::JSON::toString (userMotion.get().toJson(), true), nullptr);
#if TRENCH_DEV_PANEL
    {
        const juce::ScopedLock lock (calibrationArchiveLock);
        state.addChild (calibrationArchive.createCopy(), -1, nullptr);
    }
#endif
    // BODY travels by stable id, never by index alone: the roster appends the
    // user's bodies folder, so slot N names a different filter on another
    // machine or after any roster edit.
    state.setProperty ("bodyId",
                       trench::bodyBaseForIndex (
                           juce::roundToInt (apvts.getRawParameterValue (ParamID::body)->load())),
                       nullptr);
    // MOVEMENT travels by NAME, never by index alone (same law as BODY). A
    // future original-bank edit must not re-author saved projects.
    if (auto* mp = apvts.getParameter (ParamID::movePreset))
        state.setProperty ("movePresetName", mp->getCurrentValueAsText(), nullptr);
#ifdef TRENCH_PLAYER_EXTRAS
    state.setProperty ("clean_audio_enabled",
                       juce::var (trench::clean_audio::kEnabled()),
                       nullptr);
#endif
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}
void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xmlState);
        trench::UserMotion restoredMotion;
        const bool motionValid = trench::UserMotion::fromJson (juce::JSON::parse (tree.getProperty ("userMotion").toString()), restoredMotion);
        userMotion.set (restoredMotion);
        tree.removeProperty ("userMotion", nullptr);
#if TRENCH_DEV_PANEL
        {
            const juce::ScopedLock lock (calibrationArchiveLock);
            auto session = tree.getChildWithName ("DevCalibrationSession");
            calibrationArchive = session.isValid() ? session.createCopy() : juce::ValueTree ("DevCalibrationSession");
            if (session.isValid()) tree.removeChild (session, nullptr);
        }
#endif
        const auto bodyId = tree.getProperty ("bodyId").toString();
        const auto moveName = tree.getProperty ("movePresetName").toString();
        // Discard retired controls that have no processing effect. Live
        // parameter IDs remain unchanged so existing projects retain them.
        for (int i = tree.getNumChildren(); --i >= 0;)
        {
            const auto id = tree.getChild (i).getProperty ("id").toString();
            if (id == "amount" || id == "outputTrim" || id == ParamID::slamDrive || id == ParamID::chew || id == ParamID::deskPosition
                || id == ParamID::distortion
#if TRENCH_DEV_PANEL
                || std::any_of (std::begin (trench::calibration::retired), std::end (trench::calibration::retired),
                    [&id] (const char* retired) { return id == retired; })
#endif
               )
                tree.removeChild (i, nullptr);
        }
        // MIGRATION, stated once and applied to every parameter.
        //
        // The rewrite dropped modOn/modTrigger/modShape/modNote/modFeel/
        // modDepth/bloom and added movePreset/track. JUCE keys a VST3
        // parameter by its id STRING, so no removed id's value can arrive at a
        // new control — an old session simply has no child for the new ones,
        // and replaceState leaves a parameter it finds no child for at
        // WHATEVER THE LIVE INSTANCE HAPPENED TO BE SHOWING. That is the bug:
        // opening an old project inherited the previous patch's MOVEMENT and
        // TRACK. (It had already been patched by hand for CHEW alone.)
        //
        // So: absent from the saved tree means DEFAULT. Nothing is guessed —
        // the removed modulation has no equivalent among the curated MOVEMENT
        // phrases, and inventing a mapping would put a value on the face that
        // the session never held.
        juce::StringArray absentFromState;
        for (auto* parameter : getParameters())
            if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                if (! tree.getChildWithProperty ("id", withID->paramID).isValid())
                    absentFromState.add (withID->paramID);
#ifdef TRENCH_PLAYER_EXTRAS
        if (tree.hasProperty ("clean_audio_enabled"))
            trench::clean_audio::setEnabled (static_cast<bool> (tree.getProperty ("clean_audio_enabled")));
#endif
        apvts.replaceState (std::move (tree));
        if (! motionValid) setParameterDenormalized (ParamID::moveCustom, 0.0f);
        for (const auto& id : absentFromState)
            if (auto* parameter = apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
        if (bodyId.isNotEmpty())
        {
            // A project opens SOUNDING AS SAVED: restoring a body is a load, not
            // a travel. The flag is consumed by the async body swap this write
            // queues (handleAsyncUpdate).
            int index = trench::bodyIndexForBase (bodyId);
            if (index < 0)
            {
                trench::rescanBodyRoster();     // the saved body may be a user file
                index = trench::bodyIndexForBase (bodyId);
            }
            // A body this machine does not have lands on NO FILTER - never on
            // whatever else happens to occupy the saved index.
            setParameterDenormalized (ParamID::body,
                                      (float) (index >= 0 ? index : trench::kNoFilterIndex));
        }
        // MOVEMENT recalls by name: the saved pattern is found in the CURRENT
        // bank, wherever it sits today. A name this build does not have lands
        // on OFF - never on whatever else occupies the saved index.
        if (moveName.isNotEmpty())
            if (auto* mp = apvts.getParameter (ParamID::movePreset))
            {
                const int idx = mp->getAllValueStrings().indexOf (moveName);
                setParameterDenormalized (ParamID::movePreset,
                                          (float) (idx >= 0 ? idx : 0));
            }
    }
    if (trench::clean_audio::kEnabled())
        forceCleanAudioUiState();
    // A host may render immediately after restore without dispatching UI
    // messages. Finish the body load here; the callback only consumes the
    // published snapshot and never takes bodyStateLock or allocates a body.
    const int restoredIndex = juce::roundToInt (apvts.getRawParameterValue (ParamID::body)->load());
    pendingBodyIndex.store (restoredIndex >= 0 && restoredIndex < trench::bodyCount()
                               ? restoredIndex : trench::kNoFilterIndex,
                           std::memory_order_relaxed);
    handleAsyncUpdate();
}
void PluginProcessor::setParameterDenormalized (const char* parameterID, float value)
{
    if (auto* parameter = apvts.getParameter (parameterID))
    {
        const float normalized = parameter->convertTo0to1 (value);
        if (std::abs (parameter->getValue() - normalized) > 0.000001f)
            parameter->setValueNotifyingHost (normalized);
    }
}
void PluginProcessor::forceCleanAudioUiState()
{
    setParameterDenormalized (ParamID::body, (float) trench::kNoFilterIndex);
    setParameterDenormalized (ParamID::movePreset, 0.0f);
    setParameterDenormalized (ParamID::moveTransition, 0.0f);
    setParameterDenormalized (ParamID::moveLength, 0.0f);
    setParameterDenormalized (ParamID::movePlayback, 0.0f);
    setParameterDenormalized (ParamID::moveCustom, 0.0f);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
trench::UserMotion PluginProcessor::motionForEditing() const
{
    auto motion = usingUserMotion() ? userMotion.get()
        : trench::UserMotion::copyFactory ((int) pMovePreset->load(), (int) pMoveTransition->load(),
                                           (int) pMoveLength->load(), (int) pMovePlayback->load());
    if (usingUserMotion())
    {
        motion.length = (int) pMoveLength->load();
        motion.playback = (int) pMovePlayback->load();
    }
    return motion;
}
void PluginProcessor::applyUserMotion (const trench::UserMotion& motion, bool restart)
{
    trench::UserMotion valid;
    if (! trench::UserMotion::fromJson (motion.toJson(), valid)) return;
    userMotion.set (valid);
    for (const auto& item : { std::pair<const char*, float> { ParamID::moveLength, (float) valid.length },
                             { ParamID::movePlayback, (float) valid.playback }, { ParamID::moveCustom, 1.0f } })
    {
        auto* p = apvts.getParameter (item.first);
        const auto value = p->convertTo0to1 (item.second);
        if (p->getValue() != value)
        {
            p->beginChangeGesture(); p->setValueNotifyingHost (value); p->endChangeGesture();
        }
    }
    if (restart) restartMovement();
    updateHostDisplay();
}
