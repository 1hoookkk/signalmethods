#include "parameters/TrenchParameters.h"
#include "TrenchBodyRoster.h"
#include "dsp/Movement.h"
#if TRENCH_DEV_PANEL
#include "DevCalibration.h"
#endif
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
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamID::body, 1 },
        "Body",
        0,
        trench::kBodyParamMaxIndex,
        trench::kDefaultBodyIndex));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::preamp, 1 },
        "Input",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f, pctAttribs()));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamID::output, 1 },
        "Output",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.0f, pctAttribs()));
    {
        juce::StringArray presetNames { "OFF" };
        for (int i = 0; i < trench::kNumFuncGenPatterns; ++i)
            presetNames.add (trench::kFuncGenPatterns[i].name);
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamID::movePreset, 1 },
            "Movement",
            presetNames,
            0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamID::moveTransition, 1 },
            "Movement Transition",
            juce::StringArray { "PATTERN", "STEP", "GLIDE" },
            0));
    }
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
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::moveLength, 1 }, "Movement Rate",
        juce::StringArray { "1/4 bar", "1/2 bar", "1 bar", "2 bars", "4 bars" }, trench::Movement::kDefaultRate));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamID::movePlayback, 1 }, "Movement Playback",
        juce::StringArray { "Preset playback", "Loop", "Once" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamID::moveCustom, 1 }, "User Movement", false));
#if TRENCH_DEV_PANEL
    trench::calibration::addParameters (layout);
#endif
    return layout;
}
}
