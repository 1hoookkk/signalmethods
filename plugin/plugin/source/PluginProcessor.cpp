#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TrenchBodyRoster.h"
#include "dsp/SlamStage.h"
#include "BinaryData.h"
#include <cmath>
#include <cstring>
#include <limits>
namespace
{
constexpr int kCleanInputMode = 0;
constexpr int kMackieDeskSlam = 1;
constexpr int kSpatialOff = 2;
// PERCEPTUAL DRIVE TAPER (Tyson 2026-08-01 "too easy to destroy the sound"):
// the desk model's input stage is 1+99*drive — linear in GAIN, so +21 dB
// arrives inside the first 10% of PREAMP's travel and the rest of the throw
// is identical mush. Remap the knob linear-in-dB over the SAME 0..+40 dB
// range (half throw = +20 dB). Endpoints exact; the clean-roomed Mackie
// model itself stays verbatim.
float driveTaper (float k) noexcept
{
    k = juce::jlimit (0.0f, 1.0f, k);
    return k <= 0.0f ? 0.0f : (std::pow (10.0f, 2.0f * k) - 1.0f) * (1.0f / 99.0f);
}
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
    pQ          = apvts.getRawParameterValue (ParamID::q);
    pChew       = apvts.getRawParameterValue (ParamID::chew);
    pMix        = apvts.getRawParameterValue (ParamID::amount);
    pSlam       = apvts.getRawParameterValue (ParamID::slamDrive);
    pPreamp     = apvts.getRawParameterValue (ParamID::preamp);
    pFollow     = apvts.getRawParameterValue (ParamID::envAmount);
    pTrack      = apvts.getRawParameterValue (ParamID::track);
    pMovePreset = apvts.getRawParameterValue (ParamID::movePreset);
    pMoveDivision = apvts.getRawParameterValue (ParamID::moveDivision);
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
const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }
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
double PluginProcessor::getTailLengthSeconds() const { return 0.0; }
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
    dspBridge.prepare (sampleRate, samplesPerBlock);
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
    lastPreampActive = false;
    movement.prepare (sampleRate);
    // The trajectory buffer is the audio thread's — sized here, never touched
    // by the allocator again.
    morphBuffer.assign ((size_t) juce::jmax (samplesPerBlock, 1), 0.0f);
    effectiveMorphForUi.store (pMorph->load(), std::memory_order_relaxed);
    effectiveQForUi.store (pQ->load(), std::memory_order_relaxed);
    morphModulatedForUi.store (false, std::memory_order_relaxed);
    punchBlend.prepare (sampleRate, 0, samplesPerBlock);
    punchBlend.setLatency (0);
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
// Orchestration only. The preserved chain is:
//   dry tap -> Input/PREAMP -> six-section packed filter + BITE/CHEW -> AGC
//   -> output saturation/DC protection -> OUTPUT/SLAM -> wet safety ceiling
//   -> MIX
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
    // 1. Surplus output channels carry silence, not garbage.
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    const int numSamples = buffer.getNumSamples();
    // 2. A body that failed to load returns DRY audio — never mute the track.
    if (! lastLoadOk.load (std::memory_order_acquire) || numSamples <= 0)
        return;
    // 3. The engine cannot be freed while this scope is alive. One acquisition
    //    covers every engine touch below, telemetry included.
    TrenchDspBridge::AudioScope engineScope (dspBridge);
    // 4. OVERSIZED HOST BLOCK. JUCE: "the host may well pass a larger block".
    //    morphBuffer, monoScratch and PunchBlend's dry store are all sized from
    //    the PREPARED size, and processBlock may not allocate. Dropping to dry
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
            processChunk (slice);
        }
        return;
    }
    processChunk (buffer);
}
static constexpr float kX3VoiceGain = 1.6107f;
void PluginProcessor::processChunk (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    // 3. Every parameter, read once from the cached atomics.
    const float baseMorph = juce::jlimit (0.0f, 1.0f, pMorph->load());
    const float q         = juce::jlimit (0.0f, 1.0f, pQ->load());
    const float chew      = juce::jlimit (0.0f, 1.0f, pChew->load());
    const float mix       = juce::jlimit (0.0f, 1.0f, pMix->load());
    const float slam      = pSlam->load();
    const float preamp    = juce::jlimit (0.0f, 1.0f, pPreamp->load());
    const float follow    = juce::jlimit (0.0f, 1.0f, pFollow->load());
    const float track     = juce::jlimit (0.0f, 1.0f, pTrack->load());
    const int movePreset  = (int) pMovePreset->load();
    const int moveDivision = (int) pMoveDivision->load();
    const int keyChoice   = juce::jlimit (0, 24, (int) pKeySnap->load());
    // 4. True dry capture, before anything touches the buffer, for MIX.
    punchBlend.captureDry (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), numSamples);
    // 4b. THE INPUT METER IS THE INPUT. Taken here, from the same untouched
    //     buffer the dry capture just read — not after the cascade, SLAM and
    //     MIX, which is what it had drifted to and which made it an output
    //     meter wearing an input label.
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
        }
    // 7. The per-sample Morph trajectory — the ONLY movement law.
    movement.render (morphBuffer.data(), numSamples, baseMorph, transport, movePreset,
                     livePhraseValid.load (std::memory_order_acquire)
                         ? &livePhrase[livePhraseSlot.load (std::memory_order_acquire)].desc
                         : nullptr,
                     trench::Movement::stepBeatsFor (moveDivision));
    // 8. Static controls that changed since last block.
    const bool preampActive = preamp > 0.001f;
    if (preampActive != lastPreampActive)
    {
        dspBridge.setInputMode (preampActive ? kMackieDeskSlam : kCleanInputMode);
        lastPreampActive = preampActive;
    }
    dspBridge.setInputPreamp (driveTaper (preamp));   // same dB-linear law as SLAM
    TrenchParams params;
    params.q = q;                       // the static authored second axis
    params.poleDistortion = chew;       // BITE/CHEW, independent of Q
    params.envAmount = follow;          // FOLLOW: the one authoritative detector
    // GROWL is a movement source rendered inside the engine (it needs the
    // detected note). The dumb button: selected = on, anything else = off.
    params.growl = movePreset == trench::Movement::kGrowlIndex ? 1.0f : 0.0f;
    params.track = track;               // the Hz axis: geography follows the note
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
    // 9. The wet path — mono and stereo both traverse the real engine.
    dspBridge.processTrajectory (buffer, morphBuffer.data(), params);
    buffer.applyGain (kX3VoiceGain);
    // 10. OUTPUT/SLAM once, part of the WET voice, before MIX.
    float limitFrac = 0.0f;
    if (buffer.getNumChannels() >= 2)
        limitFrac = trench::slamOutputPressureBlockStereo (buffer.getWritePointer (0),
                                                           buffer.getWritePointer (1),
                                                           numSamples, slam);
    else if (buffer.getNumChannels() == 1)
        limitFrac = trench::slamOutputPressureBlock (buffer.getWritePointer (0),
                                                     numSamples, slam);
    // 12. MIX blend; MIX 0 returns the untouched dry signal, sample-identical.
    punchBlend.blend (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), numSamples, mix);
    // 13. Telemetry only while the editor is looking.
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
        effectiveQForUi.store (q, std::memory_order_relaxed);
        morphModulatedForUi.store (std::abs (effective - baseMorph) > 0.0005f,
                                   std::memory_order_relaxed);
        const float prevClip = outClipForUi.load (std::memory_order_relaxed);
        outClipForUi.store (juce::jmax (limitFrac, prevClip * 0.90f), std::memory_order_relaxed);
        gritActivityForUi.store (dspBridge.gritActivity(), std::memory_order_relaxed);
        agcReductionDbForUi.store (dspBridge.agcReductionDb(), std::memory_order_relaxed);
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
    const int want = pendingBodyIndex.load (std::memory_order_relaxed);
    if (want == loadedBodyIndex.load (std::memory_order_relaxed))
        return;
    lastLoadOk.store (false, std::memory_order_release);
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
            lastLoadOk.store (ok, std::memory_order_release);
            loadedBodyIndex.store (want, std::memory_order_relaxed);
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
    lastLoadOk.store (ok, std::memory_order_release);
    loadedBodyIndex.store (want, std::memory_order_relaxed);
    if (trench::bodyIsAudition (want))
        auditionSlotMtime = trench::auditionSlotFile().getLastModificationTime();
    juce::Logger::writeToLog (juce::String ("body switch -> ")
                               + trench::bodyDisplayName (want)
                               + (ok ? " ok" : " FAIL"));
}
void PluginProcessor::captureCurrentBodyBytes (const juce::String& cartridgeJson)
{
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
    juce::MemoryBlock body;
    if (! TrenchDspBridge::compileTypedBody (cards.data(), (int) cards.size(), body)
        || body.getSize() != 240)
    {
        lastLoadOk.store (false, std::memory_order_release);
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
    if (currentBodyBytes.getSize() != 240)
        return {};
    // canonical body home = repo bodies/candidates ("no documents", 2026-07-25)
    auto dir = juce::File ("C:/Users/hooki/trench-workstation/bodies/candidates");
    dir.createDirectory();
    auto base = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ")
                    .trim().replaceCharacter (' ', '_');
    if (base.isEmpty())
        base = "forge";
    auto f = dir.getChildFile (base + ".body240");
    // designer saves own their name: overwrite in place so the loaded plugin
    // hot-follows the same file. Classic saves keep suffixing.
    for (int i = 1; ! overwrite && f.existsAsFile() && i < 10000; ++i)
        f = dir.getChildFile (base + "_" + juce::String (i).paddedLeft ('0', 2) + ".body240");
    if (f.replaceWithData (currentBodyBytes.getData(), currentBodyBytes.getSize()))
    {
        juce::Logger::writeToLog ("FORGE save -> " + f.getFullPathName());
        return f;
    }
    return {};
}
bool PluginProcessor::installBodyBytes (const void* bytes, size_t len, double datumRate)
{
    if (bytes == nullptr || len != 240)
        return false;
    if (! dspBridge.loadCartridgeBytes (bytes, len, datumRate))
        return false;
    currentBodyBytes = juce::MemoryBlock (bytes, len);
    currentBodyDatumRate = datumRate;
    bodyVersionForUi.fetch_add (1, std::memory_order_relaxed);
    lastLoadOk.store (true, std::memory_order_release);
    return true;
}
bool PluginProcessor::copyCurrentBodyBytes (void* out, size_t len)
{
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
    return TrenchDspBridge::probePackedBody (currentBodyBytes.getData(), currentBodyBytes.getSize(),
                                             morph, q,
                                             getSampleRate() > 0.0 ? getSampleRate() : 48'000.0,
                                             outCoeffs, outBoost,
                                             currentBodyDatumRate);
}
// The live channel: filters/phrase_live.json holds the single phrase being
// drawn. Parse on the message thread into the slot the audio thread is NOT
// reading, then publish by flipping the index — no lock, no allocation on the
// audio thread. Absent file simply means the LIVE slot stays silent.
void PluginProcessor::pollLivePhrase()
{
    if (livePhraseFile == juce::File())
        livePhraseFile = juce::File (TRENCH_TABLE_STITCH_ROOT)
                             .getChildFile ("filters").getChildFile ("phrase_live.json");
    if (! livePhraseFile.existsAsFile())
        return;
    const auto stamp = livePhraseFile.getLastModificationTime();
    if (stamp == livePhraseMtime)
        return;
    livePhraseMtime = stamp;

    const auto parsed = juce::JSON::parse (livePhraseFile);
    const auto* vals = parsed.getProperty ("values", {}).getArray();
    if (vals == nullptr || vals->isEmpty())
        return;

    const int idle = 1 - livePhraseSlot.load (std::memory_order_relaxed);
    auto& dst = livePhrase[idle];
    const int n = juce::jlimit (1, kLiveCells, (int) vals->size());
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
        peak = juce::jmax (peak, std::abs ((float) (double) (*vals)[i]));
    if (peak < 1.0e-6f)
        return;                                  // no travel: keep what we had
    for (int i = 0; i < kLiveCells; ++i)
    {
        dst.values[i] = i < n ? juce::jlimit (-1.0f, 1.0f, (float) (double) (*vals)[i] / peak) : 0.0f;
        dst.trigs[i] = 0;
    }
    dst.desc.steps = n;
    dst.desc.direction = juce::jlimit (0, 5, (int) parsed.getProperty ("direction", 0));
    dst.desc.smooth = (bool) parsed.getProperty ("smooth", true);
    livePhraseSlot.store (idle, std::memory_order_release);
    livePhraseValid.store (true, std::memory_order_release);
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
        // The alternate suggestion is the window's runner-up (or its winner,
        // when that differs from the accepted key) — the same second opinion
        // the suggestion plate has always offered.
        int second = window.labelIndex == 0 ? 1 : 0;
        for (int i = 0; i < 24; ++i)
            if (i != window.labelIndex
                && window.probabilities[(size_t) i] > window.probabilities[(size_t) second])
                second = i;
        detectedAltKeyForUi.store (window.labelIndex == acceptedKey ? second : window.labelIndex,
                                   std::memory_order_relaxed);
    }
}

void PluginProcessor::timerCallback()
{
    pollLivePhrase();
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
    lastLoadOk.store (false, std::memory_order_release);
    const bool ok = dspBridge.loadCartridge (json);
    lastLoadOk.store (ok, std::memory_order_release);
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
    // BODY travels by stable id, never by index alone: the roster appends the
    // user's bodies folder, so slot N names a different filter on another
    // machine or after any roster edit.
    state.setProperty ("bodyId",
                       trench::bodyBaseForIndex (
                           juce::roundToInt (apvts.getRawParameterValue (ParamID::body)->load())),
                       nullptr);
    // MOVEMENT travels by NAME, never by index alone (same law as BODY, added
    // 2026-08-15 when the bank went from 9 curated phrases to the 56 E-mu
    // patterns and every saved index silently remapped). A future bank edit
    // must not re-author saved projects.
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
        const auto bodyId = tree.getProperty ("bodyId").toString();
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
        const auto moveName = tree.getProperty ("movePresetName").toString();
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
    setParameterDenormalized (ParamID::chew, 0.0f);
    setParameterDenormalized (ParamID::slamDrive, 0.0f);
    setParameterDenormalized (ParamID::movePreset, 0.0f);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
