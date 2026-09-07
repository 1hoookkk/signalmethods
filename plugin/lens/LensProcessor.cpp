#include "LensProcessor.h"
#include "LensEditor.h"
#include <cmath>

namespace lens
{
namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout out;
    out.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "morph", 1 }, "A to B", 0.0f, 1.0f, 0.0f));
    out.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    return out;
}

trench::core::PackedSection poleSection (double hz, double radius, std::uint16_t fifth)
{
    trench::core::SectionGeometry g;
    g.pole = trench::core::ConjugatePair { std::clamp (hz, 20.0, 20000.0), std::clamp (radius, 0.05, 0.99999) };
    g.zero = trench::core::ConjugatePair { 1000.0, 0.0 };
    g.scale = 1.0;
    auto w = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
    w[4] = fifth;
    return w;
}
}

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "LENS", layout())
{
    morph = state.getRawParameterValue ("morph");
    bypass = state.getRawParameterValue ("bypass");
    current.fill (trench::core::kIdentitySection);
    a = b = current;
}

Words Processor::words() const
{
    std::lock_guard<std::mutex> lock (wordsLock);
    return current;
}

void Processor::unityDc (Words& w)
{
    double product = 1.0;
    for (size_t s = 0; s < trench::core::kSectionCount; ++s)
    {
        const double num = 4.0 * trench::core::decode_word (w[s][0]), den = 4.0 * trench::core::decode_word (w[s][2]);
        if (std::abs (den) > 1e-12 && std::abs (num) > 1e-12) product *= num / den;
    }
    const double gain = std::pow (1.0 / std::max (1e-9, std::abs (product)), 1.0 / (double) trench::core::kSectionCount);
    const auto word = trench::core::encode_word (std::clamp (gain / 4.0, 0.0, 1.0));
    for (size_t s = 0; s < trench::core::kSectionCount; ++s) w[s][4] = word;
}

void Processor::publish() { generation.fetch_add (1); }

void Processor::setPole (int row, double hz, double radius)
{
    if (row < 0 || row >= (int) trench::core::kSectionCount) return;
    std::lock_guard<std::mutex> lock (wordsLock);
    current[(size_t) row] = poleSection (hz, radius, current[(size_t) row][4]);
    unityDc (current);
    publish();
}

void Processor::clearRow (int row)
{
    if (row < 0 || row >= (int) trench::core::kSectionCount) return;
    std::lock_guard<std::mutex> lock (wordsLock);
    current[(size_t) row] = trench::core::kIdentitySection;
    unityDc (current);
    publish();
}

void Processor::seed (const std::vector<std::pair<double, double>>& resonances)
{
    std::lock_guard<std::mutex> lock (wordsLock);
    current.fill (trench::core::kIdentitySection);
    for (size_t i = 0; i < std::min (resonances.size(), trench::core::kSectionCount); ++i)
    {
        const double bandwidth = std::max (30.0, resonances[i].second);
        const double radius = std::exp (-3.141592653589793 * bandwidth / trench::core::kP2kDatumHz);
        current[i] = poleSection (resonances[i].first, radius, 0);
    }
    unityDc (current);
    publish();
}

void Processor::capture (int which)
{
    std::lock_guard<std::mutex> lock (wordsLock);
    if (which == 0) { a = current; haveA = true; } else { b = current; haveB = true; }
}

void Processor::setMorph (float t)
{
    if (auto* p = state.getParameter ("morph")) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, t));
}

int Processor::pullInput (float* dst, int max)
{
    const unsigned int w = tapWrite.load (std::memory_order_acquire);
    unsigned int available = w - tapRead;
    if (available > kTap) { tapRead = w - kTap; available = kTap; }
    const int n = (int) std::min<unsigned int> (available, (unsigned int) max);
    for (int i = 0; i < n; ++i) dst[i] = tapRing[(size_t) ((tapRead + (unsigned int) i) & (kTap - 1))];
    tapRead += (unsigned int) n;
    return n;
}

