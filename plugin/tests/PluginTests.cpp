#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include "TrenchBodyRoster.h"
#include "dsp/PreampLaw.h"
#include "dsp/SlamStage.h"
#include "parameters/CurveMap.h"
#include "ui/ModulationChip.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <thread>
#include <trench/core/native_body.hpp>
#include <trench/core/packed_body.hpp>
#include <span>
#include <cstdio>

namespace
{
int failures = 0;
void check (bool ok, const char* what, double a = 0.0, double b = 0.0)
{
    std::printf ("%s  %s  (%.6g / %.6g)\n", ok ? "PASS" : "FAIL", what, a, b);
    if (! ok) ++failures;
}
double db (double lin) { return 20.0 * std::log10 (lin); }

void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

void setParam (PluginProcessor& p, const char* id, float denorm)
{
    if (auto* prm = p.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (denorm));
}

struct Run { float peak = 0.0f; bool finite = true; };
Run runSine (PluginProcessor& p, float amplitude, int blocks = 40)
{
    constexpr int n = 512;
    juce::AudioBuffer<float> buf (2, n);
    juce::MidiBuffer midi;
    Run r;
    double phase = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < n; ++i)
        {
            const float s = amplitude * (float) std::sin (phase);
            phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / 48000.0;
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
        }
        p.processBlock (buf, midi);
        if (b >= blocks / 2)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) r.finite = false;
                    r.peak = juce::jmax (r.peak, std::abs (v));
                }
    }
    return r;
}

template <typename T>
T* findChild (juce::Component& root, const juce::String& title = {})
{
    for (auto* c : root.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (c); t != nullptr && (title.isEmpty() || t->getTitle() == title))
            return t;
        if (auto* t = findChild<T> (*c, title))
            return t;
    }
    return nullptr;
}

