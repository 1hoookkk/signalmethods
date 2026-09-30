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
    explicit BodyBrowser (const Theme& theme) : t (theme)
    {
        setInterceptsMouseClicks (true, false);
        setWantsKeyboardFocus (true);
        setTitle ("Bodies");
    }
    struct Row { juce::String text; int body; bool ticked = false; bool heading = false; juce::String detail; };
    std::function<std::vector<Row>()> rowSource;
    std::function<void (int)> onPreview;
    std::function<void (int)> onCommit;
    std::function<void (int)> onRestore;

    void open (int currentBodyIndex, juce::Rectangle<int> faceLocal, juce::Rectangle<int> anchor)
    {
        current = openedWith = currentBodyIndex;
        buildRows();
        const auto room = faceLocal.reduced (kMargin);
        const int below = room.getBottom() - (anchor.getBottom() + kDrop);
        const bool dropDown = layOut (below, room.getWidth(), anchor.getWidth());
        if (! dropDown)
            layOut (room.getHeight(), room.getWidth(), anchor.getWidth());
        highlight = 0;
        for (int r = 0; r < (int) rows.size(); ++r)
            if (isTicked (r))
            {
                highlight = r;
                break;
            }
        const int w = juce::roundToInt (columnW * (float) columns) + 2 * kInset;
        const int h = juce::roundToInt (kSheetRowH * (float) perColumn) + 2 * kInset;
        const int x = juce::jlimit (room.getX(), juce::jmax (room.getX(), room.getRight() - w), anchor.getX());
        const int y = dropDown ? anchor.getBottom() + kDrop : juce::jmax (room.getY(), anchor.getY() - kDrop);
        panel = juce::Rectangle<float> ((float) x, (float) juce::jlimit (room.getY(), juce::jmax (room.getY(), room.getBottom() - h), y),
                                        (float) w, (float) h);
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
        setHighlight (highlight + step, true);
        return true;
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int row = rowAt (e.position);
        if (row >= 0 && ! rows[(size_t) row].heading && row != highlight)
        {
            highlight = row;
            repaint();
        }
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
            setHighlight (highlight + (w.deltaY > 0.0f ? -1 : 1), true);
    }
    void paint (juce::Graphics& g) override
    {
        const auto menuRect = panelBounds();
        drawSheet (g, menuRect);
        const auto inner = menuRect.reduced ((float) kInset);
        g.setColour (juce::Colour (kSheetRule));
        for (int c = 1; c < columns; ++c)
            g.fillRect (inner.getX() + columnW * (float) c, inner.getY() + 4.0f, 1.0f, inner.getHeight() - 8.0f);
        for (int r = 0; r < (int) rows.size(); ++r)
        {
            const auto cell = cellFor (inner, r);
            if (rows[(size_t) r].heading)
                drawSheetHeading (g, cell, rows[(size_t) r].text, rows[(size_t) r].detail);
            else
                drawSheetRow (g, cell, rows[(size_t) r].text, r == highlight, isTicked (r), true, rows[(size_t) r].detail);
        }
    }
private:
    static constexpr int kMargin = 6, kDrop = 3, kInset = 4;
    bool isTicked (int r) const
    {
        return rowSource != nullptr ? rows[(size_t) r].ticked : rows[(size_t) r].body == current;
    }
    juce::Rectangle<float> panelBounds() const { return panel; }
    juce::Rectangle<float> cellFor (juce::Rectangle<float> inner, int row) const
    {
        const int col = row / juce::jmax (1, perColumn);
        const int idx = row % juce::jmax (1, perColumn);
        return { inner.getX() + (float) col * columnW,
                 inner.getY() + (float) idx * kSheetRowH, columnW, kSheetRowH };
    }
    int rowAt (juce::Point<float> p) const
    {
        const auto inner = panelBounds().reduced ((float) kInset);
        if (! inner.contains (p))
            return -1;
        const int col = (int) ((p.x - inner.getX()) / columnW);
        const int idx = (int) ((p.y - inner.getY()) / kSheetRowH);
        const int r = col * perColumn + idx;
        return r >= 0 && r < (int) rows.size() ? r : -1;
    }
    void setHighlight (int r, bool preview)
    {
        r = juce::jlimit (0, juce::jmax (0, (int) rows.size() - 1), r);
        const int dir = r >= highlight ? 1 : -1;
        while (r > 0 && r < (int) rows.size() - 1 && rows[(size_t) r].heading)
            r += dir;
        if (rows[(size_t) r].heading || r == highlight)
            return;
        highlight = r;
        if (preview) startTimer (120);
        repaint();
    }
    void timerCallback() override
    {
        stopTimer();
        if (onPreview != nullptr && highlight >= 0 && highlight < (int) rows.size())
            onPreview (rows[(size_t) highlight].body);
    }
    bool layOut (int availableHeight, int availableWidth, int anchorWidth)
    {
        float widest = 0.0f, widestDetail = 0.0f;
        for (const auto& r : rows)
        {
            widest = juce::jmax (widest, juce::GlyphArrangement::getStringWidth (r.heading ? sheetHeadingFont() : sheetRowFont (true),
                                                                                 r.heading ? r.text.toUpperCase() : r.text));
            if (r.detail.isNotEmpty() && ! r.heading)
                widestDetail = juce::jmax (widestDetail, juce::GlyphArrangement::getStringWidth (sheetDetailFont(), r.detail) + 14.0f);
        }
        columnW = widest + widestDetail + kSheetGutter + kSheetPadRight;
        const int maxPerColumn = juce::jmax (1, (int) (((float) availableHeight - 2.0f * kInset) / kSheetRowH));
        const int n = juce::jmax (1, (int) rows.size());
        const float maxW = (float) availableWidth - 2.0f * kInset;
        columns = juce::jmax (1, (n + maxPerColumn - 1) / maxPerColumn);
        perColumn = (n + columns - 1) / columns;
        const auto orphanHeading = [this, n]
        {
            for (int end = perColumn; end < n; end += perColumn)
                if (rows[(size_t) end - 1].heading)
                    return true;
            return false;
        };
        while (columns > 1 && perColumn < maxPerColumn && orphanHeading())
            ++perColumn;
        if (columns == 1)
            columnW = juce::jmax (columnW, (float) anchorWidth - 2.0f * kInset);
        const bool fits = columnW * (float) columns <= maxW;
        if (! fits)
            columnW = maxW / (float) columns;
        return fits;
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
                rows.push_back ({ trench::bodyDisplayName (i), i, false, false,
                                  entries[i].poles > 0 ? juce::String (entries[i].poles) + " pole" : juce::String() });
            }
        for (int i = 0; i < count; ++i)
            if (! trench::bodyIsNoFilter (i) && juce::File::isAbsolutePath (entries[i].base))
            {
                if (! userHeading)
                {
                    rows.push_back ({ "User", -1, false, true });
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
