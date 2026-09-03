#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
namespace ParamID
{
    inline constexpr auto morph     = "morph";
    inline constexpr auto q         = "q";
    inline constexpr auto chew      = "chew";
    inline constexpr auto body      = "body";
    inline constexpr auto slamDrive = "slamDrive";
    inline constexpr auto preamp    = "preamp";    // desk BEFORE the cascade (input drive)
    inline constexpr auto keySnap     = "keySnap";
    // MOVEMENT — the audio-rate Morph renderer (trench::Movement). A preset
    // is a self-contained record: values, step law, cycle, reset. OFF is
    // index 0. DEPTH scales the travel; FOLLOW is the engine's one detector.
    inline constexpr auto movePreset = "movePreset"; // choice: OFF + curated phrases + LIVE
    inline constexpr auto moveTransition = "moveTransition"; // PATTERN / STEP / GLIDE
    inline constexpr auto envAmount  = "envAmount";  // 0..1 FOLLOW depth; 0 = off
}
namespace TrenchParameters
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
