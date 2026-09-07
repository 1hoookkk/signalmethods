#include "Library.h"
#include "dsp/Peevers.h"
#include <trench/core/body_from_audio.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <map>

namespace hs
{
namespace
{
juce::String titleOf (const juce::String& stem)
{
    juce::String out;
    for (const auto& part : juce::StringArray::fromTokens (stem, "_", ""))
    {
        if (part.isEmpty()) continue;
        if (out.isNotEmpty()) out += " ";
        out += part.substring (0, 1).toUpperCase() + part.substring (1);
    }
    return out;
}

const char* ipaOf (const juce::String& klatt)
{
    static const std::pair<const char*, const char*> table[] = {
        { "iy", "i" }, { "ih", "\xc9\xaa" }, { "ey", "e" }, { "eh", "\xc9\x9b" }, { "ae", "\xc3\xa6" }, { "aa", "\xc9\x91" },
        { "ao", "\xc9\x94" }, { "ah", "\xca\x8c" }, { "ow", "o" }, { "uh", "\xca\x8a" }, { "uw", "u" }, { "er", "\xc9\x9d" } };
    for (const auto& t : table) if (klatt == t.first) return t.second;
    return klatt.toRawUTF8();
}

std::optional<trench::core::audio::MonoClip> clipOf (const juce::File& file)
{
    if (auto wav = trench::core::audio::read_wav_mono (std::filesystem::path (file.getFullPathName().toWideCharPointer()))) return wav;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return std::nullopt;
    const int n = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (60 * 96000));
    const int channels = (int) juce::jmax (1u, reader->numChannels);
    juce::AudioBuffer<float> buffer (channels, n);
    reader->read (&buffer, 0, n, 0, true, true);
    trench::core::audio::MonoClip clip;
    clip.sample_rate_hz = reader->sampleRate > 0.0 ? reader->sampleRate : trench::core::kP2kDatumHz;
    clip.samples.assign ((size_t) n, 0.0f);
    for (int c = 0; c < channels; ++c)
    {
        const auto* in = buffer.getReadPointer (c);
        for (int i = 0; i < n; ++i) clip.samples[(size_t) i] += in[i] / (float) channels;
    }
    return clip;
}

const char* hillenbrandIpa (const juce::String& code)
{
    static const std::pair<const char*, const char*> table[] = {
        { "iy", "i" }, { "ih", "\xc9\xaa" }, { "ei", "e" }, { "eh", "\xc9\x9b" }, { "ae", "\xc3\xa6" }, { "ah", "\xc9\x91" },
        { "aw", "\xc9\x94" }, { "oa", "o" }, { "oo", "\xca\x8a" }, { "uw", "u" }, { "uh", "\xca\x8c" }, { "er", "\xc9\x9d" } };
    for (const auto& t : table) if (code == t.first) return t.second;
    return code.toRawUTF8();
}
}

std::vector<Star> loadLibrary (const juce::File& p2kDir)
{
    std::vector<Star> out;
    auto files = p2kDir.findChildFiles (juce::File::findFiles, false, "*.body240");
    files.sort();
    for (const auto& file : files)
    {
        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb) || mb.getSize() != trench::core::kLegacyBodyBytes) continue;
        const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> ((const std::uint8_t*) mb.getData(), mb.getSize()));
        for (int c = 0; c < 4; ++c)
        {
            Star s;
            s.body = titleOf (file.getFileNameWithoutExtension());
            s.corner = kPinNames[c];
            s.name = s.body + " " + s.corner;
            s.kind = "factory";
            for (size_t row = 0; row < kRows; ++row) s.words[row] = body.words[(size_t) c][row];
            out.push_back (s);
        }
    }
    return out;
}

