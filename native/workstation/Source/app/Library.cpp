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
    std::vector<Entry> out;
    auto files = p2kDir.findChildFiles (juce::File::findFiles, false, "*.body240");
    files.sort();
    for (const auto& file : files)
    {
        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb) || mb.getSize() != trench::core::kLegacyBodyBytes) continue;
        const auto body = trench::core::PackedBody::from_legacy_bytes (std::span<const std::uint8_t> ((const std::uint8_t*) mb.getData(), mb.getSize()));
        for (int side = 0; side < 2; ++side)
        {
            Entry e;
            e.body = titleOf (file.getFileNameWithoutExtension());
            e.side = side == 0 ? "M0" : "M1";
            e.name = e.body + " " + e.side;
            for (size_t s = 0; s < kRows; ++s) { e.q0[s] = body.words[(size_t) side][s]; e.q1[s] = body.words[(size_t) side + 2][s]; }
            out.push_back (e);
        }
    }
    return out;
}

Anchor factoryAnchor (const std::vector<Entry>& library, int k)
{
    Anchor a;
    if (k < 0 || k >= (int) library.size()) return a;
    const auto& e = library[(size_t) k];
    a.origin.kind = "factory"; a.origin.body = e.body; a.origin.corner = e.side;
    a.name = e.name;
    a.q0 = e.q0; a.q1 = e.q1;
    return a;
}

juce::File bodyFile (const juce::File& p2kDir, const juce::String& body)
{
    return p2kDir.getChildFile (body.toLowerCase().replaceCharacter (' ', '_') + ".body240");
}
}
