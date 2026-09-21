#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace trench::calibration
{
struct Variable
{
    const char* id;
    const char* label;
    const char* unit;
    float low, high, step, initial;
    bool logarithmic;
    const char* help;
};
inline constexpr Variable variables[] {
    { "cal_grid", "Interpolation", "", 0, 1, 1, 0, false, "0: packed words > decode > rate conversion. 1: current 65 x 17 coefficient grid. Neither is claimed hardware-exact." },
    { "cal_hop", "Control interval", "samples", 1, 128, 1, 88, true, "Samples between coefficient targets. Kernel ramps are a separate variable." },
    { "cal_ramp", "Kernel ramp", "", 0, 1, 1, 1, false, "0: immediate target. 1: ramp decoded kernel coefficients across the control interval." },
    { "cal_morph_ms", "Morph smoothing", "ms", 0, 100, 0.1f, 0, false, "0: immediate wheel position, no smoothing. Above zero: sample-rate-aware one-pole smoothing of the complete trajectory." },
    { "cal_input_db", "Input trim", "dB", -24, 24, 0.1f, 0, false, "Additional drive before the cascade; independent of final monitoring trim." },
    { "cal_feedback", "Feedback clipping", "", 0, 1, 1, 0, false, "Clips the feedback signal of each section, not the section output." },
    { "cal_feedback_db", "Feedback ceiling", "dBFS", -36, 24, 0.1f, 6, false, "Ceiling on each section's resonator state, measured against that section's DC gain. Distortion starts 4.4 dB below it." },
    { "cal_ring", "Section ring limiter", "", 0, 1, 1, 0, false, "Bounds each section's output relative to its input. Disabled in the current shipping path." },
    { "cal_ring_db", "Section gain allowance", "dB", 0, 48, 0.1f, 24, false, "Maximum tracked output/input ratio before ring reduction." },
    { "cal_ring_attack", "Ring attack", "ms", 0.1f, 100, 0.1f, 1, true, "Ring-gain attack time." },
    { "cal_ring_release", "Ring release", "ms", 5, 2000, 1, 120, true, "Ring envelope decay and gain-release time." },
    { "cal_ring_floor", "Ring absolute floor", "dBFS", -100, -30, 0.1f, -80, false, "Allows a small absolute tail beyond the relative ring limit." },
    { "cal_desk", "Output desk", "", 0, 1, 1, 1, false, "Bypasses the entire output desk, including its filters and gain." },
    { "cal_desk_clip", "Desk saturation", "", 0, 1, 1, 1, false, "Disables only desk clipping; preserves drive and its filters." },
    { "cal_comp", "Desk compensation", "", 0, 1, 1, 1, false, "Existing static desk gain compensation; not audio-reactive AGC." },
    { "cal_coupling", "Desk output coupling", "Hz", 0, 100, 0.1f, 0, false, "0: original coupling. Otherwise sets the output high-pass coefficient." },
    { "cal_output_db", "Monitor trim", "dB", -36, 6, 0.1f, 0, false, "Final comparison trim before the optional output guard. No automatic loudness matching." },
    { "cal_guard_knee", "Final guard linear fraction", "", 0.1f, 0.99f, 0.01f, 0.5f, false, "Linear region as a fraction of the output ceiling. Guard curvature above this point changes distortion." },
    { "cal_guard_ceiling", "Final output ceiling", "dBFS", -12, -0.1f, 0.1f, -0.1f, false, "Ceiling when Final output guard is enabled. No limiting when it is off." },
    { "cal_guard", "Final output guard", "", 0, 1, 1, 1, false, "Optional final soft ceiling. OFF leaves the finite filter output unbounded." }
};
inline constexpr const char* retired[] { "cal_stage", "cal_stage_db", "cal_agc", "cal_agc_db", "cal_agc_strength", "cal_agc_recovery", "cal_knee_db", "cal_knee_slope", "cal_release_slow", "cal_release_fast", "cal_hold", "cal_quiet_db", "cal_agc_position" };
inline constexpr size_t count = std::size (variables);
inline constexpr const char* taste[] { "cal_feedback", "cal_feedback_db" };
inline constexpr const char* processing[] { "cal_ramp", "cal_feedback", "cal_desk", "cal_desk_clip", "cal_comp", "cal_guard" };
using Values = std::array<float, count>;
inline Values defaults()
{
    Values out {};
    for (size_t i = 0; i < count; ++i) out[i] = variables[i].initial;
    return out;
}
inline float guard (float x, float knee, float ceiling) noexcept
{
    if (! std::isfinite (x)) return 0.0f;
    const float a = std::abs (x) / ceiling;
    if (a <= knee) return x;
    const float top = 2.0f - knee;
    if (a >= top) return std::copysign (ceiling, x);
    const float remaining = top - a;
    return std::copysign (ceiling * (1.0f - remaining * remaining / (4.0f * (1.0f - knee))), x);
}
inline void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    for (const auto& v : variables)
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { v.id, 1 },
            juce::String ("Cal: ") + v.label, juce::NormalisableRange<float> { v.low, v.high, v.step }, v.initial,
            juce::AudioParameterFloatAttributes().withLabel (v.unit).withStringFromValueFunction ([v] (float value, int)
            {
                const juce::String id (v.id);
                if (id == "cal_grid") return juce::String (value < 0.5f ? "WORDS" : "GRID");
                if (v.low == 0 && v.high == 1 && v.step == 1) return juce::String (value < 0.5f ? "OFF" : "ON");
                return juce::String (value, v.step < 1 ? 2 : 0);
            }).withValueFromStringFunction ([v] (const juce::String& text)
            {
                if (text.equalsIgnoreCase ("OFF") || text.equalsIgnoreCase ("WORDS")) return 0.0f;
                if (text.equalsIgnoreCase ("ON") || text.equalsIgnoreCase ("GRID")) return 1.0f;
                return juce::jlimit (v.low, v.high, text.getFloatValue());
            })));
}
}
