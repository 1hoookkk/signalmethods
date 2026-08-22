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
// THE LOWER BAY'S DOORS: two engraved words on the plate — GAIN and MOVEMENT —
// the way hardware silkscreens a latch (Tyson 2026-08-15 "show me", replacing
// the 2026-08-13 dropdown: a box with a spinner read as a value called GAIN,
// and stacked directly over PRESET it was two dropdowns doing different kinds
// of jobs). A word is a place, not a setting: click one and its room carves
// open beneath it, the frame breaking around the lit word — the label IS the
// tab of the drawer it opened. PRESET keeps the bay's only dropdown, because
// it is the only place a value is picked off a list.
//
// The drawer CLOSES on its own word (Tyson 2026-08-15 "click the room again
// to close it"), which retires the last of the no-off-position law: closing a
// door is cabinet behaviour, not an audio bypass — the audio's real off
// states stay where they always were (PRESET's own OFF bypasses Movement,
// the No-filter body makes the cascade inert). Closed = editor openSection
// -1: no carve, two quiet words. Both rooms carve the SAME plate rectangle
// (PluginEditor kBayRooms[0] == kBayRooms[1]).
//
// A live page you cannot see needs no mark (the citron dot retired 2026-08-14,
// "it reads weird"): an armed MOVEMENT moves the MORPH wheel and the response
// trace on every page, so the motion itself is the always-visible indicator.
class SectionRail final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    // SOURCE (the resample room) is gone; two pages is the whole bay, and there
    // is no third "neither" position - the bay always shows one of them.
    enum Section { kDrive = 0, kMotion, kNumSections };
    explicit SectionRail (const Theme& theme) : t (theme)
    {
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Section");
        setHelpText ("Two doors - click GAIN or MOVEMENT to open its room; mouse wheel swaps");
        setTooltip ("Rooms: click GAIN or MOVEMENT to open; mouse wheel swaps");
    }
    std::function<void (int)> onToggleSection;   // editor owns the open state
    /// -1 = the bay is CLOSED: both words engraved quiet, no room carved.
    void setOpenSection (int s)      { if (open != s)      { open = s; repaint(); } }
    /// One stable strip of two words occupying the bay's inner width.
    int preferredWidth() const { return kPreferredWidth; }

    /// FaceShot hook: same path as clicking a word.
    void activate (int s) { if (onToggleSection && selectable (s)) onToggleSection (s); }
    void mouseMove (const juce::MouseEvent& e) override { setHover (wordAt (e.position)); }
    void mouseEnter (const juce::MouseEvent& e) override { setHover (wordAt (e.position)); }
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }
    /// One step through the rooms, skipping any that cannot be opened.
    void step (int dir)
    {
        for (int hops = 1; hops < kNumSections; ++hops)
        {
            const int n = kNumSections;
            const int cand = ((open + dir * hops) % n + n) % n;
            if (selectable (cand))
            {
                if (cand != open && onToggleSection)
                    onToggleSection (cand);
                return;
            }
        }
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        // The face's gesture law: the mouse wheel always works.
        if (! juce::approximatelyEqual (w.deltaY, 0.0f))
            step (w.deltaY > 0 ? 1 : -1);
    }
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
        // TWO ENGRAVED WORDS, no box, no spinner. The open door's word is full
        // ink; the shut door sits back; an unavailable door (No filter) is
        // barely there. Hover lifts a shut word halfway — a door you could
        // open. The citron pilot dot stays retired (2026-08-14): an armed
        // MOVEMENT shows itself by the wheel and the trace moving.
        const auto ink = juce::Colour (0xff2a2722);
        for (int i = 0; i < kNumSections; ++i)
        {
            const float alpha = open == i     ? 0.95f
                              : hoverIdx == i ? 0.66f
                                              : 0.40f;
            drawCrispText (g, wordRect (i), kNames[i], kTabPt + 1.0f,
                           ink.withAlpha (alpha), open == i);
        }
    }
    // light, plain popup — the E-mu/'95 menu, not the dark glass family.
    // The rail has no menu of its own any more; PRESET (ModSourceBox) and the
    // body list still wear this one, so it stays here as the shared voice.
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
    // E-mu's own voice: the X3 face says GAIN (dB). MOVEMENT is the phrase
    // machine in plain words (renamed from FUNC GEN, Tyson 2026-07-31).
    static constexpr const char* kNames[kNumSections] = { "GAIN", "MOVEMENT" };
    // GAIN word + gap + MOVEMENT word. Widths sized to the 9.5pt engraving.
    static constexpr float kWordW[kNumSections] = { 38.0f, 66.0f };
    static constexpr float kWordGap = 10.0f;
    static constexpr int kPreferredWidth = 114;
    static constexpr float kTabPt  = 8.5f;
    juce::Rectangle<float> wordRect (int i) const
    {
        const auto b = getLocalBounds().toFloat();
        const float x = i == 0 ? b.getX() : b.getX() + kWordW[0] + kWordGap;
        return { x, b.getY(), kWordW[juce::jlimit (0, kNumSections - 1, i)], b.getHeight() };
    }
    int wordAt (juce::Point<float> p) const
    {
        for (int i = 0; i < kNumSections; ++i)
            if (wordRect (i).contains (p))
                return i;
        return -1;
    }
    // Both doors always open (no-fade law 2026-08-15): on No filter the
    // MOVEMENT room's controls simply have nothing to change, like the rest
    // of the filter section.
    bool selectable (int i) const { return i >= 0 && i < kNumSections; }
    void setHover (int i) { if (hoverIdx != i) { hoverIdx = i; repaint(); } }
    Theme t;
    int open = kDrive, hoverIdx = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionRail)
};

}
