#pragma once
#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <vector>

namespace trench::ui
{
class Onboarding final : public juce::Component,
                         private juce::Timer
{
public:
    struct Target
    {
        juce::Rectangle<int> rect;
        juce::String word;
        juce::String hint;
        bool seen = false;
    };
    explicit Onboarding (const Theme& theme) : t (theme)
    {
        setInterceptsMouseClicks (false, false);
    }
    ~Onboarding() override
    {
        if (watched != nullptr)
            watched->removeMouseListener (this);
    }
    void watch (juce::Component& host)
    {
        if (watched != nullptr)
            watched->removeMouseListener (this);
        watched = &host;
        watched->addMouseListener (this, true);
    }
    void setTargets (std::vector<Target> next)
    {
        for (size_t i = 0; i < next.size() && i < targets.size(); ++i)
            next[i].seen = targets[i].seen;
        targets = std::move (next);
        if (forced >= 0 && forced < (int) targets.size())
        {
            hovered = forced;
            drop = 1.0f;
        }
        repaint();
    }
    void forceHover (int index)
    {
        forced = index;
        hovered = index >= 0 && index < (int) targets.size() ? index : -1;
        drop = 1.0f;
        repaint();
    }
    std::function<void()> onComplete;
    void paint (juce::Graphics& g) override
    {
        if (hovered < 0 || hovered >= (int) targets.size())
            return;
        const auto& target = targets[(size_t) hovered];
        const float tt = juce::jlimit (0.0f, 1.0f, drop);
        const float ease = 1.0f - (1.0f - tt) * (1.0f - tt) * (1.0f - tt);
        const float settle = std::sin (tt * juce::MathConstants<float>::pi) * 2.5f;
        const float dy = -14.0f * (1.0f - ease) + settle;
        const float alpha = ease;
        const auto font = displayFont (12.5f, true);
        g.setFont (font);
        const float w = juce::TextLayout::getStringWidth (font, target.hint) + 8.0f;
        const auto r = target.rect.toFloat();
        float y = r.getCentreY() - 8.0f;
        if (r.getHeight() > 60.0f)
            y = r.getY() + 12.0f;
        else if (r.getHeight() < 30.0f)
            y = r.getY() - 20.0f;
        juce::Rectangle<float> line (r.getCentreX() - w * 0.5f, y, w, 16.0f);
        line.setX (juce::jlimit (4.0f, (float) getWidth() - 4.0f - w, line.getX()));
        line.translate (0.0f, dy);
        g.setColour (juce::Colours::black.withAlpha (0.42f * alpha));
        for (int k = 0; k < 8; ++k)
        {
            const float a = (float) k * juce::MathConstants<float>::twoPi / 8.0f;
            g.drawText (target.hint, line.translated (1.4f * std::cos (a), 1.4f * std::sin (a)), juce::Justification::centred, false);
        }
        g.setColour (juce::Colours::black.withAlpha (0.70f * alpha));
        g.drawText (target.hint, line.translated (0.0f, 1.2f), juce::Justification::centred, false);
        g.setColour (t.curveColour().withAlpha (0.98f * alpha));
        g.drawText (target.hint, line, juce::Justification::centred, false);
    }
private:
    void mouseMove (const juce::MouseEvent& e) override { track (e); }
    void mouseEnter (const juce::MouseEvent& e) override { track (e); }
    void mouseDrag (const juce::MouseEvent& e) override { track (e); }
    void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }
    void timerCallback() override
    {
        if (drop < 1.0f)
        {
            drop = juce::jmin (1.0f, drop + 1.0f / 14.0f);
            repaint();
        }
        if (completeTicks > 0 && --completeTicks == 0)
        {
            stopTimer();
            if (onComplete != nullptr)
                onComplete();
            return;
        }
        if (drop >= 1.0f && completeTicks == 0)
            stopTimer();
    }
    void track (const juce::MouseEvent& e)
    {
        const auto p = e.getEventRelativeTo (this).getPosition();
        int hit = -1;
        for (int i = 0; i < (int) targets.size(); ++i)
            if (targets[(size_t) i].rect.contains (p))
                hit = i;
        setHovered (hit);
    }
    void setHovered (int hit)
    {
        if (hit == hovered)
            return;
        hovered = hit;
        if (hit >= 0)
        {
            targets[(size_t) hit].seen = true;
            drop = 0.0f;
            startTimer (16);
        }
        repaint();
        bool all = ! targets.empty();
        for (const auto& target : targets)
            all = all && target.seen;
        if (all && completeTicks == 0)
        {
            completeTicks = 160;
            startTimer (16);
        }
    }
    Theme t;
    juce::Component* watched = nullptr;
    std::vector<Target> targets;
    int hovered = -1;
    int forced = -1;
    float drop = 1.0f;
    int completeTicks = 0;
};
}
