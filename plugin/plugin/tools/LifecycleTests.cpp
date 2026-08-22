// THE RUNTIME PROOF. Everything here is a thing a host actually did to the
// plug-in and the plug-in did not survive, or did survive dishonestly. It runs
// headless, with no DAW and no listening step.
//
//   1. create / prepare / process / release / destroy, many times over, with
//      the engine count proving every engine was freed and none leaked.
//   2. an audio callback still inside the engine when the plug-in is destroyed
//      — the FL case. The law says: leak exactly one engine, never free it
//      underneath. Proved by counting, not by hoping.
//   3. host blocks LARGER than the prepared size, and smaller, and ragged.
//      Deterministic: the same input through one big block and through the
//      chunks it decomposes into must land in the same place.
//   4. a saved session missing the parameters this build added must load them
//      at their DEFAULTS, not at whatever the live instance was showing.
//   5. AUTO KEY's worker starts, runs, and stops inside its bound at every
//      teardown — the message thread never runs an FFT.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TrenchBodyRoster.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const char* what)
{
    std::printf ("%-58s %s\n", what, condition ? "ok" : "FAIL");
    if (! condition)
        ++failures;
}

static const unsigned char kTalkingHedzBody[240] = {
    0xfc, 0x6c, 0xfc, 0xcf, 0xfc, 0xec, 0xfc, 0xb8, 0xfa, 0xd1, 0xfc, 0x90, 0xfc, 0xca, 0xfc, 0x84,
    0xfc, 0xb4, 0xfa, 0xd1, 0xfc, 0xa9, 0xfc, 0xc3, 0xfc, 0x9f, 0xfc, 0xb6, 0xfa, 0xd1, 0xfc, 0xbb,
    0xfc, 0xbb, 0xfc, 0xb1, 0xfc, 0x88, 0xfa, 0xd1, 0xfc, 0xe1, 0xfc, 0xeb, 0xfc, 0xcf, 0xfc, 0xc9,
    0xfa, 0xd1, 0xfc, 0xde, 0xf0, 0x01, 0xfb, 0x41, 0xfc, 0xa1, 0xfa, 0xd1, 0xfc, 0xa2, 0xfc, 0xcb,
    0xfc, 0xe7, 0xfc, 0xc2, 0xb0, 0xd0, 0xfc, 0x81, 0xfc, 0xc2, 0xfb, 0x41, 0xfc, 0x9f, 0xb0, 0xd0,
    0xfc, 0xb0, 0xfc, 0xda, 0xfc, 0xb1, 0xfc, 0xb1, 0xb0, 0xd0, 0xfc, 0xb9, 0xfc, 0xca, 0xfc, 0xb7,
    0xfc, 0xad, 0xb0, 0xd0, 0xfc, 0xd5, 0xfd, 0xf0, 0xfc, 0xd0, 0xfc, 0xcf, 0xb0, 0xd0, 0xfd, 0xfe,
    0xf0, 0x01, 0xfc, 0xa4, 0xfc, 0x8b, 0xb0, 0xd0, 0xfc, 0x6b, 0xfc, 0xd2, 0xfd, 0xf0, 0xfc, 0x6f,
    0x8e, 0xd1, 0xfc, 0x8f, 0xfc, 0xcb, 0xfc, 0x87, 0xfc, 0x6c, 0x8e, 0xd1, 0xfc, 0xa7, 0xfc, 0xc2,
    0xfc, 0x9d, 0xfc, 0x6b, 0x8e, 0xd1, 0xfc, 0xb9, 0xfc, 0xb7, 0xfc, 0xaf, 0xfc, 0x6a, 0x8e, 0xd1,
    0xfc, 0xe1, 0xfc, 0xee, 0xfc, 0xcd, 0xfc, 0x8d, 0x8e, 0xd1, 0xfc, 0xdb, 0xf0, 0x01, 0xf9, 0x34,
    0xfc, 0x6b, 0x8e, 0xd1, 0xfc, 0xa0, 0xfc, 0xc6, 0xfc, 0xeb, 0xfc, 0x6c, 0x5d, 0xd0, 0xfc, 0x80,
    0xfc, 0xc0, 0xf9, 0x3f, 0xfc, 0x64, 0x5d, 0xd0, 0xfc, 0xab, 0xfc, 0xda, 0xfc, 0xad, 0xfc, 0x68,
    0x5d, 0xd0, 0xfc, 0xb4, 0xfc, 0xca, 0xfc, 0xb2, 0xfc, 0x6a, 0x5d, 0xd0, 0xfc, 0xd2, 0xfd, 0xf2,
    0xfc, 0xcd, 0xfc, 0xbb, 0x5d, 0xd0, 0xfd, 0xfe, 0xf0, 0x01, 0xfc, 0x9d, 0xfc, 0x6d, 0x5d, 0xd0 };

