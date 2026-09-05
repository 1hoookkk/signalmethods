#include "Sound.h"
#include <juce_dsp/juce_dsp.h>
#include <trench/core/body_from_audio.hpp>
#include <cmath>

namespace ws
{
namespace
{
constexpr int kOrder = 11;
constexpr int kFft = 1 << kOrder;

std::vector<float> magnitudeDb (const std::vector<float>& block)
{
    juce::dsp::FFT fft (kOrder);
    std::vector<float> buf (kFft * 2, 0.0f);
    const int n = std::min ((int) block.size(), kFft);
    for (int i = 0; i < n; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * i / (float) (n - 1));
        buf[(size_t) i] = block[(size_t) i] * w;
    }
    fft.performFrequencyOnlyForwardTransform (buf.data());
    std::vector<float> out (kFft / 2);
    for (int i = 0; i < kFft / 2; ++i) out[(size_t) i] = 20.0f * std::log10 (buf[(size_t) i] / (float) n * 4.0f + 1e-9f);
    return out;
}
}

bool Sound::load (const juce::File& wav)
{
    const auto clip = trench::core::audio::read_wav_mono (wav.getFullPathName().toStdString());
    if (! clip) return false;
    file = wav;
    mono = std::make_shared<std::vector<float>> (clip->samples);
    sampleRate = clip->sample_rate_hz;
    seconds = (double) mono->size() / sampleRate;
    slice = std::min (seconds * 0.5, 0.25);
    regionA = 0.0;
    regionB = seconds;
    return ! mono->empty();
}

std::vector<float> Sound::window (double at, double length) const
{
    const int n = (int) std::round (length * sampleRate);
    const int start = juce::jlimit (0, std::max (0, (int) mono->size() - n), (int) std::round (at * sampleRate) - n / 2);
    std::vector<float> out;
    for (int i = 0; i < n && start + i < (int) mono->size(); ++i) out.push_back ((*mono)[(size_t) (start + i)]);
    return out;
}

void Sound::render (int width, int height)
{
    if (mono->empty() || width < 2 || height < 2) return;
    columns = width;
    spectrogram = juce::Image (juce::Image::RGB, width, height, true);
    juce::Image::BitmapData bits (spectrogram, juce::Image::BitmapData::writeOnly);
    const double hop = seconds / width;
    for (int x = 0; x < width; ++x)
    {
        const auto mag = magnitudeDb (window (x * hop + hop * 0.5, kFft / sampleRate));
        for (int y = 0; y < height; ++y)
        {
            const double hz = 20.0 * std::pow (1000.0, 1.0 - (double) y / (height - 1));
            const int bin = juce::jlimit (0, kFft / 2 - 1, (int) std::round (hz / sampleRate * kFft));
            const float v = juce::jlimit (0.0f, 1.0f, (mag[(size_t) bin] + 80.0f) / 80.0f);
            const auto c = juce::Colour::fromFloatRGBA (v * v, v * v * 0.9f + v * 0.1f, v, 1.0f);
            bits.setPixelColour (x, y, c);
        }
    }
}

Curve Sound::sliceMagnitude (double at) const
{
    Curve out {};
    if (mono->empty()) return out;
    const auto mag = magnitudeDb (window (at, kFft / sampleRate));
    for (int i = 0; i < kCurvePoints; ++i)
    {
        const double hz = 20.0 * std::pow (1000.0, i / double (kCurvePoints - 1));
        const int bin = juce::jlimit (0, kFft / 2 - 1, (int) std::round (hz / sampleRate * kFft));
        out[(size_t) i] = mag[(size_t) bin];
    }
    return out;
}

std::optional<Words> Sound::frameAt (double at) const
{
    if (mono->empty()) return std::nullopt;
    const auto block = window (at, 0.06);
    if (block.size() < 256) return std::nullopt;
    std::vector<trench::core::audio::Resonance> res;
    if (speech) res = trench::core::audio::speech_poles (std::span<const float> (block.data(), block.size()), sampleRate, kRows);
    else res = trench::core::audio::resonances_from_audio (std::span<const float> (block.data(), block.size()), sampleRate, kRows);
    if (res.empty()) return std::nullopt;
    std::sort (res.begin(), res.end(), [] (const auto& a, const auto& b) { return a.hz < b.hz; });
    Words w {};
    for (int s = 0; s < kRows; ++s)
    {
        trench::core::SectionGeometry g;
        if (s < (int) res.size())
        {
            const double r = std::exp (-juce::MathConstants<double>::pi * std::max (10.0, res[(size_t) s].bw_hz) / kDatumHz);
            g.pole = trench::core::ConjugatePair { juce::jlimit (20.0, 20000.0, res[(size_t) s].hz), juce::jlimit (0.0, 0.9995, r) };
        }
        else g.pole = trench::core::ConjugatePair { 20000.0, 0.0 };
        g.zero = trench::core::ConjugatePair { 20000.0, 0.0 };
        g.scale = 1.0;
        const auto ws = trench::core::words_from_geometry (g, kDatumHz);
        for (int k = 0; k < kWords; ++k) w[(size_t) s][(size_t) k] = ws[(size_t) k];
    }
    unityDc (w);
    return w;
}

std::vector<juce::File> Sound::scan (const juce::File& root)
{
    std::vector<juce::File> out;
    for (const auto& sub : { "recipes/recordings", "evidence/captures/inputs", "evidence/measured-bodies/ir_library" })
        for (const auto& f : root.getChildFile (sub).findChildFiles (juce::File::findFiles, true, "*.wav"))
            out.push_back (f);
    return out;
}
}
