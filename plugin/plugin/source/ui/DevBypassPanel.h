#pragma once
// THE DEV BYPASS DESK. The chain in signal order, one hard on/off per stage, so
// a stage can be taken out and put back BY EAR while the audio runs (Tyson
// 2026-08-13: "I want the dev tool in the plugin so I can switch on and off
// functions to actually see").
//
// It exists because three of these stages are invisible from the face. GRIT at
// zero does NOT mean the sections are linear - the state clamp sits at 2.0 and
// the pole-radius modulator at ~1.52, and a resonant cascade runs above both
// (measured 2026-08-13: disarming them takes the peak from 2.39 to 8.36 at
// -12 dBFS in). The AGC is likewise always on, and driven into its table by a
// x2.0 pre-scale the reverse-engineered DLL path does not have.
//
// NOT preset state. Nothing here is a parameter, nothing is automatable and
// nothing is saved: reopen the plug-in and every stage is back on. That is
// deliberate - a bypass you can accidentally ship in a project file is a bug.
//
// It sits BESIDE the face, not over it (Tyson 2026-08-13: "the panel should
// appear to the side, so I can use it and the plugin"). The editor grows by the
// desk's width while it is open and shrinks back when it closes, so every
// control on the plate stays where it was and stays reachable - an A/B you have
// to close the panel to hear is not an A/B.
//
// Opened with Ctrl+Shift+click on the TRENCH badge; Esc or the X closes it.
#include "Theme.h"
#include "../dsp/TrenchDspBridge.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>

namespace trench::ui
{

class DevBypassPanel final : public juce::Component
{
public:
    using Bypass = TrenchDspBridge::Bypass;

    explicit DevBypassPanel (const Theme& theme) : t (theme)
    {
        setInterceptsMouseClicks (true, true);
        setWantsKeyboardFocus (true);
    }

    /// Called on every change; the editor forwards it to the bridge.
    std::function<void (const Bypass&)> onChange;

    /// The strip the editor reserves for the desk, to the right of the plate.
    static constexpr int kWidth = 300;

    std::function<void (bool)> onOpenChanged;   // editor resizes the window

    void open (const Bypass& current)
    {
        state = current;
        setVisible (true);
        toFront (true);
        grabKeyboardFocus();
        if (onOpenChanged) onOpenChanged (true);
        repaint();
    }
    void close()
    {
        setVisible (false);
        if (onOpenChanged) onOpenChanged (false);
    }

    /// LINEAR CASCADE ONLY: every stage that is not the cascade itself, out.
    /// Public because the FaceShot proof drives the same call the button does.
    void setLinearCascadeOnly()
    {
        state.nonlinearity = false;
        state.agc = false;
        state.saturate = false;
        push();
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }
        return false;
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (hitTest2 (e.position)); }
    void mouseExit (const juce::MouseEvent&) override   { setHover (-1); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int hit = hitTest2 (e.position);
        if (hit == kCloseHit)
        {
            close();
            return;
        }
        if (hit == kAllOffHit)
        {
            setLinearCascadeOnly();
            return;
        }
        if (hit == kResetHit)
        {
            state = Bypass {};
            push();
            return;
        }
        if (hit >= 0 && hit < kNumRows)
        {
            flagFor (hit) = ! flagFor (hit);
            push();
            return;
        }
        // The AGC drive bar: click anywhere along it to set the drive.
        const auto bar = driveBar();
        if (bar.contains (e.position))
        {
            const float f = juce::jlimit (0.0f, 1.0f,
                                          (e.position.x - bar.getX()) / bar.getWidth());
            state.agcDrive = kDriveMin + f * (kDriveMax - kDriveMin);
            push();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto bar = driveBar();
        if (! bar.contains (juce::Point<float> (bar.getCentreX(), e.position.y)))
            return;
        const float f = juce::jlimit (0.0f, 1.0f, (e.position.x - bar.getX()) / bar.getWidth());
        state.agcDrive = kDriveMin + f * (kDriveMax - kDriveMin);
        push();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0d0f12));
        const auto card = cardBounds();
        g.setColour (juce::Colour (0xff15181d));
        g.fillRoundedRectangle (card, 4.0f);
        g.setColour (juce::Colour (0xff3a4048));
        g.drawRoundedRectangle (card, 4.0f, 1.0f);

