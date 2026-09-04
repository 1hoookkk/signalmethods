#pragma once
#include "Theme.h"
namespace trench::ui
{
class ModulationChip final : public juce::Component
{
public:
    explicit ModulationChip (const Theme& theme)
        : t (theme)
    {
        setInterceptsMouseClicks (false, false);
        setTitle ("Modulation");
    }
    void setActive (bool isActive)
    {
        if (isActive == active) return;
        active = isActive;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds();
        const auto ink = t.curveColour();
        const juce::Rectangle<float> lamp { (float) b.getX() + 1.0f, (float) b.getCentreY() - 2.5f, 5.0f, 5.0f };
        g.setColour (active ? t.modulationLamp() : ink.withAlpha (0.30f));
        g.fillEllipse (lamp);
        g.setFont (displayFont (kLabelPt, active));
        g.setColour (active ? t.modulationLamp().withAlpha (0.95f) : ink.withAlpha (0.58f));
        g.drawText ("Modulation", b.withTrimmedLeft (11), juce::Justification::centredLeft, false);
    }
private:
    Theme t;
    bool active = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationChip)
};
}
