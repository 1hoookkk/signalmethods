#pragma once
#include "Theme.h"
#include "../TrenchBodyRoster.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
namespace trench::ui
{
// THE BODY LIST - the E-mu filter menu, as it actually looked.
//
// Tyson's reference (2026-07-24 screenshot, ruled final 2026-08-04): a plain
// light popup that breaks into COLUMNS so the whole roster is on screen at
// once. No scrolling, no headings, no submenus, no accent colour, no bevel -
// a thin border, tight rows of small dark type, and a checkmark in the gutter
// against the one that is loaded. The palette is the face's own '95 menu
// (SectionRail::LightMenuLnF): bone field, graphite ink.
//
// It stays a component rather than a juce::PopupMenu so the audition flow and
// the offline PNG harness both keep working.
//
// Order is the roster's own: the shipped bodies in menu law (tame first,
// craziest last), the user's own bodies after them.
class BodyBrowser final : public juce::Component,
                          private juce::Timer
{
public:
    // the face's own light-menu palette - do not invent a second one
    static constexpr juce::uint32 kField = 0xfffcfcfd, kBorder = 0xff5a5750,
                                  kHighlight = 0xff3cc8be, kInk = 0xff2a2722;

    explicit BodyBrowser (const Theme& theme) : t (theme)
    {
        setInterceptsMouseClicks (true, false);
        setWantsKeyboardFocus (true);
        setTitle ("Bodies");
    }
    struct Row { juce::String text; int body; bool ticked = false; bool heading = false; };
    std::function<std::vector<Row>()> rowSource;
    std::function<void (int)> onPreview;   // hover/arrow: ear only, no parameter
    std::function<void (int)> onCommit;    // click/Enter: this is the body
    std::function<void (int)> onRestore;   // cancel: put the old one back, no travel

    /// The list is an overlay ON the face (2026-08-06): no desktop window. The
    /// separate window mis-positioned and mis-scaled under host scaling and
    /// read as a beige ghost of the plate. The component covers the whole face
    /// so a click outside the list still dismisses it; `face` is the editor's
    /// local bounds.
    void open (int currentBodyIndex, juce::Rectangle<int> faceLocal)
    {
        current = openedWith = currentBodyIndex;
        buildRows();
        layOut (faceLocal.getHeight(), faceLocal.getWidth());
        highlight = 0;
        for (int r = 0; r < (int) rows.size(); ++r)
            if (isTicked (r))
            {
                highlight = r;
                break;
            }
        const int w = juce::roundToInt (columnW * (float) columns) + 2;
        const int h = juce::roundToInt (kRowH * (float) perColumn) + 2;
        panel = juce::Rectangle<float> ((float) w, (float) h)
                    .withCentre (faceLocal.getCentre().toFloat());
        setBounds (faceLocal);
        setVisible (true);
        toFront (true);
        grabKeyboardFocus();
        repaint();
    }
    void close (bool commit)
    {
        stopTimer();
        setVisible (false);           // the list gets out of the way FIRST, so the
        const int body = highlight >= 0 && highlight < (int) rows.size()
                             ? rows[(size_t) highlight].body : -1;
        if (commit && body >= 0)
        {
            if (onCommit) onCommit (body);   // ...4-second travel is watched, not covered
        }
        else if (onRestore)
            onRestore (openedWith);
    }
    /// Face-shot proof hooks.
    juce::String debugState() const
    {
        return "current=" + juce::String (current) + " highlight=" + juce::String (highlight)
             + " rows=" + juce::String ((int) rows.size())
             + " columns=" + juce::String (columns) + " perColumn=" + juce::String (perColumn)
             + " scrolled=no";
    }
    juce::StringArray dumpOrder() const
    {
        juce::StringArray out;
        for (const auto& r : rows)
            out.add (r.text);
        return out;
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { close (false); return true; }
        if (k == juce::KeyPress::returnKey) { close (true);  return true; }
        const int step = k == juce::KeyPress::upKey    ? -1
                       : k == juce::KeyPress::downKey  ?  1
                       : k == juce::KeyPress::leftKey  ? -perColumn
                       : k == juce::KeyPress::rightKey ?  perColumn : 0;
        if (step == 0)
            return false;
        setHighlight (highlight + step);
        return true;
    }
    void mouseMove (const juce::MouseEvent&) override
    {
        // Hover no longer switches presets. Arrow keys and mouse wheel still
        // walk the list; click/Enter commits.
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int row = rowAt (e.position);
        if (row < 0)
        {
            close (false);            // clicked off the list: nothing chosen
            return;
        }
        if (rows[(size_t) row].heading)
            return;
        highlight = row;
        close (true);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY != 0.0f)         // nothing scrolls; the wheel walks the list
            setHighlight (highlight + (w.deltaY > 0.0f ? -1 : 1));
    }
    void paint (juce::Graphics& g) override
    {
        const auto menuRect = panelBounds();
        // Opaque clean field (2026-08-06): the plate bleeding through read as a
        // beige tint. White panels are locked - the list is one of them.
        g.setColour (juce::Colour (kField));
        g.fillRect (menuRect);
        g.setColour (juce::Colour (kBorder));
        g.drawRect (menuRect, 1.0f);
        const auto inner = menuRect.reduced (1.0f);
        g.setFont (rowFont());
        for (int r = 0; r < (int) rows.size(); ++r)
        {
            const auto cell = cellFor (inner, r);
            if (rows[(size_t) r].heading)
            {
                g.setFont (displayFont (10.5f, true));
                g.setColour (juce::Colour (kInk).withAlpha (0.55f));
                g.drawText (rows[(size_t) r].text.toUpperCase(), cell.withTrimmedLeft (kGutter).toNearestInt(),
                            juce::Justification::centredLeft, false);
                g.setFont (rowFont());
                continue;
            }
            const bool hot = r == highlight;
            if (hot)
            {
                g.setColour (juce::Colour (kHighlight).withAlpha (0.16f));
                g.fillRect (cell);
                g.setColour (juce::Colour (kHighlight).withAlpha (0.85f));
                g.fillRect (cell.withWidth (2.0f).translated (1.0f, 0.0f));
            }
            if (isTicked (r))
            {
                // the reference's one mark: a checkmark in the gutter
                juce::Path check;
                const float cx = cell.getX() + 8.0f, cy = cell.getCentreY();
                check.startNewSubPath (cx - 3.0f, cy);
                check.lineTo (cx - 1.0f, cy + 2.5f);
                check.lineTo (cx + 3.5f, cy - 3.0f);
                g.setColour (juce::Colour (kInk));
                g.strokePath (check, { 1.3f, juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded });
            }
            g.setColour (juce::Colour (kInk));
            g.drawText (rows[(size_t) r].text,
                        cell.withTrimmedLeft (kGutter).toNearestInt(),
                        juce::Justification::centredLeft, false);
        }
    }
private:
    bool isTicked (int r) const
    {
        return rowSource != nullptr ? rows[(size_t) r].ticked : rows[(size_t) r].body == current;
    }
    static constexpr float kRowH = 20.0f, kGutter = 16.0f, kPadRight = 12.0f;
    // Plain Arial (Tyson 2026-08-05: "plain arial"). 13.5pt so the roster is
    // legible at the 352px face ("still reading too small", 2026-08-06).
    static juce::Font rowFont()
    {
        return displayFont (13.5f, false);
    }
    juce::Rectangle<float> panelBounds() const { return panel; }
    juce::Rectangle<float> cellFor (juce::Rectangle<float> inner, int row) const
    {
        const int col = row / juce::jmax (1, perColumn);
        const int idx = row % juce::jmax (1, perColumn);
        return { inner.getX() + (float) col * columnW,
                 inner.getY() + (float) idx * kRowH, columnW, kRowH };
    }
    int rowAt (juce::Point<float> p) const
    {
        const auto inner = panelBounds().reduced (1.0f);
        if (! inner.contains (p))
            return -1;
        const int col = (int) ((p.x - inner.getX()) / columnW);
        const int idx = (int) ((p.y - inner.getY()) / kRowH);
        const int r = col * perColumn + idx;
        return r >= 0 && r < (int) rows.size() ? r : -1;
    }
    void setHighlight (int r)
    {
        r = juce::jlimit (0, juce::jmax (0, (int) rows.size() - 1), r);
        const int dir = r >= highlight ? 1 : -1;
        while (r > 0 && r < (int) rows.size() - 1 && rows[(size_t) r].heading)
            r += dir;
        if (rows[(size_t) r].heading || r == highlight)
            return;
        highlight = r;
        startTimer (120);             // audition, debounced
        repaint();
    }
    void timerCallback() override
    {
        stopTimer();
        if (onPreview != nullptr && highlight >= 0 && highlight < (int) rows.size())
            onPreview (rows[(size_t) highlight].body);
    }
    // COLUMNS, NOT SCROLLING: the whole roster is on screen at once, and the
    // list runs a little WIDER than the plugin - a menu overhangs its host.
    // Columns are added until the list is at least that much wider than the
    // face, which also keeps it shorter than the face (the reference's shape).
    void layOut (int faceHeight, int faceWidth)
    {
        const auto font = rowFont();
        float widest = 0.0f;
        for (const auto& r : rows)
            widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidth (font, r.text));
        columnW = widest + kGutter + kPadRight;
        const int maxPerColumn = juce::jmax (1, (int) (((float) faceHeight - 32.0f) / kRowH));
        const int n = juce::jmax (1, (int) rows.size());
        // Never wider than the face ("so wide", 2026-08-06): up to 3 columns,
        // shrunk to fit inside it, no forced overhang.
        columns = juce::jlimit (1, 3, (n + maxPerColumn - 1) / maxPerColumn);
        perColumn = (n + columns - 1) / columns;
        const float maxW = (float) faceWidth - 24.0f;
        if (columnW * (float) columns > maxW)
            columnW = maxW / (float) columns;
    }
    // The roster is ALREADY authored tame -> craziest (PresetRoster.inc's menu
    // law), so the list is simply the roster: "No filter" first, the shipped
    // bodies in their authored order, the user's own bodies after them.
    void buildRows()
    {
        rows.clear();
        if (rowSource != nullptr)
        {
            rows = rowSource();
            return;
        }
        int count = 0;
        const auto* entries = trench::bodyRoster (count);
        rows.push_back ({ trench::kNoFilterName, trench::kNoFilterIndex });
        for (int i = 0; i < count; ++i)
            if (! trench::bodyIsNoFilter (i) && ! juce::File::isAbsolutePath (entries[i].base))
                rows.push_back ({ trench::bodyDisplayName (i), i });
        for (int i = 0; i < count; ++i)
            if (! trench::bodyIsNoFilter (i) && juce::File::isAbsolutePath (entries[i].base))
                rows.push_back ({ trench::bodyDisplayName (i), i });
    }
    Theme t;
    juce::Rectangle<float> panel;
    std::vector<Row> rows;
    int highlight = 0, current = 0, openedWith = 0;
    int perColumn = 1, columns = 1;
    float columnW = 140.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BodyBrowser)
};
}
