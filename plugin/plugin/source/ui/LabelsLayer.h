#pragma once
#include "Theme.h"
namespace trench::ui
{
class LabelsLayer : public juce::Component
{
public:
    explicit LabelsLayer (const Theme& theme) : t (theme)
    {
        setInterceptsMouseClicks (false, false);
    }
    void setRailLabels (juce::String upper, juce::String lower)
    {
        if (upper != railUpper || lower != railLower)
        {
            railUpper = std::move (upper); railLower = std::move (lower); repaint();
        }
    }
    /// MIX at rest was unreadable (Tyson 2026-08-15 "not easy to tell what
    /// the mix % is... we will have too many white boxes"): the value is
    /// ENGRAVED in plate ink under the drum, same voice as the 100/50/0
    /// scale - a printed number, not another bone box.
    void setMixValue (float normalised)
    {
        const int pct = juce::roundToInt (juce::jlimit (0.0f, 1.0f, normalised) * 100.0f);
        if (pct != mixPct) { mixPct = pct; repaint(); }
    }
    void setBrandLit (bool lit)
    {
        if (brandLit != lit) { brandLit = lit; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        const auto draw = [this, &g] (const juce::String& id, const juce::String& text,
                                      bool centred, bool strong = false, float alpha = 1.0f)
        {
            const auto r = t.rect (id);
            const float fs = t.fontSize (id, 11.0f);
            if (r.getWidth() < 1.0f || r.getHeight() < 1.0f || fs < 0.5f || text.isEmpty())
                return;
            // E-mu's labels are compact Tahoma with native spacing. The added
            // tracking was what made short anchors like BODY / MIX look loose.
            g.setFont (displayFont (fs, strong));
            g.setColour (t.textColour (id, t.labelInk()).withMultipliedAlpha (alpha));
            g.drawFittedText (text.toUpperCase(), r.toNearestInt(),
                              centred ? juce::Justification::centred
                                       : juce::Justification::centredLeft, 1);
        };
        draw ("typeLabel",   t.text ("typeLabel",   "BODY"), true, true);
        // MIX outline retired (verdict 2026-07-31 "too much") — the label and
        // wheel stand on the plate like every other anchor.
        draw ("amountLabel", t.text ("amountLabel", "AMOUNT"), true, true);
        {
            // MIX SCALE (Tyson 2026-08-10 "like a real brutalist old rack
            // editor"): 0 / 50 / 100 engraved beside the drum with hard 1px
            // ticks — the wheel gets a printed scale like rack hardware, in
            // the plate's own ink, no colour.
            const auto drum = mixDrumRect (t.rect ("amountWheel"));
            if (! drum.isEmpty())
            {
                // 7.6pt was microscopic at the DAW's real 352px (2026-08-15
                // legibility pass) — one point up and a wider box.
                g.setFont (displayFont (8.6f, false));
                for (int v = 0; v <= 100; v += 50)
                {
                    const float y = drum.getBottom() - (float) v / 100.0f * drum.getHeight();
                    g.setColour (t.labelInk().withMultipliedAlpha (0.75f));
                    g.fillRect (drum.getX() - 6.5f, y - 0.5f, 4.5f, 1.0f);
                    g.setColour (t.labelInk());
                    g.drawFittedText (juce::String (v),
                                      juce::Rectangle<float> (drum.getX() - 28.0f, y - 5.5f,
                                                              20.0f, 11.0f).toNearestInt(),
                                      juce::Justification::centredRight, 1);
                }
                if (mixPct >= 0)
                {
                    g.setFont (displayFont (8.6f, true));
                    g.setColour (t.labelInk());
                    g.drawFittedText (juce::String (mixPct),
                                      juce::Rectangle<float> (drum.getCentreX() - 14.0f,
                                                              drum.getBottom() + 3.0f,
                                                              28.0f, 10.0f).toNearestInt(),
                                      juce::Justification::centred, 1);
                }
            }
        }
        // Silkscreen never fades (Tyson 2026-08-15 "No fucking fade") — the
        // printed panel text is ink on metal, whatever the body underneath.
        draw ("morphLabel",  railUpper, true, true);
        draw ("qLabel",      railLower, true, true);
        {
            const auto br = t.rect ("brandLabel");
            const float bfs = t.fontSize ("brandLabel", 16.5f);
            if (br.getWidth() >= 1.0f && bfs >= 0.5f)
            {
                // ENGRAVED, from d9bdfe97 (Tyson 2026-08-12: "I don't see the
                // TRENCH logo outline"). The flat-Tahoma pass that replaced it
                // dropped the catch entirely, so the word sat on the plate
                // instead of being cut into it. Arial Bold, tracked -0.012 (X3
                // logos sit almost touching), catch alpha 0.72 - the goal
                // build's exact call.
                g.setFont (juce::Font (juce::FontOptions ("Arial", bfs, juce::Font::bold))
                               .withExtraKerningFactor (-0.012f));
                const auto text = t.text ("brandLabel", "");
                const auto box  = br.toNearestInt();
                if (brandLit)
                {
                    g.setColour (t.accent().withAlpha (0.35f));
                    for (int dx = -2; dx <= 2; ++dx)
                        for (int dy = -2; dy <= 2; ++dy)
                            if (dx != 0 || dy != 0)
                                g.drawText (text, box.translated (dx, dy),
                                            juce::Justification::centredLeft, false);
                }
                drawEngravedText (g, text, box, juce::Justification::centredLeft,
                                  t.textColour ("brandLabel", t.labelInk()), 0.72f);
            }
            const auto sr = t.rect ("brandSub");
            const float sfs = t.fontSize ("brandSub", 9.0f);
            if (sr.getWidth() >= 1.0f && sfs >= 0.5f)
            {
                g.setFont (displayFont (sfs, true));
                g.setColour (t.textColour ("brandSub", t.labelInk()));
                g.drawText (t.text ("brandSub", ""), sr.toNearestInt(),
                            juce::Justification::centredLeft, false);
            }
        }
    }
private:
    Theme t;
    juce::String railUpper { "MORPH" };
    juce::String railLower {};
    bool brandLit = false;
    int mixPct = -1;
};
}
