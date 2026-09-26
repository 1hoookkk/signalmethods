#include "dsp/TrenchDspBridge.h"
#include "TestFixtures.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <complex>
#include <cstdio>

namespace
{
int failures = 0;

void check (bool ok, const char* label)
{
    std::printf ("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (! ok) ++failures;
}

double db (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-12)); }

struct Stats
{
    double peak = 0.0, energy = 0.0;
    int count = 0;
    bool finite = true;
    void add (float x)
    {
        finite = finite && std::isfinite (x);
        peak = std::max (peak, std::abs ((double) x));
        energy += (double) x * x;
        ++count;
    }
    double rms() const { return std::sqrt (energy / std::max (count, 1)); }
};

juce::MemoryBlock body (const char* name)
{
    juce::MemoryBlock bytes;
    juce::File (juce::String (TRENCH_TABLE_STITCH_ROOT)).getChildFile ("plugin/presets/p2k")
        .getChildFile (name).loadFileAsData (bytes);
    return bytes;
}

double maxResponse (const juce::MemoryBlock& bytes, float morph, float q, int key, double rate)
{
    float coeffs[trench::kUiCoeffCount] {}, boost = 1.0f;
    if (! TrenchDspBridge::probePackedBody (bytes.getData(), bytes.getSize(), morph, q, rate, coeffs, boost,
                                           TrenchDspBridge::kBodyDatumRate, key))
        return 0.0;
    double maximum = 0.0;
    for (int i = 0; i < 12000; ++i)
    {
        const double hz = 20.0 * std::pow (rate * 0.45 / 20.0, i / 11999.0);
        const auto z = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * hz / rate);
        std::complex<double> h { 1.0, 0.0 };
        for (int s = 0; s < trench::kUiStageCount; ++s)
        {
            const float* c = coeffs + s * 5;
            h *= ((double) c[0] + (double) c[1] * z + (double) c[2] * z * z)
               / (1.0 + (double) c[3] * z + (double) c[4] * z * z);
        }
        maximum = std::max (maximum, std::abs (h));
    }
    return maximum;
}

void run (double rate, int blockSize, const juce::MemoryBlock& startBody,
          const juce::MemoryBlock& finishBody, int startKey, int finishKey,
          double hz, const char* label)
{
    TrenchDspBridge bridge;
    bridge.prepare (rate, blockSize);
    bridge.loadCartridgeBytes (startBody);
    TrenchParams params;
    params.morph = 0.5f;
    params.q = 1.0f;
    params.keySnap = startKey;
    juce::AudioBuffer<float> buffer (2, blockSize);
    double phase = 0.0;
    Stats before, transition, after, silence;
    const int phaseBlocks = (int) std::ceil (rate / blockSize);
    for (int b = 0; b < phaseBlocks * 4; ++b)
    {
        if (b == phaseBlocks)
        {
            if (startBody != finishBody) bridge.loadCartridgeBytes (finishBody);
            params.keySnap = finishKey;
        }
        for (int i = 0; i < blockSize; ++i)
        {
            const float x = b >= phaseBlocks * 3 ? 0.0f : (float) (0.025 * std::sin (phase));
            phase += 2.0 * juce::MathConstants<double>::pi * hz / rate;
            buffer.setSample (0, i, x);
            buffer.setSample (1, i, x);
        }
        bridge.process (buffer, params);
        for (int i = 0; i < blockSize; ++i)
        {
            const float x = buffer.getSample (0, i);
            if (b >= phaseBlocks / 2 && b < phaseBlocks) before.add (x);
            else if (b >= phaseBlocks && b < phaseBlocks + (int) (0.1 * rate / blockSize)) transition.add (x);
            else if (b >= phaseBlocks * 2 && b < phaseBlocks * 3) after.add (x);
            else if (b >= phaseBlocks * 3 + phaseBlocks / 2) silence.add (x);
        }
    }
    std::printf ("%s sr=%.0f block=%d hz=%.2f peak(before/switch/after)=%.2f/%.2f/%.2f dB rms=%.2f/%.2f/%.2f tail=%.2f finite=%d\n",
                 label, rate, blockSize, hz, db (before.peak), db (transition.peak), db (after.peak),
                 db (before.rms()), db (transition.rms()), db (after.rms()), db (silence.peak),
                 before.finite && transition.finite && after.finite && silence.finite);
    check (before.finite && transition.finite && after.finite && silence.finite, "finite transition and tail");
    if (startBody != finishBody)
        check (transition.peak < 0.1 && silence.peak < 1.0e-6, "quiet sine switch stays below -20 dBFS and decays");
}

