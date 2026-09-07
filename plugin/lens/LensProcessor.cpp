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
    return out;
}

trench::core::PackedSection poleSection (double hz, double radius)
{
    trench::core::SectionGeometry g;
    g.pole = trench::core::ConjugatePair { std::clamp (hz, 20.0, 20000.0), std::clamp (radius, 0.05, 0.99999) };
    g.zero = trench::core::ConjugatePair { 1000.0, 0.0 };
    g.scale = 1.0;
    return trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
}

void unityDc (Words& w)
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
}

Processor::Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "LENS", layout())
{
    smooth = state.getRawParameterValue ("smooth");
    bypass = state.getRawParameterValue ("bypass");
    words.fill (trench::core::kIdentitySection);
    for (int i = 0; i < kWindow; ++i) window[(size_t) i] = 0.54f - 0.46f * std::cos (2.0f * (float) kPi * (float) i / (float) (kWindow - 1));
    for (int i = 0; i < kOrder; ++i) roots[(size_t) i] = std::polar (0.9, 2.0 * kPi * (i + 0.5) / kOrder);
    for (auto& s : shown) s.store (0.0);
}

void Processor::prepareToPlay (double sampleRate, int)
{
    rate = sampleRate;
    decimation = std::max (1, (int) std::lround (rate / 11025.0));
    decimator.fill (0.0f);
    phase = 0;
    ring.fill (0.0f);
    ringWrite = 0; sinceHop = 0;
    const auto cascade = trench::core::native::rewarp_cascade (words, trench::core::kP2kDatumHz, rate);
    for (auto& r : runners)
    {
        r.set_sample_rate (rate);
        r.reset();
        r.set_ring_leveller (true);
        r.set_pole_distortion (0.0);
        r.set_immediate (cascade);
    }
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void Processor::analyse()
{
    for (int lag = 0; lag <= kOrder; ++lag)
    {
        double acc = 0.0;
        for (int i = lag; i < kWindow; ++i)
        {
            const float a = ring[(size_t) ((ringWrite + i) % kWindow)] * window[(size_t) i];
            const float b = ring[(size_t) ((ringWrite + i - lag) % kWindow)] * window[(size_t) (i - lag)];
            acc += (double) a * (double) b;
        }
        autocorrelation[(size_t) lag] = acc;
    }
    if (autocorrelation[0] < 1.0e-9) return;
    autocorrelation[0] *= 1.0001;
    double error = autocorrelation[0];
    coefficients.fill (0.0);
    coefficients[0] = 1.0;
    for (int m = 1; m <= kOrder; ++m)
    {
        double acc = autocorrelation[(size_t) m];
        for (int i = 1; i < m; ++i) acc += coefficients[(size_t) i] * autocorrelation[(size_t) (m - i)];
        const double k = -acc / error;
        scratch = coefficients;
        for (int i = 1; i < m; ++i) coefficients[(size_t) i] = scratch[(size_t) i] + k * scratch[(size_t) (m - i)];
        coefficients[(size_t) m] = k;
        error *= (1.0 - k * k);
        if (error <= 0.0) return;
    }
    auto evaluate = [&] (std::complex<double> z)
    {
        std::complex<double> acc = 1.0;
        for (int i = 1; i <= kOrder; ++i) acc = acc * z + coefficients[(size_t) i];
        return acc;
    };
    for (int pass = 0; pass < 40; ++pass)
    {
        double moved = 0.0;
        for (int i = 0; i < kOrder; ++i)
        {
            std::complex<double> denominator = 1.0;
            for (int j = 0; j < kOrder; ++j) if (j != i) denominator *= (roots[(size_t) i] - roots[(size_t) j]);
            if (std::abs (denominator) < 1.0e-30) { roots[(size_t) i] += std::complex<double> (1.0e-3, 1.0e-3); continue; }
            const auto step = evaluate (roots[(size_t) i]) / denominator;
            roots[(size_t) i] -= step;
            moved = std::max (moved, std::abs (step));
        }
        if (moved < 1.0e-9) break;
    }
    const double fs = lpcRate();
    std::array<Pole, kOrder> found {};
    int count = 0;
    for (const auto& z : roots)
    {
        if (z.imag() <= 1.0e-6) continue;
        const double radius = std::abs (z);
        if (radius < 0.5 || radius > 1.05) continue;
        const double hz = std::arg (z) / (2.0 * kPi) * fs;
        if (hz < 40.0 || hz > fs * 0.49) continue;
        found[(size_t) count++] = { hz, -std::log (std::min (radius, 0.99999)) * fs / kPi };
    }
    std::sort (found.begin(), found.begin() + count, [] (const Pole& p, const Pole& q) { return p.hz < q.hz; });
    for (int i = 0; i < 6; ++i) poles[(size_t) i] = i < count ? found[(size_t) i] : Pole {};
    follow();
}

void Processor::follow()
{
    for (size_t row = 0; row < trench::core::kSectionCount; ++row)
    {
        const auto& p = poles[row];
        if (p.hz <= 0.0) { words[row] = trench::core::kIdentitySection; continue; }
        const double bandwidth = std::clamp (p.bandwidth, 20.0, 2000.0);
        words[row] = poleSection (p.hz, std::exp (-kPi * bandwidth / trench::core::kP2kDatumHz));
        shown[row * 2].store (p.hz);
        shown[row * 2 + 1].store (bandwidth);
    }
    for (size_t row = 0; row < trench::core::kSectionCount; ++row) if (poles[row].hz <= 0.0) { shown[row * 2].store (0.0); shown[row * 2 + 1].store (0.0); }
    unityDc (words);
    const auto cascade = trench::core::native::rewarp_cascade (words, trench::core::kP2kDatumHz, rate);
    const auto glide = (std::size_t) std::max (1.0, rate * (double) smooth->load() / 1000.0);
    for (auto& r : runners) r.set_glide (cascade, glide);
    frames.fetch_add (1);
}

void Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int channels = std::min (2, buffer.getNumChannels());
    for (int i = 0; i < n; ++i)
    {
        float mix = 0.0f;
        for (int ch = 0; ch < channels; ++ch) mix += buffer.getReadPointer (ch)[i];
        mix = channels > 0 ? mix / (float) channels : 0.0f;
        const float k = 1.0f / (float) decimation;
        decimator[0] += k * (mix - decimator[0]);
        decimator[1] += k * (decimator[0] - decimator[1]);
        decimator[2] += k * (decimator[1] - decimator[2]);
        if (++phase >= decimation)
        {
            phase = 0;
            ring[(size_t) ringWrite] = decimator[2];
            ringWrite = (ringWrite + 1) % kWindow;
            if (++sinceHop >= kHop) { sinceHop = 0; analyse(); }
        }
    }
    if (bypass->load() > 0.5f) return;
    for (int ch = 0; ch < channels; ++ch)
        runners[(size_t) ch].process (std::span<float> (buffer.getWritePointer (ch), (size_t) n));
}

void Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes)) state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* Processor::createEditor() { return new Editor (*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new lens::Processor(); }
