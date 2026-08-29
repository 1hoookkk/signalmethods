#pragma once
#include "Primitives.h"
#include <functional>
namespace trench::ui
{
// AUTO TRIM - a printed state in bay 1, not a control: the stage trims itself.
struct AutoTrimMark final : juce::Component
{
    explicit AutoTrimMark (const Theme& theme) : t (theme) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        paintWord (g, getLocalBounds().toFloat(), "AUTO TRIM", ControlState::engaged, t, { true, false, 8.5f });
    }
    Theme t;
};

// BAY 2 - GEN is preset-first: the phrase in a pill with its steppers, then
// the rate. Deeper parameters stay behind the phrase list.
struct MovementBay final : juce::Component
{
    explicit MovementBay (const Theme& theme) : t (theme) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void (int)> onStep;
    std::function<void()>     onOpenMenu;
    void setState (const juce::String& n, const juce::String& r, bool a)
    {
        if (n == name && r == rate && a == active) return;
        name = n; rate = r; active = a; repaint();
    }
    void mouseMove (const juce::MouseEvent& e) override { setHot (zoneAt (e.position)); }
    void mouseExit (const juce::MouseEvent&) override { setHot (0); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        switch (zoneAt (e.position))
        {
            case 1: if (onStep) onStep (-1); break;
            case 2: if (onStep) onStep (1); break;
            case 3: if (onOpenMenu) onOpenMenu(); break;
            default: break;
        }
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (onStep && ! juce::approximatelyEqual (w.deltaY, 0.0f)) onStep (w.deltaY > 0 ? 1 : -1);
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        paintWord (g, b.withHeight (10.0f), "PRESET", ControlState::rest, t, { false, false, 8.0f });
        const auto row = pillRow();
        paintPill (g, row, 3.0f, t, hot == 3);
        paintArrow (g, row.withWidth (14.0f).withX (row.getX() + 4.0f), false, hot == 1 ? ControlState::hover : ControlState::rest, t);
        paintArrow (g, row.withWidth (14.0f).withX (row.getRight() - 18.0f), true, hot == 2 ? ControlState::hover : ControlState::rest, t);
        g.setFont (displayFont (10.0f, false));
        g.setColour (juce::Colour (0xff2a2722));
        g.drawText (name, row.reduced (14.0f, 0.0f).toNearestInt(), juce::Justification::centred, false);
        paintWord (g, juce::Rectangle<float> (b.getX(), b.getY() + 33.0f, 30.0f, 10.0f), "RATE", ControlState::rest, t, { false, false, 8.0f });
        paintWord (g, juce::Rectangle<float> (b.getX() + 30.0f, b.getY() + 33.0f, b.getWidth() - 30.0f, 10.0f), rate,
                   active ? ControlState::engaged : ControlState::rest, t, { false, false, 8.0f });
    }
private:
    juce::Rectangle<float> pillRow() const
    {
        const auto b = getLocalBounds().toFloat();
        return { b.getX(), b.getY() + 11.0f, b.getWidth(), 16.0f };
    }
    int zoneAt (juce::Point<float> p) const
    {
        const auto row = pillRow();
        if (! row.contains (p)) return 0;
        if (p.x < row.getX() + 18.0f) return 1;
        if (p.x > row.getRight() - 18.0f) return 2;
        return 3;
    }
    void setHot (int z) { if (hot != z) { hot = z; repaint(); } }
    Theme t;
    juce::String name { "OFF" }, rate;
    bool active = false;
    int hot = 0;
};
}