        g.setFont (displayFont (11.0f, true));
        g.setColour (juce::Colour (0xffd8dee6));
        g.drawText ("POST-CASCADE CHAIN - DEV BYPASS",
                    card.withHeight (kHeaderH).reduced (10.0f, 0.0f),
                    juce::Justification::centredLeft, false);
        g.setFont (displayFont (9.0f, false));
        g.setColour (juce::Colour (0xff707a86));
        g.drawText ("not saved", card.withHeight (kHeaderH).withTrimmedRight (22.0f),
                    juce::Justification::centredRight, false);

        // Close cross
        {
            const auto x = closeBox();
            g.setColour (hoverIdx == kCloseHit ? juce::Colours::white
                                               : juce::Colour (0xff707a86));
            g.drawLine (x.getX() + 3.0f, x.getY() + 3.0f, x.getRight() - 3.0f, x.getBottom() - 3.0f, 1.2f);
            g.drawLine (x.getRight() - 3.0f, x.getY() + 3.0f, x.getX() + 3.0f, x.getBottom() - 3.0f, 1.2f);
        }

        for (int i = 0; i < kNumRows; ++i)
            paintRow (g, i);

        paintDrive (g);
        paintButton (g, allOffBox(), "LINEAR CASCADE ONLY", hoverIdx == kAllOffHit);
        paintButton (g, resetBox(),  "SHIPPED CHAIN",       hoverIdx == kResetHit);
    }

    /// Live AGC pull, fed from the processor each frame. The whole point of a
    /// desk in the plug-in rather than a bench script is watching a number move
    /// while the wheel moves.
    void setAgcReductionDb (float db)
    {
        if (! juce::approximatelyEqual (db, agcPullDb))
        {
            agcPullDb = db;
            if (isVisible())
                repaint (rowBounds (kAgc).getSmallestIntegerContainer());
        }
    }

