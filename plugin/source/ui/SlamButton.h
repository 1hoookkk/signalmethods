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
        setTooltip ("SLAM: clips before the filter. INPUT drives it harder; OUTPUT sets the final level. No automatic makeup.");
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        capOff = juce::ImageCache::getFromMemory (BinaryData::trench_slam_cap_off_png, BinaryData::trench_slam_cap_off_pngSize);
        capOn = juce::ImageCache::getFromMemory (BinaryData::trench_slam_cap_on_png, BinaryData::trench_slam_cap_on_pngSize);
        shadow = juce::ImageCache::getFromMemory (BinaryData::trench_slam_cap_shadow_png, BinaryData::trench_slam_cap_shadow_pngSize);
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
        if (! capOff.isValid() || ! capOn.isValid())
            return;
        auto area = getLocalBounds().toFloat();
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (shadow, area, juce::RectanglePlacement::centred);
        if (pressed)
            area = area.withSizeKeepingCentre (area.getWidth() * 0.96f, area.getHeight() * 0.96f).translated (0.0f, 0.5f);
        g.drawImage (isOn() ? capOn : capOff, area, juce::RectanglePlacement::centred);
        if (! isOn() && isMouseOver())
        {
            g.setOpacity (0.3f);
            g.drawImage (capOn, area, juce::RectanglePlacement::centred);
        }
    }
private:
    Theme t;
    juce::Image capOff, capOn, shadow;
    bool pressed = false;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlamButton)
};
}