std::vector<Star> loadVowels (const juce::File& bankFile)
{
    std::vector<Star> out;
    if (! bankFile.existsAsFile()) return out;
    const auto v = juce::JSON::parse (bankFile);
    auto* frames = v.getProperty ("frames", juce::var()).getArray();
    if (frames == nullptr) return out;
    const auto bank = v.getProperty ("name", bankFile.getFileNameWithoutExtension()).toString();
    for (const auto& f : *frames)
    {
        Star s;
        s.kind = "vowel";
        s.body = bank;
        const auto raw = f.getProperty ("name", "").toString();
        const auto tokens = juce::StringArray::fromTokens (raw, " ", "");
        const auto klatt = tokens.size() >= 3 && tokens[0] == "vowel" ? tokens[1] : raw;
        s.corner = klatt;
        const bool grouped = tokens.size() >= 3 && tokens[0] == "vowel";
        s.name = juce::String (juce::CharPointer_UTF8 (grouped ? hillenbrandIpa (klatt) : ipaOf (klatt)));
        if (grouped) s.name += " " + tokens[2];
        auto* rows = f.getProperty ("rawWords", juce::var()).getArray();
        if (rows == nullptr || rows->size() != kRows) continue;
        bool ok = true;
        for (int r = 0; r < kRows && ok; ++r)
        {
            auto* words = (*rows)[r].getArray();
            ok = words != nullptr && words->size() == kWords;
            if (ok) for (int k = 0; k < kWords; ++k) s.words[(size_t) r][(size_t) k] = (std::uint16_t) (int) (*words)[k];
        }
        if (ok && admit (s.words)) out.push_back (s);
    }
    return out;
}

std::vector<Star> loadBodies (const juce::File& dir)
{
    static std::map<juce::String, std::vector<Star>> cache;
    const auto key = dir.getFullPathName();
    if (const auto it = cache.find (key); it != cache.end()) return it->second;
    std::vector<Star> out;
    auto files = dir.findChildFiles (juce::File::findFiles, true, "*.wav");
    files.sort();
    for (const auto& file : files)
    {
        if (file.getFileNameWithoutExtension().containsIgnoreCase ("sweep")) continue;
        auto star = readWav (file);
        if (! star) continue;
        star->kind = "body";
        star->body = file.getParentDirectory().getFileName();
        out.push_back (*star);
    }
    cache[key] = out;
    return out;
}

std::vector<Star> loadTable (const juce::File& csv, const juce::String& bank)
{
    std::vector<Star> out;
    juce::StringArray lines;
    csv.readLines (lines);
    for (const auto& line : lines)
    {
        auto cols = juce::StringArray::fromTokens (line, ",", "");
        if (cols.size() < 5 || ! cols[2].trim().containsOnly ("0123456789.")) continue;
        Star s;
        s.kind = "vowel";
        s.body = bank;
        s.corner = cols[0].trim();
        s.name = s.corner + " " + cols[1].trim();
        s.words = vowelWords ({ cols[2].getDoubleValue(), cols[3].getDoubleValue(), cols[4].getDoubleValue(), kNeutralF4 });
        out.push_back (s);
    }
    return out;
}

std::vector<Star> loadReads (const juce::File& dir, const juce::File& census)
{
    static std::map<juce::String, std::vector<Star>> cache;
    const auto key = dir.getFullPathName() + "|" + census.getFullPathName();
    if (const auto it = cache.find (key); it != cache.end()) return it->second;
    std::vector<Star> out;
    juce::StringArray families, lines;
    const bool everyFamily = ! census.existsAsFile();
    census.readLines (lines);
    for (const auto& line : lines)
    {
        if (! (line.contains ("KEY-TRACKED") || line.contains ("FIXED"))) continue;
        const int cut = line.indexOf ("  ");
        if (cut > 0) families.add (line.substring (0, cut).trim());
    }
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.wav");
    files.sort();
    for (const auto& file : files)
    {
        const auto stem = file.getFileNameWithoutExtension();
        const int space = stem.lastIndexOfChar (' ');
        const auto family = space > 0 ? stem.substring (0, space) : stem;
        if (! everyFamily && ! families.contains (family)) continue;
        auto star = readWav (file);
        if (! star) continue;
        star->body = family;
        star->corner = space > 0 ? stem.substring (space + 1) : juce::String();
        star->path = file.getFullPathName();
        out.push_back (*star);
    }
    cache[key] = out;
    return out;
}

void unityDc (Words& words)
{
    double product = 1.0;
    for (size_t s = 0; s < kRows; ++s)
    {
        const auto& r = words[s];
        const double num = 4.0 * trench::core::decode_word (r[0]), den = 4.0 * trench::core::decode_word (r[2]);
        if (std::abs (den) > 1e-12 && std::abs (num) > 1e-12) product *= num / den;
    }
    const double gain = std::pow (1.0 / std::max (1e-9, std::abs (product)), 1.0 / kRows);
    const auto word = trench::core::encode_word (std::clamp (gain / 4.0, 0.0, 1.0));
    for (size_t s = 0; s < kRows; ++s) words[s][4] = word;
}