private:
    // Signal order. The caption is what the stage DOES, in the plainest words
    // available - this panel is read while listening, not studied.
    enum Row { kNonlinearity = 0, kAgc, kSaturate, kDcBlock, kX3Movement, kNumRows };
    static constexpr int kCloseHit  = 100;
    static constexpr int kAllOffHit = 101;
    static constexpr int kResetHit  = 102;
    static constexpr float kDriveMin = 1.0f;
    static constexpr float kDriveMax = 4.0f;
    static constexpr float kHeaderH = 22.0f;
    static constexpr float kRowH    = 34.0f;
    static constexpr float kDriveH  = 26.0f;
    static constexpr float kFootH   = 30.0f;

    // NAMED FOR WHAT THEY ARE (Tyson 2026-08-13: "make it obvious, not dumbed
    // down, what the controls are"). This is the desk you read while deciding
    // whether a stage belongs in the product, so every row states the actual
    // mechanism and the actual number it fires at. No product words.
    static const char* rowName (int i)
    {
        switch (i)
        {
            case kNonlinearity: return "STATE CLIP + POLE MOD";
            case kAgc:          return "AGC LEVELLER";
            case kSaturate:     return "OUTPUT TANH";
            case kDcBlock:      return "DC BLOCKER";
            default:            return "X3 MOVEMENT";
        }
    }
    static const char* rowWhat (int i)
    {
        switch (i)
        {
            case kNonlinearity:
                return "state clamp 2.0, pole R bend 1.52 - runs at GRIT 0";
            case kAgc:
                return "16-tooth E-mu table, per sample, index = gain x |x|";
            case kSaturate:
                return "soft clip, knee +12.0 dBFS, ceiling +18.1 dBFS";
            case kDcBlock:
                return "highpass at 5 Hz";
            default:
                return "block-rate rebuild, kernel ramp, morph one-pole R 0.452";
        }
    }
    bool& flagFor (int i)
    {
        switch (i)
        {
            case kNonlinearity: return state.nonlinearity;
            case kAgc:          return state.agc;
            case kSaturate:     return state.saturate;
            case kDcBlock:      return state.dcBlock;
            default:            return state.x3Movement;
        }
    }
    bool flagAt (int i) const
    {
        switch (i)
        {
            case kNonlinearity: return state.nonlinearity;
            case kAgc:          return state.agc;
            case kSaturate:     return state.saturate;
            case kDcBlock:      return state.dcBlock;
            default:            return state.x3Movement;
        }
    }

    juce::Rectangle<float> cardBounds() const
    {
        const float h = kHeaderH + kNumRows * kRowH + kDriveH + kFootH + 10.0f;
        return juce::Rectangle<float> (8.0f, 8.0f, (float) getWidth() - 16.0f, h);
    }
    juce::Rectangle<float> closeBox() const
    {
        const auto c = cardBounds();
        return { c.getRight() - 20.0f, c.getY() + 5.0f, 14.0f, 14.0f };
    }
    juce::Rectangle<float> rowBounds (int i) const
    {
        const auto c = cardBounds();
        return { c.getX() + 8.0f, c.getY() + kHeaderH + (float) i * kRowH,
                 c.getWidth() - 16.0f, kRowH };
    }
    juce::Rectangle<float> capBox (int i) const
    {
        const auto r = rowBounds (i);
        return juce::Rectangle<float> (44.0f, 17.0f)
                   .withCentre ({ r.getRight() - 26.0f, r.getCentreY() });
    }
    juce::Rectangle<float> driveBar() const
    {
        const auto c = cardBounds();
        const float y = c.getY() + kHeaderH + kNumRows * kRowH + 6.0f;
        return { c.getX() + 98.0f, y, c.getWidth() - 114.0f, 10.0f };
    }
    juce::Rectangle<float> allOffBox() const
    {
        const auto c = cardBounds();
        return { c.getX() + 8.0f, c.getBottom() - kFootH, (c.getWidth() - 24.0f) * 0.5f, 20.0f };
    }
    juce::Rectangle<float> resetBox() const
    {
        const auto a = allOffBox();
        return a.withX (a.getRight() + 8.0f);
    }

    int hitTest2 (juce::Point<float> p) const
    {
        if (closeBox().expanded (3.0f).contains (p)) return kCloseHit;
        if (allOffBox().contains (p))                return kAllOffHit;
        if (resetBox().contains (p))                 return kResetHit;
        for (int i = 0; i < kNumRows; ++i)
            if (rowBounds (i).contains (p))
                return i;
        return -1;
    }
    void setHover (int i) { if (hoverIdx != i) { hoverIdx = i; repaint(); } }
    void push()
    {
        if (onChange) onChange (state);
        repaint();
    }

    void paintRow (juce::Graphics& g, int i)
    {
        const auto r = rowBounds (i);
        const bool on = flagAt (i);
        if (hoverIdx == i)
        {
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);
        }
        g.setFont (displayFont (11.0f, true));
        g.setColour (on ? juce::Colour (0xffe4e9ef) : juce::Colour (0xff5c646e));
        g.drawText (rowName (i), r.withTrimmedLeft (8.0f).withHeight (16.0f).translated (0.0f, 3.0f),
                    juce::Justification::centredLeft, false);
        g.setFont (displayFont (9.0f, false));
        g.setColour (juce::Colour (on ? 0xff7d8894 : 0xff4a515a));
        g.drawText (rowWhat (i),
                    r.withTrimmedLeft (8.0f).withTrimmedRight (52.0f)
                     .withHeight (13.0f).translated (0.0f, 17.0f),
                    juce::Justification::centredLeft, false);
        // What it is doing RIGHT NOW, beside the name, for the one stage that
        // reports it. -0.0 dB with the music playing means it is not engaging.
        if (i == kAgc && on)
        {
            g.setFont (displayFont (10.0f, true));
            g.setColour (agcPullDb < -0.05f ? juce::Colour (0xffe8b04a)
                                            : juce::Colour (0xff5c646e));
            g.drawText (juce::String (agcPullDb, 1) + " dB",
                        r.withTrimmedRight (72.0f).withHeight (16.0f)
                         .translated (0.0f, 3.0f).toNearestInt(),
                        juce::Justification::centredRight, false);
        }
        paintCap (g, capBox (i), on);
    }

    // The state IS the word. An off stage says OFF and goes dark; there is no
    // separate lamp to read against a label.
    static void paintCap (juce::Graphics& g, juce::Rectangle<float> b, bool on)
    {
        g.setColour (on ? juce::Colour (0xff2f6f4f) : juce::Colour (0xff2a2e34));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (on ? juce::Colour (0xff4fbf87) : juce::Colour (0xff474d55));
        g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
        g.setFont (displayFont (10.0f, true));
        g.setColour (on ? juce::Colour (0xffd6f5e5) : juce::Colour (0xff767d86));
        g.drawText (on ? "ON" : "OFF", b.toNearestInt(), juce::Justification::centred, false);
    }

    static void paintButton (juce::Graphics& g, juce::Rectangle<float> b,
                             const juce::String& text, bool hot)
    {
        g.setColour (hot ? juce::Colour (0xff262c34) : juce::Colour (0xff1c2027));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (juce::Colour (0xff444c56));
        g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
        g.setFont (displayFont (10.0f, true));
        g.setColour (juce::Colour (0xffc3cbd4));
        g.drawText (text, b.toNearestInt(), juce::Justification::centred, false);
    }

    void paintDrive (juce::Graphics& g)
    {
        const auto bar = driveBar();
        g.setFont (displayFont (10.0f, false));
        g.setColour (juce::Colour (state.agc ? 0xff9aa4af : 0xff4a515a));
        g.drawText ("AGC PRE-SCALE",
                    juce::Rectangle<float> (cardBounds().getX() + 8.0f, bar.getY() - 3.0f,
                                            88.0f, 16.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);
        g.setColour (juce::Colour (0xff23272d));
        g.fillRoundedRectangle (bar, 2.0f);
        const float f = juce::jlimit (0.0f, 1.0f,
                                      (state.agcDrive - kDriveMin) / (kDriveMax - kDriveMin));
        g.setColour (state.agc ? juce::Colour (0xff4fbf87) : juce::Colour (0xff414852));
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * f)), 2.0f);
        // x1.0 is the DLL's own: no pre-scale at all. Marked so the panel says
        // where the hardware sits without needing a legend.
        const float unityX = bar.getX();
        g.setColour (juce::Colour (0xff8a939d));
        g.drawLine (unityX, bar.getY() - 2.0f, unityX, bar.getBottom() + 2.0f, 1.0f);
        g.setFont (displayFont (9.0f, false));
        g.setColour (juce::Colour (0xff9aa4af));
        const juce::String mark = state.agcDrive <= 1.005f ? "   = the DLL path, no pre-scale"
                              : (std::abs (state.agcDrive - 2.0f) < 0.005f ? "   = shipped" : "");
        g.drawText ("x" + juce::String (state.agcDrive, 2) + mark,
                    juce::Rectangle<float> (bar.getX(), bar.getBottom() + 1.0f,
                                            bar.getWidth(), 12.0f).toNearestInt(),
                    juce::Justification::centredRight, false);
    }

    Theme t;
    Bypass state;
    float agcPullDb = 0.0f;
    int hoverIdx = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DevBypassPanel)
};

} // namespace trench::ui