void fillTone (juce::AudioBuffer<float>& buffer, double phaseStart = 0.0)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (ch, i, 0.4f * (float) std::sin (
                juce::MathConstants<double>::twoPi * 220.0 * ((double) i + phaseStart) / 48000.0));
}

void runBlocks (PluginProcessor& processor, const std::vector<int>& sizes, int channels = 2)
{
    juce::MidiBuffer midi;
    double phase = 0.0;
    for (int n : sizes)
    {
        juce::AudioBuffer<float> buffer (channels, n);
        fillTone (buffer, phase);
        processor.processBlock (buffer, midi);
        phase += n;
    }
}

// 1 + 5. Repeated open/process/close. Every engine created must be freed:
// nothing is in flight at any of these teardowns, so the leak branch must never
// be taken.
void testCreateProcessDestroyCycles()
{
    const int leakedBefore = TrenchDspBridge::leakedEngines().load();
    for (int cycle = 0; cycle < 8; ++cycle)
    {
        const int liveBefore = TrenchDspBridge::liveEngines().load();
        {
            PluginProcessor processor;
            processor.prepareToPlay (48000.0, 512);
            runBlocks (processor, { 512, 512, 256, 512 });
            processor.releaseResources();
            processor.prepareToPlay (44100.0, 128);
            runBlocks (processor, { 128, 128 });
            processor.releaseResources();
        }
        if (TrenchDspBridge::liveEngines().load() != liveBefore)
        {
            check (false, "cycle freed exactly the engines it created");
            return;
        }
    }
    check (true, "8x create/prepare/process/release/destroy frees every engine");
    check (TrenchDspBridge::leakedEngines().load() == leakedBefore,
           "no engine leaked when nothing was in flight");
}

// 1b. OPEN AND CLOSE THE WINDOW WHILE AUDIO RUNS. This is where the crash the
// dumps recorded actually fired: 0xC0000409 with fail-fast code 2, a /GS stack
// cookie failure, four times, always at the same address. The editor is the
// only thing that asks the core for coefficients — trench_engine_get_coeffs and
// trench_packed_probe_at each write NUM_STAGES * NUM_COEFFS, and when the
// cascade went from six sections to seven the C++ buffers stayed at 30. Both
// calls then wrote 5 floats past the end of a STACK array, which is why it
// fired the moment a window opened and never once headless.
//
// So: open, process, close, repeatedly, with the probe and the snapshot both
// live. kUiCoeffCount is asserted against the number the library itself
// reports, so this cannot silently go stale again.
void testEditorOpenCloseWhileProcessing()
{
    check (trench_engine_coeff_count() == trench::kUiCoeffCount,
           "the UI's coefficient count is the number the core reports");
    PluginProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    for (int cycle = 0; cycle < 6; ++cycle)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        if (editor == nullptr)
        {
            check (false, "the processor made an editor");
            return;
        }
        editor->setBounds (0, 0, editor->getWidth(), editor->getHeight());
        processor.setEditorOpen (true);       // telemetry + the curve probe go live
        runBlocks (processor, { 512, 512, 512 });
        // The editor's own per-frame probe, on the same stack buffers the crash
        // overran.
        float coeffs[trench::kUiCoeffCount] = {};
        float boost = 1.0f;
        processor.probeCurrentBodyForUi (0.25f, 0.5f, coeffs, boost);
        runBlocks (processor, { 512, 1024 });  // and an oversized one, window open
        processor.setEditorOpen (false);
        editor.reset();
        runBlocks (processor, { 512 });        // audio keeps running after close
    }
    processor.releaseResources();
    check (true, "6x open editor / process / probe / close while audio runs");
}