std::optional<Star> readWav (const juce::File& wav)
{
    const auto clip = trench::core::audio::read_wav_mono (std::filesystem::path (wav.getFullPathName().toWideCharPointer()));
    if (! clip || clip->samples.size() < 256) return std::nullopt;
    auto found = trench::core::audio::speech_poles (clip->samples, clip->sample_rate_hz, 6);
    if (found.empty()) return std::nullopt;
    std::sort (found.begin(), found.end(), [] (const auto& a, const auto& b) { return a.hz < b.hz; });
    Star s;
    s.kind = "read";
    s.body = wav.getFileNameWithoutExtension();
    s.name = s.body;
    s.path = wav.getFullPathName();
    const double support[2] = { 3500.0, 4500.0 };
    size_t supported = 0;
    for (size_t i = 0; i < kRows - 1; ++i)
    {
        trench::core::SectionGeometry g;
        if (i < found.size())
        {
            const double hz = std::clamp (found[i].hz, 60.0, 12000.0);
            const double poleRadius = std::clamp (std::exp (-3.141592653589793 * std::max (10.0, found[i].bw_hz) / trench::core::kP2kDatumHz), 0.0, 0.9995);
            g.pole = trench::core::ConjugatePair { hz, poleRadius };
            g.zero = trench::core::DegeneratePair {};
        }
        else
        {
            const double hz = support[std::min<size_t> (supported++, 1)] * (supported > 2 ? 1.0 + 0.2 * (double) (supported - 2) : 1.0);
            g.pole = trench::core::ConjugatePair { hz, radiusForWidth (hz, 4.0) };
            g.zero = trench::core::DegeneratePair {};
        }
        s.words[i] = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
    }
    s.words[kRows - 1] = rowWords ({ RowType::notch, kFreqCodes - 1, 0 }, 0);
    unityDc (s.words);
    if (! admit (s.words)) return std::nullopt;
    return s;
}

Words fittedWords (const Fitted& fit, double sampleRateHz)
{
    constexpr double tau = 6.283185307179586;
    const double rate = sampleRateHz > 0.0 ? sampleRateHz : trench::core::kP2kDatumHz;
    const double warp = rate / trench::core::kP2kDatumHz;
    struct Root { double hz = 0.0, radius = 0.0; };
    std::vector<Root> poles, zeros;
    auto gather = [&] (const std::vector<std::complex<double>>& in, std::vector<Root>& to) {
        for (const auto& r : in)
        {
            if (! (r.imag() > 0.0)) continue;
            const double hz = std::arg (r) * rate / tau;
            if (! (hz > 0.0)) continue;
            to.push_back ({ hz, std::abs (r) });
        }
    };
    gather (fit.poles, poles);
    gather (fit.zeros, zeros);
    std::sort (poles.begin(), poles.end(), [] (const Root& a, const Root& b) { return a.hz < b.hz; });
    std::vector<int> mate (poles.size(), -1);
    std::vector<bool> taken (zeros.size(), false);
    for (size_t i = 0; i < poles.size(); ++i)
    {
        int best = -1;
        double gap = 1.0e300;
        for (size_t j = 0; j < zeros.size(); ++j)
        {
            if (taken[j]) continue;
            const double d = std::abs (std::log (zeros[j].hz / poles[i].hz));
            if (d < gap) { gap = d; best = (int) j; }
        }
        if (best >= 0) { taken[(size_t) best] = true; mate[i] = best; }
    }
    Words w {};
    const double support[2] = { 3500.0, 4500.0 };
    size_t supported = 0;
    for (size_t i = 0; i + 1 < kRows; ++i)
    {
        trench::core::SectionGeometry g;
        if (i < poles.size())
        {
            g.pole = trench::core::ConjugatePair { std::clamp (poles[i].hz, 60.0, 12000.0),
                std::clamp (std::pow (std::clamp (poles[i].radius, 1.0e-9, 0.99999), warp), 0.0, 0.9995) };
            if (mate[i] >= 0)
            {
                const auto& zero = zeros[(size_t) mate[i]];
                g.zero = trench::core::ConjugatePair { std::clamp (zero.hz, 20.0, 20000.0),
                    std::clamp (std::pow (std::clamp (zero.radius, 1.0e-9, 1.0), warp), 0.0, 1.0) };
            }
            else g.zero = trench::core::DegeneratePair {};
        }
        else
        {
            const double hz = support[std::min<size_t> (supported++, 1)] * (supported > 2 ? 1.0 + 0.2 * (double) (supported - 2) : 1.0);
            g.pole = trench::core::ConjugatePair { hz, radiusForWidth (hz, 4.0) };
            g.zero = trench::core::DegeneratePair {};
        }
        w[i] = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
    }
    w[kRows - 1] = rowWords ({ RowType::notch, kFreqCodes - 1, 0 }, 0);
    unityDc (w);
    return w;
}

