#pragma once
#include "Primitives.h"
#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
namespace trench::ui
{

class MovementChip final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    static constexpr int   kHeight  = 13;
    static constexpr float kNamePt  = 9.8f;
    static constexpr float kLampD   = 6.0f;
    static constexpr float kLampGap = 5.0f;
    explicit MovementChip (const Theme& theme) : t (theme)
    {
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Movement");
        setHelpText ("Movement pattern - mouse wheel steps it, and each step restarts; click opens the list");
        setTooltip ("Movement: wheel = step (auditions), click = list");
    }
    std::function<void (int)> onStep;
    std::function<void()>     onOpenMenu;
    void setState (const juce::String& patternName, bool isActive)
    {
        if (patternName == name && isActive == active)
            return;
        name = patternName;
        active = isActive;
        setTooltip ("Movement: " + name + " - wheel = step (auditions), click = list");
        setSize (preferredWidth(), kHeight);
        repaint();
    }
    void setMenuOpen (bool open)
    {
        if (menuOpen != open) { menuOpen = open; repaint(); }
    }
    int preferredWidth() const
    {
        return juce::roundToInt (kLampD + kLampGap
                                 + juce::GlyphArrangement::getStringWidth (telemetryFont (kNamePt), kWord))
               + 2;
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {

        if (onStep != nullptr && ! juce::approximatelyEqual (w.deltaY, 0.0f))
            onStep (w.deltaY > 0 ? 1 : -1);
    }

    void setOnPlate (bool p) { if (onPlate != p) { onPlate = p; repaint(); } }
    void mouseDown (const juce::MouseEvent&) override
    {
        if (onOpenMenu != nullptr)
            onOpenMenu();
    }
    void paint (juce::Graphics& g) override
    {

        paintWord (g, getLocalBounds().toFloat(), kWord, stateOf (active, hover || menuOpen), t,
                   { true, false, kNamePt });
    }
private:
    static constexpr const char* kWord = "Modulation";
    Theme t;
    juce::String name { "MOVEMENT" };
    bool active = false;
    bool onPlate = false;
    bool hover = false;
    bool menuOpen = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MovementChip)
};
}
