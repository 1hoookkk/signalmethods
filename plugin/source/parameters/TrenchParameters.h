#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
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
}
namespace TrenchParameters
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
