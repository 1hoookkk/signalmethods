#pragma once
// THE MOVEMENT SKETCH. Draw a phrase, hear it through the LIVE slot, SAVE it as
// a Function Generator template the bake ships. Dev-only: it is opened from the
// bypass desk, it is not preset state, and nothing here is saved with a session.
#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>

namespace trench::ui
{

class MovementSketch final : public juce::Component,
                             private juce::Timer
{
public:
    static constexpr int kHeight = 150;
    static constexpr int kCells  = 64;

    MovementSketch() { setInterceptsMouseClicks (true, true); }

    std::function<void()> onChanged, onSave, onClose;

    const float* values() const noexcept { return vals; }
    int  steps() const noexcept          { return numSteps; }
    bool smooth() const noexcept         { return isSmooth; }
    int  direction() const noexcept      { return dir; }
    int  stepsPerBeat() const noexcept   { return kRates[rateIdx]; }

    void setStatus (const juce::String& s)
    {
        status = s;
        startTimer (2000);
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (hitButton (e.position)); }
    void mouseExit (const juce::MouseEvent&) override   { setHover (-1); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int b = hitButton (e.position);
        if (b >= 0)
        {
            press (b);
            return;
        }
        if (! stripBounds().contains (e.position))
            return;
        if (e.mods.isRightButtonDown())
        {
            erasing = true;
            lastStep = -1;
            eraseAt (e.position);
            return;
        }
        if (std::abs (e.position.x - markerX()) <= 5.0f)
        {
            draggingEnd = true;
            return;
        }
        painting = true;
        lastStep = -1;
        if (e.mods.isShiftDown())
        {
            anchorStep = juce::jlimit (0, numSteps - 1, rawStep (e.position.x));
            anchorValue = valueAt (e.position.y);
            lineMode = true;
        }
        paintAt (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingEnd)
            setStepsFromX (e.position.x);
        else if (erasing)
            eraseAt (e.position);
        else if (lineMode)
            lineTo (e.position);
        else if (painting)
            paintAt (e.position);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! painting && ! draggingEnd && ! erasing)
            return;
        painting = draggingEnd = erasing = lineMode = false;
        if (onChanged) onChanged();
    }

    void paint (juce::Graphics& g) override
    {
        const auto card = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff15181d));
        g.fillRoundedRectangle (card, 4.0f);
        g.setColour (juce::Colour (0xff3a4048));
        g.drawRoundedRectangle (card, 4.0f, 1.0f);

        const auto s = stripBounds();
        g.setColour (juce::Colour (0xff0d0f12));
        g.fillRoundedRectangle (s, 3.0f);
        const float colW = s.getWidth() / (float) kCells;
        g.setColour (juce::Colour (0xff262c34));
        for (int i = 4; i < kCells; i += 4)
            g.drawLine (s.getX() + (float) i * colW, s.getY() + 2.0f,
                        s.getX() + (float) i * colW, s.getBottom() - 2.0f, 1.0f);
        g.setColour (juce::Colour (0xff444c56));
        g.drawLine (s.getX(), s.getCentreY(), s.getRight(), s.getCentreY(), 1.0f);

        const float half = halfHeight();
        const float base = s.getCentreY();
        juce::Path curve;
        for (int i = 0; i < numSteps; ++i)
        {
            const float x = s.getX() + (float) i * colW;
            const float y = base - vals[i] * half;
            if (i == 0)
                curve.startNewSubPath (x, y);
            if (isSmooth)
                curve.lineTo (x + colW * 0.5f, y);
            else
            {
                curve.lineTo (x, y);
                curve.lineTo (x + colW, y);
            }
        }
        if (isSmooth)
            curve.lineTo (markerX(), base - vals[numSteps - 1] * half);
        juce::Path area (curve);
        area.lineTo (markerX(), base);
        area.lineTo (s.getX(), base);
        area.closeSubPath();
        g.setColour (juce::Colour (0x334fbf87));
        g.fillPath (area);
        g.setColour (juce::Colour (0xff4fbf87));
        g.strokePath (curve, juce::PathStrokeType (1.5f));
        g.setColour (juce::Colour (0xffe8b04a));
        g.drawLine (markerX(), s.getY(), markerX(), s.getBottom(), 1.5f);

        g.setFont (displayFont (9.0f, false));
        g.setColour (juce::Colour (0xff707a86));
        g.drawText (status.isNotEmpty() ? status
                                       : juce::String (numSteps) + " steps \xc2\xb7 " + rateName (rateIdx),
                    s.withTrimmedRight (4.0f).withHeight (13.0f).translated (0.0f, 2.0f).toNearestInt(),
                    juce::Justification::centredRight, false);

        for (int i = 0; i < kNumButtons; ++i)
            paintButton (g, buttonBox (i), buttonText (i), hoverIdx == i, buttonLit (i));
    }