void referenceSwitch (double rate, int blockSize, const juce::MemoryBlock& from,
                      const juce::MemoryBlock& to, int key, int signal, int channels)
{
    TrenchDspBridge tested, oldReference, newReference;
    for (auto* bridge : { &tested, &oldReference, &newReference })
    {
        bridge->prepare (rate, blockSize);
        auto filtersOnly = bridge->getBypass();
        filtersOnly.mackity = false;
        bridge->setBypass (filtersOnly);
    }
    tested.loadCartridgeBytes (from);
    oldReference.loadCartridgeBytes (from);
    newReference.loadCartridgeBytes (to);
    TrenchParams params;
    params.morph = 0.5f;
    params.q = 1.0f;
    params.keySnap = key;
    juce::AudioBuffer<float> actual (channels, blockSize), oldAudio (channels, blockSize), newAudio (channels, blockSize);
    const int switchBlock = (int) std::ceil (rate * 0.15 / blockSize);
    double violation = 0.0, finalError = 0.0;
    bool finite = true;
    for (int b = 0; b < switchBlock * 2; ++b)
    {
        if (b == switchBlock) tested.loadCartridgeBytes (to);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < blockSize; ++i)
            {
                const double time = (double) (b * blockSize + i) / rate;
                const double phase = time * (c == 0 ? 49.0 : 73.0);
                const float x = signal == 0 ? 0.0f : signal == 1
                    ? (float) (0.2511886 * (2.0 * (phase - std::floor (phase)) - 1.0))
                    : (float) (0.025 * std::sin (time * 4000.0 * juce::MathConstants<double>::twoPi));
                actual.setSample (c, i, x);
                oldAudio.setSample (c, i, x);
                newAudio.setSample (c, i, x);
            }
        tested.process (actual, params);
        oldReference.process (oldAudio, params);
        if (b < switchBlock) continue;
        newReference.process (newAudio, params);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < blockSize; ++i)
            {
                const double x = actual.getSample (c, i), a = oldAudio.getSample (c, i), z = newAudio.getSample (c, i);
                finite = finite && std::isfinite (x);
                violation = std::max ({ violation, x - std::max (a, z), std::min (a, z) - x });
                if ((b - switchBlock) * blockSize + i > rate * 0.011)
                    finalError = std::max (finalError, std::abs (x - z));
            }
    }
    if (! finite || violation > 2.0e-6 || finalError > 2.0e-6)
        std::printf ("reference failure sr=%.0f block=%d key=%d signal=%d channels=%d excess=%g error=%g\n",
                     rate, blockSize, key, signal, channels, violation, finalError);
    check (finite && violation <= 2.0e-6 && finalError <= 2.0e-6,
           "switch stays between independent old/new outputs and reaches the new body");
}

void limitedSwitch (double rate, int blockSize, const juce::MemoryBlock& from, const juce::MemoryBlock& to)
{
    TrenchDspBridge bridge;
    bridge.prepare (rate, blockSize);
    bridge.loadCartridgeBytes (from);
    TrenchParams params;
    params.morph = 0.5f;
    params.q = 1.0f;
    juce::AudioBuffer<float> audio (2, blockSize);
    const int switchBlock = (int) std::ceil (rate * 0.5 / blockSize);
    double settledPeak = 0.0, switchPeak = 0.0, afterPeak = 0.0;
    bool finite = true;
    for (int b = 0; b < switchBlock * 2; ++b)
    {
        if (b == switchBlock) bridge.loadCartridgeBytes (to);
        for (int i = 0; i < blockSize; ++i)
        {
            const double phase = (double) (b * blockSize + i) / rate * 49.0;
            const float x = (float) (0.2511886 * (2.0 * (phase - std::floor (phase)) - 1.0));
            audio.setSample (0, i, x);
            audio.setSample (1, i, x);
        }
        bridge.process (audio, params);
        for (int i = 0; i < blockSize; ++i)
        {
            const float y = audio.getSample (0, i);
            finite = finite && std::isfinite (y);
            if (b >= switchBlock / 2 && b < switchBlock) settledPeak = std::max (settledPeak, (double) std::abs (y));
            if (b >= switchBlock && b < switchBlock + (int) std::ceil (rate * 0.05 / blockSize)) switchPeak = std::max (switchPeak, (double) std::abs (y));
            if (b >= switchBlock * 3 / 2) afterPeak = std::max (afterPeak, (double) std::abs (y));
        }
    }
    const bool ok = finite && switchPeak <= std::max (settledPeak, afterPeak) * 1.26 + 0.02;
    if (! ok)
        std::printf ("limited switch sr=%.0f block=%d before %.3f switch %.3f after %.3f\n", rate, blockSize, settledPeak, switchPeak, afterPeak);
    check (ok, "a body switch on a driven saw stays finite and within 2 dB of the louder of the two settled bodies");
}