std::optional<Star> fitWav (const juce::File& wav)
{
    auto clip = clipOf (wav);
    if (! clip || clip->samples.size() < 256) return std::nullopt;
    Peevers peevers;
    peevers.avgk = 0.9f;
    peevers.lpcenv = 1;
    peevers.setParms (2048, 2048, 512, 7);
    std::vector<float> mono = clip->samples;
    if (mono.size() < 4096)
        while (mono.size() < 16384) mono.insert (mono.end(), clip->samples.begin(), clip->samples.end());
    const size_t win = (size_t) peevers.winsize, hop = (size_t) peevers.stride;
    std::vector<float> raw ((size_t) peevers.nfft + 2, 0.0f);
    std::vector<double> mean ((size_t) peevers.nfft2 + 1, 0.0);
    const double keep = (double) peevers.avgk;
    int frames = 0;
    for (size_t at = 0; at + win <= mono.size(); at += hop)
    {
        peevers.averagedFrame (mono.data() + at);
        peevers.spectrum (peevers.synth.data(), peevers.nfft, raw.data(), peevers.nfft);
        for (size_t i = 0; i < mean.size(); ++i) mean[i] = mean[i] * keep + (double) raw[i] * (1.0 - keep);
        ++frames;
    }
    if (frames == 0) return std::nullopt;
    double top = 0.0;
    for (double v : mean) top = std::max (top, v);
    if (! (top > 0.0)) return std::nullopt;
    std::vector<double> magnitudeDb (mean.size(), 0.0);
    for (size_t i = 0; i < mean.size(); ++i) magnitudeDb[i] = 10.0 * std::log10 (std::max (mean[i] / top, 1.0e-14));
    std::vector<std::complex<double>> response;
    minimumPhaseResponse (magnitudeDb, response);
    if (response.empty()) return std::nullopt;
    const auto fit = vectorFit (response, clip->sample_rate_hz, 5, 8);
    if (fit.poles.empty()) return std::nullopt;
    Star s;
    s.kind = "read";
    s.body = wav.getFileNameWithoutExtension();
    s.name = s.body + " fit";
    s.corner = "fit";
    s.path = wav.getFullPathName();
    s.words = fittedWords (fit, clip->sample_rate_hz);
    if (! admit (s.words)) return std::nullopt;
    return s;
}

Words vowelWords (const std::array<double, 4>& formants)
{
    const double hzs[5] = { formants[0], formants[1], formants[2], formants[3], 3750.0 };
    const double widths[5] = { 2.35, 1.63, 2.19, 2.0, 0.9 };
    Words w {};
    for (size_t i = 0; i < 5; ++i)
    {
        trench::core::SectionGeometry g;
        const double hz = std::clamp (hzs[i], 60.0, 12000.0);
        const double poleBw = hz * (std::pow (2.0, widths[i] / 12.0) - 1.0), zeroBw = hz * (std::pow (2.0, 16.0 * widths[i] / 12.0) - 1.0);
        g.pole = trench::core::ConjugatePair { hz, std::clamp (std::exp (-3.141592653589793 * poleBw / trench::core::kP2kDatumHz), 0.0, 0.9995) };
        g.zero = trench::core::ConjugatePair { hz, std::clamp (std::exp (-3.141592653589793 * zeroBw / trench::core::kP2kDatumHz), 0.0, 0.9995) };
        w[i] = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
    }
    w[5] = rowWords ({ RowType::notch, kFreqCodes - 1, 0 }, 0);
    unityDc (w);
    return w;
}

