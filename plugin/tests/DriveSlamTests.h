#pragma once
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "TestFixtures.h"
#include <array>
#include <tuple>
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

inline int driveSlamTests()
{
    int failed = 0;
    const auto check = [&] (bool ok, const char* label)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", label);
        if (! ok) ++failed;
    };
    const auto withDesk = [] (TrenchDspBridge& b)
    {
        int bytes = 0;
        const auto* json = BinaryData::getNamedResource ("trench_8bus_main_json", bytes);
        return b.loadDeskModel (json, (size_t) juce::jmax (0, bytes));
    };
    using Planes = std::vector<std::vector<float>>;
    const auto gainOf = [] (float db) { return juce::Decibels::decibelsToGain (db); };
    const auto sineAt = [] (double rate, float amplitude, long n)
    {
        return amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 375.0 * (double) n / rate);
    };
    const auto renderBridge = [&] (double rate, int channels, int block, float inputDb, float outputDb, long total,
                                   const std::function<float (int, long)>& source, bool desks = true)
    {
        TrenchDspBridge b;
        b.prepare (rate, 1024);
        withDesk (b);
        b.loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
        auto bypass = b.getBypass();
        bypass.outputDesk = desks;
        b.setBypass (bypass);
        b.setInputDrive (gainOf (inputDb));
        b.setOutputLevel (gainOf (outputDb));
        Planes out ((size_t) channels, std::vector<float> ((size_t) total));
        juce::AudioBuffer<float> buffer (channels, 1024);
        for (long start = 0; start < total; start += block)
        {
            const int length = (int) std::min<long> (block, total - start);
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < length; ++i)
                    buffer.setSample (c, i, source (c, start + i));
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), channels, length);
            b.process (view, {});
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < length; ++i)
                    out[(size_t) c][(size_t) (start + i)] = view.getSample (c, i);
        }
        return out;
    };
    const auto tailOf = [] (const std::vector<float>& x, double rate)
    {
        const auto n = (size_t) std::lround (rate / 15.0);
        return std::vector<float> (x.end() - (std::ptrdiff_t) n, x.end());
    };
    const auto harmonicRatio = [] (const std::vector<float>& tail, double rate)
    {
        double fundamental = 0.0, harmonics = 0.0;
        for (int k = 1; k <= 7; ++k)
        {
            double re = 0.0, im = 0.0;
            for (size_t i = 0; i < tail.size(); ++i)
            {
                const double a = 2.0 * juce::MathConstants<double>::pi * 375.0 * k * (double) i / rate;
                re += tail[i] * std::cos (a);
                im += tail[i] * std::sin (a);
            }
            (k == 1 ? fundamental : harmonics) += re * re + im * im;
        }
        return std::sqrt (harmonics / fundamental);
    };
    const auto peakOf = [] (const std::vector<float>& x)
    {
        float peak = 0.0f;
        for (float v : x) peak = std::max (peak, std::abs (v));
        return peak;
    };
    const auto settledDeviation = [&] (const std::vector<float>& out, double rate, float amplitude, float scale)
    {
        const long total = (long) out.size();
        const long n = (long) tailOf (out, rate).size();
        double worst = 0.0, peak = 0.0;
        for (long i = total - n; i < total; ++i)
        {
            const float want = sineAt (rate, amplitude, i) * scale;
            worst = std::max (worst, (double) std::abs (out[(size_t) i] - want));
            peak = std::max (peak, (double) std::abs (want));
        }
        return worst / peak;
    };
    {
        TrenchDspBridge bridge;
        bridge.prepare (48000.0, 128);
        check (withDesk (bridge), "8-Bus desk model loads into both seats of the bridge");
        check (bridge.loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size), "gain test body loads");
        check (TrenchDspBridge::deskBlend (0.5f) == 0.0f && TrenchDspBridge::deskBlend (1.0f) == 0.0f
               && TrenchDspBridge::deskBlend (gainOf (12.0f)) >= 0.999f && TrenchDspBridge::deskBlend (gainOf (24.0f)) == 1.0f
               && TrenchDspBridge::deskBlend (gainOf (6.0f)) > 0.0f && TrenchDspBridge::deskBlend (gainOf (6.0f)) < 1.0f,
               "the desk onset is 0 at or below 0 dB and reaches 1 at +12 dB");
    }
    const std::array<double, 3> rates { 44100.0, 48000.0, 96000.0 };
    for (const double rate : rates)
        for (const int channels : { 1, 2 })
        {
            const auto source = [rate] (int c, long n)
            {
                const float level = (n / 2048) % 2 == 0 ? 4.0f : 0.125f;
                return (c == 0 ? level : 0.125f) * (float) std::sin (0.07 * (double) n);
            };
            const auto out = renderBridge (rate, channels, 128, 0.0f, 0.0f, 16384, source);
            bool exact = true;
            for (int c = 0; c < channels; ++c)
                for (long n = 0; n < 16384; ++n)
                    exact = exact && out[(size_t) c][(size_t) n] == source (c, n);
            check (exact, "INPUT 0 dB and OUTPUT 0 dB give output bit-identical to input, per channel");
        }
    for (const double rate : rates)
        for (const int channels : { 1, 2 })
        {
            const long total = (long) rate / 2;
            const auto source = [rate, &sineAt] (int c, long n) { return c == 0 ? sineAt (rate, 0.5f, n) : 0.0f; };
            std::vector<float> original ((size_t) total);
            for (long n = 0; n < total; ++n) original[(size_t) n] = sineAt (rate, 0.5f, n);
            const auto sourceRatio = harmonicRatio (tailOf (original, rate), rate);
            const float quiet = gainOf (-12.0f);
            double worstScale = 0.0, worstRatio = 0.0;
            const std::array<std::array<float, 3>, 3> cases { { { -12.0f, 0.0f, quiet }, { 0.0f, -12.0f, quiet }, { -12.0f, -12.0f, quiet * quiet } } };
            for (const auto& one : cases)
            {
                const auto out = renderBridge (rate, channels, 128, one[0], one[1], total, source);
                worstScale = std::max (worstScale, settledDeviation (out[0], rate, 0.5f, one[2]));
                worstRatio = std::max (worstRatio, std::abs (harmonicRatio (tailOf (out[0], rate), rate) - sourceRatio));
            }
            std::printf ("      INPUT/OUTPUT -12 dB at %.0f Hz, %d ch: worst scaling error %.3g of peak, harmonic ratio change %.3g (source %.3g)\n",
                         rate, channels, worstScale, worstRatio, sourceRatio);
            check (worstScale <= 1.0e-5, "INPUT -12 dB and OUTPUT -12 dB are pure scalings, settled");
            check (worstRatio <= 1.0e-4 * std::max (sourceRatio, 1.0e-3), "scaling below 0 dB leaves the harmonic ratio of the source");
        }
    for (const double rate : rates)
    {
        const long total = (long) rate;
        const auto source = [rate, &sineAt] (int, long n) { return sineAt (rate, 0.5f, n); };
        for (const bool inputKnob : { true, false })
        {
            std::printf ("      %s knob at %.0f Hz (other knob 0 dB), 375 Hz sine at 0.5\n", inputKnob ? "INPUT" : "OUTPUT", rate);
            double previous = -1.0;
            bool rising = true, finite = true;
            for (const float db : { 0.0f, 3.0f, 6.0f, 12.0f, 18.0f, 24.0f })
            {
                const auto out = renderBridge (rate, 1, 128, inputKnob ? db : 0.0f, inputKnob ? 0.0f : db, total, source);
                const auto tail = tailOf (out[0], rate);
                const double ratio = harmonicRatio (tail, rate);
                const float peak = peakOf (tail);
                std::printf ("      %s %+5.1f dB  harmonic ratio %.6f  settled peak %.4f\n", inputKnob ? "INPUT " : "OUTPUT", db, ratio, peak);
                if (db > 0.0f) rising = rising && ratio > previous;
                finite = finite && std::isfinite (peak) && std::isfinite (ratio);
                previous = ratio;
            }
            check (rising, inputKnob ? "INPUT raises the harmonic ratio monotonically from +3 to +24 dB at OUTPUT 0"
                                     : "OUTPUT raises the harmonic ratio monotonically from +3 to +24 dB at INPUT 0");
            check (finite, inputKnob ? "INPUT sweep stays finite" : "OUTPUT sweep stays finite");
        }
    }
    for (const double rate : rates)
    {
        const long total = (long) rate;
        const auto source = [rate, &sineAt] (int, long n) { return sineAt (rate, 0.5f, n); };
        for (const bool inputKnob : { true, false })
            for (const float db : { 0.1f, 1.0f })
            {
                const auto out = renderBridge (rate, 1, 128, inputKnob ? db : 0.0f, inputKnob ? 0.0f : db, total, source);
                const double ratio = harmonicRatio (tailOf (out[0], rate), rate);
                if (rate == 48000.0)
                {
                    const double deviation = settledDeviation (out[0], rate, 0.5f, gainOf (db));
                    std::printf ("      %s +%.1f dB at %.0f Hz: harmonic ratio %.6f, deviation from plain gain %.4f%% of peak\n",
                                 inputKnob ? "INPUT" : "OUTPUT", db, rate, ratio, 100.0 * deviation);
                    check (deviation < 0.01, "at 48 kHz the settled waveform just above 0 dB is within 1% of the plain gain: no jump crossing 0 dB");
                }
                else
                    std::printf ("      %s +%.1f dB at %.0f Hz: harmonic ratio %.6f\n", inputKnob ? "INPUT" : "OUTPUT", db, rate, ratio);
                check (ratio < 0.005, "the first fraction of a dB above 0 is nearly clean: harmonic ratio below 0.5%");
            }
    }
    {
        const auto run = [&] (float inputDb, float outputDb, std::vector<float>& tail)
        {
            const auto pOwner = std::make_unique<PluginProcessor>();
            auto& p = *pOwner;
            p.setRateAndBufferSizeDetails (48000, 128);
            p.prepareToPlay (48000, 128);
            p.setEditorOpen (true);
            const auto fixture = fixtureBody ("xml_crisp.body240");
            const bool loaded = p.installBodyBytes (fixture.getData(), fixture.getSize());
            auto* in = p.apvts.getParameter (ParamID::preamp);
            auto* out = p.apvts.getParameter (ParamID::output);
            in->setValueNotifyingHost (in->convertTo0to1 (inputDb));
            out->setValueNotifyingHost (out->convertTo0to1 (outputDb));
            juce::AudioBuffer<float> b (2, 128);
            juce::MidiBuffer midi;
            tail.clear();
            for (int block = 0; block < 150; ++block)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 128; ++i)
                        b.setSample (c, i, sineAt (48000.0, 0.2f, (long) block * 128 + i));
                p.processBlock (b, midi);
                if (block >= 90) tail.insert (tail.end(), b.getReadPointer (0), b.getReadPointer (0) + 128);
            }
            return loaded && std::abs (p.getEffectiveMorphForUi() - 0.5f) < 1.0e-6f;
        };
        std::vector<float> before, after;
        const bool a = run (18.0f, 0.0f, before);
        const bool b = run (0.0f, 18.0f, after);
        const auto rmsOf = [] (const std::vector<float>& x) { double s = 0.0; for (float v : x) s += (double) v * v; return std::sqrt (s / (double) x.size()); };
        std::vector<float> difference (before.size());
        bool finite = true;
        for (size_t i = 0; i < before.size(); ++i)
        {
            difference[i] = before[i] - after[i];
            finite = finite && std::isfinite (before[i]) && std::isfinite (after[i]);
        }
        const double rmsBefore = rmsOf (before), rmsAfter = rmsOf (after), rmsDifference = rmsOf (difference);
        std::printf ("      resonant body: INPUT +18 / OUTPUT 0  rms %.4f peak %.4f;  INPUT 0 / OUTPUT +18  rms %.4f peak %.4f;  rms of difference %.4f\n",
                     rmsBefore, peakOf (before), rmsAfter, peakOf (after), rmsDifference);
        check (a && b && finite, "resonance integration body loads, outputs stay finite and neither knob moves the MORPH wheel");
        check (rmsDifference > 0.1 * std::min (rmsBefore, rmsAfter), "INPUT +18 dB before the filter and OUTPUT +18 dB after it sound clearly different on a resonant body");
    }
    for (const double rate : rates)
    {
        const long total = (long) rate;
        const auto source = [rate, &sineAt] (int, long n) { return sineAt (rate, 0.5f, n); };
        const auto driven = renderBridge (rate, 1, 128, 12.0f, 0.0f, total, source);
        const auto drivenQuiet = renderBridge (rate, 1, 128, 12.0f, -12.0f, total, source);
        const float scale = gainOf (-12.0f);
        const float peak = peakOf (tailOf (driven[0], rate));
        const auto drivenTail = tailOf (driven[0], rate), quietTail = tailOf (drivenQuiet[0], rate);
        double worst = 0.0;
        for (size_t i = 0; i < drivenTail.size(); ++i)
            worst = std::max (worst, (double) std::abs (quietTail[i] - drivenTail[i] * scale));
        std::printf ("      INPUT +12 with OUTPUT -12 vs OUTPUT 0 times 0.2512 at %.0f Hz: worst error %.3g of peak %.4f\n", rate, worst / peak, peak);
        check (peak > 0.0f && worst <= 1.0e-5 * peak, "OUTPUT at or below 0 dB after a driven INPUT is a pure scaling, sample for sample");
        const auto lowIn = renderBridge (rate, 1, 128, -12.0f, 12.0f, total, source);
        const auto zeroIn = renderBridge (rate, 1, 128, 0.0f, 12.0f, total, source);
        const double lowRatio = harmonicRatio (tailOf (lowIn[0], rate), rate), zeroRatio = harmonicRatio (tailOf (zeroIn[0], rate), rate);
        std::printf ("      OUTPUT +12 at %.0f Hz: harmonic ratio %.6f with INPUT -12 dB, %.6f with INPUT 0 dB\n", rate, lowRatio, zeroRatio);
        check (std::isfinite (peakOf (lowIn[0])) && lowRatio < zeroRatio, "backing INPUT down before a driven OUTPUT lowers the output desk's harmonic ratio");
    }
    {
        const auto noFilterOwner = std::make_unique<PluginProcessor>();
        auto& noFilter = *noFilterOwner;
        noFilter.setRateAndBufferSizeDetails (48000, 128);
        noFilter.prepareToPlay (48000, 128);
        const auto selected = noFilter.apvts.getParameter (ParamID::body);
        selected->setValueNotifyingHost (selected->convertTo0to1 ((float) trench::kNoFilterIndex));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        check (noFilter.getLoadedBodyIndex() == trench::kNoFilterIndex, "No Filter is the selected body");
        juce::AudioBuffer<float> wet (2, 128);
        juce::MidiBuffer m;
        const auto measure = [&] (float inputDb, float outputDb)
        {
            auto* in = noFilter.apvts.getParameter (ParamID::preamp);
            auto* out = noFilter.apvts.getParameter (ParamID::output);
            in->setValueNotifyingHost (in->convertTo0to1 (inputDb));
            out->setValueNotifyingHost (out->convertTo0to1 (outputDb));
            std::vector<float> tail;
            float peak = 0.0f;
            for (int block = 0; block < 96; ++block)
            {
                for (int i = 0; i < 128; ++i)
                {
                    const float v = sineAt (48000.0, 0.25f, (long) block * 128 + i);
                    wet.setSample (0, i, v);
                    wet.setSample (1, i, v);
                }
                noFilter.processBlock (wet, m);
                if (block >= 64)
                {
                    tail.insert (tail.end(), wet.getReadPointer (0), wet.getReadPointer (0) + 128);
                    peak = std::max (peak, wet.getMagnitude (0, 0, 128));
                }
            }
            return std::make_pair (harmonicRatio (tail, 48000.0), peak);
        };
        const auto clean = measure (0.0f, 0.0f);
        const auto inputDriven = measure (18.0f, 0.0f);
        const auto outputDriven = measure (0.0f, 18.0f);
        std::printf ("      No Filter: harmonic ratio %.6f at 0 dB, %.6f at INPUT +18, %.6f at OUTPUT +18\n", clean.first, inputDriven.first, outputDriven.first);
        check (clean.first < 0.005 && inputDriven.first > 0.05 && inputDriven.first > 10.0 * clean.first && std::isfinite (inputDriven.second),
               "No Filter still gets the pre seat: harmonic ratio at INPUT +18 dB is clearly above the 0 dB case");
        check (outputDriven.first > 0.05 && outputDriven.first > 10.0 * clean.first && std::isfinite (outputDriven.second),
               "No Filter still gets the post seat: harmonic ratio at OUTPUT +18 dB is clearly above the 0 dB case");
    }
    {
        const auto sourceOwner = std::make_unique<PluginProcessor>();
        auto& source = *sourceOwner;
        source.setRateAndBufferSizeDetails (48000, 128);
        source.prepareToPlay (48000, 128);
        auto* inputParameter = source.apvts.getParameter (ParamID::preamp);
        auto* outputParameter = source.apvts.getParameter (ParamID::output);
        check (inputParameter != nullptr && outputParameter != nullptr
               && inputParameter->convertFrom0to1 (inputParameter->getDefaultValue()) == 0.0f
               && outputParameter->convertFrom0to1 (outputParameter->getDefaultValue()) == 0.0f
               && source.apvts.getRawParameterValue (ParamID::preamp)->load() == 0.0f
               && source.apvts.getRawParameterValue (ParamID::output)->load() == 0.0f,
               "INPUT and OUTPUT exist and default to 0 dB");
        check (source.apvts.getParameter (ParamID::desk) == nullptr && source.apvts.getParameter (ParamID::inputSlam) == nullptr,
               "DESK and the SLAM switch are not host parameters");
        inputParameter->setValueNotifyingHost (inputParameter->convertTo0to1 (7.0f));
        outputParameter->setValueNotifyingHost (outputParameter->convertTo0to1 (-5.0f));
        juce::MemoryBlock saved;
        source.getStateInformation (saved);
        const auto restoredOwner = std::make_unique<PluginProcessor>();
        auto& restored = *restoredOwner;
        restored.setRateAndBufferSizeDetails (48000, 128);
        restored.prepareToPlay (48000, 128);
        restored.apvts.getParameter (ParamID::preamp)->setValueNotifyingHost (restored.apvts.getParameter (ParamID::preamp)->convertTo0to1 (-9.0f));
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        check (std::abs (restored.apvts.getRawParameterValue (ParamID::preamp)->load() - 7.0f) < 1.0e-3f
               && std::abs (restored.apvts.getRawParameterValue (ParamID::output)->load() + 5.0f) < 1.0e-3f,
               "INPUT +7 dB and OUTPUT -5 dB survive project recall");
    }
    {
        const auto legacySourceOwner = std::make_unique<PluginProcessor>();
        auto& legacySource = *legacySourceOwner;
        legacySource.setRateAndBufferSizeDetails (48000, 128);
        legacySource.prepareToPlay (48000, 128);
        const auto setNative = [&] (const char* id, float value)
        {
            auto* parameter = legacySource.apvts.getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        setNative (ParamID::preamp, 7.0f);
        setNative (ParamID::output, -5.0f);
        setNative (ParamID::body, 5.0f);
        setNative (ParamID::morph, 0.3f);
        setNative (ParamID::q, 0.7f);
        setNative (ParamID::movePreset, 3.0f);
        auto legacy = legacySource.apvts.copyState();
        const auto addRetired = [&] (const char* id, float value)
        {
            juce::ValueTree entry ("PARAM");
            entry.setProperty ("id", id, nullptr);
            entry.setProperty ("value", value, nullptr);
            legacy.addChild (entry, -1, nullptr);
        };
        addRetired (ParamID::desk, 0.6f);
        addRetired (ParamID::inputSlam, 1.0f);
        addRetired (ParamID::slamDrive, 1.0f);
        juce::MemoryBlock bytes;
        juce::AudioProcessor::copyXmlToBinary (*legacy.createXml(), bytes);
        const auto recalledOwner = std::make_unique<PluginProcessor>();
        auto& recalled = *recalledOwner;
        recalled.setRateAndBufferSizeDetails (48000, 128);
        recalled.prepareToPlay (48000, 128);
        recalled.setStateInformation (bytes.getData(), (int) bytes.getSize());
        const auto read = [&] (const char* id) { return recalled.apvts.getRawParameterValue (id)->load(); };
        const auto kept = recalled.apvts.copyState();
        check (! kept.getChildWithProperty ("id", ParamID::desk).isValid() && ! kept.getChildWithProperty ("id", ParamID::inputSlam).isValid()
               && ! kept.getChildWithProperty ("id", ParamID::slamDrive).isValid(),
               "retired desk, inputSlam and slamDrive entries are discarded on recall");
        check (std::abs (read (ParamID::preamp) - 7.0f) < 1.0e-3f && std::abs (read (ParamID::output) + 5.0f) < 1.0e-3f
               && read (ParamID::body) == 5.0f && std::abs (read (ParamID::morph) - 0.3f) < 1.0e-3f
               && std::abs (read (ParamID::q) - 0.7f) < 1.0e-3f && read (ParamID::movePreset) == 3.0f,
               "a state carrying retired entries keeps INPUT, OUTPUT, body, Morph, Q and the movement preset");
        check (recalled.apvts.getParameter (ParamID::desk) == nullptr && recalled.apvts.getParameter (ParamID::inputSlam) == nullptr,
               "DESK and inputSlam are still not host parameters after recall");
    }
    for (const char* id : { ParamID::preamp, ParamID::output })
    {
        const auto ramp = [id] (bool automate)
        {
            const auto pOwner = std::make_unique<PluginProcessor>();
            auto& p = *pOwner;
            p.setRateAndBufferSizeDetails (48000, 128);
            p.prepareToPlay (48000, 128);
            p.installBodyBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
            auto* parameter = p.apvts.getParameter (id);
            parameter->setValueNotifyingHost (automate ? parameter->convertTo0to1 (0.0f) : parameter->convertTo0to1 (24.0f));
            juce::AudioBuffer<float> audio (2, 128);
            juce::MidiBuffer midi;
            bool finite = true;
            float previous = 0.0f, jump = 0.0f, seam = 0.0f;
            constexpr int blocks = 400;
            for (int block = 0; block < blocks; ++block)
            {
                if (automate)
                {
                    const float phase = (float) block / (float) (blocks / 2);
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (24.0f * (phase <= 1.0f ? phase : 2.0f - phase)));
                }
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 128; ++i)
                        audio.setSample (c, i, 0.25f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 375.0 * (block * 128 + i) / 48000.0));
                p.processBlock (audio, midi);
                for (int i = 0; i < 128; ++i)
                {
                    const float v = audio.getSample (0, i);
                    finite = finite && std::isfinite (v);
                    if (block > 0 && (automate || block >= blocks / 2))
                    {
                        jump = std::max (jump, std::abs (v - previous));
                        if (i == 0) seam = std::max (seam, std::abs (v - previous));
                    }
                    previous = v;
                }
            }
            return std::array<float, 3> { finite ? 1.0f : 0.0f, jump, seam };
        };
        const auto steady = ramp (false);
        const auto automated = ramp (true);
        const char* name = juce::String (id) == ParamID::preamp ? "INPUT" : "OUTPUT";
        std::printf ("      %s ramp 0 -> +24 -> 0 dB: largest sample step %.4f, at a block edge %.4f (steady +24 dB: %.4f and %.4f)\n", name, automated[1], automated[2], steady[1], steady[2]);
        check (steady[0] > 0.5f && automated[0] > 0.5f, juce::String (juce::String (name) + " ramp 0 -> +24 -> 0 dB automated every block produces finite output").toRawUTF8());
        check (automated[2] <= steady[2] * 1.05f, juce::String (juce::String (name) + " ramp: the step at an automation block edge is no larger than the steady +24 dB waveform's own block-edge step").toRawUTF8());
    }
    for (const bool inputKnob : { true, false })
        for (const double rate : rates)
        {
            const long total = (long) rate / 2;
            const auto reference = [rate, &sineAt] (int c, long n) { return c == 0 ? sineAt (rate, 0.25f, n) : 0.0f; };
            const auto burst = [rate, &sineAt] (int c, long n)
            {
                if (c == 0) return sineAt (rate, 0.25f, n);
                return n >= (long) rate / 10 && n < (long) rate / 4 ? 0.9f * sineAt (rate, 1.0f, n) : 0.0f;
            };
            const auto quiet = renderBridge (rate, 2, 128, inputKnob ? 12.0f : 0.0f, inputKnob ? 0.0f : 12.0f, total, reference);
            const auto hot = renderBridge (rate, 2, 128, inputKnob ? 12.0f : 0.0f, inputKnob ? 0.0f : 12.0f, total, burst);
            check (quiet[0] == hot[0] && peakOf (hot[1]) > 0.1f,
                   inputKnob ? "a hot burst on the right channel does not change the left channel at INPUT +12 dB"
                             : "a hot burst on the right channel does not change the left channel at OUTPUT +12 dB");
        }
    for (const bool inputKnob : { true, false })
        for (const double rate : rates)
        {
            const long total = (long) rate / 2;
            const auto source = [rate, &sineAt] (int, long n) { return sineAt (rate, 0.25f, n); };
            const float inDb = inputKnob ? 12.0f : 0.0f, outDb = inputKnob ? 0.0f : 12.0f;
            const auto reference = renderBridge (rate, 1, 128, inDb, outDb, total, source);
            bool exact = true, finite = true;
            for (const int block : { 1, 3, 7, 17, 64, 512, 1000 })
            {
                const auto out = renderBridge (rate, 1, block, inDb, outDb, total, source);
                for (float v : out[0]) finite = finite && std::isfinite (v);
                exact = exact && out[0] == reference[0];
            }
            check (finite && exact, inputKnob ? "the INPUT +12 dB waveform is bit-exact across block sizes 1, 3, 7, 17, 64, 128, 512, 1000 at every rate"
                                              : "the OUTPUT +12 dB waveform is bit-exact across block sizes 1, 3, 7, 17, 64, 128, 512, 1000 at every rate");
        }
    for (const double rate : rates)
    {
        const long burstEnd = (long) rate / 10;
        const long total = burstEnd + (long) rate + 256;
        const auto source = [rate, burstEnd, &sineAt] (int, long n) { return n < burstEnd ? sineAt (rate, 1.0f, n) : 0.0f; };
        const auto out = renderBridge (rate, 2, 128, 24.0f, 24.0f, total, source);
        bool finite = true;
        for (const auto& plane : out)
            for (float v : plane) finite = finite && std::isfinite (v);
        float residual = 0.0f;
        for (long n = total - 128; n < total; ++n) residual = std::max (residual, std::abs (out[0][(size_t) n]));
        check (finite && residual < 1.0e-4f, "after 1 s of silence following a full-scale burst with both knobs at +24 dB the output has died away, no NaN or Inf");
    }
    for (const bool inputKnob : { true, false })
        for (const double rate : rates)
        {
            TrenchDspBridge bridge, reference;
            for (auto* b : { &bridge, &reference })
            {
                b->prepare (rate, 128);
                withDesk (*b);
                b->loadCartridgeBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
            }
            const auto setKnob = [&] (float db)
            {
                (inputKnob ? bridge.setInputDrive (gainOf (db)) : bridge.setOutputLevel (gainOf (db)));
            };
            juce::AudioBuffer<float> audio (2, 128), plain (2, 128);
            long n = 0;
            const auto run = [&] (long samples, bool compare)
            {
                bool exact = true;
                double worst = 0.0;
                for (long done = 0; done < samples; done += 128)
                {
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < 128; ++i)
                        {
                            audio.setSample (c, i, sineAt (rate, 0.25f, n + i));
                            plain.setSample (c, i, sineAt (rate, 0.25f, n + i));
                        }
                    bridge.process (audio, {});
                    reference.process (plain, {});
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < 128; ++i)
                        {
                            exact = exact && audio.getSample (c, i) == plain.getSample (c, i);
                            worst = std::max (worst, (double) std::abs (audio.getSample (c, i) - plain.getSample (c, i)));
                        }
                    n += 128;
                }
                if (compare) std::printf ("      %s back at 0 dB at %.0f Hz: worst difference from the plain-gain path %.3g\n", inputKnob ? "INPUT" : "OUTPUT", rate, worst);
                return compare ? exact : true;
            };
            setKnob (12.0f);
            run (long (rate * 0.25), false);
            setKnob (0.0f);
            run (long (rate * 0.2) + 128, false);
            check (run (long (rate * 0.1), true),
                   inputKnob ? "after INPUT returns from +12 dB to 0 dB and 200 ms pass, output is bit-identical to the plain-gain path again"
                             : "after OUTPUT returns from +12 dB to 0 dB and 200 ms pass, output is bit-identical to the plain-gain path again");
        }
    for (const double rate : { 44100.0, 96000.0 })
        for (const bool inputKnob : { true, false })
        {
            uint32_t seed = 12345u;
            std::vector<float> noise (4096);
            for (auto& v : noise)
            {
                seed = seed * 1664525u + 1013904223u;
                v = 0.02f * ((float) (seed >> 8) / 8388608.0f - 1.0f);
            }
            const long total = 8192;
            const auto source = [&noise] (int, long n) { return n < (long) noise.size() ? noise[(size_t) n] : 0.0f; };
            const float inDb = inputKnob ? 0.1f : 0.0f, outDb = inputKnob ? 0.0f : 0.1f;
            const auto reference = renderBridge (rate, 1, 128, inDb, outDb, total, source, false);
            const auto engaged = renderBridge (rate, 1, 128, inDb, outDb, total, source, true);
            double best = -1.0;
            int bestLag = 0;
            for (int lag = -64; lag <= 1024; ++lag)
            {
                double sum = 0.0;
                for (long n = 0; n < total; ++n)
                {
                    const long m = n - lag;
                    if (m >= 0 && m < total) sum += (double) engaged[0][(size_t) n] * reference[0][(size_t) m];
                }
                if (sum > best) { best = sum; bestLag = lag; }
            }
            std::printf ("      %s seat latency at %.0f: %d samples\n", inputKnob ? "pre" : "post", rate, bestLag);
        }
    for (const double radiusMode : { 0.0, 0.01 })
        for (const double feedbackMode : { 0.0, 1.0 })
        {
            trench::core::Cascade coefficients {};
            for (auto& stage : coefficients) stage = { 1.0, 0.0, 0.0, 0.0, 0.0 };
            coefficients[0] = { 1000.0, 0.0, 0.0, -0.5, 0.25 };
            coefficients[1][0] = 0.001;
            trench::core::CascadeRunner runner;
            runner.set_ring_leveller (false);
            runner.set_immediate (coefficients);
            runner.set_stage_saturation (true, 1.0);
            runner.set_pole_distortion (feedbackMode);
            runner.set_radius_distortion (radiusMode);
            float sample = 0.1f;
            runner.process (std::span<float> (&sample, 1));
            check (std::isfinite (sample) && std::abs (sample) <= 0.002f,
                "first section bounds a +40 dBFS peak before downstream attenuation in every processing branch");
        }
    const auto processorOwner = std::make_unique<PluginProcessor>();
    auto& processor = *processorOwner;
    processor.setRateAndBufferSizeDetails (48000, 128);
    processor.prepareToPlay (48000, 128);
    processor.setEditorOpen (true);
    check (processor.installBodyBytes (BinaryData::identity_body240, BinaryData::identity_body240Size), "output gain test body loads");
    check (processor.apvts.getParameter (ParamID::slamDrive) == nullptr, "separate SLAM parameter is retired");
    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    auto* outLevel = processor.apvts.getParameter (ParamID::output);
    outLevel->setValueNotifyingHost (outLevel->convertTo0to1 (24.0f));
    const auto settled = [&]
    {
        float peak = 0.0f;
        for (int block = 0; block < 400; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i) audio.setSample (c, i, 0.9f * (float) std::sin (0.05 * (block * 128 + i)));
            processor.processBlock (audio, midi);
            if (block >= 300) peak = std::max (peak, audio.getMagnitude (0, 0, 128));
        }
        return peak;
    };
    const float held = settled();
    {
        const auto fromStartOwner = std::make_unique<PluginProcessor>();
        auto& fromStart = *fromStartOwner;
        fromStart.setRateAndBufferSizeDetails (48000, 128);
        auto* out = fromStart.apvts.getParameter (ParamID::output);
        out->setValueNotifyingHost (out->convertTo0to1 (24.0f));
        fromStart.prepareToPlay (48000, 128);
        fromStart.installBodyBytes (BinaryData::identity_body240, BinaryData::identity_body240Size);
        juce::AudioBuffer<float> first (2, 128);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i) first.setSample (c, i, 0.9f * (float) std::sin (0.05 * i));
        fromStart.processBlock (first, midi);
        const float firstPeak = first.getMagnitude (0, 0, 128);
        std::printf ("      OUTPUT +24 dB from the first block: peak %.3f (settled %.3f)\n", firstPeak, held);
        check (firstPeak < 2.0f * held, "OUTPUT already above 0 dB when playback starts: the desk is in from the first sample, no plain-gain burst");
    }
    auto* output = processor.apvts.getParameter (ParamID::output);
    output->setValueNotifyingHost (output->convertTo0to1 (0.0f));
    const float unity = settled();
    std::printf ("      OUTPUT +24 dB on a full-scale tone: %.3f (unity %.3f)\n", held, unity);
    check (held > 2.0f * unity && held < 12.0f * unity,
        "OUTPUT +24 dB drives the desk: well above unity, held under the plain +24 dB, no final ceiling");
    output->setValueNotifyingHost (output->convertTo0to1 (24.0f));
    settled();
    check (processor.getOutClipForUi() > 0.0f, "the output meter reports overs without changing the audio");
    check (processor.apvts.getParameter ("outputTrim") == nullptr, "no secondary output trim control exists");
    auto savedTree = processor.apvts.copyState();
    juce::ValueTree retired ("PARAM");
    retired.setProperty ("id", "outputTrim", nullptr);
    retired.setProperty ("value", 6.0f, nullptr);
    savedTree.addChild (retired, -1, nullptr);
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary (*savedTree.createXml(), saved);
    processor.setStateInformation (saved.getData(), (int) saved.getSize());
    check (! processor.apvts.copyState().getChildWithProperty ("id", "outputTrim").isValid(),
        "retired output trim is discarded when loading an interim saved project");
    {
        const float after = settled();
        check (after == held, "retired output trim cannot change the requested output gain");
    }
    check (processor.apvts.getParameter ("amount") == nullptr, "redundant MIX parameter is removed");
    auto oldMixState = processor.apvts.copyState();
    juce::ValueTree oldMix ("PARAM");
    oldMix.setProperty ("id", "amount", nullptr);
    oldMix.setProperty ("value", 0.0f, nullptr);
    oldMixState.addChild (oldMix, -1, nullptr);
    juce::MemoryBlock oldMixBytes;
    juce::AudioProcessor::copyXmlToBinary (*oldMixState.createXml(), oldMixBytes);
    processor.setStateInformation (oldMixBytes.getData(), (int) oldMixBytes.getSize());
    check (! processor.apvts.copyState().getChildWithProperty ("id", "amount").isValid(),
        "old dry MIX settings are discarded on project recall");
    check (settled() == held, "effect stays fully wet after recalling a former dry MIX setting");
    auto legacy = processor.apvts.copyState();
    juce::ValueTree oldSlam ("PARAM");
    oldSlam.setProperty ("id", ParamID::slamDrive, nullptr);
    oldSlam.setProperty ("value", 1.0f, nullptr);
    legacy.addChild (oldSlam, -1, nullptr);
    juce::MemoryBlock legacyBytes;
    juce::AudioProcessor::copyXmlToBinary (*legacy.createXml(), legacyBytes);
    processor.setStateInformation (legacyBytes.getData(), (int) legacyBytes.getSize());
    check (! processor.apvts.copyState().getChildWithProperty ("id", ParamID::slamDrive).isValid(),
        "old SLAM settings cannot silently re-enable output distortion");
    auto withDistortion = processor.apvts.copyState();
    juce::ValueTree oldDistortion ("PARAM");
    oldDistortion.setProperty ("id", ParamID::distortion, nullptr);
    oldDistortion.setProperty ("value", 1.0f, nullptr);
    withDistortion.addChild (oldDistortion, -1, nullptr);
    juce::MemoryBlock distortionBytes;
    juce::AudioProcessor::copyXmlToBinary (*withDistortion.createXml(), distortionBytes);
    processor.setStateInformation (distortionBytes.getData(), (int) distortionBytes.getSize());
    check (! processor.apvts.copyState().getChildWithProperty ("id", ParamID::distortion).isValid(),
        "old Distortion settings are discarded on project recall");
    {
        trench::EightBusDesk desk;
        int bytes = 0;
        const auto* json = BinaryData::getNamedResource ("trench_8bus_main_json", bytes);
        check (desk.load (json, (size_t) bytes, 0.001f), "the 8-Bus desk model loads at the plugin's compiled size");
        desk.prepare (48000.0, 128);
        float quiet = 0.0f, hot = 0.0f;
        std::array<float, 128> block {};
        for (int b = 0; b < 375; ++b)
        {
            for (int i = 0; i < 128; ++i) block[(size_t) i] = 0.01f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (b * 128 + i) / 48000.0);
            desk.process (block.data(), 128);
            if (b > 187) for (float v : block) quiet = std::max (quiet, std::abs (v));
        }
        desk.reset();
        for (int b = 0; b < 375; ++b)
        {
            for (int i = 0; i < 128; ++i) block[(size_t) i] = 4.0f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (b * 128 + i) / 48000.0);
            desk.process (block.data(), 128);
            for (float v : block) hot = std::max (hot, std::abs (v));
        }
        std::printf ("      8-Bus desk: quiet 0.01 -> %.4f, hot 4.0 -> %.3f\n", quiet, hot);
        desk.reset();
        float beyond = 0.0f;
        for (int b = 0; b < 375; ++b)
        {
            for (int i = 0; i < 128; ++i) block[(size_t) i] = 16.0f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * (b * 128 + i) / 48000.0);
            desk.process (block.data(), 128);
            for (float v : block) beyond = std::max (beyond, std::abs (v));
        }
        check (std::abs (quiet - 0.01f) < 0.002f && hot > 0.02f && hot < 0.2f && beyond > 0.02f && beyond < 0.2f,
            "the 8-Bus desk padded to unity passes quiet material and holds hot and absurd ones at its rail");
    }
#if TRENCH_DEV_PANEL
    for (const char* id : trench::calibration::retired)
        check (processor.apvts.getParameter (id) == nullptr, "retired dynamics control is absent from Dev");
#endif
    return failed;
}
