#include "PluginProcessor.h"
#include "BinaryData.h"
#include "TrenchBodyRoster.h"
#include "dsp/TrenchDspBridge.h"
#include "parameters/TrenchParameters.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <trench/core/audition.hpp>
#include <trench/core/packed_body.hpp>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace
{
int failures = 0;

void check (bool ok, const char* what, double a = 0.0, double b = 0.0)
{
    std::printf ("%s  %s  (%.6g / %.6g)\n", ok ? "PASS" : "FAIL", what, a, b);
    if (! ok) ++failures;
}

double db (double lin) { return 20.0 * std::log10 (std::max (lin, 1.0e-12)); }

void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

void setParam (PluginProcessor& p, const char* id, float denorm)
{
    if (auto* prm = p.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (denorm));
}

juce::MemoryBlock resourceBytes (const char* originalName)
{
    for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
        if (juce::String (BinaryData::originalFilenames[r]) == originalName)
        {
            int size = 0;
            const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[r], size);
            return juce::MemoryBlock (data, (size_t) size);
        }
    return {};
}

int rosterIndexContaining (const char* needle)
{
    const int n = trench::bodyCount();
    for (int i = 0; i < n; ++i)
        if (trench::bodyDisplayName (i).containsIgnoreCase (needle))
            return i;
    return -1;
}

int anotherPackedBody (int notThis)
{
    const int n = trench::bodyCount();
    for (int i = 1; i < n; ++i)
    {
        if (i == notThis) continue;
        juce::MemoryBlock raw;
        if (trench::bodyRawBytes (i, raw) && raw.getSize() == 240)
            return i;
    }
    return -1;
}

struct TestPlayHead final : juce::AudioPlayHead
{
    double ppq = 0.0;
    double bpm = 120.0;
    double sampleRate = 48000.0;
    std::int64_t samples = 0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (playing);
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        info.setTimeInSamples (samples);
        info.setTimeInSeconds ((double) samples / sampleRate);
        return info;
    }
    void advance (int numSamples)
    {
        samples += numSamples;
        ppq += (double) numSamples / sampleRate * bpm / 60.0;
    }
};

void fillSine (juce::AudioBuffer<float>& buf, double& phase, double hz, double rate, float amp)
{
    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        const float s = amp * (float) std::sin (phase);
        phase += 2.0 * juce::MathConstants<double>::pi * hz / rate;
        for (int c = 0; c < buf.getNumChannels(); ++c)
            buf.setSample (c, i, s);
    }
}

double responseDbFromCoeffs (const float* coeffs, double f, double sr)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
    std::complex<double> h (1.0, 0.0);
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    for (int sct = 0; sct < trench::kUiStageCount; ++sct)
    {
        const float* c = coeffs + sct * trench::kUiCoeffsPerStage;
        h *= ((double) c[0] + (double) c[1] * z1 + (double) c[2] * z2)
             / (1.0 + (double) c[3] * z1 + (double) c[4] * z2);
    }
    return db (std::abs (h));
}

double measuredDb (const juce::AudioBuffer<float>& impulse, double f, double sr, double impulseAmp, double voiceGain)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
    std::complex<double> acc (0.0, 0.0);
    for (int i = 0; i < impulse.getNumSamples(); ++i)
        acc += (double) impulse.getSample (0, i) * std::polar (1.0, -w * i);
    return db (std::abs (acc) / impulseAmp / voiceGain);
}

trench::core::Cascade cascadeFromProbe (const juce::MemoryBlock& body, float morph, double runtimeRate)
{
    float coeffs[trench::kUiCoeffCount] = {};
    float boost = 1.0f;
    TrenchDspBridge::probePackedBody (body.getData(), body.getSize(), morph, 0.0f, runtimeRate, coeffs, boost);
    trench::core::Cascade out {};
    int index = 0;
    for (auto& section : out)
        for (auto& coefficient : section)
            coefficient = coeffs[index++];
    return out;
}

