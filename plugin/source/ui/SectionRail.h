#pragma once
#include "SelectorLookAndFeel.h"
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
// THE LOWER BAY'S DOOR: one engraved word on the plate — GAIN — the way
// hardware silkscreens a latch (Tyson 2026-08-15 "show me", replacing the
// 2026-08-13 dropdown: a box with a spinner read as a value called GAIN). A
// word is a place, not a setting: click it and its room carves open beneath
// it, the frame breaking around the lit word — the label IS the tab of the
// drawer it opened.
//
// The drawer CLOSES on its own word (Tyson 2026-08-15 "click the room again
// to close it"), which retires the last of the no-off-position law: closing a
// door is cabinet behaviour, not an audio bypass — the audio's real off
// states stay where they always were (the No-filter body makes the cascade
// inert, Movement's own OFF stops the wheel travelling). Closed = editor
// openSection -1: no carve, one quiet word.
//
// MOVEMENT left the bay 2026-08-25: the pattern is a chip on the display
// glass now (MovementChip), so the bay keeps one room.
class SectionRail final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    // SOURCE (the resample room) and MOVEMENT are both gone; GAIN is the whole
    // bay, open or shut.
    enum Section { kDrive = 0, kNumSections };
    explicit SectionRail (const Theme& theme) : t (theme)
    {
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Section");
        setHelpText ("One door - click FX+ to open the effects room, click it again to shut it");
        setTooltip ("FX+: click to open the effects room, click again to shut");
    }
    std::function<void (int)> onToggleSection;   // editor owns the open state
    /// -1 = the bay is CLOSED: the word engraved quiet, no room carved.
    void setOpenSection (int s)      { if (open != s)      { open = s; repaint(); } }
    /// The word's own width, which the plate's frame break is measured from.
    int preferredWidth() const { return kPreferredWidth; }

    /// FaceShot hook: same path as clicking a word.
    void activate (int s) { if (onToggleSection && selectable (s)) onToggleSection (s); }
    void mouseMove (const juce::MouseEvent& e) override { setHover (wordAt (e.position)); }
    void mouseEnter (const juce::MouseEvent& e) override { setHover (wordAt (e.position)); }
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        // Clicking the OPEN door's word closes the drawer (Tyson 2026-08-15
        // "make it like you can click the room again to close it") — the
        // editor toggles on a repeat pick. A shut word opens its room.
        const int hit = wordAt (e.position);
        if (hit >= 0 && selectable (hit))
            activate (hit);
    }
    void paint (juce::Graphics& g) override
    {
        // THE CHIP SPEAKS MODULATION'S LANGUAGE (Tyson 2026-08-28 "The
        // Modulation chip is what Fx+ should be"): a lamp and a quiet word on
        // the glass - no pill. Open = the lamp lit and the word full ink.
        const auto b = getLocalBounds().toFloat();
        for (int i = 0; i < kNumSections; ++i)
        {
            const bool lit = open == i;
            const float ink = lit ? 0.95f : hoverIdx == i ? 0.80f : 0.55f;
            const juce::Rectangle<float> lamp { b.getX() + 1.0f, b.getCentreY() - 3.0f, 6.0f, 6.0f };
            g.setColour (lit ? t.modulationLamp() : t.telemetry().withAlpha (0.45f));
            g.fillEllipse (lamp);
            if (hoverIdx == i)
            {
                g.setColour ((lit ? t.modulationLamp() : t.telemetry()).withAlpha (0.25f));
                g.fillEllipse (lamp.expanded (2.0f));
            }
            g.setFont (telemetryFont (9.8f));
            g.setColour (t.telemetry().withAlpha (ink));
            g.drawText (kNames[i], b.withTrimmedLeft (12.0f).toNearestInt(),
                        juce::Justification::centredLeft, false);
        }
    }
    // light, plain popup — the E-mu/'95 menu, not the dark glass family.
    // The rail has no menu of its own any more; the MOVEMENT chip's list and
    // the body list still wear this one, so it stays here as the shared voice.
    struct LightMenuLnF : juce::LookAndFeel_V4
    {
        juce::Font getPopupMenuFont() override { return displayFont (13.0f, false); }
        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (juce::Colour (0xfffcfcfd));   // clean white, not beige (2026-08-06)
            g.setColour (juce::Colour (0xff5a5750));
            g.drawRect (juce::Rectangle<int> (0, 0, w, h), 1);
        }
        void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                bool, bool isActive, bool isHighlighted, bool isTicked,
                                bool, const juce::String& text, const juce::String&,
                                const juce::Drawable*, const juce::Colour*) override
        {
            // The preset list's selection, exactly (BodyBrowser kHighlight):
            // a solid blue bar with a white label. This menu used to answer
            // with a pale grey wash and dark text, so the two lists on the
            // same face disagreed about what "selected" looks like.
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
    static constexpr const char* kNames[kNumSections] = { "FX+" };
    // Width sized to the 9.5pt label in its pill.
    static constexpr float kWordW = 48.0f;
    static constexpr int kPreferredWidth = 48;
    juce::Rectangle<float> wordRect (int) const
    {
        const auto b = getLocalBounds().toFloat();
        return { b.getX(), b.getY(), kWordW, b.getHeight() };
    }
    int wordAt (juce::Point<float> p) const
    {
        for (int i = 0; i < kNumSections; ++i)
            if (wordRect (i).contains (p))
                return i;
        return -1;
    }
    // The door always opens (no-fade law 2026-08-15): on No filter its
    // controls simply have nothing to change, like the rest of the face.
    bool selectable (int i) const { return i >= 0 && i < kNumSections; }
    void setHover (int i) { if (hoverIdx != i) { hoverIdx = i; repaint(); } }
    Theme t;
    int open = kDrive, hoverIdx = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionRail)
};

}
