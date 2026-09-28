#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
namespace trench::ui
{
class SlamButton final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    SlamButton (juce::AudioProcessorValueTreeState& apvts, const Theme& theme)
        : t (theme), param (apvts.getParameter (ParamID::inputSlam))
    {
        setTitle ("Slam");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("SLAM: flattens the source first: Mackity driven +24 dB, brought back to the same level, before INPUT");
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
    }
    bool isOn() const noexcept { return param != nullptr && param->getValue() > 0.5f; }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (param == nullptr || attachment == nullptr || ! getLocalBounds().contains (e.position.toInt()))
            return;
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, param);
            return;
        }
        attachment->setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds();
        g.setFont (displayFont (10.5f, true));
        if (isOn())
        {
            g.setColour (juce::Colours::black.withAlpha (0.30f));
            g.drawText ("SLAM", b.translated (0, 1), juce::Justification::centredRight, false);
            g.setColour (t.accent());
        }
        else
        {
            g.setColour (t.labelInk().withAlpha (isMouseOver() ? 0.75f : 0.45f));
        }
        g.drawText ("SLAM", b, juce::Justification::centredRight, false);
    }
private:
    Theme t;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlamButton)
};
}