// 1c. REPEAT-INSTANCE DETERMINISM, at the WRAPPER level. The headless
// diagnostic saw its numbers deteriorate across successive engine
// create/destroy cycles and had to render every state in a FRESH PROCESS to get
// stable results. If that is real above the FFI, then two identically
// configured PluginProcessors render different audio, and every A/B ever run
// in one process is suspect.
//
// KEY is pinned to a manual choice: on AUTO the detector retunes the geometry
// from whatever it hears, which is a legitimate difference between instances
// and would mask the thing being tested.
void testRepeatInstanceDeterminism()
{
    constexpr int kN = 4096;
    auto render = [] (juce::AudioBuffer<float>& out)
    {
        PluginProcessor p;
        p.prepareToPlay (48000.0, 512);
        if (auto* mix = p.apvts.getParameter (ParamID::amount))
            mix->setValueNotifyingHost (1.0f);
        if (auto* key = p.apvts.getParameter (ParamID::keySnap))
            key->setValueNotifyingHost (13.0f / 24.0f);   // a manual key, never AUTO
        juce::MidiBuffer midi;
        out.setSize (2, kN);
        fillTone (out);
        for (int start = 0; start < kN; start += 512)
        {
            float* ch[2] = { out.getWritePointer (0) + start, out.getWritePointer (1) + start };
            juce::AudioBuffer<float> slice (ch, 2, 512);
            p.processBlock (slice, midi);
        }
        p.releaseResources();
    };

    juce::AudioBuffer<float> first;
    render (first);
    float worst = 0.0f;
    int worstInstance = 0;
    for (int instance = 1; instance < 8; ++instance)
    {
        juce::AudioBuffer<float> again;
        render (again);
        float d = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kN; ++i)
                d = juce::jmax (d, std::abs (again.getSample (ch, i) - first.getSample (ch, i)));
        if (d > worst) { worst = d; worstInstance = instance; }
    }
    std::printf ("   8 successive instances, worst deviation from the first: %.3e"
                 " (instance %d)\n", worst, worstInstance);
    check (worst == 0.0f, "successive plug-in instances render bit-identical audio");
}

// 2. THE FL CASE. Destroy the plug-in while a callback is inside the engine.
// The law: leak one engine rather than free it underneath. Anything else here
// is a use-after-free, and a use-after-free is what the crash dumps were.
void testDestroyWhileCallbackInFlight()
{
    const int leakedBefore = TrenchDspBridge::leakedEngines().load();
    auto bridge = std::make_unique<TrenchDspBridge>();
    bridge->prepare (48000.0, 256);
    std::atomic<bool> inside { false };
    std::atomic<bool> release { false };
    std::thread audio ([&]
    {
        TrenchDspBridge::AudioScope scope (*bridge);
        inside.store (true);
        while (! release.load())
            std::this_thread::yield();
    });
    while (! inside.load())
        std::this_thread::yield();
    bridge->retire();                 // the destroy, with the callback inside
    check (TrenchDspBridge::leakedEngines().load() == leakedBefore + 1,
           "destroy during an in-flight callback leaks, never frees");
    release.store (true);
    audio.join();
    bridge.reset();
}

