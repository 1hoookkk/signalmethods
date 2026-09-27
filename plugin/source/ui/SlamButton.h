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
        setTooltip ("SLAM: Mackie preamp before the filter, driven by INPUT");
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
        const auto b = getLocalBounds().toFloat();
        drawMutedBoneReadout (g, b, b.getHeight() * 0.17f, isOn() || isMouseOver(), t);
        g.setFont (displayFont (juce::jmin (8.5f, b.getHeight() * 0.55f), true));
        g.setColour (isOn() ? t.accent() : t.labelInk().withAlpha (0.55f));
        g.drawText ("SLAM", b.toNearestInt(), juce::Justification::centred, false);
    }
private:
    Theme t;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlamButton)
};
}
