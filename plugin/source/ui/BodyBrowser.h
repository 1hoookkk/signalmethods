#pragma once
#include "Theme.h"
#include "../TrenchBodyRoster.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
namespace trench::ui
{
class BodyBrowser final : public juce::Component,
                          private juce::Timer
{
public:
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
    std::function<void (int)> onPreview;
    std::function<void (int)> onCommit;
    std::function<void (int)> onRestore;

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
        setVisible (false);
        const int body = highlight >= 0 && highlight < (int) rows.size()
                             ? rows[(size_t) highlight].body : -1;
        if (commit && body >= 0)
        {
            if (onCommit) onCommit (body);
        }
        else if (onRestore)
            onRestore (openedWith);
    }
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
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int row = rowAt (e.position);
        if (row < 0)
        {
            close (false);
            return;
        }
        if (rows[(size_t) row].heading)
            return;
        highlight = row;
        close (true);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY != 0.0f)
            setHighlight (highlight + (w.deltaY > 0.0f ? -1 : 1));
    }
    void paint (juce::Graphics& g) override
    {
        const auto menuRect = panelBounds();
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
        startTimer (120);
        repaint();
    }
    void timerCallback() override
    {
        stopTimer();
        if (onPreview != nullptr && highlight >= 0 && highlight < (int) rows.size())
            onPreview (rows[(size_t) highlight].body);
    }
    void layOut (int faceHeight, int faceWidth)
    {
        const auto font = rowFont();
        float widest = 0.0f;
        for (const auto& r : rows)
            widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidth (font, r.text));
        columnW = widest + kGutter + kPadRight;
        const int maxPerColumn = juce::jmax (1, (int) (((float) faceHeight - 32.0f) / kRowH));
        const int n = juce::jmax (1, (int) rows.size());
        columns = juce::jlimit (1, 3, (n + maxPerColumn - 1) / maxPerColumn);
        perColumn = (n + columns - 1) / columns;
        const float maxW = (float) faceWidth - 24.0f;
        if (columnW * (float) columns > maxW)
            columnW = maxW / (float) columns;
    }
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
        juce::String heading;
        bool userHeading = false;
        for (int i = 0; i < count; ++i)
            if (! trench::bodyIsNoFilter (i) && ! juce::File::isAbsolutePath (entries[i].base))
            {
                const juce::String category (entries[i].category);
                if (category != heading)
                {
                    rows.push_back ({ category, -1, false, true });
                    heading = category;
                }
                rows.push_back ({ trench::bodyDisplayName (i), i });
            }
        for (int i = 0; i < count; ++i)
            if (! trench::bodyIsNoFilter (i) && juce::File::isAbsolutePath (entries[i].base))
            {
                if (! userHeading)
                {
                    rows.push_back ({ "Your bodies", -1, false, true });
                    userHeading = true;
                }
                rows.push_back ({ trench::bodyDisplayName (i), i });
            }
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