// 3. Oversized and ragged host blocks. JUCE permits a block larger than the
// prepared size; the old guard returned the whole block DRY.
void testBlockSizes()
{
    PluginProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    // Nothing here may crash, and nothing may allocate on the audio path. The
    // sizes straddle the prepared size in both directions and do not divide it.
    runBlocks (processor, { 1, 255, 256, 257, 512, 1024, 2048, 333, 4096, 7 });
    runBlocks (processor, { 1, 255, 256, 257, 1024 }, 1);   // mono, same story
    check (true, "blocks 1..4096 around a prepared 256 survive, mono and stereo");

    // DETERMINISM. One oversized block must equal the chunks it decomposes
    // into: same input, same prepared size, same output, sample for sample.
    const int big = 1024;
    auto renderOneShot = [big] (juce::AudioBuffer<float>& out)
    {
        PluginProcessor p;
        p.prepareToPlay (48000.0, 256);
        juce::MidiBuffer midi;
        out.setSize (2, big);
        fillTone (out);
        p.processBlock (out, midi);
    };
    auto renderChunked = [big] (juce::AudioBuffer<float>& out)
    {
        PluginProcessor p;
        p.prepareToPlay (48000.0, 256);
        juce::MidiBuffer midi;
        out.setSize (2, big);
        fillTone (out);
        for (int start = 0; start < big; start += 256)
        {
            float* chans[2] = { out.getWritePointer (0) + start, out.getWritePointer (1) + start };
            juce::AudioBuffer<float> slice (chans, 2, 256);
            p.processBlock (slice, midi);
        }
    };
    juce::AudioBuffer<float> oneShot, chunked;
    renderOneShot (oneShot);
    renderChunked (chunked);
    float maxDelta = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < big; ++i)
            maxDelta = juce::jmax (maxDelta, std::abs (oneShot.getSample (ch, i)
                                                        - chunked.getSample (ch, i)));
    std::printf ("   one 1024 block vs four 256 blocks: max delta %.3e\n", maxDelta);
    check (maxDelta == 0.0f, "an oversized block equals the chunks it decomposes into");

    // And the oversized path must actually PROCESS, not pass dry through.
    PluginProcessor wet;
    check (wet.installBodyBytes (kTalkingHedzBody, sizeof (kTalkingHedzBody)),
           "a real body installs for the oversized-block check");
    wet.prepareToPlay (48000.0, 256);
    if (auto* mix = wet.apvts.getParameter (ParamID::amount))
        mix->setValueNotifyingHost (1.0f);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> input (2, 1024), output (2, 1024);
    fillTone (input);
    output.makeCopyOf (input);
    wet.processBlock (output, midi);
    float changed = 0.0f;
    for (int i = 0; i < 1024; ++i)
        changed = juce::jmax (changed, std::abs (output.getSample (0, i) - input.getSample (0, i)));
    check (changed > 1.0e-6f, "an oversized block is processed, not returned dry");
}

