#include "PluginProcessor.h"
#include "DriveSlamTests.h"
#include "DriveLawTests.h"
#include "FrontendTests.h"
#include "ModulationTimingTests.h"
#include "UserMotionTests.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include "TestFixtures.h"
#include "TrenchBodyRoster.h"
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
    failures += driveSlamTests();
    failures += driveLawTests();
    failures += frontendTests();
    failures += modulationTimingTests();
    failures += userMotionTests();

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
    {
        PluginProcessor ranges;
        auto* in = ranges.apvts.getParameter (ParamID::preamp);
        auto* out = ranges.apvts.getParameter (ParamID::output);
        check (in->convertFrom0to1 (in->getDefaultValue()) == 0.0f && out->convertFrom0to1 (out->getDefaultValue()) == 0.0f
               && in->convertFrom0to1 (0.0f) == -24.0f && in->convertFrom0to1 (1.0f) == 24.0f && out->getLabel() == "dB",
               "INPUT and OUTPUT are levels from -24 to +24 dB, 0 dB by default", 0.0, 0.0);
    }

    std::printf ("== movement interpolation ==\n");
    {
        trench::MovementTransport transport;
        transport.bpm = 60.0;
        transport.ppq = 0.0;
        transport.playing = true;
        const auto& rail = trench::kFuncGenPatterns[1];
        float stepped[8] = {}, glided[8] = {};
        trench::Movement stepMovement, glideMovement;
        stepMovement.prepare (16.0);
        glideMovement.prepare (16.0);
        stepMovement.render (stepped, 8, transport, 2, trench::Movement::StepTransition);
        glideMovement.render (glided, 8, transport, 2, trench::Movement::GlideTransition);
        check (std::abs (stepped[6] - rail.values[1]) < 1.0e-6f,
               "STEP holds the pattern's cell", stepped[6], rail.values[1]);
        const float between = rail.values[1] + (rail.values[2] - rail.values[1]) * 0.5f;
        check (std::abs (glided[6] - between) < 1.0e-6f,
               "GLIDE interpolates directly between Morph cells", glided[6], between);
        float full[64] = {};
        trench::Movement fullMovement;
        fullMovement.prepare (16.0);
        fullMovement.render (full, 64, transport, 2, trench::Movement::StepTransition);
        bool exact = true;
        for (int i = 0; i < rail.steps && 4 * i < 64; ++i)
            exact = exact && full[4 * i] == rail.values[i];
        check (exact, "every cell plays exactly as authored");
        float anchorSample[1] = {}, wrapped[1] = {};
        trench::Movement loopMovement;
        loopMovement.prepare (16.0);
        auto loopEnd = transport; loopEnd.ppq = 9.0;
        loopMovement.render (anchorSample, 1, loopEnd, 2, trench::Movement::StepTransition);
        auto loopStart = transport; loopStart.ppq = 0.5;
        loopMovement.render (wrapped, 1, loopStart, 2, trench::Movement::StepTransition);
        check (std::abs (wrapped[0] - rail.values[2]) < 1.0e-6f,
               "a host loop wrap keeps the pattern on the song's bar grid",
               wrapped[0], rail.values[2]);
        PluginProcessor chipProcessor;
        const trench::UiLayout chipLayout { trench::UiLayout::defaults() };
        const trench::ui::Theme chipTheme { chipLayout };
        trench::ui::ModulationChip chip (chipProcessor.apvts, chipTheme);
        setParam (chipProcessor, ParamID::morph, 0.7f);
        const float morphBefore = chipProcessor.apvts.getRawParameterValue (ParamID::morph)->load();
        chip.selectPattern (2);
        check (std::abs (chipProcessor.apvts.getRawParameterValue (ParamID::morph)->load() - morphBefore) < 1.0e-6f
                   && chipProcessor.apvts.getRawParameterValue (ParamID::movePreset)->load() == 2.0f,
               "selecting a modulation preset leaves Morph where it is", chipProcessor.apvts.getRawParameterValue (ParamID::morph)->load(), morphBefore);
        setParam (chipProcessor, ParamID::morph, 0.3f);
        chip.selectPattern (0);
        check (chipProcessor.apvts.getRawParameterValue (ParamID::morph)->load() == 0.3f,
               "selecting OFF leaves Morph where it is", chipProcessor.apvts.getRawParameterValue (ParamID::morph)->load(), 0.3);

        const auto heardOver = [] (PluginProcessor& p, int blocks)
        {
            juce::AudioBuffer<float> buf (2, 64);
            juce::MidiBuffer midi;
            std::vector<float> heard;
            for (int b = 0; b < blocks; ++b)
            {
                buf.clear();
                p.processBlock (buf, midi);
                heard.push_back (p.getEffectiveMorphForUi());
            }
            return heard;
        };
        juce::MemoryBlock hedz;
        juce::File (juce::String (TRENCH_TABLE_STITCH_ROOT)).getChildFile ("plugin/presets/p2k/talking_hedz.body240").loadFileAsData (hedz);
        if (hedz.getSize() != 240)
            check (false, "the decoded Talking Hedz body is on disk", (double) hedz.getSize(), 240.0);
        else
        {
            const auto decoded = trench::core::PackedBody::from_body_bytes (std::span { static_cast<const std::uint8_t*> (hedz.getData()), hedz.getSize() });
            const auto poleHz = [] (const trench::core::Cascade& c, double rate)
            {
                std::vector<double> hz;
                for (const auto& s : c)
                    if (s[4] > 0.0 && s[3] * s[3] < 4.0 * s[4])
                        hz.push_back (std::acos (-s[3] / (2.0 * std::sqrt (s[4]))) * rate / (2.0 * juce::MathConstants<double>::pi));
                std::sort (hz.begin(), hz.end());
                return hz;
            };
            const auto peakOf = [] (const trench::core::Cascade& c, double rate)
            {
                const auto db = [&c, rate] (double f) { return trench::core::cascade_response_db (c, f, rate); };
                double best = 20.0, bestDb = db (20.0);
                for (double f = 20.0; f < std::min (20000.0, 0.45 * rate); f *= std::pow (2.0, 1.0 / 48.0))
                    if (const double d = db (f); d > bestDb) { best = f; bestDb = d; }
                const double coarse = best;
                for (double f = coarse * std::pow (2.0, -50.0 / 1200.0); f <= coarse * std::pow (2.0, 50.0 / 1200.0); f *= std::pow (2.0, 1.0 / 1200.0))
                    if (const double d = db (f); d > bestDb) { best = f; bestDb = d; }
                return std::pair<double, double> { best, bestDb };
            };
            const auto played = [] (PluginProcessor& p)
            {
                float raw[trench::kUiCoeffCount] = {};
                float boost = 1.0f;
                p.dspBridge.readUiSnapshot (raw, boost);
                trench::core::Cascade c {};
                for (std::size_t s = 0; s < c.size(); ++s)
                    for (std::size_t k = 0; k < c[s].size(); ++k)
                        c[s][k] = (double) raw[s * c[s].size() + k];
                return c;
            };
            const auto cents = [] (double a, double b) { return 1200.0 * std::abs (std::log2 (a / b)); };
            const auto hedzAt = [&hedz] (double rate, float morph, float q, float pattern)
            {
                auto p = std::make_unique<PluginProcessor>();
                setParam (*p, ParamID::morph, morph);
                setParam (*p, ParamID::q, q);
                setParam (*p, ParamID::movePreset, pattern);
                p->prepareToPlay (rate, 64);
                p->installBodyBytes (hedz.getData(), hedz.getSize());
                p->setEditorOpen (true);
                return p;
            };
            const auto c0Poles = poleHz (decoded.interpolate_biquads (0.0f, 0.0f, 0.0f), 44100.0);
            const auto reference = peakOf (decoded.interpolate_biquads (0.47f, 1.0f, 0.0f), 44100.0);
            std::printf ("decoded Talking Hedz at 44.1 kHz: C0 poles");
            for (const double hz : c0Poles) std::printf (" %.0f", hz);
            std::printf (" Hz; MORPH 47 %% Q 100 %% peak %.1f Hz %+.2f dB\n", reference.first, reference.second);
            bool polesMatch = c0Poles.size() == 6, grabbed = true, still = true, noJump = true, inStep = true;
            double worstPoleCents = 0.0, worstPeakCents = 0.0, worstPeakDb = 0.0;
            float fitLow = 1.0f, fitHigh = 0.0f, topLow = 1.0f, topHigh = 0.0f;
            for (const double rate : { 44100.0, 48000.0, 96000.0 })
            {
                const int second = (int) std::lround (rate / 64.0);
                {
                    auto corner = hedzAt (rate, 0.0f, 0.0f, 0.0f);
                    heardOver (*corner, second / 4);
                    const auto poles = poleHz (played (*corner), rate);
                    polesMatch = polesMatch && poles.size() == c0Poles.size();
                    for (std::size_t i = 0; polesMatch && i < poles.size(); ++i)
                        worstPoleCents = std::max (worstPoleCents, cents (poles[i], c0Poles[i]));
                }
                {
                    auto edge = hedzAt (rate, 0.0f, 0.0f, 1.0f);
                    const auto heard = heardOver (*edge, second * 2);
                    fitLow = std::min (fitLow, *std::min_element (heard.begin(), heard.end()));
                    fitHigh = std::max (fitHigh, *std::max_element (heard.begin(), heard.end()));
                }
                {
                    auto top = hedzAt (rate, 0.9f, 0.0f, 1.0f);
                    const auto heard = heardOver (*top, second * 2);
                    topLow = std::min (topLow, *std::min_element (heard.begin(), heard.end()));
                    topHigh = std::max (topHigh, *std::max_element (heard.begin(), heard.end()));
                }
                {
                    auto grab = hedzAt (rate, 0.47f, 1.0f, 2.0f);
                    auto twin = hedzAt (rate, 0.47f, 1.0f, 2.0f);
                    const float caught = heardOver (*grab, second * 3 / 10).back();
                    heardOver (*twin, second * 3 / 10);
                    grab->holdMorph (true);
                    grabbed = grabbed && std::abs (grab->apvts.getRawParameterValue (ParamID::morph)->load() - caught) < 0.001f;
                    setParam (*grab, ParamID::morph, 0.47f);
                    const auto held = heardOver (*grab, second / 2);
                    heardOver (*twin, second / 2);
                    still = still && std::all_of (held.begin(), held.end(), [] (float m) { return std::abs (m - 0.47f) < 1.0e-6f; });
                    const auto peak = peakOf (played (*grab), rate);
                    std::printf ("held at MORPH 47 %% Q 100 %% at %.0f Hz: peak %.1f Hz %+.2f dB\n", rate, peak.first, peak.second);
                    worstPeakCents = std::max (worstPeakCents, cents (peak.first, reference.first));
                    worstPeakDb = std::max (worstPeakDb, std::abs (peak.second - reference.second));
                    grab->holdMorph (false);
                    const auto back = heardOver (*grab, second / 2);
                    const auto twinBack = heardOver (*twin, second / 2);
                    noJump = noJump && std::abs (back.front() - 0.47f) < 0.01f;
                    for (std::size_t b = (std::size_t) std::ceil (0.25 * rate / 64.0) + 1; b < back.size(); ++b)
                        inStep = inStep && std::abs (back[b] - twinBack[b]) < 1.0e-6f;
                }
            }
            check (polesMatch && worstPoleCents < 17.0,
                   "Talking Hedz C0 plays its decoded poles at 44.1, 48 and 96 kHz", worstPoleCents, 17.0);
            check (std::abs (fitLow) < 1.0e-6f && std::abs (fitHigh - 0.5f) < 1.0e-6f
                       && std::abs (topLow - 0.45f) < 1.0e-5f && std::abs (topHigh - 0.95f) < 1.0e-5f,
                   "the wheel places the whole riff in its room: at the bottom at 0 %, 90 % of the way up at 90 %",
                   topLow, topHigh);
            check (grabbed && still,
                   "grabbing MORPH catches it where you hear it and holds it where the hand puts it");
            check (worstPeakCents < 17.0 && worstPeakDb < 1.0,
                   "held at MORPH 47 % Q 100 % the sound is Hedz's decoded peak at every rate", worstPeakCents, worstPeakDb);
            check (noJump && inStep,
                   "letting go brings the movement back without a jump, in step with an untouched twin after 0.25 s at every rate");
        }
    }

    std::printf ("== bridge boundary ==\n");
    {
        TrenchDspBridge bridge;
        bridge.prepare (48000.0, 512);
        const bool loaded = bridge.loadCartridgeBytes (BinaryData::identity_body240, (size_t) BinaryData::identity_body240Size);
        check (loaded, "identity body loads into the bridge");
        bridge.setInputDrive (1.0f);
        juce::AudioBuffer<float> buf (2, 64);
        buf.clear();
        buf.setSample (0, 0, 0.0625f);
        buf.setSample (1, 0, 0.0625f);
        TrenchParams params;
        bridge.process (buf, params);
        trench::DeskDrive model;
        model.prepare (48000.0);
        model.setEnabled (true);
        float worst = 0.0f;
        for (int i = 0; i < 64; ++i) worst = std::max (worst, std::abs (buf.getSample (0, i) - model.process (i == 0 ? 0.0625f : 0.0f)));
        check (worst < 1.0e-7f, "INPUT 0 dB through the identity body is exactly Mackity's own impulse response", worst, 1.0e-7);
    }

    std::printf ("== bridge cost ==\n");
    {
        juce::MemoryBlock crisp;
        crisp = fixtureBody ("xml_crisp.body240");
        check (crisp.getSize() > 0, "crisp body bytes found for the cost probe", (double) crisp.getSize(), 240.0);
        for (const double rate : { 44100.0, 48000.0, 96000.0 })
        {
            TrenchDspBridge bridge;
            bridge.prepare (rate, 512);
            bridge.loadCartridgeBytes (crisp);
            bridge.setRingLeveller (false);
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
        crisp = fixtureBody ("xml_crisp.body240");
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
        second = fixtureBody ("xml_crisp.body240");
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


    setParam (processor, ParamID::preamp, 20.0f);
    const auto driven = runSine (processor, in);
    check (std::abs (db (driven.peak / base.peak) - 20.0) < 0.1, "INPUT +20 dB adds 20 dB before the filter", db (driven.peak / base.peak), 20.0);
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
        const auto driven = capture (24.0f, 0.0f, 0.25f);
        const double f = goertzel (driven, 37);
        const double h = goertzel (driven, 74) + goertzel (driven, 111) + goertzel (driven, 148) + goertzel (driven, 185);
        check (f > 0.01 && h / f > 0.02, "INPUT at full into the safety clip: overs are clipped, not ducked", h / f, 0.02);
        const auto clean = capture (0.0f, 0.0f, 0.25f);
        const double f0 = goertzel (clean, 37);
        const double h0 = goertzel (clean, 74) + goertzel (clean, 111) + goertzel (clean, 148) + goertzel (clean, 185);
        check (h0 / f0 < 0.005, "INPUT at 0 is the clean path (harmonic ratio)", h0 / f0, 0.005);

        check (processor.apvts.getParameter (ParamID::slamDrive) == nullptr, "separate SLAM stage has no host control");
    }
    const auto loud = runSine (processor, 0.9f);
    check (loud.finite && loud.peak > 0.1f, "full-scale input at defaults stays finite and audible", loud.peak, 0.9);
    check (loud.peak <= trench::kFinalSafetyCeiling + 1.0e-4f, "safety ceiling bounds a full-scale input at -0.1 dBFS", loud.peak, trench::kFinalSafetyCeiling);
    const auto over = runSine (processor, 1.25f);
    check (over.finite && over.peak <= trench::kFinalSafetyCeiling, "No filter also contains over-range input", over.peak, trench::kFinalSafetyCeiling);
    setParam (processor, ParamID::preamp, 24.0f);
    const auto ceilinged = runSine (processor, 0.9f);
    check (ceilinged.finite && ceilinged.peak <= trench::kFinalSafetyCeiling + 1.0e-4f, "ceiling holds with INPUT at full", ceilinged.peak, trench::kFinalSafetyCeiling);
    check (ceilinged.peak > 0.5f, "full DRIVE into the ceiling does not mute", ceilinged.peak, 0.5);
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
        const auto measure = [&processor, hostRate] (int bodyIndex, float morph, float qValue, const juce::MemoryBlock* installed = nullptr)
        {
            if (installed != nullptr)
                processor.installBodyBytes (installed->getData(), installed->getSize());
            else
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
        const auto crossBandBytes = fixtureBody ("xml_cross_band.body240");
        check (crossBandBytes.getSize() == 240, "Cross Band fixture is available", (double) crossBandBytes.getSize(), 240.0);
        if (crossBandBytes.getSize() == 240)
        {
            const auto crossed = measure (-1, 0.5f, 0.3f, &crossBandBytes);
            std::printf ("THD 220 Hz at -12 dBFS, Cross Band MORPH 0.5 Q 0.3: %.3f %%  (fundamental %.4f, peak %.4f)\n",
                         crossed.thd, crossed.fundamental, crossed.peak);
            check (crossed.thd < 1.0, "Cross Band leaves a -12 dBFS sine under 1 % THD below the clip knee", crossed.thd, 1.0);
            setParam (processor, ParamID::body, 0.0f);
            pump (100);
        }
        double worst = 0.0;
        int worstBody = -1;
        float worstPeak = 0.0f;
        bool bounded = true, cleanBelowKnee = true;
        for (int i = 1; i < rosterCount; ++i)
        {
            const auto run = measure (i, 0.5f, 0.3f);
            bounded = bounded && std::isfinite (run.thd) && std::isfinite (run.fundamental)
                && run.peak <= trench::kFinalSafetyCeiling;
            if (run.fundamental >= 0.05 && run.peak <= trench::kFinalSafetyKnee)
                cleanBelowKnee = cleanBelowKnee && run.thd < 1.0;
            if (run.fundamental < 0.05 || run.thd <= worst)
                continue;
            worst = run.thd;
            worstBody = i;
            worstPeak = run.peak;
        }
        std::printf ("worst THD across the roster at MORPH 0.5 Q 0.3: %.3f %%  (%s, peak %.4f)\n",
                     worst, worstBody >= 0 ? trench::bodyDisplayName (worstBody).toRawUTF8() : "none", worstPeak);
        check (bounded, "all bodies remain finite and bounded when filter gain reaches the soft clip");
        check (cleanBelowKnee, "bodies below the soft clip knee retain under 1 % THD");
        setParam (processor, ParamID::body, (float) trench::kNoFilterIndex);
        setParam (processor, ParamID::morph, 0.0f);
        setParam (processor, ParamID::q, 0.0f);
        pump (200);
    }

    std::printf ("== ring leveller ==\n");
    {
        int rosterCount = 0;
        trench::bakedRoster (rosterCount);
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
        const auto ringMeasure = [hostRate] (PluginProcessor& processor, int bodyIndex, bool leveller)
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
            const auto measureFresh = [&] (bool leveller)
            {
                PluginProcessor fresh;
                fresh.setPlayConfigDetails (2, 2, hostRate, 512);
                fresh.prepareToPlay (hostRate, 512);
                return ringMeasure (fresh, ringBody, leveller);
            };
            const auto off = measureFresh (false);
            const auto on = measureFresh (true);
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
        crisp = fixtureBody ("xml_crisp.body240");
        auto runBite = [&crisp] (float bite, float* activityOut = nullptr, float amplitude = 0.1f)
        {
            TrenchDspBridge bridge;
            bridge.prepare (48000.0, 512);
            bridge.loadCartridgeBytes (crisp);
            bridge.setRingLeveller (false);
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
                        buf.setSample (c, i, (rng.nextFloat() * 2.0f - 1.0f) * amplitude);
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
        check (clean.first == bitten.first, "BITE is transparent below overload");
        check (quietActivity == 0.0f, "grit telemetry silent at Z 0", quietActivity, 0.0);
        check (bittenActivity == 0.0f, "BITE telemetry stays dark below overload", bittenActivity, 0.0);
        const auto hotClean = runBite (0.0f, nullptr, 8.0f);
        const auto hotBite = runBite (1.0f, &bittenActivity, 8.0f);
        check (hotBite.second && hotClean.first != hotBite.first, "BITE changes overloaded feedback and remains finite");
        check (bittenActivity > 0.0f, "BITE telemetry lights on overload");
        const auto cleanAgain = runBite (0.0f);
        check (clean.first == cleanAgain.first, "Z at 0 is deterministic and untouched");
    }

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
    {
        const double fs = 44100.0;
        const auto pole = [fs] (double hz, double r)
        {
            trench::core::Cascade c {};
            for (auto& sec : c) sec = { 1.0, 0.0, 0.0, 0.0, 0.0 };
            c[0] = { 1.0, 0.3, 0.2, -2.0 * r * std::cos (2.0 * juce::MathConstants<double>::pi * hz / fs), r * r };
            return c;
        };
        const auto hzOf = [fs] (const trench::core::Biquad& b)
        {
            const double r = std::sqrt (b[4]);
            return std::acos (-b[3] / (2.0 * r)) * fs / (2.0 * juce::MathConstants<double>::pi);
        };
        const auto sharp = pole (460.0, 0.995);
        check (trench::KeySnap::apply (sharp, 0, fs, nullptr, 1.0) == sharp, "KEY OFF leaves the cascade untouched");
        const auto inC = trench::KeySnap::apply (sharp, 13, fs, nullptr, 1.0);
        check (std::abs (hzOf (inC[0]) - 440.0) < 0.05, "KEY C major moves a sharp 460 Hz peak to A 440", hzOf (inC[0]), 440.0);
        check (inC[0][4] == sharp[0][4] && inC[0][0] == sharp[0][0] && inC[0][1] == sharp[0][1] && inC[0][2] == sharp[0][2],
               "KEY keeps the pole radius and the zeros");
        const auto broad = pole (460.0, 0.6);
        check (trench::KeySnap::apply (broad, 13, fs, nullptr, 1.0) == broad, "KEY leaves a broad pole alone");
        const auto eb = pole (305.0, 0.995);
        const double inMinor = hzOf (trench::KeySnap::apply (eb, 1, fs, nullptr, 1.0)[0]);
        const double inMajor = hzOf (trench::KeySnap::apply (eb, 13, fs, nullptr, 1.0)[0]);
        check (std::abs (inMinor - 311.13) < 0.1 && std::abs (inMajor - 293.66) < 0.1, "KEY 305 Hz goes to Eb in C minor and to D in C major", inMinor, inMajor);
    }
    {
        trench::rescanBodyRoster();
        int n = 0; trench::bodyRoster (n);
        int bodyIndex = -1;
        for (int i = 0; i < n; ++i)
            if (trench::bodyDisplayName (i).containsIgnoreCase ("Vowel Ah")) { bodyIndex = i; break; }
        check (bodyIndex >= 0, "a sharp-peaked body is in the roster", bodyIndex, n);
        if (bodyIndex >= 0)
        {
            setParam (processor, ParamID::body, (float) bodyIndex);
            setParam (processor, ParamID::morph, 0.3f);
            setParam (processor, ParamID::q, 0.4f);
            setParam (processor, ParamID::keySnap, 10.0f);
            pump (400);
            float coeffs[trench::kUiCoeffCount] = {};
            float boost = 1.0f;
            const bool probed = processor.probeCurrentBodyForUi (0.3f, 0.4f, coeffs, boost);
            double worstCents = 0.0;
            int snapped = 0;
            for (int s = 0; probed && s < trench::kUiStageCount; ++s)
            {
                trench::core::Biquad b { coeffs[s * 5], coeffs[s * 5 + 1], coeffs[s * 5 + 2], coeffs[s * 5 + 3], coeffs[s * 5 + 4] };
                double hz = 0.0, r = 0.0;
                if (! trench::KeySnap::snappable (b, processor.getSampleRate(), hz, r))
                    continue;
                ++snapped;
                const double midi = 69.0 + 12.0 * std::log2 (hz / 440.0);
                worstCents = juce::jmax (worstCents, 100.0 * std::abs (midi - std::round (midi)));
                check (trench::KeySnap::inScale ((int) std::round (midi), 10), "every sharp peak lands on an A minor note");
            }
            check (probed && snapped >= 3 && worstCents < 1.0, "Vowel Ah-Ee peaks sit on A minor notes within 1 cent", worstCents, (double) snapped);
            setParam (processor, ParamID::keySnap, 0.0f);
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
        const auto frame = editor->getLocalBounds();
        int outside = 0;
        auto* facePanel = editor->findChildWithID ("face");
        check (facePanel != nullptr, "the face lives in one scalable panel");
        for (auto* c : (facePanel != nullptr ? facePanel : editor)->getChildren())
        {
            if (! c->isVisible()) continue;
            const auto b = editor->getLocalArea (c, c->getLocalBounds());
            if (frame.contains (b)) continue;
            ++outside;
            const auto who = c->getTitle().isNotEmpty() ? c->getTitle() : c->getName();
            std::printf ("      OUTSIDE  %s  %d,%d %dx%d\n", who.toRawUTF8(),
                         b.getX(), b.getY(), b.getWidth(), b.getHeight());
        }
        check (outside == 0, "every visible child sits inside the face", outside, 0);
        int deskKnobs = 0;
        for (auto* c : (facePanel != nullptr ? facePanel : editor)->getChildren())
            if (c->isVisible() && dynamic_cast<trench::ui::DeskKnob*> (c) != nullptr) ++deskKnobs;
        check (deskKnobs == 2, "INPUT and OUTPUT are the only gain knobs on the face", deskKnobs, 2);
        check (findChild<trench::ui::KeySnapBox> (*editor) != nullptr,
               "KEY sits on the permanent face");
        if (auto* graph = findChild<trench::ui::GraphDisplay> (*editor))
        {
            bool self = true, children = true;
            graph->getInterceptsMouseClicks (self, children);
            check (! self && ! children, "response graph has no hidden drag control");
        }
        const trench::UiLayout faceLayout { trench::UiLayout::defaults() };
        const trench::ui::Theme faceTheme { faceLayout };
        const auto glass = faceTheme.rect ("spectrumGrid").getSmallestIntegerContainer();
        auto* movement = findChild<trench::ui::ModulationChip> (*editor);
        check (movement != nullptr, "MOVE exists on the face");
        if (movement != nullptr)
        {
            check (! glass.intersects (movement->getBounds()) && movement->getY() > glass.getBottom(), "MOVE sits under the wheels, below the response display");
            movement->selectPattern (1);
            check (processor.apvts.getRawParameterValue (ParamID::movePreset)->load() == 1.0f,
                   "MOVE pattern selector changes the processor pattern");
            processor.setEditorOpen (true);
            const float oldMorph = processor.apvts.getRawParameterValue (ParamID::morph)->load();
            setParam (processor, ParamID::morph, 0.5f);
            juce::AudioBuffer<float> audio (2, 512);
            juce::MidiBuffer midi;
            float lo = 1.0f, hi = 0.0f;
            bool running = true;
            for (int block = 0; block < 180; ++block)
            {
                audio.clear();
                processor.processBlock (audio, midi);
                const float position = processor.getEffectiveMorphForUi();
                lo = std::min (lo, position); hi = std::max (hi, position);
                running = running && processor.isMorphModulatedForUi();
            }
            check (hi - lo > 0.25f, "selected modulation moves the effective Morph position");
            check (running, "modulation remains active across resting-position crossings");
            movement->selectPattern (0);
            audio.clear();
            processor.processBlock (audio, midi);
            check (! processor.isMorphModulatedForUi(), "Modulation OFF stops the pattern");
            check (std::abs (processor.getEffectiveMorphForUi() - 0.5f) < 0.0001f,
                   "Modulation OFF returns to the resting wheel");
            setParam (processor, ParamID::morph, oldMorph);
        }
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
    check (! editor->isResizable() && editor->getWidth() == trench::ui::kFaceLockedWidth,
           "editor uses the reference hardware face size", editor->getWidth(), editor->getHeight());
    auto* faceMove = findChild<trench::ui::ModulationChip> (*editor);
    check (faceMove != nullptr, "MOVE selector exists");
    if (faceMove == nullptr)
        return 1;
    check (faceMove->isShowing(), "MOVE is visible as an inline performance control");
    for (const char* gone : { "Follow", "Low", "Track", "Division", "Color 1", "Color 2", "Color 3", "Generator" })
        check (! anyVisibleOfTitle (*editor, gone), (juce::String ("absent from the face: ") + gone).toRawUTF8());
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
