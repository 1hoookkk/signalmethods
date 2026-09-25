#pragma once
#include "SelectorLookAndFeel.h"
#include "Theme.h"
#include "../dsp/FuncGenPatterns.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>
#include <vector>
namespace trench::ui
{
class SectionRail final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    enum Section { kDrive = 0, kMotion, kNumSections };
    explicit SectionRail (const Theme& theme) : t (theme)
    {
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Section");
        setTooltip ("GAIN / MOVEMENT - choose which room is open below");
    }
    std::function<void (int)> onToggleSection;
    void setOpenSection (int s)      { if (open != s)      { open = s; repaint(); } }
    void setHot (int s, bool h)
    {
        if (s >= 0 && s < kNumSections && hot[(size_t) s] != h)
        {
            hot[(size_t) s] = h;
            repaint();
        }
    }
    void setMotionAvailable (bool a) { if (motionOk != a) { motionOk = a; repaint(); } }
    void activate (int s) { if (onToggleSection) onToggleSection (s); }
    void mouseEnter (const juce::MouseEvent&) override { hoverIdx = 0; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hoverIdx = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()))
            return;
        juce::PopupMenu m;
        m.setLookAndFeel (&lightMenu);
        for (int i = 0; i < kNumSections; ++i)
            m.addItem (i + 1, kNames[i], i != kMotion || motionOk, open == i);
        juce::Component::SafePointer<SectionRail> self (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [self] (int id)
                         {
                             if (self != nullptr && id > 0 && self->onToggleSection)
                                 self->onToggleSection (id - 1);
                         });
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        drawMutedBoneReadout (g, b, b.getHeight() * 0.18f, hoverIdx >= 0, t);
        g.setFont (displayFont (9.5f, true).withExtraKerningFactor (0.03f));
        g.setColour (juce::Colour (0xff0b0b0b).withAlpha (hoverIdx >= 0 ? 1.0f : 0.9f));
        const float dividerX = b.getRight() - 21.0f;
        if (open >= 0 && open < kNumSections)
            g.drawText (kNames[open],
                        juce::Rectangle<float> (b.getX() + 6.0f, b.getY(),
                                                dividerX - b.getX() - 9.0f, b.getHeight()).toNearestInt(),
                        juce::Justification::centredLeft, false);
        g.setColour (juce::Colour (0xff6a6256).withAlpha (0.52f));
        g.drawLine (dividerX, b.getY() + 3.2f, dividerX, b.getBottom() - 3.2f, 0.9f);
        const float cx = (dividerX + b.getRight() - 5.0f) * 0.5f, cy = b.getCentreY() - 1.0f;
        juce::Path chevron;
        chevron.startNewSubPath (cx - 4.4f, cy - 2.0f);
        chevron.lineTo (cx, cy + 2.4f);
        chevron.lineTo (cx + 4.4f, cy - 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.40f));
        g.strokePath (chevron, { 1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded },
                      juce::AffineTransform::translation (0.0f, 0.8f));
        g.setColour (t.arrow());
        g.strokePath (chevron, { 1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }
    struct LightMenuLnF : juce::LookAndFeel_V4
    {
        juce::Font getPopupMenuFont() override { return displayFont (11.0f, false); }
        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (juce::Colour (0xfff4f2ea));
            g.setColour (juce::Colour (0xff5a5750));
            g.drawRect (juce::Rectangle<int> (0, 0, w, h), 1);
        }
        void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                bool, bool isActive, bool isHighlighted, bool isTicked,
                                bool, const juce::String& text, const juce::String&,
                                const juce::Drawable*, const juce::Colour*) override
        {
            if (isHighlighted && isActive)
            {
                g.setColour (juce::Colour (0xffd9d5c8));
                g.fillRect (area.reduced (1));
            }
            g.setFont (displayFont (11.0f, isTicked));
            g.setColour (juce::Colour (0xff2a2722).withAlpha (isActive ? 1.0f : 0.4f));
            g.drawText (text, area.reduced (22, 0), juce::Justification::centredLeft, false);
            if (isTicked)
            {
                juce::Path check;
                const float cx = (float) area.getX() + 9.0f, cy = (float) area.getCentreY();
                check.startNewSubPath (cx - 3.0f, cy);
                check.lineTo (cx - 1.0f, cy + 2.5f);
                check.lineTo (cx + 3.5f, cy - 3.0f);
                g.setColour (juce::Colour (0xff2a2722));
                g.strokePath (check, { 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
            }
        }
    };
private:
    static constexpr const char* kNames[kNumSections] = { "GAIN", "MOVEMENT" };
    Theme t;
    LightMenuLnF lightMenu;
    int open = kDrive, hoverIdx = -1;
    bool hot[kNumSections] = { false, false };
    bool motionOk = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionRail)
};
}
