#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include "../parameters/TrenchParameters.h"
#include "BinaryData.h"
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
        lampOff = juce::ImageCache::getFromMemory (BinaryData::trench_slam_lamp_off_png, BinaryData::trench_slam_lamp_off_pngSize);
        lampOn = juce::ImageCache::getFromMemory (BinaryData::trench_slam_lamp_on_png, BinaryData::trench_slam_lamp_on_pngSize);
    }
    bool isOn() const noexcept { return param != nullptr && param->getValue() > 0.5f; }
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        pressed = false;
        repaint();
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
        if (! lampOff.isValid() || ! lampOn.isValid())
            return;
        auto area = getLocalBounds().toFloat();
        if (pressed)
            area = area.withSizeKeepingCentre (area.getWidth() * 0.94f, area.getHeight() * 0.94f).translated (0.0f, 0.6f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (isOn() ? lampOn : lampOff, area, juce::RectanglePlacement::centred);
        if (! isOn() && isMouseOver())
        {
            g.setOpacity (0.35f);
            g.drawImage (lampOn, area, juce::RectanglePlacement::centred);
        }
    }
private:
    Theme t;
    juce::Image lampOff, lampOn;
    bool pressed = false;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlamButton)
};
}
