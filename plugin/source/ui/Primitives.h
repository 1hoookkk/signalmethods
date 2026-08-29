#pragma once
#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
namespace trench::ui
{

enum class ControlState { rest, hover, engaged, engagedHover };
inline ControlState stateOf (bool engaged, bool hover)
{
    return engaged ? (hover ? ControlState::engagedHover : ControlState::engaged)
                   : (hover ? ControlState::hover : ControlState::rest);
}
inline bool isEngaged (ControlState s) { return s == ControlState::engaged || s == ControlState::engagedHover; }
inline bool isHover   (ControlState s) { return s == ControlState::hover   || s == ControlState::engagedHover; }

inline void paintPill (juce::Graphics& g, juce::Rectangle<float> r, float radius, const Theme& t, bool lit = false)
{
    drawMutedBoneReadout (g, r, radius, lit, t);
}

struct WordStyle
{
    bool lamp = false;
    bool latch = false;
    float pt = 0.0f;
};
inline void paintWord (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& word,
                       ControlState s, const Theme& t, WordStyle style = {})
{
    const auto ink = t.labelInk();
    const auto alive = t.modulationLamp();
    float x = r.getX();
    if (style.latch)
    {
        juce::Path tri;
        const float cx = x + 4.0f, cy = r.getCentreY();
        if (isEngaged (s)) tri.addTriangle (cx - 3.0f, cy - 1.5f, cx + 3.0f, cy - 1.5f, cx, cy + 2.5f);
        else               tri.addTriangle (cx - 1.5f, cy - 3.0f, cx - 1.5f, cy + 3.0f, cx + 2.5f, cy);
        g.setColour (ink.withAlpha (isEngaged (s) ? 0.92f : isHover (s) ? 0.80f : 0.58f));
        g.fillPath (tri);
        x += 10.0f;
    }
    if (style.lamp)
    {
        const juce::Rectangle<float> lamp { x + 1.0f, r.getCentreY() - 2.5f, 5.0f, 5.0f };
        g.setColour (isEngaged (s) ? alive : ink.withAlpha (isHover (s) ? 0.80f : 0.45f));
        g.fillEllipse (lamp);
        if (isHover (s))
        {
            g.setColour ((isEngaged (s) ? alive : ink).withAlpha (0.25f));
            g.fillEllipse (lamp.expanded (2.0f));
        }
        x += 10.0f;
    }
    g.setFont (style.pt > 0.0f ? telemetryFont (style.pt, false) : t.smallLabel (true));
    g.setColour (isHover (s) && ! isEngaged (s) ? alive.withAlpha (0.95f)
                                                : ink.withAlpha (isEngaged (s) ? 0.92f : 0.58f));
    g.drawText (word, juce::Rectangle<float> (x, r.getY(), r.getRight() - x, r.getHeight()).toNearestInt(),
                juce::Justification::centredLeft, false);
}

inline void paintArrow (juce::Graphics& g, juce::Rectangle<float> r, bool pointsRight, ControlState s, const Theme& t)
{
    juce::Path p;
    const float cy = r.getCentreY();
    if (pointsRight) p.addTriangle (r.getX(), cy - 3.5f, r.getX(), cy + 3.5f, r.getX() + 4.5f, cy);
    else             p.addTriangle (r.getRight(), cy - 3.5f, r.getRight(), cy + 3.5f, r.getRight() - 4.5f, cy);
    g.setColour (isHover (s) ? t.modulationLamp() : juce::Colour (0xff2a2722).withAlpha (0.85f));
    g.fillPath (p);
}
}