void Processor::prepareToPlay (double sampleRate, int)
{
    rate = sampleRate;
    const auto cascade = trench::core::native::rewarp_cascade (words(), trench::core::kP2kDatumHz, rate);
    for (auto& r : runners)
    {
        r.set_sample_rate (rate);
        r.reset();
        r.set_ring_leveller (true);
        r.set_pole_distortion (0.0);
        r.set_immediate (cascade);
    }
    consumed = generation.load();
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            burstGain = m.getFloatVelocity();
            burstLeft = (int) std::lround (rate * (0.001 + 0.004 * burstGain));
            struck.fetch_add (1);
        }
    }
    {
        unsigned int w = tapWrite.load (std::memory_order_relaxed);
        const int channels = std::min (2, buffer.getNumChannels());
        for (int i = 0; i < n; ++i)
        {
            float mix = 0.0f;
            for (int ch = 0; ch < channels; ++ch) mix += buffer.getReadPointer (ch)[i];
            tapRing[(size_t) (w++ & (kTap - 1))] = channels > 0 ? mix / (float) channels : 0.0f;
        }
        tapWrite.store (w, std::memory_order_release);
    }
    const float t = morph->load();
    if (t != lastMorph)
    {
        std::unique_lock<std::mutex> lock (wordsLock, std::try_to_lock);
        if (lock.owns_lock())
        {
            if (haveA && haveB)
            {
                for (size_t s = 0; s < trench::core::kSectionCount; ++s)
                    for (size_t k = 0; k < 5; ++k) current[s][k] = trench::core::interpolate_word (a[s][k], b[s][k], t);
                generation.fetch_add (1);
            }
            lastMorph = t;
        }
    }
    if (generation.load() != consumed)
    {
        std::unique_lock<std::mutex> lock (wordsLock, std::try_to_lock);
        if (lock.owns_lock())
        {
            const auto cascade = trench::core::native::rewarp_cascade (current, trench::core::kP2kDatumHz, rate);
            for (auto& r : runners) r.set_glide (cascade, 64);
            consumed = generation.load();
        }
    }
    if (burstLeft > 0)
    {
        const int len = std::min (burstLeft, n);
        for (int i = 0; i < len; ++i)
        {
            random = random * 1664525u + 1013904223u;
            const float noise = ((float) (random >> 8) / 16777216.0f - 0.5f) * 2.0f * burstGain * 0.8f;
            for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch) buffer.getWritePointer (ch)[i] += noise;
        }
        burstLeft -= len;
    }
    if (bypass->load() > 0.5f) return;
    for (int ch = 0; ch < std::min (2, buffer.getNumChannels()); ++ch)
        runners[(size_t) ch].process (std::span<float> (buffer.getWritePointer (ch), (size_t) n));
}

void Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    {
        std::lock_guard<std::mutex> lock (wordsLock);
        auto pack = [] (const Words& w) { juce::MemoryBlock mb; for (const auto& s : w) for (auto v : s) mb.append (&v, sizeof (v)); return mb.toBase64Encoding(); };
        tree.setProperty ("current", pack (current), nullptr);
        tree.setProperty ("a", pack (a), nullptr);
        tree.setProperty ("b", pack (b), nullptr);
        tree.setProperty ("haveA", haveA, nullptr);
        tree.setProperty ("haveB", haveB, nullptr);
    }
    if (auto xml = tree.createXml()) copyXmlToBinary (*xml, destData);
}

void Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return;
    {
        std::lock_guard<std::mutex> lock (wordsLock);
        auto unpack = [] (const juce::String& text, Words& w)
        {
            juce::MemoryBlock mb; mb.fromBase64Encoding (text);
            if (mb.getSize() != sizeof (std::uint16_t) * 30) return;
            const auto* p = static_cast<const std::uint16_t*> (mb.getData());
            for (size_t s = 0; s < trench::core::kSectionCount; ++s) for (size_t k = 0; k < 5; ++k) w[s][k] = p[s * 5 + k];
        };
        unpack (tree["current"].toString(), current);
        unpack (tree["a"].toString(), a);
        unpack (tree["b"].toString(), b);
        haveA = (bool) tree["haveA"]; haveB = (bool) tree["haveB"];
        generation.fetch_add (1);
    }
    for (const char* key : { "current", "a", "b", "haveA", "haveB" }) tree.removeProperty (key, nullptr);
    state.replaceState (tree);
    lastMorph = -1.0f;
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new lens::Processor(); }
