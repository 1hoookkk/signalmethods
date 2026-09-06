#include "Library.h"

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
}

std::vector<Entry> loadLibrary (const juce::File& p2kDir)
{
    static const char* tags[] = { "M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1" };
    std::vector<Entry> out;
    auto files = p2kDir.findChildFiles (juce::File::findFiles, false, "*.body240");
    files.sort();
    for (const auto& file : files)
    {
        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb) || mb.getSize() != trench::core::kLegacyBodyBytes) continue;
        const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> ((const std::uint8_t*) mb.getData(), mb.getSize()));
        for (int c = 0; c < 4; ++c)
        {
            Entry e;
            e.body = titleOf (file.getFileNameWithoutExtension());
            e.corner = tags[c];
            e.name = e.body + " " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + " " + e.corner;
            for (size_t s = 0; s < kRows; ++s) e.words[s] = body.words[(size_t) c][s];
            out.push_back (e);
        }
    }
    return out;
}

int partnerOf (const std::vector<Entry>& library, int k, const juce::String& q)
{
    if (k < 0 || k >= (int) library.size()) return -1;
    const auto& e = library[(size_t) k];
    const auto corner = e.corner.substring (0, 2) + " " + q;
    for (int i = 0; i < (int) library.size(); ++i)
        if (library[(size_t) i].body == e.body && library[(size_t) i].corner == corner) return i;
    return -1;
}

Column factoryColumn (const std::vector<Entry>& library, int k)
{
    Column c;
    const int q0 = partnerOf (library, k, "Q0"), q1 = partnerOf (library, k, "Q1");
    if (q0 < 0 || q1 < 0) return c;
    const auto& e = library[(size_t) k];
    c.origin.kind = "factory"; c.origin.body = e.body; c.origin.corner = e.corner.substring (0, 2);
    c.name = e.body + " " + c.origin.corner;
    c.q0 = library[(size_t) q0].words; c.q1 = library[(size_t) q1].words;
    return c;
}

juce::File bodyFile (const juce::File& p2kDir, const juce::String& body)
{
    return p2kDir.getChildFile (body.toLowerCase().replaceCharacter (' ', '_') + ".body240");
}
}
