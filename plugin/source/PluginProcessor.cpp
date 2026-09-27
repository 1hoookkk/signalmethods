#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TrenchBodyRoster.h"
#include "BodyAxes.h"
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
constexpr double kMovementReturnSeconds = 0.25;
int snapChoiceForDetection (int label) noexcept
{
    if (label < 0 || label >= 24)
        return 0;
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
    pMorph      = apvts.getRawParameterValue (ParamID::morph);
#if TRENCH_DEV_PANEL
    for (size_t i = 0; i < trench::calibration::count; ++i)
        calibrationParameters[i] = apvts.getRawParameterValue (trench::calibration::variables[i].id);
#endif
    pQ          = apvts.getRawParameterValue (ParamID::q);
    for (const bool isMorph : { true, false })
        if (auto* axis = dynamic_cast<TrenchParameters::AxisParameter*> (apvts.getParameter (isMorph ? ParamID::morph : ParamID::q)))
            axis->setNameSource ([this, isMorph] {
                const auto names = trench::axisNamesForBody (loadedBodyIndex.load (std::memory_order_relaxed));
                return trench::hostAxisName (isMorph ? names.morph : names.q);
            });
    pPreamp     = apvts.getRawParameterValue (ParamID::preamp);
    pOutput     = apvts.getRawParameterValue (ParamID::output);
    pMovePreset = apvts.getRawParameterValue (ParamID::movePreset);
    pMoveTransition = apvts.getRawParameterValue (ParamID::moveTransition);
    pMoveLength = apvts.getRawParameterValue (ParamID::moveLength);
    pMovePlayback = apvts.getRawParameterValue (ParamID::movePlayback);
    pMoveCustom = apvts.getRawParameterValue (ParamID::moveCustom);
    userMotion.readAudio (audioMotion);
    pKeySnap    = apvts.getRawParameterValue (ParamID::keySnap);
    pSlam       = apvts.getRawParameterValue (ParamID::inputSlam);
    if (trench::clean_audio::kEnabled())
        forceCleanAudioUiState();
    const int startIndex = juce::jlimit (0, juce::jmax (0, trench::bodyCount() - 1),
                                         (int) apvts.getRawParameterValue (ParamID::body)->load());
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
    keyWorker.stop();
    stopTimer();
    apvts.removeParameterListener (ParamID::body, this);
    cancelPendingUpdate();
}
const juce::String PluginProcessor::getName() const { return "TRENCH"; }
bool PluginProcessor::acceptsMidi() const
{
    return true;
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
    morphWasHeld = false;
    movementDepth = 1.0f;
    movementReturnStep = (float) (1.0 / (kMovementReturnSeconds * sampleRate));
    morphBuffer.assign ((size_t) juce::jmax (samplesPerBlock, 1), 0.0f);
    qBuffer.assign ((size_t) juce::jmax (samplesPerBlock, 1), 0.0f);
    effectiveMorphForUi.store (pMorph->load(), std::memory_order_relaxed);
    effectiveQForUi.store (pQ->load(), std::memory_order_relaxed);
    morphModulatedForUi.store (false, std::memory_order_relaxed);
    preparedBlockSize = juce::jmax (1, samplesPerBlock);
    keyWorker.stop();
    keyEvidence.fill (0.0);
    keyWindows = 0;
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
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    const int numSamples = buffer.getNumSamples();
    if (! lastLoadOk.load (std::memory_order_acquire) || numSamples <= 0 || morphBuffer.empty())
        return;
    TrenchDspBridge::AudioScope engineScope (dspBridge);
    static constexpr int kMaxNotes = 64;
    std::array<int, kMaxNotes> noteAt {};
    int notes = 0;
    for (const auto metadata : midiMessages)
        if (metadata.getMessage().isNoteOn() && notes < kMaxNotes)
            noteAt[(size_t) notes++] = juce::jlimit (0, numSamples - 1, metadata.samplePosition);
    const int maxChunk = juce::jmax (1, juce::jmin (preparedBlockSize, (int) morphBuffer.size()));
    if (numSamples <= maxChunk && notes == 0)
    {
        processChunk (buffer);
        return;
    }
    static constexpr int kMaxChannels = 8;
    const int channels = juce::jmin (kMaxChannels, buffer.getNumChannels());
    if (channels <= 0)
        return;
    int next = 0;
    for (int start = 0; start < numSamples;)
    {
        while (next < notes && noteAt[(size_t) next] <= start)
        {
            restartMovement();
            ++next;
        }
        int end = juce::jmin (numSamples, start + maxChunk);
        if (next < notes)
            end = juce::jmin (end, noteAt[(size_t) next]);
        float* chans[kMaxChannels];
        for (int c = 0; c < channels; ++c)
            chans[c] = buffer.getWritePointer (c) + start;
        juce::AudioBuffer<float> slice (chans, channels, end - start);
        processChunk (slice, start);
        start = end;
    }
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
    using trench::curves::Axis;
    using trench::curves::curveMap;
    const bool held = morphHeld.load (std::memory_order_acquire);
    const float baseMorph = curveMap (Axis::morph,  juce::jlimit (0.0f, 1.0f, pMorph->load()));
    const float q         = curveMap (Axis::q,      juce::jlimit (0.0f, 1.0f, pQ->load()));
    float qHeard = q;
    bool qFollowing = false;
    const int movePreset  = (int) pMovePreset->load();
    const int moveTransition = (int) pMoveTransition->load();
    const int keyChoice   = juce::jlimit (0, 24, (int) pKeySnap->load());
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
    if (keyChoice == 0)
        keyDetector.pushAudio (buffer);
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
    const bool customMotion = pMoveCustom->load() > 0.5f;
    if (customMotion) userMotion.readAudio (audioMotion);
    const auto customPattern = audioMotion.pattern();
    movement.render (morphBuffer.data(), numSamples, transport,
                     movePreset, customMotion ? 0 : moveTransition, (int) pMoveLength->load(),
                     (int) pMovePlayback->load(), movementRestart.load (std::memory_order_relaxed),
                     customMotion ? &customPattern : nullptr, customMotion ? audioMotion.loopSteps : 0);
    const float wheel = baseMorph;
    if (held && ! morphWasHeld)
    {
        wheelRampFrom = wheel;
        movementDepth = 0.0f;
    }
    morphWasHeld = held;
    float low = 0.0f, high = 0.0f;
    if (customMotion)
        trench::Movement::travel (customPattern, low, high);
    else if (movePreset >= 1 && movePreset <= trench::kNumFuncGenPatterns)
        trench::Movement::travel (trench::kFuncGenPatterns[movePreset - 1], low, high);
    const float from = wheelRampFrom < 0.0f ? wheel : wheelRampFrom;
    const float step = (wheel - from) / (float) juce::jmax (1, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        movementDepth = held ? 0.0f : juce::jmin (1.0f, movementDepth + movementReturnStep);
        const float room = juce::jmax (0.0f, 1.0f - movementDepth * (high - low));
        const float place = (from + step * (float) (i + 1)) * room;
        morphBuffer[(size_t) i] = juce::jlimit (0.0f, 1.0f, place + movementDepth * (morphBuffer[(size_t) i] - low));
    }
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
    {
        using trench::calibration::indexOf;
        const bool follow1 = calibration[indexOf ("cal_follow_1")] > 0.5f;
        const bool follow2 = calibration[indexOf ("cal_follow_2")] > 0.5f;
        if (follow1 || follow2)
        {
            const double fs = juce::jmax (1.0, getSampleRate());
            const float attack = (float) std::exp (-1.0 / (0.001 * calibration[indexOf ("cal_follow_attack")] * fs));
            const float release = (float) std::exp (-1.0 / (0.001 * calibration[indexOf ("cal_follow_release")] * fs));
            const float floorDb = calibration[indexOf ("cal_follow_floor")];
            const float topDb = juce::jmax (floorDb + 1.0f, calibration[indexOf ("cal_follow_top")]);
            const float depth = calibration[indexOf ("cal_follow_depth")];
            const float drive = juce::Decibels::decibelsToGain (pPreamp->load());
            const int channels = juce::jmin (2, buffer.getNumChannels());
            for (int i = 0; i < numSamples; ++i)
            {
                float x = 0.0f;
                for (int c = 0; c < channels; ++c) x = juce::jmax (x, std::abs (buffer.getSample (c, i)));
                x *= drive;
                followEnvelope = x + (x > followEnvelope ? attack : release) * (followEnvelope - x);
                const float db = juce::Decibels::gainToDecibels (followEnvelope, -120.0f);
                followPush = depth * juce::jlimit (0.0f, 1.0f, (db - floorDb) / (topDb - floorDb));
                if (follow1)
                    morphBuffer[(size_t) i] = juce::jlimit (0.0f, 1.0f, morphBuffer[(size_t) i] + followPush);
                qBuffer[(size_t) i] = juce::jlimit (0.0f, 1.0f, q + followPush);
            }
            if (follow2)
            {
                qHeard = qBuffer[(size_t) numSamples - 1];
                qFollowing = true;
            }
        }
        else
        {
            followEnvelope = 0.0f;
            followPush = 0.0f;
        }
    }
#endif
    dspBridge.setInputDrive (juce::Decibels::decibelsToGain (pPreamp->load()));
    dspBridge.setOutputLevel (juce::Decibels::decibelsToGain (pOutput->load()));
    dspBridge.setInputSlam (pSlam->load() > 0.5f);
    TrenchParams params;
    params.q = qHeard;
    params.poleDistortion = 0.0f;
#if TRENCH_DEV_PANEL
    params.poleDistortion = calibration[5];
#endif
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
    dspBridge.processTrajectory (buffer, morphBuffer.data(), params, qFollowing ? qBuffer.data() : nullptr);
#if TRENCH_DEV_PANEL
    const float monitorGain = std::pow (10.0f, calibration[16] / 20.0f);
    buffer.applyGainRamp (0, numSamples, calibrationOutputGain, monitorGain);
    calibrationOutputGain = monitorGain;
#endif
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* d = buffer.getWritePointer (c);
        for (int i = 0; i < numSamples; ++i)
            d[i] = std::isfinite (d[i]) ? d[i] : 0.0f;
    }
    const float limitFrac = dspBridge.caughtFractionForUi();