double worstRelative (const trench::core::Cascade& a, const trench::core::Cascade& b)
{
    double worst = 0.0;
    for (std::size_t s = 0; s < a.size(); ++s)
        for (std::size_t c = 0; c < a[s].size(); ++c)
            worst = std::max (worst, std::abs (a[s][c] - b[s][c]) / std::max (1.0e-9, std::abs (b[s][c])));
    return worst;
}

constexpr double kHostRate = 48000.0;
constexpr float kVoiceGain = 1.0f;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::MemoryBlock crisp = resourceBytes ("xml_crisp.body240");
    check (crisp.getSize() == 240, "crisp body bytes available", (double) crisp.getSize(), 240.0);

    std::printf ("== engine budget: seconds of engine per second of audio, per instance ==\n");
    {
        TrenchDspBridge bridge;
        bridge.prepare (kHostRate, 512);
        bridge.loadCartridgeBytes (crisp);
        TrenchParams params;
        params.q = 0.3f;
        juce::AudioBuffer<float> buf (2, 512);
        std::vector<float> morph (512);
        juce::Random rng (3);
        const int blocks = (int) (kHostRate / 512.0);
        double phase = 0.0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b)
        {
            fillSine (buf, phase, 220.0, kHostRate, 0.1f);
            for (int i = 0; i < 512; ++i)
                morph[i] = 0.5f + 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * (b * 512 + i) / kHostRate);
            bridge.processTrajectory (buf, morph.data(), params);
        }
        const double engine = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double perSecond = engine / ((double) blocks * 512.0 / kHostRate);
        std::printf ("engine %.4f s per 1 s of stereo audio at 48 kHz, MORPH moving every sample\n", perSecond);
        check (perSecond < 0.05, "one instance costs under 5% of real time at 48 kHz (engine s / audio s)", perSecond, 0.05);
        params.keySnap = 7;
        const auto t1 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b)
        {
            fillSine (buf, phase, 220.0, kHostRate, 0.1f);
            bridge.processTrajectory (buf, morph.data(), params);
        }
        const double keyed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t1).count()
                             / ((double) blocks * 512.0 / kHostRate);
        std::printf ("engine %.4f s per 1 s with KEY on\n", keyed);
        check (keyed < 1.5 * std::max (perSecond, 0.005), "KEY on does not double the engine cost", keyed, perSecond);
    }

    std::printf ("== coefficient ramp reaches its target ==\n");
    {
        const auto a = trench::core::encode_cascade (cascadeFromProbe (crisp, 0.0f, kHostRate));
        const auto b = trench::core::encode_cascade (cascadeFromProbe (crisp, 1.0f, kHostRate));
        trench::core::CascadeRunner direct;
        direct.set_target (b);
        std::vector<float> zeros (1, 0.0f);

        trench::core::CascadeRunner once;
        once.set_target (a);
        once.set_target (b);
        for (std::size_t i = 0; i < trench::core::kApproachSamples + 8; ++i)
            once.process (zeros);
        check (worstRelative (once.coefficients(), direct.coefficients()) < 1.0e-9,
               "one set_target then kApproachSamples samples lands exactly on the target",
               worstRelative (once.coefficients(), direct.coefficients()), 1.0e-9);

        trench::core::CascadeRunner retargeted;
        retargeted.set_target (a);
        for (std::size_t i = 0; i < 2 * trench::core::kApproachSamples; ++i)
        {
            retargeted.set_target (b);
            retargeted.process (zeros);
        }
        const double err = worstRelative (retargeted.coefficients(), direct.coefficients());
        check (err < 1.0e-6,
               "set_target every sample (the bridge's pattern) still reaches the target within 2x kApproachSamples",
               err, 1.0e-6);
        check (retargeted.remaining() == 0, "the countdown completes under per-sample re-targeting",
               (double) retargeted.remaining(), 0.0);
    }

    std::printf ("== processor: body switch is never dry ==\n");
    trench::rescanBodyRoster();
    const int crispIndex = rosterIndexContaining ("crisp");
    const int otherIndex = anotherPackedBody (crispIndex);
    check (crispIndex >= 0 && otherIndex >= 0, "two packed bodies in the roster", crispIndex, otherIndex);
    if (crispIndex >= 0 && otherIndex >= 0)
    {
        PluginProcessor processor;
        processor.setPlayConfigDetails (2, 2, kHostRate, 512);
        processor.prepareToPlay (kHostRate, 512);
        setParam (processor, ParamID::body, (float) crispIndex);
        setParam (processor, ParamID::morph, 0.5f);
        pump (300);
        check (processor.getLastLoadOk(), "crisp loaded before the switch hammer");
        std::atomic<bool> stop { false };
        std::atomic<int> dryBlocks { 0 };
        std::atomic<int> totalBlocks { 0 };
        std::atomic<bool> finite { true };
        std::thread audio ([&]
        {
            juce::AudioBuffer<float> in (2, 512), out (2, 512);
            juce::MidiBuffer midi;
            double phase = 0.0;
            while (! stop.load (std::memory_order_relaxed))
            {
                fillSine (in, phase, 220.0, kHostRate, 0.05f);
                out.makeCopyOf (in);
                processor.processBlock (out, midi);
                bool identical = true;
                for (int i = 0; i < 512 && identical; ++i)
                {
                    const float v = out.getSample (0, i);
                    if (! std::isfinite (v)) finite.store (false, std::memory_order_relaxed);
                    identical = v == in.getSample (0, i);
                }
                if (identical) dryBlocks.fetch_add (1, std::memory_order_relaxed);
                totalBlocks.fetch_add (1, std::memory_order_relaxed);
            }
        });
        for (int swap = 0; swap < 200; ++swap)
        {
            setParam (processor, ParamID::body, (float) (swap % 2 == 0 ? otherIndex : crispIndex));
            pump (3);
        }
        stop.store (true, std::memory_order_relaxed);
        audio.join();
        std::printf ("blocks %d, dry blocks %d\n", totalBlocks.load(), dryBlocks.load());
        check (finite.load(), "audio finite through 200 body switches under load");
        check (dryBlocks.load() == 0, "no block passes through dry during a body switch (blocks)", dryBlocks.load(), 0.0);
        check (processor.getLastLoadOk(), "load flag is true after the hammer");
    }

    std::printf ("== processor: a rejected body keeps the previous one playing ==\n");
    {
        PluginProcessor processor;
        processor.setPlayConfigDetails (2, 2, kHostRate, 512);
        processor.prepareToPlay (kHostRate, 512);
        setParam (processor, ParamID::body, (float) std::max (crispIndex, 0));
        pump (300);
        unsigned char garbage[10] = {};
        check (! processor.installBodyBytes (garbage, sizeof (garbage)), "10 bytes are refused");
        check (processor.getLastLoadOk(), "load flag survives a refused install");
        int unloadable = -1;
        for (int i = 1; i < trench::bodyCount(); ++i)
        {
            juce::MemoryBlock raw;
            if (! trench::bodyRawBytes (i, raw) || raw.getSize() != 240) { unloadable = i; break; }
        }
        if (unloadable >= 0)
        {
            setParam (processor, ParamID::body, (float) unloadable);
            pump (400);
            std::printf ("roster slot %d (%s) is not a 240-byte body\n", unloadable, trench::bodyDisplayName (unloadable).toRawUTF8());
            check (processor.getLastLoadOk(), "selecting a body that cannot load leaves the plugin live, not dry");
        }
        else
        {
            std::printf ("every roster slot is a 240-byte body; the failed-load path is not reachable from the roster\n");
        }
        juce::AudioBuffer<float> in (2, 512), out (2, 512);
        juce::MidiBuffer midi;
        double phase = 0.0;
        bool identical = true;
        for (int b = 0; b < 8; ++b)
        {
            fillSine (in, phase, 220.0, kHostRate, 0.05f);
            out.makeCopyOf (in);
            processor.processBlock (out, midi);
            if (b == 7)
                for (int i = 0; i < 512 && identical; ++i) identical = out.getSample (0, i) == in.getSample (0, i);
        }
        check (! identical, "audio after the refused loads is processed, not dry");
    }

    std::printf ("== processor: a host block larger than prepared equals four prepared blocks ==\n");
    {
        for (const bool movement : { false, true })
        {
            PluginProcessor whole, chunked;
            TestPlayHead headWhole, headChunked;
            for (auto* p : { &whole, &chunked })
            {
                p->setPlayConfigDetails (2, 2, kHostRate, 512);
                p->prepareToPlay (kHostRate, 512);
                setParam (*p, ParamID::body, (float) std::max (crispIndex, 0));
                setParam (*p, ParamID::morph, 0.4f);
                setParam (*p, ParamID::movePreset, movement ? 2.0f : 0.0f);
            }
            whole.setPlayHead (&headWhole);
            chunked.setPlayHead (&headChunked);
            pump (300);
            juce::AudioBuffer<float> source (2, 2048);
            double phase = 0.0;
            fillSine (source, phase, 220.0, kHostRate, 0.05f);
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> big;
            big.makeCopyOf (source);
            whole.processBlock (big, midi);
            headWhole.advance (2048);
            juce::AudioBuffer<float> pieces;
            pieces.makeCopyOf (source);
            for (int start = 0; start < 2048; start += 512)
            {
                float* ch[2] = { pieces.getWritePointer (0) + start, pieces.getWritePointer (1) + start };
                juce::AudioBuffer<float> slice (ch, 2, 512);
                chunked.processBlock (slice, midi);
                headChunked.advance (512);
            }
            double worst = 0.0;
            for (int i = 0; i < 2048; ++i)
                worst = std::max (worst, (double) std::abs (big.getSample (0, i) - pieces.getSample (0, i)));
            check (worst < 1.0e-5, movement ? "2048-sample block matches 4 x 512 with MOVEMENT running (peak diff)"
                                             : "2048-sample block matches 4 x 512 with MOVEMENT off (peak diff)", worst, 1.0e-5);
        }
    }

    std::printf ("== processor: body recall by id, and an out-of-range slot lands on NO FILTER ==\n");
    {
        juce::MemoryBlock saved, savedOutOfRange;
        {
            PluginProcessor a;
            a.setPlayConfigDetails (2, 2, kHostRate, 512);
            a.prepareToPlay (kHostRate, 512);
            setParam (a, ParamID::body, (float) std::max (crispIndex, 0));
            setParam (a, ParamID::morph, 0.7f);
            pump (300);
            a.getStateInformation (saved);
            setParam (a, ParamID::body, (float) (trench::bodyCount() + 40));
            pump (300);
            a.getStateInformation (savedOutOfRange);
        }
        PluginProcessor b;
        b.setPlayConfigDetails (2, 2, kHostRate, 512);
        b.prepareToPlay (kHostRate, 512);
        b.setStateInformation (saved.getData(), (int) saved.getSize());
        pump (400);
        check (b.getLoadedBodyIndex() == crispIndex, "BODY recalls by id into a fresh instance", b.getLoadedBodyIndex(), crispIndex);
        check (b.getLastLoadOk(), "recalled body loaded");
        PluginProcessor c;
        c.setPlayConfigDetails (2, 2, kHostRate, 512);
        c.prepareToPlay (kHostRate, 512);
        c.setStateInformation (savedOutOfRange.getData(), (int) savedOutOfRange.getSize());
        pump (400);
        check (c.getLoadedBodyIndex() == trench::kNoFilterIndex, "an out-of-range saved slot lands on NO FILTER", c.getLoadedBodyIndex(), trench::kNoFilterIndex);
        check (c.getLastLoadOk(), "NO FILTER loads after an out-of-range recall");
    }

    std::printf ("== processor: the probed curve is the heard curve with KEY on ==\n");
    if (crispIndex >= 0)
    {
        PluginProcessor processor;
        processor.setPlayConfigDetails (2, 2, kHostRate, 512);
        processor.prepareToPlay (kHostRate, 512);
        setParam (processor, ParamID::body, (float) crispIndex);
        setParam (processor, ParamID::morph, 0.5f);
        setParam (processor, ParamID::q, 0.0f);
        setParam (processor, ParamID::keySnap, 7.0f);
        pump (400);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> settle (2, 512);
        for (int b = 0; b < 8; ++b) { settle.clear(); processor.processBlock (settle, midi); }
        constexpr int N = 8192;
        juce::AudioBuffer<float> buf (2, N);
        buf.clear();
        buf.setSample (0, 0, 0.5f); buf.setSample (1, 0, 0.5f);
        for (int start = 0; start < N; start += 512)
        {
            float* ch[2] = { buf.getWritePointer (0) + start, buf.getWritePointer (1) + start };
            juce::AudioBuffer<float> slice (ch, 2, 512);
            processor.processBlock (slice, midi);
        }
        float coeffs[trench::kUiCoeffCount] = {}; float boost = 1.0f;
        processor.probeCurrentBodyForUi (0.5f, 0.0f, coeffs, boost);
        double worst = 0.0;
        for (double f : { 120.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 })
        {
            const double heard = measuredDb (buf, f, kHostRate, 0.5, kVoiceGain);
            const double shown = responseDbFromCoeffs (coeffs, f, kHostRate);
            worst = std::max (worst, std::abs (heard - shown));
            std::printf ("  %7.0f Hz  heard %7.2f  shown %7.2f\n", f, heard, shown);
        }
        check (worst < 1.0, "graph matches audio with KEY F# (worst dB)", worst, 1.0);
    }

    std::printf ("== processor: mono layout ==\n");
    {
        PluginProcessor processor;
        processor.setPlayConfigDetails (1, 1, kHostRate, 512);
        processor.prepareToPlay (kHostRate, 512);
        setParam (processor, ParamID::body, (float) std::max (crispIndex, 0));
        setParam (processor, ParamID::morph, 0.5f);
        pump (300);
        juce::AudioBuffer<float> in (1, 512), out (1, 512);
        juce::MidiBuffer midi;
        double phase = 0.0;
        bool finite = true, identical = true;
        float peak = 0.0f;
        for (int b = 0; b < 20; ++b)
        {
            fillSine (in, phase, 220.0, kHostRate, 0.05f);
            out.makeCopyOf (in);
            processor.processBlock (out, midi);
            if (b < 10) continue;
            for (int i = 0; i < 512; ++i)
            {
                const float v = out.getSample (0, i);
                finite = finite && std::isfinite (v);
                peak = std::max (peak, std::abs (v));
                identical = identical && v == in.getSample (0, i);
            }
        }
        check (finite && peak > 0.001f, "mono output finite and audible", peak, 0.001);
        check (! identical, "mono output is filtered, not passed through");
    }

    std::printf ("== processor: sample-rate changes mid-session, then a zero-length prepare ==\n");
    {
        PluginProcessor processor;
        processor.setPlayConfigDetails (2, 2, 44100.0, 512);
        processor.prepareToPlay (44100.0, 512);
        setParam (processor, ParamID::body, (float) std::max (crispIndex, 0));
        setParam (processor, ParamID::morph, 0.5f);
        pump (300);
        for (const double rate : { 44100.0, 48000.0, 96000.0, 44100.0 })
        {
            processor.setPlayConfigDetails (2, 2, rate, 512);
            processor.prepareToPlay (rate, 512);
            pump (200);
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            double phase = 0.0;
            bool finite = true;
            float peak = 0.0f;
            for (int b = 0; b < 20; ++b)
            {
                fillSine (buf, phase, 220.0, rate, 0.05f);
                processor.processBlock (buf, midi);
                if (b < 10) continue;
                for (int i = 0; i < 512; ++i)
                {
                    finite = finite && std::isfinite (buf.getSample (0, i));
                    peak = std::max (peak, std::abs (buf.getSample (0, i)));
                }
            }
            check (finite && peak > 0.001f, "finite, audible output after a rate change (rate)", rate, peak);
        }
        processor.prepareToPlay (48000.0, 0);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        buf.clear();
        buf.setSample (0, 0, 0.5f);
        processor.processBlock (buf, midi);
        check (std::isfinite (buf.getSample (0, 100)), "processBlock after prepareToPlay(rate, 0) does not crash or emit NaN");
    }

    std::printf ("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
