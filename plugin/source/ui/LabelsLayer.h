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
            g.setFont (displayFont (fs, strong));
            const auto just = centred ? juce::Justification::centred : juce::Justification::centredLeft;
            g.setColour (t.textColour (id, t.labelInk()).withMultipliedAlpha (alpha));
            g.drawFittedText (text.upToFirstOccurrenceOf (" (", false, false).toUpperCase() + text.fromFirstOccurrenceOf (" (", true, false), r.toNearestInt(), just, 1);
        };
        draw ("typeLabel",   t.text ("typeLabel",   "BODY"), true, true);
        draw ("morphLabel",  railUpper, true, true);
        draw ("qLabel",      railLower, true, true);
        draw ("inputLabel",  t.text ("inputLabel",  ""), true, false);
        draw ("outputLabel", t.text ("outputLabel", ""), true, false);
        {
            const auto br = t.rect ("brandLabel");
            const float bfs = t.fontSize ("brandLabel", 16.5f);
            if (br.getWidth() >= 1.0f && bfs >= 0.5f)
            {
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
                                  t.textColour ("brandLabel", t.labelInk()), 0.45f);
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
};
}
