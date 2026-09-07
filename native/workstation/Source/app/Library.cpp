#include "Library.h"
#include <trench/core/body_from_audio.hpp>
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
    const double support[2] = { 3500.0, 4500.0 };
    size_t supported = 0;
    for (size_t i = 0; i < kRows - 1; ++i)
    {
        trench::core::SectionGeometry g;
        if (i < found.size())
        {
            const double hz = std::clamp (found[i].hz, 60.0, 12000.0);
            const double poleRadius = std::clamp (std::exp (-3.141592653589793 * std::max (10.0, found[i].bw_hz) / trench::core::kP2kDatumHz), 0.0, 0.9995);
            const double prominence = std::clamp (found[i].gain_db, 3.0, 30.0);
            g.pole = trench::core::ConjugatePair { hz, poleRadius };
            g.zero = trench::core::ConjugatePair { hz, std::clamp (1.0 - (1.0 - poleRadius) * std::pow (10.0, prominence / 20.0), 0.05, poleRadius - 1e-4) };
        }
        else
        {
            const double hz = support[std::min<size_t> (supported++, 1)] * (supported > 2 ? 1.0 + 0.2 * (double) (supported - 2) : 1.0);
            g.pole = trench::core::ConjugatePair { hz, radiusForWidth (hz, 4.0) };
            g.zero = trench::core::ConjugatePair { hz, radiusForWidth (hz, 6.0) };
        }
        s.words[i] = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
    }
    s.words[kRows - 1] = rowWords ({ RowType::notch, kFreqCodes - 1, 0 }, 0);
    unityDc (s.words);
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
