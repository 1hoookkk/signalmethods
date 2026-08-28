#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace trench
{
// X3 RUNTIME PRESET — the xStream law, plugin side.
//
// The Emulator X3 stores FOUR pre-compiled coefficient banks per filter
// (44.1k / 48k / 96k / 192k). They are distinct designs, not rate-converted
// copies (proven by cross-rate re-encoding mismatch,
// scratchpad/rate_bank_redundancy.py). The engine selects the nearest bank and
// plays it verbatim - datum_rate = 0, no recompilation, ever.
//
// This is the OTHER regime to .body240. A body we author from a measurement is
// Hz-anchored and recompiles to the host rate; it has to be, because there is
// no factory bank set to select from. A runtime preset already has its banks,
// so migrating one would be inventing data E-mu never shipped.
//
// Consequence the caller must honour: the chosen bank is a function of the
// host rate. A preset loaded at 44.1k is WRONG once the host switches to 96k -
// an octave out. Whoever owns prepareToPlay must re-load on a rate change.
// That is what `bankForRate` exists to make cheap.
//
// File form: X3F_<stem>.x3preset.json, written by
// tools/x3_fundamentals_to_cartridges.py. Words are UNPADDED
// (4 corners x activeStages x 5) - exactly what the FFI wants; the Rust side
// pads to six stages with the identity sentinel.
struct RuntimePreset
{
    juce::String name;
    juce::String stem;
    int activeStages = 0;
    // Parallel arrays, one entry per populated bank. Small and fixed (<= 4),
    // so a flat scan beats a map.
    std::vector<double> bankRates;
    std::vector<std::vector<unsigned short>> bankWords;

    bool isValid() const noexcept
    {
        return activeStages >= 1 && activeStages <= 3 && ! bankRates.empty()
            && bankRates.size() == bankWords.size();
    }

    /// Index of the bank whose authored rate is nearest `hostRate`, or -1.
    int bankForRate (double hostRate) const noexcept
    {
        int best = -1;
        double bestDist = std::numeric_limits<double>::infinity();
        for (int i = 0; i < (int) bankRates.size(); ++i)
        {
            const auto dist = std::abs (bankRates[(size_t) i] - hostRate);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = i;
            }
        }
        return best;
    }
};

/// Is this a runtime-preset cartridge rather than a keyframe cartridge?
/// Both are .json, so the roster's `*.json` glob catches both — the format
/// string is what separates them. Cheap enough to run before a full parse.
inline bool isRuntimePresetJson (const juce::String& json) noexcept
{
    return json.contains ("trench-x3-runtime-preset");
}

inline bool isRuntimePresetFile (const juce::File& file) noexcept
{
    return file.getFileName().endsWithIgnoreCase (".x3preset.json");
}

/// Parse X3F_*.x3preset.json. Returns an invalid preset on any malformed
/// input — the caller checks isValid() and falls back rather than half-loading.
inline RuntimePreset parseRuntimePreset (const juce::String& json)
{
    RuntimePreset preset;
    const auto root = juce::JSON::parse (json);
    const auto* obj = root.getDynamicObject();
    if (obj == nullptr)
        return preset;

    if (! obj->getProperty ("format").toString().startsWith ("trench-x3-runtime-preset"))
        return preset;

    preset.name = obj->getProperty ("name").toString();
    preset.stem = obj->getProperty ("stem").toString();
    preset.activeStages = (int) obj->getProperty ("active_stages");
    if (preset.activeStages < 1 || preset.activeStages > 3)
        return preset;

    const auto banksVar = obj->getProperty ("banks");
    const auto* banks = banksVar.getDynamicObject();
    if (banks == nullptr)
        return preset;

    const auto expected = (size_t) (4 * preset.activeStages * 5);
    for (const auto& entry : banks->getProperties())
    {
        const double rate = entry.name.toString().getDoubleValue();
        if (rate <= 0.0)
            continue;
        const auto* arr = entry.value.getArray();
        if (arr == nullptr || (size_t) arr->size() != expected)
            continue;   // wrong word count: skip this bank, keep the others

        std::vector<unsigned short> words;
        words.reserve (expected);
        bool ok = true;
        for (const auto& w : *arr)
        {
            const int v = (int) w;
            if (v < 0 || v > 0xffff) { ok = false; break; }
            words.push_back ((unsigned short) v);
        }
        if (! ok)
            continue;

        preset.bankRates.push_back (rate);
        preset.bankWords.push_back (std::move (words));
    }
    return preset;
}
}
