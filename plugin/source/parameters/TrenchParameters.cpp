#include "parameters/TrenchParameters.h"
#include "TrenchBodyRoster.h"
#include "dsp/Movement.h"
namespace TrenchParameters
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto pctAttribs = [] {
        return juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v * 100.0f, 1); })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
    };
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::morph, 1 },
        "Morph",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f, pctAttribs()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::q, 1 },
        "Q",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f, pctAttribs()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::chew, 1 },
        "Bite",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f, pctAttribs()));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamID::body, 1 },
        "Body",
        0,
        trench::kBodyParamMaxIndex,
        trench::kDefaultBodyIndex));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::lowKeep, 1 },
        "Low",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f, pctAttribs()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::slamDrive, 1 },
        "Output",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::preamp, 1 },
        "Input",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f, pctAttribs()));
    // The shipping bank is independently authored in
    // filters/original_patterns.json and baked into FuncGenPatterns.h. The
    // choice list contains no vendor template names or values.
    {
        juce::StringArray presetNames { "OFF" };
        for (int i = 0; i < trench::kNumFuncGenPatterns; ++i)
            presetNames.add (trench::kFuncGenPatterns[i].name);
        // GROWL (verdicted 2026-08-10 "speaker in a trunk"): pitch-locked
        // sub-octave wheel oscillation, rendered inside the engine. One dumb
        // button — no rate, no depth.
        presetNames.add ("GROWL");
        presetNames.add ("LIVE");   // the phrase being drawn, followed from disk
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamID::movePreset, 1 },
            "Movement",
            presetNames,
            0));
    }
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::moveDivision, 1 },
        "Division",
        juce::StringArray { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T" },
        3));
    // DEPTH retired 2026-08-10 ("it's confusing"): the bank reaches both walls
    // from wherever the wheel rests (Movement::render), never a second dial.
    // Envelope macro depth. Its own parameter rather than
    // a mode flag: 0 IS off, so "is the follower armed" needs no second piece
    // of state and saves/restores by itself.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::envAmount, 1 },
        "Follow",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f, pctAttribs()));
    // RETIRED FROM THE FACE 2026-08-15 (no knob; the parameter stays so old
    // sessions keep their saved value and their sound). The cube's authored
    // third axis is the successor for pitch-following filters.
    // TRACK — the Hz axis (Tyson 2026-08-10 "wake the hz axis"): the whole
    // authored geography slides with the played note:
    // sub-anchors pinned, walls guarded). 1.0 = octave-for-octave.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::track, 1 },
        "Track",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f, pctAttribs()));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::keySnap, 1 },
        "Key Snap",
        juce::StringArray {
            "OFF",
            "C m", "C# m", "D m", "D# m", "E m", "F m",
            "F# m", "G m", "G# m", "A m", "A# m", "B m",
            "C M", "C# M", "D M", "D# M", "E M", "F M",
            "F# M", "G M", "G# M", "A M", "A# M", "B M"
        },
        0));
    return layout;
}
}
