#pragma once
// GOD MODE - the operator's own layout desk, so a verdict ("move that left")
// lands on the face directly instead of round-tripping through a constant, a
// rebuild and a screenshot. It edits the SAME trench::UiLayout the face already
// reads and writes it back through UiLayout::toJson(), so nothing here is a
// second source of truth.
//
// It is compiled ONLY when the CMake option TRENCH_GOD_MODE is ON. The shipping
// VST3 contains none of this file - not a symbol, not a string.
#if TRENCH_GOD_MODE
#include "../TrenchBodyRoster.h"
#include "../UiLayout.h"
#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <limits>
namespace trench::ui
{
// The mutations, as free functions: every gesture below goes through these, and
// they need no window - which is how the round trip gets proved headlessly.
// The JSON stores SOURCE coordinates (the 1010x1557 panel space), never editor
// pixels: anything written back must be converted or the file is wrong.
/// The watched/saved file. A settable global so the headless proof can point
/// the whole feature at a scratch file instead of the operator's own.
inline juce::File& godLayoutFile()
{
    static juce::File f { trench::uiLayoutFile() };
    return f;
}
inline juce::Point<float> editorDeltaToSource (juce::Point<float> d) noexcept
{
    return { d.x * kPanelSourceWidth  / (float) kEditorWidth,
             d.y * kPanelSourceHeight / (float) kEditorHeight };
}
inline void godSetRect (UiLayout& l, const juce::String& id, juce::Rectangle<float> r)
{
    const auto it = l.elements.find (id);
    if (it == l.elements.end())
        return;
    it->second.sourceRect = { std::round (r.getX()), std::round (r.getY()),
                              juce::jmax (1.0f, std::round (r.getWidth())),
                              juce::jmax (1.0f, std::round (r.getHeight())) };
}
inline void godMove (UiLayout& l, const juce::String& id, float dx, float dy)
{
    godSetRect (l, id, l.sourceRectFor (id).translated (dx, dy));
}
inline bool godSaveLayout (const UiLayout& l, const juce::File& f)
{
    f.getParentDirectory().createDirectory();
    return f.replaceWithText (l.toJson());
}
// The overlay. Invisible and mouse-transparent until armed with Ctrl+Shift+G;
// while off the face behaves exactly as it always did.
class GodModeOverlay final : public juce::Component,
                             public juce::KeyListener
{
public:
    explicit GodModeOverlay (UiLayout& l) : layout (l)
    {
        setInterceptsMouseClicks (false, false);
        setWantsKeyboardFocus (true);
        setAlwaysOnTop (true);
        setVisible (false);
    }
    std::function<void()> onLayoutChanged;
    bool isActive() const noexcept { return active; }
    void setActive (bool shouldBeActive)
    {
        active = shouldBeActive;
        setVisible (active);
        setInterceptsMouseClicks (active, active);
        if (active)
        {
            toFront (false);
            grabKeyboardFocus();
        }
        repaint();
    }
    bool keyPressed (const juce::KeyPress& k) override { return handleKey (k); }
    bool keyPressed (const juce::KeyPress& k, juce::Component*) override { return handleKey (k); }
private:
    juce::String hitTest (juce::Point<float> p) const
    {
        juce::String best;
        float bestArea = std::numeric_limits<float>::max();
        for (const auto& e : layout.elements)
        {
            if (e.second.sourceRect.isEmpty())
                continue;
            const auto r = sourceRectToEditor (e.second.sourceRect);
            const float area = r.getWidth() * r.getHeight();
            if (r.contains (p) && area < bestArea)
            {
                best = e.first;
                bestArea = area;
            }
        }
        return best;
    }
    juce::Rectangle<float> editorRectOf (const juce::String& id) const
    {
        return sourceRectToEditor (layout.sourceRectFor (id));
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto id = hitTest (e.position);
        if (id != hovered)
        {
            hovered = id;
            repaint();
        }
    }
    void mouseExit (const juce::MouseEvent&) override { hovered = {}; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        selected = hitTest (e.position);
        hovered = selected;
        dragStart = layout.sourceRectFor (selected);
        resizing = e.mods.isCtrlDown();
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (selected.isEmpty())
            return;
        const auto d = editorDeltaToSource (e.getOffsetFromDragStart().toFloat());
        auto r = dragStart;
        if (resizing)
            r.setSize (r.getWidth() + d.x, r.getHeight() + d.y);
        else
            r.translate (d.x, d.y);
        godSetRect (layout, selected, r);
        changed();
    }
    bool handleKey (const juce::KeyPress& k)
    {
        const auto& m = k.getModifiers();
        const int code = k.getKeyCode();
        if (m.isCtrlDown() && m.isShiftDown() && code == 'G')
        {
            setActive (! active);
            return true;
        }
        if (! active)
            return false;
        if (m.isCtrlDown() && code == 'S')
        {
            status = (godSaveLayout (layout, godLayoutFile()) ? "SAVED " : "SAVE FAILED ")
                   + godLayoutFile().getFullPathName();
            repaint();
            return true;
        }
        const int dx = code == juce::KeyPress::leftKey ? -1 : code == juce::KeyPress::rightKey ? 1 : 0;
        const int dy = code == juce::KeyPress::upKey   ? -1 : code == juce::KeyPress::downKey  ? 1 : 0;
        if ((dx == 0 && dy == 0) || selected.isEmpty())
            return false;
        const float step = m.isShiftDown() ? 10.0f : 1.0f;
        godMove (layout, selected, (float) dx * step, (float) dy * step);
        changed();
        return true;
    }
    void changed()
    {
        if (onLayoutChanged != nullptr)
            onLayoutChanged();
        repaint();
    }
    juce::String describe (const juce::String& id) const
    {
        const auto r = layout.sourceRectFor (id);
        return id + "  " + juce::String ((int) r.getX()) + " " + juce::String ((int) r.getY())
             + "  " + juce::String ((int) r.getWidth()) + "x" + juce::String ((int) r.getHeight());
    }
    void paint (juce::Graphics& g) override
    {
        if (! active)
            return;
        g.setFont (10.0f);
        if (selected.isNotEmpty())
        {
            g.setColour (juce::Colours::yellow);
            g.drawRect (editorRectOf (selected), 1.0f);
        }
        if (hovered.isNotEmpty())
        {
            g.setColour (juce::Colours::cyan);
            g.drawRect (editorRectOf (hovered), 1.0f);
        }
        const auto readout = hovered.isNotEmpty() ? describe (hovered)
                           : selected.isNotEmpty() ? describe (selected)
                                                   : juce::String ("GOD MODE");
        auto bar = juce::Rectangle<int> (0, getHeight() - 24, getWidth(), 24);
        g.setColour (juce::Colours::black);
        g.fillRect (bar);
        g.setColour (juce::Colours::white);
        g.drawText (readout, bar.removeFromTop (12), juce::Justification::centredLeft, false);
        g.drawText (status, bar, juce::Justification::centredLeft, false);
    }
    UiLayout& layout;
    bool active = false, resizing = false;
    juce::String hovered, selected, status { "drag move / ctrl+drag size / arrows / ctrl+S save" };
    juce::Rectangle<float> dragStart;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GodModeOverlay)
};
}
#endif
