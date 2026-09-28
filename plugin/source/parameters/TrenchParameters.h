#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
namespace ParamID
{
    inline constexpr auto morph     = "morph";
    inline constexpr auto q         = "q";
    inline constexpr auto chew      = "chew";
    inline constexpr auto distortion = "distortion";
    inline constexpr auto output    = "output";
    inline constexpr auto body      = "body";
    inline constexpr auto slamDrive = "slamDrive";
    inline constexpr auto preamp    = "preamp";
    inline constexpr auto keySnap     = "keySnap";
    inline constexpr auto movePreset = "movePreset";
    inline constexpr auto moveTransition = "moveTransition";
    inline constexpr auto moveLength = "moveLength";
    inline constexpr auto movePlayback = "movePlayback";
    inline constexpr auto moveCustom = "moveCustom";
    inline constexpr auto deskPosition = "deskPosition";
    inline constexpr auto inputSlam = "inputSlam";
    inline constexpr auto fiveD = "fiveD";
}
namespace TrenchParameters
{
    class AxisParameter final : public juce::AudioParameterFloat
    {
    public:
        AxisParameter (const juce::ParameterID& id, const juce::String& name, juce::NormalisableRange<float> range,
                       float defaultValue, const juce::AudioParameterFloatAttributes& attributes)
            : juce::AudioParameterFloat (id, name, range, defaultValue, attributes), fallbackName (name) {}
        void setNameSource (std::function<juce::String()> source) { nameSource = std::move (source); }
        juce::String getName (int maximumStringLength) const override
        {
            return (nameSource ? nameSource() : fallbackName).substring (0, maximumStringLength);
        }
    private:
        juce::String fallbackName;
        std::function<juce::String()> nameSource;
    };
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
