#pragma once
#include "Theme.h"
#include "SelectorLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
namespace trench::ui
{
class GlassWords final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    explicit GlassWords (const Theme& theme) : t (theme)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Movement Preset");
        setTooltip ("Modulation: click for the list, wheel to step");
    }
    std::function<void (int)> onStep;
    std::function<void()>     onOpenMenu;
    void setState (const juce::String& presetName, bool isActive)
    {
        if (presetName == preset && isActive == active) return;
        preset = presetName; active = isActive;
        setTooltip ("Modulation: " + preset + " - click for the list, wheel to step");
        repaint();
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (onOpenMenu) onOpenMenu();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (onStep && ! juce::approximatelyEqual (w.deltaY, 0.0f)) onStep (w.deltaY > 0 ? 1 : -1);
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds();
        const auto ink = t.curveColour();
        g.setFont (displayFont (kLabelPt, false));
        g.setColour (ink.withAlpha (0.55f));
        const juce::Rectangle<float> lamp { (float) b.getX() + 1.0f, (float) b.getCentreY() - 2.5f, 5.0f, 5.0f };
        if (! active && ! hover && preset == "OFF")
        {
            g.setColour (ink.withAlpha (0.30f));
            g.fillEllipse (lamp);
            return;
        }
        g.setColour (active ? t.modulationLamp() : ink.withAlpha (hover ? 0.80f : 0.45f));
        g.fillEllipse (lamp);
        if (hover)
        {
            g.setColour ((active ? t.modulationLamp() : ink).withAlpha (0.25f));
            g.fillEllipse (lamp.expanded (2.0f));
        }
        g.setColour (ink.withAlpha (active || hover ? 0.92f : 0.58f));
        g.drawText ("Modulation", b.withTrimmedLeft (11), juce::Justification::centredLeft, false);
    }
    struct LightMenuLnF : juce::LookAndFeel_V4
    {
        juce::Font getPopupMenuFont() override { return displayFont (13.0f, false); }
        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (juce::Colour (0xfffcfcfd));
            g.setColour (juce::Colour (0xff5a5750));
            g.drawRect (juce::Rectangle<int> (0, 0, w, h), 1);
        }
        void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                bool, bool isActive, bool isHighlighted, bool isTicked,
                                bool, const juce::String& text, const juce::String&,
                                const juce::Drawable*, const juce::Colour*) override
        {
            const bool hot = isHighlighted && isActive;
            if (hot)
            {
                g.setColour (juce::Colour (0xff5A91E2));
                g.fillRect (area.reduced (1));
            }
            g.setFont (displayFont (13.0f, isTicked));
            g.setColour (hot ? juce::Colours::white
                             : juce::Colour (0xff2a2722).withAlpha (isActive ? 1.0f : 0.4f));
            g.drawText (text, area.reduced (8, 0), juce::Justification::centredLeft, false);
            if (isTicked)
            {
                juce::Path check;
                const float cx = (float) area.getX() + 9.0f, cy = (float) area.getCentreY();
                check.startNewSubPath (cx - 3.0f, cy);
                check.lineTo (cx - 1.0f, cy + 2.5f);
                check.lineTo (cx + 3.5f, cy - 3.0f);
                g.setColour (hot ? juce::Colours::white : juce::Colour (0xff2a2722));
                g.strokePath (check, { 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
            }
        }
    };
private:
    Theme t;
    juce::String preset { "OFF" };
    bool hover = false;
    bool active = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassWords)
};
}
