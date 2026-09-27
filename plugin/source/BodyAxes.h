#pragma once
#include "TrenchBodyRoster.h"
#include <juce_core/juce_core.h>

namespace trench
{
struct AxisNames
{
    const char* morph;
    const char* q;
};

inline AxisNames axisNamesForBase (const juce::String& base) noexcept
{
    if (base.startsWith ("util_lp_") || base.startsWith ("util_hp_")) return { "CUTOFF", "RESO" };
    if (base.startsWith ("util_bp_")) return { "CENTER", "RESO" };
    if (base.startsWith ("util_sweep_eq_")) return { "FREQ", "GAIN" };
    if (base.startsWith ("util_vowel_")) return { "VOWEL", "MOUTH" };
    if (base.startsWith ("util_phaser_")) return { "NOTCH", "DEPTH" };
    if (base.startsWith ("util_flanger_")) return { "SWEEP", "DEPTH" };
    return { "MORPH", "Q" };
}

inline AxisNames axisNamesForBody (int index) noexcept
{
    return axisNamesForBase (bodyBaseForIndex (index));
}

inline juce::String hostAxisName (const char* faceName)
{
    const juce::String s (faceName);
    return s.length() <= 1 ? s : s.substring (0, 1) + s.substring (1).toLowerCase();
}
}