// 4. A session saved before this build's parameters existed. movePreset and
// track are simply absent from it; they must land on their DEFAULTS, not on
// whatever the instance was showing when the project opened.
void testStateMigration()
{
    PluginProcessor processor;
    // Move the live instance well away from the defaults first — that is the
    // state the old bug inherited.
    auto set = [&processor] (const char* id, float normalised)
    {
        if (auto* p = processor.apvts.getParameter (id))
            p->setValueNotifyingHost (normalised);
    };
    set (ParamID::movePreset, 1.0f);
    set (ParamID::track, 1.0f);
    set (ParamID::chew, 1.0f);
    const float defaultMove  = processor.apvts.getParameter (ParamID::movePreset)->getDefaultValue();
    const float defaultTrack = processor.apvts.getParameter (ParamID::track)->getDefaultValue();
    const float defaultChew  = processor.apvts.getParameter (ParamID::chew)->getDefaultValue();
    check (processor.apvts.getParameter (ParamID::track)->getValue() != defaultTrack,
           "the live instance starts away from the defaults");

    // An old state: the shared parameters, none of the new ones, none of the
    // removed ones reinterpreted.
    juce::ValueTree old (processor.apvts.state.getType());
    auto addParam = [&old] (const char* id, float value)
    {
        juce::ValueTree child ("PARAM");
        child.setProperty ("id", id, nullptr);
        child.setProperty ("value", value, nullptr);
        old.appendChild (child, nullptr);
    };
    addParam (ParamID::morph, 0.25f);
    addParam (ParamID::q, 0.75f);
    addParam (ParamID::amount, 0.5f);
    // ... and the modulation this build removed. None of it may reach a control.
    addParam ("modOn", 1.0f);
    addParam ("modDepth", 0.9f);
    addParam ("bloom", 0.8f);

    juce::MemoryBlock blob;
    {
        std::unique_ptr<juce::XmlElement> xml (old.createXml());
        juce::AudioProcessor::copyXmlToBinary (*xml, blob);
    }
    processor.setStateInformation (blob.getData(), (int) blob.getSize());

    check (juce::approximatelyEqual (processor.apvts.getParameter (ParamID::movePreset)->getValue(),
                                     defaultMove),
           "MOVEMENT absent from an old session loads at its default");
    check (juce::approximatelyEqual (processor.apvts.getParameter (ParamID::track)->getValue(),
                                     defaultTrack),
           "TRACK absent from an old session loads at its default");
    check (juce::approximatelyEqual (processor.apvts.getParameter (ParamID::chew)->getValue(),
                                     defaultChew),
           "CHEW absent from an old session loads at its default");
    // What the session DID carry must survive untouched.
    check (std::abs (processor.apvts.getRawParameterValue (ParamID::morph)->load() - 0.25f) < 1.0e-4f,
           "a parameter the old session carried is restored, not defaulted");

    // And a round trip of a CURRENT state must change nothing.
    set (ParamID::track, 0.6f);
    juce::MemoryBlock current;
    processor.getStateInformation (current);
    const float trackBefore = processor.apvts.getRawParameterValue (ParamID::track)->load();
    processor.setStateInformation (current.getData(), (int) current.getSize());
    check (std::abs (processor.apvts.getRawParameterValue (ParamID::track)->load() - trackBefore) < 1.0e-4f,
           "a current session round-trips unchanged");
}

// 5. AUTO KEY's worker. It must be running while audio is prepared, and it must
// be stopped — inside its bound — by releaseResources and by destruction. A
// shutdown that waits on an analysis window is the freeze this replaced.
void testAutoKeyWorkerLifecycle()
{
    const auto start = juce::Time::getMillisecondCounterHiRes();
    {
        PluginProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        // AUTO is parameter index 0 on keySnap; feed it real audio for a while.
        runBlocks (processor, std::vector<int> (64, 512));
        processor.releaseResources();
        processor.prepareToPlay (48000.0, 512);
        runBlocks (processor, std::vector<int> (16, 512));
        processor.releaseResources();
    }
    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;
    std::printf ("   worker start/stop cycle took %.0f ms\n", elapsed);
    // Two stops, each bounded at 2 s, plus the audio. Anything near or past the
    // bound means a stop that actually waited on an FFT.
    check (elapsed < 4000.0, "AUTO KEY worker starts and stops well inside its bound");
}
}

void testEveryRosterBodyLoads()
{
    PluginProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);
    int bodies = 0;
    int failed = 0;
    for (int index = 0; index < trench::bodyCount(); ++index)
    {
        juce::MemoryBlock raw;
        if (! trench::bodyRawBytes (index, raw))
            continue;
        ++bodies;
        if (! processor.installBodyBytes (raw.getData(), raw.getSize()))
        {
            ++failed;
            std::printf ("   %s did not install\n", trench::bodyDisplayName (index).toRawUTF8());
        }
    }
    processor.releaseResources();
    std::printf ("   %d baked 240-byte bodies\n", bodies);
    check (bodies > 0 && failed == 0, "every baked 240-byte body in the roster installs");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    testCreateProcessDestroyCycles();
    testEditorOpenCloseWhileProcessing();
    testRepeatInstanceDeterminism();
    testDestroyWhileCallbackInFlight();
    testBlockSizes();
    testStateMigration();
    testAutoKeyWorkerLifecycle();
    testEveryRosterBodyLoads();
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL",
                 failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
