#pragma once
#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
namespace trench::ui
{
// MOVEMENT ON THE GLASS (Tyson 2026-08-25): the pattern stopped being a room
// in the bay and became a chip in the display's lower-left. The chip is the
// fixed word "Modulation" in the readout voice — dim at rest, glowing under
// the cursor and while its menu is up (Tyson: "dim ... when clicked it glows
// and when hove it glows"); the lamp alone says whether the wheel is moving.
// The mouse wheel steps the bank and every step restarts on the retrigger
// law, so wheeling the chip IS auditioning; a click opens the whole list.
class MovementChip final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    static constexpr int   kHeight  = 13;
    static constexpr float kNamePt  = 9.8f;   // the glass's telemetry voice
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
        // The face's gesture law: the mouse wheel always works.
        if (onStep != nullptr && ! juce::approximatelyEqual (w.deltaY, 0.0f))
            onStep (w.deltaY > 0 ? 1 : -1);
    }
    /// Row seat (Tyson 2026-08-28): the picker rides row 2 of the FX+ drawer,
    /// on the plate - its quiet voice is ink there, not glass telemetry.
    void setOnPlate (bool p) { if (onPlate != p) { onPlate = p; repaint(); } }
    void mouseDown (const juce::MouseEvent&) override
    {
        if (onOpenMenu != nullptr)
            onOpenMenu();
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const juce::Rectangle<float> lamp { b.getX() + 1.0f, b.getCentreY() - kLampD * 0.5f,
                                            kLampD, kLampD };
        const bool glowing = hover || menuOpen;
        const auto quietInk = onPlate ? t.labelInk() : t.telemetry();
        g.setColour (active ? t.modulationLamp()
                            : t.labelInk().withAlpha (glowing ? 0.80f : 0.45f));
        g.fillEllipse (lamp);
        if (glowing)
        {
            g.setColour ((active ? t.modulationLamp() : quietInk).withAlpha (0.25f));
            g.fillEllipse (lamp.expanded (2.0f));
        }
        g.setFont (telemetryFont (kNamePt));
        g.setColour (quietInk.withAlpha (glowing ? 0.95f : (active ? 0.80f : 0.55f)));
        g.drawText (kWord, b.withTrimmedLeft (kLampD + kLampGap + 1.0f),
                    juce::Justification::centredLeft, false);
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
