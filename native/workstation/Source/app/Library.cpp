#include "Library.h"
#include <trench/core/body_from_audio.hpp>
#include <cmath>

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
    for (const auto& f : *frames)
    {
        Star s;
        s.kind = "vowel";
        s.body = "Klatt 1980";
        const auto klatt = f.getProperty ("name", "").toString();
        s.corner = klatt;
        s.name = juce::String (juce::CharPointer_UTF8 (ipaOf (klatt))) + "  " + klatt;
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
    for (size_t i = 0; i < kRows; ++i)
    {
        trench::core::SectionGeometry g;
        g.pole = trench::core::ConjugatePair { 20000.0, 0.0 };
        g.zero = trench::core::ConjugatePair { 20000.0, 0.0 };
        if (i < found.size())
            g.pole = trench::core::ConjugatePair { std::clamp (found[i].hz, 20.0, 20000.0),
                                                   std::clamp (std::exp (-3.141592653589793 * std::max (10.0, found[i].bw_hz) / trench::core::kP2kDatumHz), 0.0, 0.9995) };
        s.words[i] = trench::core::words_from_geometry (g, trench::core::kP2kDatumHz);
    }
    unityDc (s.words);
    if (! admit (s.words)) return std::nullopt;
    return s;
}

juce::File bodyFile (const juce::File& p2kDir, const juce::String& body)
{
    return p2kDir.getChildFile (body.toLowerCase().replaceCharacter (' ', '_') + ".body240");
}
}
