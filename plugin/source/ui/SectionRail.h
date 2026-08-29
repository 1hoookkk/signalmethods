#pragma once
#include "SelectorLookAndFeel.h"
#include "Primitives.h"
#include "Theme.h"
#include "../dsp/FuncGenPatterns.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>
namespace trench::ui
{

class SectionRail final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:

    enum Section { kDrive = 0, kMovement = 1, kNumSections };
    explicit SectionRail (const Theme& theme) : t (theme)
    {
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Section");
        setHelpText ("The dock rail - 1 AMP and 2 GEN each open their bay, one or both");
        setTooltip ("1 AMP / 2 GEN: click a section to open its bay, again to shut");
    }
    std::function<void (int)> onToggleSection;

    void setOpenMask (int m)         { if (open != m)      { open = m; repaint(); } }

    void setStatus (const juce::String& s) { if (status != s) { status = s; repaint(); } }
    static constexpr int kWordX[kNumSections] = { 0, 118 };

    int preferredWidth() const { return kPreferredWidth; }

    void activate (int s) { if (onToggleSection && selectable (s)) onToggleSection (s); }
    void mouseMove (const juce::MouseEvent& e) override { setHover (wordAt (e.position)); }
    void mouseEnter (const juce::MouseEvent& e) override { setHover (wordAt (e.position)); }
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }
    void mouseUp (const juce::MouseEvent& e) override
    {

        const int hit = wordAt (e.position);
        if (hit >= 0 && selectable (hit))
            activate (hit);
    }
    void paint (juce::Graphics& g) override
    {

        const auto b = getLocalBounds().toFloat();
        for (int i = 0; i < kNumSections; ++i)
        {
            const auto r = wordRect (i);
            const auto s = stateOf ((open & (1 << i)) != 0, hoverIdx == i);
            paintWord (g, r.withWidth (54.0f), kNames[i], s, t, { false, true, 0.0f });
            if (i == kMovement && status.isNotEmpty())
                paintWord (g, r.withTrimmedLeft (56.0f), status, ControlState::rest, t, { false, false, 8.5f });
        }
        juce::ignoreUnused (b);
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
    static constexpr const char* kNames[kNumSections] = { "1  AMP", "2  GEN" };

    static constexpr int kPreferredWidth = 214;
    juce::Rectangle<float> wordRect (int i) const
    {
        const auto b = getLocalBounds().toFloat();
        const float x0 = b.getX() + (float) kWordX[i];
        const float x1 = i + 1 < kNumSections ? b.getX() + (float) kWordX[i + 1] : b.getRight();
        return { x0, b.getY(), x1 - x0, b.getHeight() };
    }
    int wordAt (juce::Point<float> p) const
    {
        for (int i = 0; i < kNumSections; ++i)
            if (wordRect (i).contains (p))
                return i;
        return -1;
    }

    bool selectable (int i) const { return i >= 0 && i < kNumSections; }
    void setHover (int i) { if (hoverIdx != i) { hoverIdx = i; repaint(); } }
    Theme t;
    int open = 0, hoverIdx = -1;
    juce::String status;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionRail)
};

}
