#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

namespace trench::ui
{
class FiveDButton final : public juce::Button
{
public:
    FiveDButton (juce::AudioProcessorValueTreeState& apvts, const Theme& theme)
        : juce::Button ("5D"), t (theme), param (apvts.getParameter (ParamID::fiveD))
    {
        setComponentID ("fiveDButton");
        setTitle ("5D");
        setClickingTogglesState (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("5D: QSound stereo spatial effect");
        if (param != nullptr)
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                apvts, ParamID::fiveD, *this);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, param);
            return;
        }
        juce::Button::mouseDown (e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu()) juce::Button::mouseUp (e);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        drawSwitchFace (g, getLocalBounds().toFloat(), getToggleState(), over || down, "5D", t);
    }

private:
    Theme t;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FiveDButton)
};
}