void rapidSwitch (double rate, int blockSize, const juce::MemoryBlock& a, const juce::MemoryBlock& b)
{
    TrenchDspBridge bridge;
    bridge.prepare (rate, blockSize);
    TrenchParams params;
    params.morph = 0.5f;
    params.q = 1.0f;
    params.keySnap = 1;
    juce::AudioBuffer<float> audio (2, blockSize);
    bool finite = true;
    for (int block = 0; block < 100; ++block)
    {
        if (block < 20) bridge.loadCartridgeBytes (block % 2 ? b : a);
        for (int i = 0; i < blockSize; ++i)
        {
            const float x = (float) (0.025 * std::sin ((block * blockSize + i) * 220.0 * juce::MathConstants<double>::twoPi / rate));
            audio.setSample (0, i, x);
            audio.setSample (1, i, -x);
        }
        bridge.process (audio, params);
        for (int i = 0; i < blockSize; ++i)
            finite = finite && std::isfinite (audio.getSample (0, i)) && std::abs (audio.getSample (0, i)) < 0.1f
                     && std::abs (audio.getSample (0, i) + audio.getSample (1, i)) < 1.0e-6f;
    }
    float heard[trench::kUiCoeffCount] {}, expected[trench::kUiCoeffCount] {}, boost = 1.0f;
    bridge.readUiSnapshot (heard, boost);
    TrenchDspBridge::probePackedBody (b.getData(), b.getSize(), params.morph, params.q, rate, expected, boost,
                                    TrenchDspBridge::kBodyDatumRate, params.keySnap);
    double error = 0.0;
    for (int i = 0; i < trench::kUiCoeffCount; ++i) error = std::max (error, (double) std::abs (heard[i] - expected[i]));
    check (finite && error < 1.0e-6, "rapid requests stay quiet, preserve stereo and land on the latest body");
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto hedz = body ("talking_hedz.body240");
    const auto millennium = body ("millennium.body240");
    const auto vowel = fixtureBody ("util_vowel_ah_ee.body240");
    const auto identity = fixtureBody ("identity.body240");
    std::printf ("fixtures hedz=%zu millennium=%zu vowel=%zu\n", hedz.getSize(), millennium.getSize(), vowel.getSize());
    if (hedz.getSize() != 240 || millennium.getSize() != 240 || vowel.getSize() != 240) return 1;
    for (const auto rate : { 44100.0, 48000.0 })
    {
        std::printf ("response sr=%.0f Hedz off/Cm %.2f/%.2f Millennium %.2f/%.2f\n", rate,
                     db (maxResponse (hedz, 0.5f, 1.0f, 0, rate)), db (maxResponse (hedz, 0.5f, 1.0f, 1, rate)),
                     db (maxResponse (millennium, 0.5f, 1.0f, 0, rate)), db (maxResponse (millennium, 0.5f, 1.0f, 1, rate)));
        for (double hz : { 100.0, 220.0, 400.0, 415.3, 440.0, 1000.0, 4000.0 })
        {
            run (rate, 256, hedz, hedz, 0, 1, hz, "KEY ON");
            run (rate, 256, hedz, hedz, 1, 0, hz, "KEY OFF");
            if (vowel.getSize() == 240) run (rate, 256, vowel, hedz, 1, 1, hz, "BODY");
        }
    }
    {
        double worst = 0.0;
        for (const auto* bytes : { &hedz, &millennium, &vowel })
            for (const double rate : { 44100.0, 48000.0, 96000.0 })
                for (const float morph : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
                    for (const float q : { 0.0f, 0.5f, 1.0f })
                        for (const int key : { 0, 1, 7 })
                        {
                            float coeffs[trench::kUiCoeffCount] {}, boost = 1.0f;
                            TrenchDspBridge::probePackedBody (bytes->getData(), bytes->getSize(), morph, q, rate, coeffs, boost,
                                                             TrenchDspBridge::kBodyDatumRate, key);
                            trench::core::Cascade cascade {};
                            for (int s = 0; s < trench::kUiStageCount; ++s)
                                for (int c = 0; c < trench::kUiCoeffsPerStage; ++c)
                                    cascade[(size_t) s][(size_t) c] = coeffs[s * 5 + c];
                            for (size_t s = 0; s < cascade.size(); ++s)
                            {
                                const auto back = trench::core::decode_section (trench::core::encode_section (cascade[s]));
                                for (size_t c = 0; c < back.size(); ++c)
                                    worst = std::max (worst, std::abs (back[c] - cascade[s][c]) / std::max (1.0, std::abs (cascade[s][c])));
                            }
                        }
        std::printf ("encoded-domain round trip worst relative error %g\n", worst);
        check (worst < 1.0e-9, "the encoded (log word) ramp domain reproduces every shipped cascade, KEY on or off");
    }
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (int blockSize : { 64, 256, 512 })
        {
            rapidSwitch (rate, blockSize, vowel, hedz);
            rapidSwitch (rate, blockSize, identity, hedz);
            limitedSwitch (rate, blockSize, vowel, hedz);
            limitedSwitch (rate, blockSize, hedz, identity);
            limitedSwitch (rate, blockSize, identity, hedz);
            for (int key : { 0, 1 })
            {
                referenceSwitch (rate, blockSize, hedz, identity, key, 1, 2);
                referenceSwitch (rate, blockSize, identity, hedz, key, 1, 2);
            }
            for (int key : { 0, 1 })
                for (int signal : { 0, 1, 2 })
                    for (int channels : { 1, 2 })
                    {
                        referenceSwitch (rate, blockSize, vowel, hedz, key, signal, channels);
                        referenceSwitch (rate, blockSize, hedz, millennium, key, signal, channels);
                        referenceSwitch (rate, blockSize, millennium, hedz, key, signal, channels);
                    }
        }
    std::printf ("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
