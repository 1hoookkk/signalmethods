#pragma once
#include <juce_core/juce_core.h>
#include <vector>

namespace trench
{
struct RuntimePreset
{
    juce::String name;
    juce::String stem;
    int activeStages = 0;
    std::vector<double> bankRates;
    std::vector<std::vector<unsigned short>> bankWords;

    bool isValid() const noexcept
    {
        return activeStages >= 1 && activeStages <= 3 && ! bankRates.empty()
            && bankRates.size() == bankWords.size();
    }

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

inline bool isRuntimePresetJson (const juce::String& json) noexcept
{
    return json.contains ("trench-x3-runtime-preset");
}

inline bool isRuntimePresetFile (const juce::File& file) noexcept
{
    return file.getFileName().endsWithIgnoreCase (".x3preset.json");
}

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
            continue;

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
