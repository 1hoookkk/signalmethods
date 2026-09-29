#include "PluginProcessor.h"
#include "TrenchBodyRoster.h"
#include "BinaryData.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>
#include <utility>

namespace
{
int failures = 0;
void check (bool ok, const char* name)
{
    std::printf ("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (! ok) ++failures;
}
void selectBody (PluginProcessor& processor, int index)
{
    auto* parameter = processor.apvts.getParameter (ParamID::body);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto fixture = juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getNonexistentChildFile ("trench-user-body-test", "", false);
    check (fixture.createDirectory().wasOk(), "create isolated library fixture");
    const auto z = fixture.getChildFile ("Zebra.body240");
    const auto a = fixture.getChildFile ("Alpha.body240");
    check (z.replaceWithData (BinaryData::identity_body240, 240), "write valid fixture");
    fixture.getChildFile ("broken.body240").replaceWithText ("short");
    fixture.getChildFile ("ignored.json").replaceWithText ("{}");
    trench::detail::RosterStore store;
    trench::detail::buildRosterStore (store);
    const auto baked = store.entries.size();
    trench::detail::appendUserBodies (store, fixture);
    check (store.entries.size() == baked + 1, "only 240-byte body files enter library");
    check (store.bases[baked] == z.getFullPathName().toStdString(), "user body follows factory roster");
    a.replaceWithData (BinaryData::identity_body240, 240);
    trench::detail::appendUserBodies (store, fixture);
    check (store.bases[baked] == z.getFullPathName().toStdString()
           && store.bases.back() == a.getFullPathName().toStdString(), "new files do not renumber existing bodies");
    trench::detail::appendUserBodies (store, fixture);
    check (store.entries.size() == baked + 2, "rescan does not duplicate bodies");
    {
        juce::MemoryBlock foreign;
        juce::File (juce::String (TRENCH_TABLE_STITCH_ROOT)).getChildFile ("plugin/presets/bodies/xml_crisp.body240").loadFileAsData (foreign);
        PluginProcessor recalled;
        recalled.bodyRecoveryDirectory = fixture;
        auto state = recalled.apvts.copyState();
        state.setProperty ("bodyId", "D:/somewhere else/Crisp Take.body240", nullptr);
        state.setProperty ("bodyBytes", foreign.toBase64Encoding(), nullptr);
        juce::MemoryBlock packed;
        juce::AudioProcessor::copyXmlToBinary (*state.createXml(), packed);
        recalled.setStateInformation (packed.getData(), (int) packed.getSize());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        std::array<unsigned char, 240> got {};
        const auto recovered = fixture.getChildFile ("Crisp Take.body240");
        check (foreign.getSize() == 240 && recovered.existsAsFile() && recalled.getLastLoadOk()
               && recalled.copyCurrentBodyBytes (got.data(), got.size()) && foreign == juce::MemoryBlock (got.data(), got.size()),
               "a project whose body file is missing recovers it from the embedded bytes into the library");
    }
    {
        juce::MemoryBlock saved;
        juce::File (juce::String (TRENCH_TABLE_STITCH_ROOT)).getChildFile ("plugin/presets/bodies/xml_crisp.body240").loadFileAsData (saved);
        juce::MemoryBlock library;
        const int lp12 = trench::bodyIndexForBase ("util_lp_12");
        PluginProcessor recalled;
        auto state = recalled.apvts.copyState();
        state.setProperty ("bodyId", "util_lp_12", nullptr);
        state.setProperty ("bodyBytes", saved.toBase64Encoding(), nullptr);
        juce::MemoryBlock packed;
        juce::AudioProcessor::copyXmlToBinary (*state.createXml(), packed);
        recalled.setStateInformation (packed.getData(), (int) packed.getSize());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        std::array<unsigned char, 240> got {};
        juce::MemoryBlock libraryAfter;
        check (lp12 > 0 && saved.getSize() == 240 && trench::bodyRawBytes (lp12, library) && library != saved
               && recalled.copyCurrentBodyBytes (got.data(), got.size()) && saved == juce::MemoryBlock (got.data(), got.size())
               && trench::bodyRawBytes (lp12, libraryAfter) && libraryAfter == library,
               "recall plays the saved bytes when the library body under the same id has changed, and leaves the library alone");
    }
    for (const auto& file : fixture.findChildFiles (juce::File::findFiles, false)) file.deleteFile();
    fixture.deleteFile();

    if (argc == 2)
    {
        const juce::File body (juce::String::fromUTF8 (argv[1]));
        juce::MemoryBlock expected;
        check (body.loadFileAsData (expected) && expected.getSize() == 240, "read requested body unchanged");
        trench::rescanBodyRoster();
        const int index = trench::bodyIndexForBase (body.getFullPathName());
        check (index > 0, "requested body appears in TRENCH menu roster");
        if (index > 0)
        {
            PluginProcessor source;
            source.setPlayConfigDetails (2, 2, 44100, 256);
            source.prepareToPlay (44100, 256);
            selectBody (source, index);
            std::array<unsigned char, 240> actual {};
            check (source.copyCurrentBodyBytes (actual.data(), actual.size())
                   && expected == juce::MemoryBlock (actual.data(), actual.size()), "menu selection installs the exact exported 240 bytes");
            float actualCoeffs[trench::kUiCoeffCount] {}, expectedCoeffs[trench::kUiCoeffCount] {};
            float actualBoost = 0, expectedBoost = 0;
            bool probes = true;
            double worst = 0;
            for (const float m : { 0.0f, 0.5f, 1.0f })
                for (const float q : { 0.0f, 1.0f })
                {
                    probes = source.probeCurrentBodyForUi (m, q, actualCoeffs, actualBoost) && probes;
                    probes = TrenchDspBridge::probePackedBody (expected.getData(), expected.getSize(), m, q,
                        44100, expectedCoeffs, expectedBoost, 44100) && probes;
                    for (int c = 0; c < trench::kUiCoeffCount; ++c)
                        worst = std::max (worst, (double) std::abs (actualCoeffs[c] - expectedCoeffs[c]));
                }
            check (probes && worst == 0, "loaded body uses the explicit 44.1 kHz datum at corners and interior");
            juce::MemoryBlock state;
            source.getStateInformation (state);
            PluginProcessor restored;
            restored.setPlayConfigDetails (2, 2, 44100, 256);
            restored.prepareToPlay (44100, 256);
            restored.setStateInformation (state.getData(), (int) state.getSize());
            check (restored.getLoadedBodyIndex() == index
                   && restored.copyCurrentBodyBytes (actual.data(), actual.size())
                   && expected == juce::MemoryBlock (actual.data(), actual.size()), "DAW state recalls the selected file and exact bytes");
            juce::AudioBuffer<float> audio (2, 256);
            juce::MidiBuffer midi;
            bool finite = true;
            double energy = 0;
            for (int block = 0; block < 100; ++block)
            {
                audio.clear();
                if (block == 0) audio.setSample (0, 0, 0.0001f);
                restored.processBlock (audio, midi);
                for (int s = 0; s < audio.getNumSamples(); ++s)
                {
                    const auto v = audio.getSample (0, s);
                    finite = finite && std::isfinite (v);
                    energy += (double) v * v;
                }
            }
            check (finite && energy > 0, "selected body processes an impulse with finite nonzero output");
        }
    }
    std::printf ("== sidecar bank selection ==\n");
    {
        const juce::File fixtureDir = juce::File (juce::String (TRENCH_TABLE_STITCH_ROOT)).getChildFile ("plugin/presets/bodies");
        juce::MemoryBlock baseBytes, side48, side96, side192;
        fixtureDir.getChildFile ("xml_crisp.body240").loadFileAsData (baseBytes);
        fixtureDir.getChildFile ("_xml_crisp.48000.body240").loadFileAsData (side48);
        fixtureDir.getChildFile ("_xml_crisp.96000.body240").loadFileAsData (side96);
        fixtureDir.getChildFile ("_xml_crisp.192000.body240").loadFileAsData (side192);
        check (baseBytes.getSize() == 240 && side48.getSize() == 240 && side96.getSize() == 240 && side192.getSize() == 240,
               "sidecar fixture set (base + 48/96/192k banks) available");

        const auto userDir = trench::userBodyDirectory();
        userDir.createDirectory();
        const juce::String stem = "zz_trench_sidecar_selftest";
        const auto baseFile = userDir.getChildFile (stem + ".body240");
        const auto file48 = userDir.getChildFile ("_" + stem + ".48000.body240");
        const auto file96 = userDir.getChildFile ("_" + stem + ".96000.body240");
        const auto file192 = userDir.getChildFile ("_" + stem + ".192000.body240");
        baseFile.replaceWithData (baseBytes.getData(), baseBytes.getSize());
        file48.replaceWithData (side48.getData(), side48.getSize());
        file96.replaceWithData (side96.getData(), side96.getSize());
        file192.replaceWithData (side192.getData(), side192.getSize());

        trench::rescanBodyRoster();
        const int index = trench::bodyIndexForBase (baseFile.getFullPathName());
        check (index > 0, "sidecar test body appears in the roster");

        if (index > 0)
        {
            const auto exactCascade = [] (const juce::MemoryBlock& bytes, float m, float q, double datumRate, double runtimeRate)
            {
                const auto packed = trench::core::PackedBody::from_body_bytes (std::span {
                    static_cast<const std::uint8_t*> (bytes.getData()), bytes.getSize() });
                const auto words = packed.interpolate_words (m, q, 0.0f);
                if (datumRate > 0.0 && runtimeRate > 0.0 && ! juce::approximatelyEqual (datumRate, runtimeRate))
                    return trench::core::native::rewarp_cascade (words, datumRate, runtimeRate);
                trench::core::Cascade out {};
                for (std::size_t i = 0; i < out.size(); ++i)
                    out[i] = trench::core::section_words_to_biquad (words[i]);
                return out;
            };
            const auto cascadeDiff = [] (const trench::core::Cascade& a, const trench::core::Cascade& b)
            {
                double worst = 0.0;
                for (std::size_t s = 0; s < a.size(); ++s)
                    for (std::size_t c = 0; c < a[s].size(); ++c)
                        worst = std::max (worst, std::abs (a[s][c] - b[s][c]));
                return worst;
            };
            const auto heardAt = [&] (double rate)
            {
                PluginProcessor p;
                p.setPlayConfigDetails (2, 2, rate, 512);
                p.prepareToPlay (rate, 512);
                selectBody (p, index);
                if (auto* mp = p.apvts.getParameter (ParamID::morph))
                    mp->setValueNotifyingHost (mp->convertTo0to1 (0.0f));
                if (auto* qp = p.apvts.getParameter (ParamID::q))
                    qp->setValueNotifyingHost (qp->convertTo0to1 (0.0f));
                juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
                juce::AudioBuffer<float> buf (2, 512);
                juce::MidiBuffer midi;
                for (int b = 0; b < 40; ++b) { buf.clear(); p.processBlock (buf, midi); }
                std::array<unsigned char, 240> got {};
                const bool copiedBase = p.copyCurrentBodyBytes (got.data(), got.size())
                    && baseBytes == juce::MemoryBlock (got.data(), got.size());
                return std::make_pair (p.dspBridge.heardCascadeForTests(), copiedBase);
            };

            const auto at48 = heardAt (48000.0);
            const auto expected48 = exactCascade (side48, 0.0f, 0.0f, 48000.0, 48000.0);
            check (cascadeDiff (at48.first, expected48) == 0.0,
                   "selecting the sidecar body at 48 kHz installs the 48 kHz bank words with datumRate 48000 (no rewarp)");
            check (at48.second, "copyCurrentBodyBytes returns the base 44.1 kHz bytes at 48 kHz");

            const auto at44 = heardAt (44100.0);
            const auto expected44 = exactCascade (baseBytes, 0.0f, 0.0f, 44100.0, 44100.0);
            check (cascadeDiff (at44.first, expected44) == 0.0,
                   "selecting the sidecar body at 44.1 kHz installs the base bytes with datumRate 44100 (no bank at this rate)");
            check (at44.second, "copyCurrentBodyBytes returns the base 44.1 kHz bytes at 44.1 kHz");

            const auto at882 = heardAt (88200.0);
            const auto expected882 = exactCascade (baseBytes, 0.0f, 0.0f, 44100.0, 88200.0);
            check (cascadeDiff (at882.first, expected882) == 0.0,
                   "selecting the sidecar body at 88.2 kHz (no matching bank) falls back to the base bytes at datumRate 44100, rewarped");
            check (at882.second, "copyCurrentBodyBytes returns the base 44.1 kHz bytes at 88.2 kHz");
        }

        for (const auto& file : { baseFile, file48, file96, file192 })
            file.deleteFile();
        trench::rescanBodyRoster();
    }

    std::printf ("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
