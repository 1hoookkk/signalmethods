#include "PluginProcessor.h"
#include "TrenchBodyRoster.h"
#include "BinaryData.h"
#include <array>
#include <cmath>
#include <cstdio>

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
    std::printf ("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