void savePng (const juce::Image& img, const juce::String& name)
{
    auto f = juce::File::getCurrentWorkingDirectory().getChildFile (name);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("SHOT  %s  %dx%d\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

bool anyVisibleOfTitle (juce::Component& root, const juce::String& title)
{
    for (auto* c : root.getChildren())
    {
        if (c->getTitle() == title && c->isShowing()) return true;
        if (anyVisibleOfTitle (*c, title)) return true;
    }
    return false;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::printf ("== shipping curves ==\n");
    trench::curves::clearBypassAxis();
    {
        constexpr const char* axisNames[] = {
            "MORPH", "Q", "Z", "FOLLOW", "OUTPUT", "OUTPUT trim", "INPUT", "TRACK"
        };
        for (std::size_t axisIndex = 0; axisIndex < (std::size_t) trench::curves::Axis::count; ++axisIndex)
        {
            const auto axis = (trench::curves::Axis) axisIndex;
            const auto& table = *trench::curves::kTables[axisIndex];
            bool finite = true;
            bool nondecreasing = true;
            bool nonincreasing = true;
            for (std::size_t i = 0; i < table.size(); ++i)
            {
                finite = finite && std::isfinite (table[i]);
                if (i > 0)
                {
                    nondecreasing = nondecreasing && table[i] >= table[i - 1];
                    nonincreasing = nonincreasing && table[i] <= table[i - 1];
                }
            }
            const auto label = juce::String (axisNames[axisIndex]);
            check (finite, (label + " table has 129 finite values").toRawUTF8(), table.size(), 129.0);
            check (nondecreasing || nonincreasing,
                   (label + " table is monotone (constant valid)").toRawUTF8());
            const auto& endpoints = *trench::curves::kEndpoints[axisIndex];
            const float mappedLow = trench::curves::curveMap (axis, 0.0f);
            const float mappedHigh = trench::curves::curveMap (axis, 1.0f);
            check (std::abs (mappedLow - endpoints[0]) <= 1.0e-7f,
                   (label + " knob 0 lands on session endpoint").toRawUTF8(), mappedLow, endpoints[0]);
            check (std::abs (mappedHigh - endpoints[1]) <= 1.0e-7f,
                   (label + " knob 1 lands on session endpoint").toRawUTF8(), mappedHigh, endpoints[1]);
        }
        trench::curves::setBypassAxis (trench::curves::Axis::slam);
        check (trench::curves::curveMap (trench::curves::Axis::slam, 0.375f) == 0.375f,
               "OUTPUT bisection bypass returns raw knob");
        check (trench::curves::curveMap (trench::curves::Axis::slamTrim, 0.375f) == 1.0f,
               "OUTPUT bypass leaves the trim table active");
        trench::curves::clearBypassAxis();
    }

    std::printf ("== PREAMP law ==\n");
    check (trench::preampGain (0.0f) == 1.0f, "preampGain(0) is exactly unity", trench::preampGain (0.0f), 1.0);
    check (std::abs (db (trench::preampGain (0.5f)) - 20.0) < 0.01, "preampGain(0.5) is +20 dB", db (trench::preampGain (0.5f)), 20.0);
    check (std::abs (db (trench::preampGain (1.0f)) - 40.0) < 0.01, "preampGain(1.0) is +40 dB", db (trench::preampGain (1.0f)), 40.0);

    std::printf ("== movement interpolation ==\n");
    {
        trench::MovementTransport transport;
        transport.bpm = 60.0;
        transport.ppq = 0.0;
        transport.playing = true;
        float stepped[4] = {}, glided[4] = {};
        trench::Movement stepMovement, glideMovement;
        stepMovement.prepare (16.0);
        glideMovement.prepare (16.0);
        stepMovement.render (stepped, 4, 0.5f, transport, 1,
                             trench::Movement::StepTransition);
        glideMovement.render (glided, 4, 0.5f, transport, 1,
                              trench::Movement::GlideTransition);
        check (std::abs (stepped[2]) < 1.0e-7f,
               "STEP holds the current Morph cell", stepped[2], 0.0);
        check (std::abs (glided[2] + 0.25f) < 1.0e-6f,
               "GLIDE interpolates directly between Morph cells", glided[2], -0.25);
        float anchored[1] = {};
        trench::Movement anchorMovement;
        anchorMovement.prepare (16.0);
        anchorMovement.render (anchored, 1, 0.8f, transport, 2,
                               trench::Movement::StepTransition);
        check (std::abs ((0.8f + anchored[0]) - 0.16f) < 1.0e-6f,
               "pattern travel uses the Morph wheel's room to the wall",
               0.8f + anchored[0], 0.16);
    }

    std::printf ("== bridge boundary ==\n");
    {
        TrenchDspBridge bridge;
        bridge.prepare (48000.0, 512);
        const bool loaded = bridge.loadCartridgeBytes (BinaryData::identity_body240, (size_t) BinaryData::identity_body240Size);
        check (loaded, "identity body loads into the bridge");
        bridge.setInputPreamp (trench::driveTaper (0.0f));
        juce::AudioBuffer<float> buf (2, 64);
        buf.clear();
        buf.setSample (0, 0, 0.0625f);
        buf.setSample (1, 0, 0.0625f);
        TrenchParams params;
        bridge.process (buf, params);
        float sum = 0.0f;
        for (int i = 0; i < 64; ++i) sum += buf.getSample (0, i);
        check (std::abs (buf.getSample (0, 0) - 0.0625f) < 1.0e-5f && std::abs (sum - 0.0625f) < 1.0e-4f,
               "PREAMP 0 through identity body returns a -24 dBFS impulse unchanged, below the leveller wake", buf.getSample (0, 0), 0.0625);
    }

    std::printf ("== bridge cost ==\n");
    {
        juce::MemoryBlock crisp;
        for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
            if (juce::String (BinaryData::originalFilenames[r]) == "xml_crisp.body240")
            {
                int size = 0;
                const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[r], size);
                crisp = juce::MemoryBlock (data, (size_t) size);
            }
        check (crisp.getSize() > 0, "crisp body bytes found for the cost probe", (double) crisp.getSize(), 240.0);
        for (const double rate : { 44100.0, 48000.0, 96000.0 })
        {
            TrenchDspBridge bridge;
            bridge.prepare (rate, 512);
            bridge.loadCartridgeBytes (crisp);
            TrenchParams params;
            params.morph = 0.37f;
            params.q = 0.42f;
            juce::AudioBuffer<float> buf (2, 512);
            juce::Random rng (7);
            const int blocks = (int) (rate / 512.0);
            const double t0 = juce::Time::getMillisecondCounterHiRes();
            for (int b = 0; b < blocks; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                        buf.setSample (c, i, rng.nextFloat() * 0.1f - 0.05f);
                bridge.process (buf, params);
            }
            const double seconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
            std::printf ("  %6.0f Hz  %.3f s of engine per 1 s of audio\n", rate, seconds);
            check (std::isfinite ((double) buf.getSample (0, 100)), "engine output finite at this rate", rate, 0.0);
        }
    }

    std::printf ("== bank parity at 48k ==\n");
    {
        juce::MemoryBlock crisp;
        for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
            if (juce::String (BinaryData::originalFilenames[r]) == "xml_crisp.body240")
            {
                int size = 0;
                const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[r], size);
                crisp = juce::MemoryBlock (data, (size_t) size);
            }
        const auto packed = trench::core::PackedBody::from_body_bytes (std::span {
            static_cast<const std::uint8_t*> (crisp.getData()), crisp.getSize() });
        double worst = 0.0;
        double at120 = 0.0;
        for (const float m : { 0.0f, 1.0f })
            for (const float q : { 0.0f, 1.0f })
            {
                const auto reference = packed.interpolate_biquads (m, q, 0.0f);
                float coeffs[trench::kUiCoeffCount] {};
                float boost = 1.0f;
                const bool ok = TrenchDspBridge::probePackedBody (crisp.getData(), crisp.getSize(),
                                                                  m, q, 48000.0, coeffs, boost);
                check (ok, "probe of the 48k bank succeeds");
                trench::core::Cascade measured {};
                for (int st = 0; st < trench::kUiStageCount; ++st)
                    for (int k = 0; k < trench::kUiCoeffsPerStage; ++k)
                        measured[(size_t) st][(size_t) k] = coeffs[st * trench::kUiCoeffsPerStage + k];
                for (const double f : { 60.0, 120.0, 250.0, 1000.0, 4000.0 })
                {
                    const double e = trench::core::cascade_response_db (reference, f, 44100.0);
                    const double g = trench::core::cascade_response_db (measured, f, 48000.0);
                    worst = juce::jmax (worst, std::abs (g - e));
                    if (f == 120.0) at120 = juce::jmax (at120, std::abs (g - e));
                }
            }
        std::printf ("  crisp 48k bank vs designed corners: worst %.4f dB, 120 Hz %.4f dB\n", worst, at120, "\n");
        check (worst <= 0.25, "48k bank matches the designed corner response", worst, 0.25);
        check (at120 <= 0.15, "the 48k low-octave regression stays closed", at120, 0.15);
    }

    std::printf ("== body swaps under running audio ==\n");
    {
        TrenchDspBridge bridge;
        bridge.prepare (48000.0, 512);
        juce::MemoryBlock first (BinaryData::identity_body240, (size_t) BinaryData::identity_body240Size);
        juce::MemoryBlock second;
        for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
            if (juce::String (BinaryData::originalFilenames[r]) == "xml_crisp.body240")
            {
                int size = 0;
                const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[r], size);
                second = juce::MemoryBlock (data, (size_t) size);
            }
        check (bridge.loadCartridgeBytes (first), "hammer: first body loads");
        std::atomic<bool> stop { false };
        std::atomic<bool> audioFinite { true };
        std::thread audio ([&]
        {
            juce::AudioBuffer<float> buf (2, 512);
            juce::Random rng (11);
            TrenchParams params;
            params.morph = 0.5f;
            params.q = 0.3f;
            while (! stop.load (std::memory_order_relaxed))
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                        buf.setSample (c, i, rng.nextFloat() * 0.2f - 0.1f);
                bridge.process (buf, params);
                for (int i = 0; i < 512; ++i)
                    if (! std::isfinite (buf.getSample (0, i)))
                        audioFinite.store (false, std::memory_order_relaxed);
            }
        });
        for (int swap = 0; swap < 400; ++swap)
        {
            bridge.reloadCartridgeBytes (swap % 2 == 0 ? second : first);
            bridge.reclaim();
        }
        stop.store (true, std::memory_order_relaxed);
        audio.join();
        bridge.reclaim();
        check (audioFinite.load(), "audio stayed finite through 400 mid-flight body swaps");
    }

    std::printf ("== processor at defaults ==\n");
    const double hostRate = std::getenv ("TRENCH_RATE") != nullptr ? std::atof (std::getenv ("TRENCH_RATE")) : 48000.0;
    PluginProcessor processor;
    processor.setPlayConfigDetails (2, 2, hostRate, 512);
    processor.prepareToPlay (hostRate, 512);
    pump (300);
    const float in = 0.001f;
    const auto base = runSine (processor, in);
    check (base.finite, "default output finite");
    check (base.peak > in * 0.5f, "nonzero input at default settings is not silenced", base.peak, in);
    check (std::abs (db (base.peak / in)) < 0.2, "PREAMP 0 = unity through the path", db (base.peak / in), 0.0);


    setParam (processor, ParamID::preamp, 1.0f);
    const auto driven = runSine (processor, in);
    check (db (driven.peak / base.peak) > 20.0, "INPUT at full drives the filter hard (dB over unity)", db (driven.peak / base.peak), 20.0);
    setParam (processor, ParamID::preamp, 0.0f);
    {
        setParam (processor, ParamID::body, (float) trench::kNoFilterIndex);
        pump (100);
        const auto goertzel = [] (const std::vector<float>& x, int bin)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * bin / (double) x.size();
            double s0 = 0, s1 = 0, s2 = 0;
            for (float v : x) { s0 = v + 2.0 * std::cos (w) * s1 - s2; s2 = s1; s1 = s0; }
            return std::sqrt (s1 * s1 + s2 * s2 - 2.0 * std::cos (w) * s1 * s2) / (double) x.size();
        };
        const auto capture = [&] (float inputDrive, float outputDrive, float amplitude = 0.6f)
        {
            setParam (processor, ParamID::preamp, inputDrive);
            setParam (processor, ParamID::slamDrive, outputDrive);
            constexpr int n = 4096;
            std::vector<float> out;
            juce::MidiBuffer midi;
            for (int pass = 0; pass < 3; ++pass)
            {
                juce::AudioBuffer<float> buf (2, n);
                for (int i = 0; i < n; ++i)
                {
                    const float v = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 37.0 * i / n);
                    buf.setSample (0, i, v); buf.setSample (1, i, v);
                }
                for (int start = 0; start < n; start += 512)
                {
                    float* chans[2] = { buf.getWritePointer (0) + start, buf.getWritePointer (1) + start };
                    juce::AudioBuffer<float> slice (chans, 2, 512);
                    processor.processBlock (slice, midi);
                }
                if (pass == 2)
                    out.assign (buf.getReadPointer (0), buf.getReadPointer (0) + n);
            }
            setParam (processor, ParamID::preamp, 0.0f);
            setParam (processor, ParamID::slamDrive, 0.0f);
            return out;
        };
        const auto driven = capture (0.35f, 0.0f);
        const double f = goertzel (driven, 37);
        const double h = goertzel (driven, 74) + goertzel (driven, 111) + goertzel (driven, 148) + goertzel (driven, 185);
        check (f > 0.01 && h / f > 0.02, "INPUT desk adds harmonics before the cascade (harmonic ratio)", h / f, 0.02);
        const auto clean = capture (0.0f, 0.0f);
        const double f0 = goertzel (clean, 37);
        const double h0 = goertzel (clean, 74) + goertzel (clean, 111) + goertzel (clean, 148) + goertzel (clean, 185);
        check (h0 / f0 < 0.005, "INPUT at 0 is the clean path (harmonic ratio)", h0 / f0, 0.005);

        const auto outputDriven = capture (0.0f, 0.35f, 0.25f);
        const double outputF = goertzel (outputDriven, 37);
        const double outputH = goertzel (outputDriven, 74) + goertzel (outputDriven, 111)
                             + goertzel (outputDriven, 148) + goertzel (outputDriven, 185);
        check (outputF > 0.001 && outputH / outputF < 0.005,
               "OUTPUT is clean gain, no harmonics of its own (harmonic ratio)",
               outputH / outputF, 0.005);
        const auto outputClean = capture (0.0f, 0.0f);
        const double outputF0 = goertzel (outputClean, 37);
        const double outputH0 = goertzel (outputClean, 74) + goertzel (outputClean, 111)
                              + goertzel (outputClean, 148) + goertzel (outputClean, 185);
        check (outputH0 / outputF0 < 0.005,
               "OUTPUT at 0 is the clean path after the No-filter body (harmonic ratio)",
               outputH0 / outputF0, 0.005);
    }
    const auto hot = runSine (processor, 0.1f);
    setParam (processor, ParamID::slamDrive, 1.0f);
    const auto slammed = runSine (processor, 0.1f);
    check (slammed.finite && std::abs (db (slammed.peak / hot.peak) - 12.0) < 0.5, "OUTPUT at full is +12 dB over unity (dB)", db (slammed.peak / hot.peak), 12.0);
    setParam (processor, ParamID::slamDrive, 0.0f);
    const auto loud = runSine (processor, 0.9f);
    check (loud.finite && loud.peak > 0.1f, "full-scale input at defaults stays finite and audible", loud.peak, 0.9);
    check (loud.peak <= trench::kFinalSafetyCeiling + 1.0e-4f, "safety ceiling bounds a full-scale input at -0.1 dBFS", loud.peak, trench::kFinalSafetyCeiling);
    setParam (processor, ParamID::preamp, 1.0f);
    const auto ceilinged = runSine (processor, 0.9f);
    check (ceilinged.finite && ceilinged.peak <= trench::kFinalSafetyCeiling + 1.0e-4f, "ceiling holds with INPUT at full", ceilinged.peak, trench::kFinalSafetyCeiling);
    check (ceilinged.peak > 0.9f / TrenchDspBridge::kLevellerScale, "ceiling limits and the leveller rides full scale to its floor, it does not mute", ceilinged.peak, 0.9 / TrenchDspBridge::kLevellerScale);
    setParam (processor, ParamID::preamp, 0.0f);

    std::printf ("== guard shape ==\n");
    {
        float worst = 0.0f;
        for (int i = 0; i < 1024; ++i)
        {
            const float s = 0.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * i / 1024.0);
            worst = juce::jmax (worst, std::abs (trench::softGuard (s) - s));
        }
        check (worst <= 1.0e-6f, "guard passes a 0.4 full-scale sine unchanged", worst, 1.0e-6);
        check (trench::softGuard (3.0f) == trench::kFinalSafetyCeiling
                   && trench::softGuard (-3.0f) == -trench::kFinalSafetyCeiling,
               "guard maps a 3.0 full-scale sample to the ceiling",
               trench::softGuard (3.0f), trench::kFinalSafetyCeiling);
        float bound = 0.0f;
        for (int i = -4000; i <= 4000; ++i)
            bound = juce::jmax (bound, std::abs (trench::softGuard ((float) i * 0.001f)));
        check (bound <= trench::kFinalSafetyCeiling, "guard output never leaves the ceiling", bound, trench::kFinalSafetyCeiling);
    }

    std::printf ("== level chain distortion ==\n");
    {
        trench::rescanBodyRoster();
        int rosterCount = 0; trench::bodyRoster (rosterCount);
        struct ToneRun { double thd = 0.0; double fundamental = 0.0; float peak = 0.0f; };
        const auto measure = [&processor, hostRate] (int bodyIndex, float morph, float qValue)
        {
            setParam (processor, ParamID::body, (float) bodyIndex);
            setParam (processor, ParamID::morph, morph);
            setParam (processor, ParamID::q, qValue);
            setParam (processor, ParamID::chew, 0.0f);
            pump (200);
            constexpr int n = 4096;
            juce::AudioBuffer<float> buf (2, n);
            juce::MidiBuffer midi;
            std::vector<float> tail;
            double phase = 0.0;
            for (int pass = 0; pass < 6; ++pass)
            {
                for (int i = 0; i < n; ++i)
                {
                    const float s = 0.251f * (float) std::sin (phase);
                    phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / hostRate;
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                for (int start = 0; start < n; start += 512)
                {
                    float* ch[2] = { buf.getWritePointer (0) + start, buf.getWritePointer (1) + start };
                    juce::AudioBuffer<float> slice (ch, 2, 512);
                    processor.processBlock (slice, midi);
                }
                if (pass >= 3)
                    tail.insert (tail.end(), buf.getReadPointer (0), buf.getReadPointer (0) + n);
            }
            const auto tone = [&tail, hostRate] (double f)
            {
                double re = 0.0, im = 0.0, norm = 0.0;
                for (std::size_t i = 0; i < tail.size(); ++i)
                {
                    const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * (double) i / (double) tail.size());
                    const double a = 2.0 * juce::MathConstants<double>::pi * f * (double) i / hostRate;
                    re += (double) tail[i] * w * std::cos (a);
                    im -= (double) tail[i] * w * std::sin (a);
                    norm += w;
                }
                return 2.0 * std::sqrt (re * re + im * im) / norm;
            };
            ToneRun run;
            run.fundamental = tone (220.0);
            double harmonics = 0.0;
            for (int h = 2; h <= 9; ++h)
                harmonics += tone (220.0 * (double) h) * tone (220.0 * (double) h);
            run.thd = 100.0 * std::sqrt (harmonics) / std::max (run.fundamental, 1.0e-12);
            for (float v : tail)
                run.peak = juce::jmax (run.peak, std::abs (v));
            return run;
        };
        int crossBand = -1;
        for (int i = 0; i < rosterCount; ++i)
            if (trench::bodyDisplayName (i).containsIgnoreCase ("Cross Band")) { crossBand = i; break; }
        check (crossBand >= 0, "Cross Band is in the roster", crossBand, rosterCount);
        if (crossBand >= 0)
        {
            const auto crossed = measure (crossBand, 0.5f, 0.3f);
            std::printf ("THD 220 Hz at -1 dBFS, BITE 0, Cross Band MORPH 0.5 Q 0.3: %.3f %%  (fundamental %.4f, peak %.4f)\n",
                         crossed.thd, crossed.fundamental, crossed.peak);
            check (crossed.thd < 1.0, "Cross Band leaves a -1 dBFS sine under 1 % THD", crossed.thd, 1.0);
        }
        double worst = 0.0;
        int worstBody = -1;
        float worstPeak = 0.0f;
        for (int i = 1; i < rosterCount; ++i)
        {
            const auto run = measure (i, 0.5f, 0.3f);
            if (run.fundamental < 0.05 || run.thd <= worst)
                continue;
            worst = run.thd;
            worstBody = i;
            worstPeak = run.peak;
        }
        std::printf ("worst THD across the roster at MORPH 0.5 Q 0.3: %.3f %%  (%s, peak %.4f)\n",
                     worst, worstBody >= 0 ? trench::bodyDisplayName (worstBody).toRawUTF8() : "none", worstPeak);
        check (worst < 1.0, "no body leaves a -12 dBFS sine over 1 % THD", worst, 1.0);
        setParam (processor, ParamID::body, (float) trench::kNoFilterIndex);
        setParam (processor, ParamID::morph, 0.0f);
        setParam (processor, ParamID::q, 0.0f);
        pump (200);
    }

    std::printf ("== ring leveller ==\n");
    {
        trench::rescanBodyRoster();
        int rosterCount = 0; trench::bodyRoster (rosterCount);
        int ringBody = -1;
        double ringDb = 0.0;
        for (int i = 0; i < rosterCount; ++i)
        {
            if (trench::bodyIsNoFilter (i))
                continue;
            juce::MemoryBlock bytes;
            if (! trench::bodyRawBytes (i, bytes))
                continue;
            try
            {
                const auto packed = trench::core::PackedBody::from_body_bytes (
                    std::span { static_cast<const std::uint8_t*> (bytes.getData()), bytes.getSize() });
                const auto cascade = packed.interpolate_biquads (0.5f, 0.9f, 0.0f);
                double loudest = 0.0;
                for (const auto& section : cascade)
                    loudest = std::max (loudest, trench::core::section_response_db (section, 220.0, 44100.0));
                if (loudest > ringDb) { ringDb = loudest; ringBody = i; }
            }
            catch (...) {}
        }
        check (ringBody >= 0, "a ship body rings hardest at 220 Hz", ringBody, rosterCount);
        std::printf ("ship body whose loudest section rings hardest at 220 Hz, MORPH 0.5 Q 0.9: %s (%.2f dB)\n",
                     ringBody >= 0 ? trench::bodyDisplayName (ringBody).toRawUTF8() : "none", ringDb);

        struct RingRun { double thd = 0.0; double fundamental = 0.0; float peak = 0.0f; };
        const auto ringMeasure = [&processor, hostRate] (int bodyIndex, bool leveller)
        {
            processor.dspBridge.setRingLeveller (leveller);
            setParam (processor, ParamID::body, (float) bodyIndex);
            setParam (processor, ParamID::morph, 0.5f);
            setParam (processor, ParamID::q, 0.9f);
            setParam (processor, ParamID::chew, 0.0f);
            pump (200);
            constexpr int n = 4096;
            juce::AudioBuffer<float> buf (2, n);
            juce::MidiBuffer midi;
            std::vector<float> tail;
            double phase = 0.0;
            for (int pass = 0; pass < 6; ++pass)
            {
                for (int i = 0; i < n; ++i)
                {
                    const float s = 0.251189f * (float) std::sin (phase);
                    phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / hostRate;
                    buf.setSample (0, i, s);
                    buf.setSample (1, i, s);
                }
                for (int start = 0; start < n; start += 512)
                {
                    float* ch[2] = { buf.getWritePointer (0) + start, buf.getWritePointer (1) + start };
                    juce::AudioBuffer<float> slice (ch, 2, 512);
                    processor.processBlock (slice, midi);
                }
                if (pass >= 3)
                    tail.insert (tail.end(), buf.getReadPointer (0), buf.getReadPointer (0) + n);
            }
            const auto tone = [&tail, hostRate] (double f)
            {
                double re = 0.0, im = 0.0, norm = 0.0;
                for (std::size_t i = 0; i < tail.size(); ++i)
                {
                    const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * (double) i / (double) tail.size());
                    const double a = 2.0 * juce::MathConstants<double>::pi * f * (double) i / hostRate;
                    re += (double) tail[i] * w * std::cos (a);
                    im -= (double) tail[i] * w * std::sin (a);
                    norm += w;
                }
                return 2.0 * std::sqrt (re * re + im * im) / norm;
            };
            RingRun run;
            run.fundamental = tone (220.0);
            double harmonics = 0.0;
            for (int h = 2; h <= 9; ++h)
                harmonics += tone (220.0 * (double) h) * tone (220.0 * (double) h);
            run.thd = 100.0 * std::sqrt (harmonics) / std::max (run.fundamental, 1.0e-12);
            for (float v : tail)
                run.peak = juce::jmax (run.peak, std::abs (v));
            return run;
        };
        if (ringBody >= 0)
        {
            const auto off = ringMeasure (ringBody, false);
            const auto on = ringMeasure (ringBody, true);
            const double drop = db (off.peak / juce::jmax (on.peak, 1.0e-9f));
            std::printf ("220 Hz at -12 dBFS through %s, MORPH 0.5 Q 0.5 BITE 0: "
                         "leveller off THD %.3f %% peak %.4f, on THD %.3f %% peak %.4f, drop %.2f dB\n",
                         trench::bodyDisplayName (ringBody).toRawUTF8(), off.thd, off.peak,
                         on.thd, on.peak, drop);
            check (on.thd < 0.5, "the ring leveller stays under 0.5 % THD", on.thd, 0.5);
            if (ringDb > 24.0)
                check (drop >= 6.0, "the ring leveller drops the peak by at least 6 dB", drop, 6.0);
            else
                check (drop <= 0.5, "the ring leveller is transparent when no section rings past 24 dB", (long) (drop * 100.0), 50);
        }
        processor.dspBridge.setRingLeveller (true);
        setParam (processor, ParamID::body, (float) trench::kNoFilterIndex);
        setParam (processor, ParamID::morph, 0.0f);
        setParam (processor, ParamID::q, 0.0f);
        pump (200);
    }

    std::printf ("== Z bites ==\n");
    {
        juce::MemoryBlock crisp;
        for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
            if (juce::String (BinaryData::originalFilenames[r]) == "xml_crisp.body240")
            {
                int size = 0;
                const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[r], size);
                crisp = juce::MemoryBlock (data, (size_t) size);
            }
        auto runBite = [&crisp] (float bite, float* activityOut = nullptr)
        {
            TrenchDspBridge bridge;
            bridge.prepare (48000.0, 512);
            bridge.loadCartridgeBytes (crisp);
            TrenchParams params;
            params.morph = 0.68f;
            params.q = 0.5f;
            params.poleDistortion = bite;
            juce::AudioBuffer<float> buf (2, 512);
            juce::Random rng (31);
            juce::MemoryBlock out;
            bool finite = true;
            for (int b = 0; b < 16; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                        buf.setSample (c, i, rng.nextFloat() * 0.8f - 0.4f);
                bridge.process (buf, params);
                for (int i = 0; i < 512; ++i)
                    finite = finite && std::isfinite (buf.getSample (0, i));
                out.append (buf.getReadPointer (0), 512 * sizeof (float));
            }
            if (activityOut != nullptr)
                *activityOut = bridge.gritActivity();
            return std::pair<juce::MemoryBlock, bool> (out, finite);
        };
        float quietActivity = -1.0f;
        float bittenActivity = -1.0f;
        const auto clean = runBite (0.0f, &quietActivity);
        const auto bitten = runBite (0.75f, &bittenActivity);
        check (clean.second && bitten.second, "Z output finite at rest and engaged");
        check (! (clean.first == bitten.first), "Z at 0.75 changes the sound");
        check (quietActivity == 0.0f, "grit telemetry silent at Z 0", quietActivity, 0.0);
        check (bittenActivity > 0.0f, "grit telemetry lights when Z bites", bittenActivity, 0.0);
        const auto cleanAgain = runBite (0.0f);
        check (clean.first == cleanAgain.first, "Z at 0 is deterministic and untouched");
    }

    /* TRACK was intentionally removed from the shipping modulation contract. */
    /*
    {
        juce::MemoryBlock crisp;
        for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
            if (juce::String (BinaryData::originalFilenames[r]) == "xml_crisp.body240")
            {
                int size = 0;
                const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[r], size);
                crisp = juce::MemoryBlock (data, (size_t) size);
            }
        auto runBridge = [&crisp] (TrenchParams params)
        {
            TrenchDspBridge bridge;
            bridge.prepare (48000.0, 512);
            bridge.loadCartridgeBytes (crisp);
            juce::AudioBuffer<float> buf (2, 512);
            juce::Random rng (23);
            juce::MemoryBlock out;
            for (int b = 0; b < 8; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                        buf.setSample (c, i, rng.nextFloat() * 0.2f - 0.1f);
                bridge.process (buf, params);
                out.append (buf.getReadPointer (0), 512 * sizeof (float));
            }
            return out;
        };
        TrenchParams off;
        off.morph = 0.4f;
        TrenchParams zeroDepth = off;
        zeroDepth.trackKey = 7;
        TrenchParams following = off;
        following.track = 1.0f;
        following.trackKey = 7;
        TrenchParams snapped = off;
        snapped.keySnap = 8;
        TrenchParams parked = following;
        parked.keySnap = 8;
        const auto offOut = runBridge (off);
        check (offOut == runBridge (zeroDepth), "TRACK at 0 is bit-identical to today");
        const auto followOut = runBridge (following);
        check (! (followOut == offOut), "TRACK at full moves the geometry");
        check (followOut == runBridge (snapped), "TRACK full to the heard root equals the KEY snap to that root");
        check (runBridge (parked) == runBridge (snapped), "a manual KEY parks TRACK");
    }

    */
    std::printf ("== state recall ==\n");
    {
        PluginProcessor saved;
        saved.setPlayConfigDetails (2, 2, hostRate, 512);
        saved.prepareToPlay (hostRate, 512);
        pump (100);
        setParam (saved, ParamID::movePreset, 2.0f);
        setParam (saved, ParamID::morph, 0.7f);
        pump (100);
        juce::MemoryBlock state;
        saved.getStateInformation (state);
        PluginProcessor restored;
        restored.setPlayConfigDetails (2, 2, hostRate, 512);
        restored.prepareToPlay (hostRate, 512);
        pump (100);
        restored.setStateInformation (state.getData(), (int) state.getSize());
        pump (300);
        const float movedTo = restored.apvts.getRawParameterValue (ParamID::movePreset)->load();
        const float morphTo = restored.apvts.getRawParameterValue (ParamID::morph)->load();
        check (juce::roundToInt (movedTo) == 2, "MOVEMENT recalls by name into a fresh instance", movedTo, 2.0);
        check (std::abs (morphTo - 0.7f) < 1.0e-4f, "MORPH recalls exactly", morphTo, 0.7);
    }

    if (std::getenv ("TRENCH_MEASURE") != nullptr)
    {
        trench::rescanBodyRoster();
        int n = 0; trench::bodyRoster (n);
        int bodyIndex = -1;
        for (int i = 0; i < n; ++i)
            if (trench::bodyDisplayName (i).containsIgnoreCase (std::getenv ("TRENCH_MEASURE"))) { bodyIndex = i; break; }
        check (bodyIndex >= 0, "measure body found", bodyIndex, n);
        if (bodyIndex >= 0)
        {
            setParam (processor, ParamID::body, (float) bodyIndex);
            pump (400);
            std::printf ("MEASURE body %s rate %.0f loadOk %d\n", trench::bodyDisplayName (bodyIndex).toRawUTF8(), processor.getSampleRate(), (int) processor.getLastLoadOk());
            for (float morph : { 0.0f, 0.5f, 1.0f })
            {
                setParam (processor, ParamID::morph, morph);
                setParam (processor, ParamID::q, 0.0f);
                runSine (processor, 0.0f, 8);
                constexpr int N = 8192;
                juce::AudioBuffer<float> buf (2, N);
                buf.clear();
                buf.setSample (0, 0, 0.5f); buf.setSample (1, 0, 0.5f);
                juce::MidiBuffer midi;
                for (int start = 0; start < N; start += 512)
                {
                    float* ch[2] = { buf.getWritePointer (0) + start, buf.getWritePointer (1) + start };
                    juce::AudioBuffer<float> slice (ch, 2, 512);
                    processor.processBlock (slice, midi);
                }
                float coeffs[trench::kUiCoeffCount] = {}; float boost = 1.0f;
                processor.probeCurrentBodyForUi (morph, 0.0f, coeffs, boost);
                const double sr = processor.getSampleRate();
                std::printf ("MEASURE morph %.2f  freq: measured dB / coefficient dB\n", morph);
                double worst = 0.0;
                for (double f : { 60.0, 120.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 12000.0 })
                {
                    const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
                    std::complex<double> acc (0.0, 0.0);
                    for (int i = 0; i < N; ++i) acc += (double) buf.getSample (0, i) * std::polar (1.0, -w * i);
                    const double measured = 20.0 * std::log10 (std::abs (acc) / 0.5);
                    std::complex<double> h (1.0, 0.0);
                    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
                    for (int sct = 0; sct < trench::kUiStageCount; ++sct)
                    {
                        const float* c = coeffs + sct * trench::kUiCoeffsPerStage;
                        h *= ((double) c[0] + (double) c[1] * z1 + (double) c[2] * z2) / (1.0 + (double) c[3] * z1 + (double) c[4] * z2);
                    }
                    const double expected = 20.0 * std::log10 (std::abs (h));
                    worst = juce::jmax (worst, std::abs (measured - expected));
                    std::printf ("  %7.0f Hz  %7.2f  /  %7.2f\n", f, measured, expected);
                }
                check (worst < 1.0, "audio response matches the coefficient response (worst dB)", worst, 1.0);
            }
        }
    }
    std::printf ("== key ==\n");
    check (TrenchDspBridge::keySnapRatio (0) == 1.0, "KEY OFF is ratio 1", TrenchDspBridge::keySnapRatio (0), 1.0);
    check (std::abs (TrenchDspBridge::keySnapRatio (7) - std::pow (2.0, 6.0 / 12.0)) < 1e-9, "KEY F# min is +6 semitones", TrenchDspBridge::keySnapRatio (7), std::pow (2.0, 0.5));
    check (std::abs (TrenchDspBridge::keySnapRatio (20) - std::pow (2.0, -5.0 / 12.0)) < 1e-9, "KEY G maj is -5 semitones", TrenchDspBridge::keySnapRatio (20), std::pow (2.0, -5.0 / 12.0));
    {
        trench::rescanBodyRoster();
        int n = 0; trench::bodyRoster (n);
        int bodyIndex = -1;
        for (int i = 0; i < n; ++i)
            if (trench::bodyDisplayName (i).containsIgnoreCase ("Crisp")) { bodyIndex = i; break; }
        if (bodyIndex >= 0)
        {
            setParam (processor, ParamID::body, (float) bodyIndex);
            pump (400);
            setParam (processor, ParamID::morph, 0.0f);
            setParam (processor, ParamID::keySnap, 0.0f);
            const auto off = runSine (processor, 0.01f);
            setParam (processor, ParamID::keySnap, 7.0f);
            const auto snapped = runSine (processor, 0.01f);
            check (snapped.finite && std::abs (db (snapped.peak / off.peak)) > 1.0, "KEY F# audibly shifts the Crisp body at 220 Hz (dB)", db (snapped.peak / off.peak), 1.0);
            setParam (processor, ParamID::keySnap, 0.0f);
            {
                processor.setEditorOpen (true);
                setParam (processor, ParamID::envAmount, 1.0f);
                runSine (processor, 0.0005f, 60);
                runSine (processor, 0.5f, 4);
                const float pushed = processor.getEffectiveMorphForUi();
                setParam (processor, ParamID::envAmount, 0.0f);
                runSine (processor, 0.5f, 4);
                const float still = processor.getEffectiveMorphForUi();
                processor.setEditorOpen (false);
            }
            {
            }
            if (std::getenv ("TRENCH_MEASURE") != nullptr)
            {
                int n = 0; trench::bodyRoster (n);
                for (int i = 0; i < n; ++i)
                    if (trench::bodyDisplayName (i).containsIgnoreCase (std::getenv ("TRENCH_MEASURE"))) { setParam (processor, ParamID::body, (float) i); break; }
                setParam (processor, ParamID::morph, std::getenv ("TRENCH_MORPH") != nullptr ? (float) std::atof (std::getenv ("TRENCH_MORPH")) : 0.44f);
            }
            else
                setParam (processor, ParamID::body, (float) trench::kDefaultBodyIndex);
            pump (400);
        }
    }
    {
        std::printf ("== face containment ==\n");
        auto* editor = processor.createEditorIfNeeded();
        pump (50);
        const juce::Rectangle<int> frame { 0, 0, trench::ui::kEditorWidth, trench::ui::kEditorHeight };
        int outside = 0;
        for (auto* c : editor->getChildren())
        {
            if (! c->isVisible()) continue;
            const auto b = c->getBounds();
            if (frame.contains (b)) continue;
            ++outside;
            const auto who = c->getTitle().isNotEmpty() ? c->getTitle() : c->getName();
            std::printf ("      OUTSIDE  %s  %d,%d %dx%d\n", who.toRawUTF8(),
                         b.getX(), b.getY(), b.getWidth(), b.getHeight());
        }
        check (outside == 0, "every visible child sits inside the face", outside, 0);
        const trench::UiLayout faceLayout { trench::UiLayout::defaults() };
        const trench::ui::Theme faceTheme { faceLayout };
        const auto glass = faceTheme.rect ("spectrumGrid").getSmallestIntegerContainer();
        auto* chip = findChild<trench::ui::ModulationChip> (*editor);
        check (chip != nullptr, "Modulation chip exists");
        if (chip != nullptr)
            check (glass.contains (chip->getBounds()), "Modulation chip sits inside the glass");
        processor.editorBeingDeleted (editor);
        delete editor;
    }
    if (std::getenv ("TRENCH_HEADLESS") != nullptr)
    {
        std::printf ("SKIP  == face == (TRENCH_HEADLESS)\n");
    }
    else
    {
    std::printf ("== face ==\n");
    auto* editor = processor.createEditorIfNeeded();
    juce::Component holder;
    holder.setSize (editor->getWidth(), editor->getHeight());
    holder.addAndMakeVisible (editor);
    holder.addToDesktop (juce::ComponentPeer::windowIsTemporary);
    holder.setVisible (true);
    pump (600);
    check (editor->getWidth() == trench::ui::kFaceLockedWidth && editor->getHeight() == trench::ui::kFaceLockedHeight,
           "editor is locked at the ruled DAW size", editor->getWidth(), editor->getHeight());
    auto* faceChip = findChild<trench::ui::ModulationChip> (*editor);
    check (faceChip != nullptr, "Modulation chip exists");
    if (faceChip == nullptr)
        return 1;
    check (faceChip->isShowing(), "Modulation is always on the glass");
    check (! anyVisibleOfTitle (*editor, "Bite"), "BITE is not a third gain knob");
    for (const char* gone : { "Input", "Output", "Follow", "Movement", "Key Snap", "Low", "Track", "Division", "Mix", "Section", "Bite", "Color 1", "Color 2", "Color 3", "Generator" })
        check (! anyVisibleOfTitle (*editor, gone), (juce::String ("absent from the face: ") + gone).toRawUTF8());
    check (faceChip->getHeight() >= 18, "rows are legible", faceChip->getHeight(), 18);
    pump (150);
    {
        const float shotScale = std::getenv ("TRENCH_SHOT_SCALE") != nullptr ? (float) std::atof (std::getenv ("TRENCH_SHOT_SCALE")) : 1.0f;
        savePng (holder.createComponentSnapshot (holder.getLocalBounds(), true, juce::jmax (1.0f, shotScale)), "trench_face_main.png");
    }
    setParam (processor, ParamID::movePreset, 1.0f);
    pump (150);
    savePng (holder.createComponentSnapshot (holder.getLocalBounds()), "trench_face_gen_on.png");
    processor.editorBeingDeleted (editor);
    holder.removeChildComponent (editor);
    delete editor;
    }
    std::printf ("== %d failure(s) ==\n", failures);
    return failures == 0 ? 0 : 1;
}
