#include "LensProcessor.h"
#include "LensEditor.h"
#include <algorithm>
#include <cmath>

namespace lens
{
namespace
{
constexpr double kPi = 3.141592653589793;

juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout out;
    out.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "smooth", 1 }, "Smooth", juce::NormalisableRange<float> (5.0f, 400.0f, 1.0f, 0.5f), 40.0f));
    out.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    out.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gate", 1 }, "Gate", juce::NormalisableRange<float> (-90.0f, -20.0f, 1.0f), -60.0f));
    return out;
}
}

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "LENS", layout())
{
    smooth = state.getRawParameterValue ("smooth");
    bypass = state.getRawParameterValue ("bypass");
    gate = state.getRawParameterValue ("gate");
    for (auto& r : ranked) r.store (-1);
    for (auto& d : rankedDistance) d.store (0.0f);
    for (auto& s : slots) s.store (-1);
    loaded = locator.load (juce::File (TRENCH_TABLE_STITCH_ROOT).getChildFile ("evidence/research-results/corpus_index/corpus_index.bin"));
}

void Processor::prepareToPlay (double sampleRate, int)
{
    rate = sampleRate;
    decimation = std::max (1, (int) std::lround (rate / 11025.0));
    const int taps = (int) fir.size(), half = taps / 2;
    const double cutoff = 0.45 / decimation;
    double sum = 0.0;
    for (int i = 0; i < taps; ++i)
    {
        const double t = i - half;
        const double sinc = t == 0.0 ? 2.0 * cutoff : std::sin (2.0 * kPi * cutoff * t) / (kPi * t);
        const double w = 0.42 - 0.5 * std::cos (2.0 * kPi * i / (taps - 1)) + 0.08 * std::cos (4.0 * kPi * i / (taps - 1));
        fir[(size_t) i] = (float) (sinc * w);
        sum += fir[(size_t) i];
    }
    for (auto& v : fir) v = (float) (v / sum);
    firHistory.fill (0.0f);
    firWrite = 0; phase = 0; ringWrite = 0; sinceHop = 0;
    ring.fill (0.0f);
    for (auto& r : runners)
    {
        r.set_sample_rate (rate);
        r.reset();
        r.set_ring_leveller (true);
        r.set_pole_distortion (0.0);
        trench::core::CornerWords identity;
        identity.fill (trench::core::kIdentitySection);
        if (playing >= 0 && playing < (int) locator.nodes().size())
            r.set_immediate (trench::core::native::rewarp_cascade (locator.nodes()[(size_t) playing].words, locator.nodes()[(size_t) playing].datum, rate));
        else
            r.set_immediate (trench::core::native::rewarp_cascade (identity, trench::core::kP2kDatumHz, rate));
    }
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void Processor::locate()
{
    for (int i = 0; i < kFrame; ++i) frame[(size_t) i] = ring[(size_t) ((ringWrite + i) % kFrame)];
    const bool silent = Locator::levelDb (frame.data(), kFrame) < gate->load();
    quiet.store (silent);
    if (silent || ! loaded) return;
    const auto d = locator.describeAveraged (frame.data(), analysisRate());
    descriptorSeq.fetch_add (1);
    shownDescriptor = d;
    descriptorSeq.fetch_add (1);
    const auto best = locator.rank (d, kRanked);
    for (int i = 0; i < kRanked; ++i)
    {
        ranked[(size_t) i].store (i < (int) best.size() ? best[(size_t) i].node : -1);
        rankedDistance[(size_t) i].store (i < (int) best.size() ? best[(size_t) i].distance : 0.0f);
    }
    if (best.empty()) return;
    matched.store (best[0].node);
    if (best[0].node != playing)
    {
        playing = best[0].node;
        const auto& node = locator.nodes()[(size_t) playing];
        const auto cascade = trench::core::native::rewarp_cascade (node.words, node.datum, rate);
        const auto glide = (std::size_t) std::max (1.0, rate * (double) smooth->load() / 1000.0);
        for (auto& r : runners) r.set_glide (cascade, glide);
    }
    frames.fetch_add (1);
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = std::min (2, buffer.getNumChannels());
    const int taps = (int) fir.size();
    for (int i = 0; i < n; ++i)
    {
        float mix = 0.0f;
        for (int ch = 0; ch < channels; ++ch) mix += buffer.getReadPointer (ch)[i];
        mix = channels > 0 ? mix / (float) channels : 0.0f;
        firHistory[(size_t) (firWrite & 63)] = mix;
        if (++phase >= decimation)
        {
            phase = 0;
            float acc = 0.0f;
            for (int j = 0; j < taps; ++j) acc += fir[(size_t) j] * firHistory[(size_t) ((firWrite - j) & 63)];
            ring[(size_t) ringWrite] = acc;
            ringWrite = (ringWrite + 1) % kFrame;
            if (++sinceHop >= kHop) { sinceHop = 0; locate(); }
        }
        ++firWrite;
    }
    if (bypass->load() > 0.5f) return;
    for (int ch = 0; ch < channels; ++ch)
        runners[(size_t) ch].process (std::span<float> (buffer.getWritePointer (ch), (size_t) n));
}

void Processor::capture (int slot)
{
    if (slot < 0 || slot > 3) return;
    slots[(size_t) slot].store (matched.load());
}

void Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    for (int i = 0; i < 4; ++i)
    {
        const int node = slots[(size_t) i].load();
        tree.setProperty ("slot" + juce::String (i), node >= 0 && node < (int) locator.nodes().size() ? juce::String (locator.nodes()[(size_t) node].name) : juce::String(), nullptr);
    }
    if (auto xml = tree.createXml()) copyXmlToBinary (*xml, destData);
}

void Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return;
    for (int i = 0; i < 4; ++i)
    {
        const auto name = tree["slot" + juce::String (i)].toString().toStdString();
        int found = -1;
        for (int k = 0; k < (int) locator.nodes().size() && found < 0; ++k) if (locator.nodes()[(size_t) k].name == name) found = k;
        slots[(size_t) i].store (found);
        tree.removeProperty ("slot" + juce::String (i), nullptr);
    }
    state.replaceState (tree);
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new lens::Processor(); }