#if TRENCH_DEV_PANEL
    calibrationOutputRms.store (rms(), std::memory_order_relaxed);
    calibrationCeilingFraction.store (limitFrac, std::memory_order_relaxed);
    for (size_t i = 0; i < calibration.size(); ++i)
        calibrationReceived[i].store (calibration[i], std::memory_order_relaxed);
    calibrationBlocks.fetch_add (1, std::memory_order_release);
#endif
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
        effectiveQForUi.store (qHeard, std::memory_order_relaxed);
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
    bodyVersionForUi.fetch_add (1, std::memory_order_relaxed);
    bool ok;
    if (raw.getSize() == 240)
    {
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
    return TrenchDspBridge::probePackedBody (currentBodyBytes.getData(), currentBodyBytes.getSize(),
                                             morph, q,
                                             getSampleRate() > 0.0 ? getSampleRate() : 48'000.0,
                                             outCoeffs, outBoost,
                                             currentBodyDatumRate,
                                             keyChoice);
}
void PluginProcessor::updateAutoKey()
{
    if (juce::jlimit (0, 24, (int) pKeySnap->load()) != 0)
        return;
    trench::KeyDetector::Result window;
    while (keyDetector.analyse (window))
    {
        for (size_t k = 0; k < keyEvidence.size(); ++k)
            keyEvidence[k] = keyEvidence[k] * kKeyForget + std::log (juce::jmax (1.0e-4, (double) window.probabilities[k]));
        ++keyWindows;
        size_t best = 0;
        for (size_t k = 1; k < keyEvidence.size(); ++k)
            if (keyEvidence[k] > keyEvidence[best])
                best = k;
        double spread = 0.0;
        for (const double e : keyEvidence)
            spread += std::exp (e - keyEvidence[best]);
        keyConfidenceForUi.store ((float) (1.0 / spread), std::memory_order_relaxed);
        if (keyWindows >= kKeyMinWindows)
            detectedKeyForUi.store ((int) best, std::memory_order_relaxed);
    }
}