private:
    enum { kRate = 0, kSmooth, kDir, kClear, kSave, kClose, kNumButtons };
    static constexpr float kBtnH = 20.0f;
    static constexpr int   kRates[7] = { 1, 2, 3, 4, 6, 8, 16 };

    void timerCallback() override
    {
        stopTimer();
        status = {};
        repaint();
    }

    juce::Rectangle<float> stripBounds() const
    {
        return getLocalBounds().toFloat().reduced (8.0f).withTrimmedBottom (kBtnH + 6.0f);
    }
    juce::Rectangle<float> buttonBox (int i) const
    {
        const auto r = getLocalBounds().toFloat().reduced (8.0f);
        const float w = (r.getWidth() - (float) (kNumButtons - 1) * 4.0f) / (float) kNumButtons;
        return { r.getX() + (float) i * (w + 4.0f), r.getBottom() - kBtnH, w, kBtnH };
    }
    float halfHeight() const { return stripBounds().getHeight() * 0.5f - 2.0f; }
    float markerX() const
    {
        const auto s = stripBounds();
        return s.getX() + (float) numSteps * s.getWidth() / (float) kCells;
    }
    int hitButton (juce::Point<float> p) const
    {
        for (int i = 0; i < kNumButtons; ++i)
            if (buttonBox (i).contains (p))
                return i;
        return -1;
    }
    void setHover (int i) { if (hoverIdx != i) { hoverIdx = i; repaint(); } }

    juce::String buttonText (int i) const
    {
        switch (i)
        {
            case kRate:   return rateName (rateIdx);
            case kSmooth: return "SMOOTH";
            case kDir:    return dirName (dir);
            case kClear:  return "CLEAR";
            case kSave:   return "SAVE";
            default:      return "CLOSE";
        }
    }
    bool buttonLit (int i) const { return i == kSmooth && isSmooth; }
    static const char* rateName (int i)
    {
        switch (i)
        {
            case 0:  return "1/4";
            case 1:  return "1/8";
            case 2:  return "1/8T";
            case 3:  return "1/16";
            case 4:  return "1/16T";
            case 5:  return "1/32";
            default: return "1/64";
        }
    }
    static const char* dirName (int i)
    {
        switch (i)
        {
            case 0:  return "FWD";
            case 1:  return "REV";
            case 2:  return "PEND";
            case 3:  return "RAND";
            case 4:  return "BROWN";
            default: return "1SHOT";
        }
    }

    void press (int i)
    {
        switch (i)
        {
            case kRate:   rateIdx = (rateIdx + 1) % 7; break;
            case kSmooth: isSmooth = ! isSmooth; break;
            case kDir:    dir = (dir + 1) % 6; break;
            case kClear:  for (auto& v : vals) v = 0.0f; break;
            case kSave:   if (onSave) onSave(); return;
            default:      if (onClose) onClose(); return;
        }
        repaint();
        if (onChanged) onChanged();
    }

    int rawStep (float x) const
    {
        const auto s = stripBounds();
        return (int) std::floor ((x - s.getX()) * (float) kCells / s.getWidth());
    }
    float valueAt (float y) const
    {
        return juce::jlimit (-1.0f, 1.0f, (stripBounds().getCentreY() - y) / halfHeight());
    }

    void paintAt (juce::Point<float> p)
    {
        const int step = rawStep (p.x);
        if (step < 0 || step >= numSteps)
            return;
        const float v = valueAt (p.y);
        if (lastStep >= 0 && std::abs (step - lastStep) > 1)
        {
            const int span = std::abs (step - lastStep);
            const int way = step > lastStep ? 1 : -1;
            for (int k = 1; k < span; ++k)
            {
                const int i = lastStep + way * k;
                if (i >= 0 && i < numSteps)
                    vals[i] = lastValue + (v - lastValue) * ((float) k / (float) span);
            }
        }
        vals[step] = v;
        lastStep = step;
        lastValue = v;
        repaint();
    }

    void lineTo (juce::Point<float> p)
    {
        const int step = juce::jlimit (0, numSteps - 1, rawStep (p.x));
        const float v = valueAt (p.y);
        const float span = (float) (step - anchorStep);
        for (int i = juce::jmin (anchorStep, step); i <= juce::jmax (anchorStep, step); ++i)
            vals[i] = span != 0.0f ? anchorValue + (v - anchorValue) * ((float) (i - anchorStep) / span)
                                   : v;
        repaint();
    }

    void eraseAt (juce::Point<float> p)
    {
        const int step = rawStep (p.x);
        if (step < 0 || step >= numSteps)
            return;
        const int from = lastStep >= 0 ? lastStep : step;
        for (int i = juce::jmin (from, step); i <= juce::jmax (from, step); ++i)
            vals[i] = 0.0f;
        lastStep = step;
        repaint();
    }

    void setStepsFromX (float x)
    {
        const auto s = stripBounds();
        const int n = juce::jlimit (1, kCells,
                                    juce::roundToInt ((x - s.getX()) * (float) kCells / s.getWidth()));
        if (n != numSteps)
        {
            numSteps = n;
            repaint();
        }
    }

    static void paintButton (juce::Graphics& g, juce::Rectangle<float> b,
                             const juce::String& text, bool hot, bool lit)
    {
        g.setColour (lit ? juce::Colour (0xff2f6f4f)
                         : (hot ? juce::Colour (0xff262c34) : juce::Colour (0xff1c2027)));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (lit ? juce::Colour (0xff4fbf87) : juce::Colour (0xff444c56));
        g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
        g.setFont (displayFont (10.0f, true));
        g.setColour (juce::Colour (lit ? 0xffd6f5e5 : 0xffc3cbd4));
        g.drawText (text, b.toNearestInt(), juce::Justification::centred, false);
    }

    float vals[kCells] {};
    int numSteps = kCells;
    bool isSmooth = true;
    int dir = 0;
    int rateIdx = 6;
    int hoverIdx = -1;
    bool painting = false, draggingEnd = false, erasing = false, lineMode = false;
    int lastStep = -1;
    float lastValue = 0.0f;
    int anchorStep = 0;
    float anchorValue = 0.0f;
    juce::String status;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MovementSketch)
};

} // namespace trench::ui