Words transposed (const Words& words, double ratio)
{
    Words out = words;
    if (ratio <= 0.0) return out;
    for (size_t r = 0; r + 1 < kRows; ++r)
    {
        auto g = trench::core::geometry_from_words (words[r], trench::core::kP2kDatumHz);
        auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
        if (pole == nullptr || pole->radius < 0.05) continue;
        auto shift = [ratio] (trench::core::ConjugatePair& p) {
            p.hz = std::clamp (p.hz * ratio, 20.0, 20000.0);
            p.radius = std::clamp (std::pow (std::clamp (p.radius, 1e-9, 1.0), ratio), 0.0, 0.99999);
        };
        shift (*pole);
        if (auto* zero = std::get_if<trench::core::ConjugatePair> (&g.zero); zero != nullptr && zero->radius > 0.05) shift (*zero);
        out[r] = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
        out[r][4] = words[r][4];
    }
    return out;
}

Words relaxed (const Words& words)
{
    Words out = words;
    for (size_t r = 0; r + 1 < kRows; ++r)
    {
        auto g = trench::core::geometry_from_words (words[r], trench::core::kP2kDatumHz);
        auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
        if (pole == nullptr || pole->radius < 0.05 || pole->hz <= 0.0) continue;
        const double target = 500.0 * (2.0 * (double) r + 1.0);
        const double ratio = target / pole->hz;
        pole->hz = target;
        if (auto* zero = std::get_if<trench::core::ConjugatePair> (&g.zero); zero != nullptr && zero->radius > 0.05)
            zero->hz = std::clamp (zero->hz * ratio, 20.0, 20000.0);
        out[r] = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
        out[r][4] = words[r][4];
    }
    unityDc (out);
    return out;
}

Words lensed (const Words& words, double f1, double f2)
{
    Words out = words;
    std::vector<std::pair<double, size_t>> lanes;
    for (size_t r = 0; r + 1 < kRows; ++r)
    {
        const auto g = trench::core::geometry_from_words (words[r], trench::core::kP2kDatumHz);
        const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
        if (pole == nullptr || pole->radius < 0.05 || pole->hz < 60.0 || pole->hz > 6000.0) continue;
        const double width = 12.0 * std::log2 (1.0 + (-std::log (std::max (pole->radius, 1e-9)) * trench::core::kP2kDatumHz / 3.141592653589793) / pole->hz);
        if (width < 6.0) lanes.push_back ({ pole->hz, r });
    }
    std::sort (lanes.begin(), lanes.end());
    const double targets[2] = { f1, f2 };
    for (size_t lane = 0; lane < std::min<size_t> (2, lanes.size()); ++lane)
    {
        const size_t r = lanes[lane].second;
        auto g = trench::core::geometry_from_words (words[r], trench::core::kP2kDatumHz);
        auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
        const double target = std::clamp (targets[lane], 60.0, 8000.0), ratio = target / pole->hz;
        pole->hz = target;
        if (auto* zero = std::get_if<trench::core::ConjugatePair> (&g.zero); zero != nullptr && zero->radius > 0.05)
            zero->hz = std::clamp (zero->hz * ratio, 20.0, 20000.0);
        out[r] = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
        out[r][4] = words[r][4];
    }
    unityDc (out);
    return out;
}

juce::String formantName (const Words& words)
{
    const auto f = formantsOf (words);
    if (f[0] <= 0.0) return "sound";
    return juce::String ((int) std::lround (f[0])) + "/" + juce::String ((int) std::lround (f[1]));
}

Star madeVowel (double f1, double f2)
{
    Star s;
    s.kind = "made";
    s.words = vowelWords ({ f1, f2, kNeutralF3, kNeutralF4 });
    s.name = juce::String ((int) std::lround (f1)) + "/" + juce::String ((int) std::lround (f2));
    s.body = s.name;
    return s;
}

Star schwa()
{
    Star s = madeVowel (kSchwaF1, kSchwaF2);
    s.kind = "vowel";
    s.name = juce::String (juce::CharPointer_UTF8 ("ə"));
    s.body = "neutral";
    s.corner = "schwa";
    return s;
}

juce::File bodyFile (const juce::File& p2kDir, const juce::String& body)
{
    return p2kDir.getChildFile (body.toLowerCase().replaceCharacter (' ', '_') + ".body240");
}
}
