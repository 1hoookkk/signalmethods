#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
namespace trench::ui
{
class ChoiceStepper final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    ChoiceStepper (juce::AudioProcessorValueTreeState& apvts, const Theme& theme,
                   const juce::String& paramID, juce::String caption)
        : t (theme), label (std::move (caption)), param (apvts.getParameter (paramID))
    {
        if (param != nullptr)
            att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle (label);
    }
    void step (int dir)
    {
        if (param == nullptr) return;
        const int n = param->getAllValueStrings().size();
        if (n <= 0) return;
        const int next = ((index() + dir) % n + n) % n;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) next));
        param->endChangeGesture();
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (! juce::approximatelyEqual (w.deltaY, 0.0f))
            step (w.deltaY > 0 ? 1 : -1);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()))
            return;
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, param);
            return;
        }
        const auto box = boxBounds();
        if (! box.contains (e.position))
            return;
        if (e.position.x > box.getRight() - kStepperW)
            step (e.position.y < box.getCentreY() ? +1 : -1);
        else
            step (+1);
    }
    void paint (juce::Graphics& g) override
    {
        juce::Graphics::ScopedSaveState s (g);
        g.setOpacity (isEnabled() ? 1.0f : 0.32f);
        const auto box = boxBounds();
        drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
        auto nameArea = box.reduced (2.0f, 0.5f);
        nameArea.removeFromRight (kStepperW);
        nameArea.removeFromLeft (6.0f);
        drawCrispText (g, nameArea, param != nullptr ? param->getCurrentValueAsText() : juce::String(),
                       kNamePt, juce::Colour (0xff2a2722), true);
        drawEmuSpinner (g, juce::Rectangle<float> (box.getRight() - kStepperW, box.getY(),
                                                   kStepperW, box.getHeight()),
                        param != nullptr, t);
        drawCrispText (g, getLocalBounds().toFloat().withHeight (12.0f), label, 8.0f,
                       juce::Colour (0xff222222), false);
    }
private:
    static constexpr float kStepperW = 13.0f;
    static constexpr float kNamePt = 9.5f;
    juce::Rectangle<float> boxBounds() const
    {
        return getLocalBounds().toFloat().withTrimmedTop (12.5f);
    }
    int index() const noexcept
    {
        if (param == nullptr) return 0;
        return juce::jlimit (0, juce::jmax (0, param->getAllValueStrings().size() - 1),
                             juce::roundToInt (param->convertFrom0to1 (param->getValue())));
    }
    Theme t;
    juce::String label;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
    bool hover = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceStepper)
};
}