void PluginProcessor::timerCallback()
{
    const juce::ScopedLock bodyLock (bodyStateLock);
    dspBridge.reclaim();
    {
        const auto names = trench::axisNamesForBody (loadedBodyIndex.load (std::memory_order_relaxed));
        const auto published = juce::String (names.morph) + "/" + names.q;
        if (published != publishedAxisNames)
        {
            publishedAxisNames = published;
            updateHostDisplay (ChangeDetails().withParameterInfoChanged (true));
        }
    }
    {
        const auto base = trench::bodyBaseForIndex (loadedBodyIndex.load (std::memory_order_relaxed));
        if (juce::File::isAbsolutePath (base) && base.endsWithIgnoreCase (".body240"))
        {
            const juce::File f (base);
            const auto t = f.existsAsFile() ? f.getLastModificationTime() : juce::Time();
            if (base != watchedBodyPath)
            {
                watchedBodyPath = base;
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
    state.setProperty ("bodyId",
                       trench::bodyBaseForIndex (
                           juce::roundToInt (apvts.getRawParameterValue (ParamID::body)->load())),
                       nullptr);
    if (auto* mp = apvts.getParameter (ParamID::movePreset))
        state.setProperty ("movePresetName", mp->getCurrentValueAsText(), nullptr);
    {
        const juce::ScopedLock bodyLock (bodyStateLock);
        if (currentBodyBytes.getSize() == 240)
            state.setProperty ("bodyBytes", currentBodyBytes.toBase64Encoding(), nullptr);
    }
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
    juce::MemoryBlock recoveredBytes;
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
        const auto bodyBytesText = tree.getProperty ("bodyBytes").toString();
        tree.removeProperty ("bodyBytes", nullptr);
        const auto moveName = tree.getProperty ("movePresetName").toString();
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
            int index = trench::bodyIndexForBase (bodyId);
            if (index < 0)
            {
                trench::rescanBodyRoster();
                index = trench::bodyIndexForBase (bodyId);
            }
            if (index < 0)
                index = recoverBodyFromState (bodyId, bodyBytesText, recoveredBytes);
            else if (bodyBytesText.isNotEmpty())
            {
                juce::MemoryBlock embedded, library;
                if (embedded.fromBase64Encoding (bodyBytesText) && embedded.getSize() == 240
                    && trench::bodyRawBytes (index, library) && library != embedded)
                    recoveredBytes = embedded;
            }
            setParameterDenormalized (ParamID::body,
                                      (float) (index >= 0 ? index : trench::kNoFilterIndex));
        }
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
    const int restoredIndex = juce::roundToInt (apvts.getRawParameterValue (ParamID::body)->load());
    pendingBodyIndex.store (restoredIndex >= 0 && restoredIndex < trench::bodyCount()
                               ? restoredIndex : trench::kNoFilterIndex,
                           std::memory_order_relaxed);
    handleAsyncUpdate();
    if (recoveredBytes.getSize() == 240)
    {
        const juce::ScopedLock bodyLock (bodyStateLock);
        if (currentBodyBytes != recoveredBytes)
            installBodyBytes (recoveredBytes.getData(), recoveredBytes.getSize());
    }
}
int PluginProcessor::recoverBodyFromState (const juce::String& bodyId, const juce::String& bodyBytesText, juce::MemoryBlock& unlisted)
{
    juce::MemoryBlock bytes;
    if (bodyBytesText.isEmpty() || ! bytes.fromBase64Encoding (bodyBytesText) || bytes.getSize() != 240)
        return -1;
    unlisted = bytes;
    const int count = trench::bodyCount();
    for (int i = 0; i < count; ++i)
    {
        juce::MemoryBlock existing;
        if (! trench::bodyIsNoFilter (i) && trench::bodyRawBytes (i, existing) && existing == bytes)
        {
            unlisted.reset();
            return i;
        }
    }
    auto stem = juce::File (bodyId).getFileNameWithoutExtension();
    if (! juce::File::isAbsolutePath (bodyId))
        stem = bodyId;
    stem = stem.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ").trim();
    if (stem.isEmpty() || stem.startsWithChar ('_'))
        stem = "recovered body";
    const auto directory = bodyRecoveryDirectory;
    if (! directory.createDirectory())
        return -1;
    juce::File target = directory.getChildFile (stem + ".body240");
    if (target.existsAsFile())
        target = directory.getChildFile (stem + " " + juce::String::toHexString (bytes.getData(), 4, 0) + ".body240");
    if (target.existsAsFile() || ! target.replaceWithData (bytes.getData(), bytes.getSize()))
        return -1;
    trench::rescanBodyRoster();
    const int index = trench::bodyIndexForBase (target.getFullPathName());
    if (index >= 0)
        unlisted.reset();
    juce::Logger::writeToLog ("body recovered from project state -> " + target.getFullPathName()
                              + (index >= 0 ? "" : " (not listed)"));
    return index;
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
void PluginProcessor::holdMorph (bool hold)
{
    if (hold && morphModulatedForUi.load (std::memory_order_relaxed))
        setParameterDenormalized (ParamID::morph,
                                  trench::curves::uncurveMap (trench::curves::Axis::morph,
                                                              effectiveMorphForUi.load (std::memory_order_relaxed)));
    morphHeld.store (hold, std::memory_order_release);
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
