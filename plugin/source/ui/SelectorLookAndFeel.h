#pragma once
#include "Theme.h"
#include <juce_gui_basics/juce_gui_basics.h>
namespace trench::ui
{
inline juce::Component* menuParentFor (juce::Component* owner)
{
    for (auto* c = owner; c != nullptr; c = c->getParentComponent())
        if (c->getComponentID() == "face") return c;
    return nullptr;
}
class SelectorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SelectorLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colours::transparentBlack);
    }
    static constexpr int kBorder = 10;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override {}
    juce::Font getComboBoxFont (juce::ComboBox&) override { return displayFont (12.0f, true); }
    juce::Font getPopupMenuFont() override { return sheetRowFont(); }
    juce::Component* getParentComponentForMenuOptions (const juce::PopupMenu::Options& options) override
    {
        if (auto* parent = options.getParentComponent()) return parent;
        return menuParentFor (options.getTargetComponent());
    }
    int getPopupMenuBorderSize() override { return kBorder; }
    int getPopupMenuBorderSizeWithOptions (const juce::PopupMenu::Options&) override { return kBorder; }
    void drawResizableFrame (juce::Graphics&, int, int, const juce::BorderSize<int>&) override {}
    void drawPopupMenuBackgroundWithOptions (juce::Graphics& g, int width, int height, const juce::PopupMenu::Options&) override
    {
        drawSheet (g, juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced ((float) kBorder - 3.0f), 4);
    }
    void drawPopupMenuSectionHeaderWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                const juce::String& text, const juce::PopupMenu::Options&) override
    {
        drawSheetHeading (g, area.toFloat(), text);
    }
    void getIdealPopupMenuSectionHeaderSizeWithOptions (const juce::String& text, int, int& idealWidth, int& idealHeight,
                                                        const juce::PopupMenu::Options&) override
    {
        idealHeight = juce::roundToInt (kSheetRowH);
        idealWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (sheetHeadingFont(), text.toUpperCase())
                                       + kSheetGutter + kSheetPadRight + 40.0f);
    }
    void drawPopupMenuItemWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area, bool isHighlighted,
                                       const juce::PopupMenu::Item& item, const juce::PopupMenu::Options&) override
    {
        auto r = area.toFloat();
        if (item.isSeparator)
        {
            g.setColour (juce::Colour (kSheetRule));
            g.fillRect (r.getX() + kSheetGutter, r.getCentreY(), r.getWidth() - kSheetGutter - kSheetPadRight, 1.0f);
            return;
        }
        drawSheetRow (g, r, item.text, isHighlighted, item.isTicked, item.isEnabled);
        if (item.subMenu != nullptr)
        {
            const auto arrow = r.removeFromRight (kSheetPadRight + 2.0f).withSizeKeepingCentre (4.0f, 7.0f);
            juce::Path path;
            path.startNewSubPath (arrow.getX(), arrow.getY());
            path.lineTo (arrow.getRight(), arrow.getCentreY());
            path.lineTo (arrow.getX(), arrow.getBottom());
            g.setColour (juce::Colour (kSheetInk).withAlpha (item.isEnabled ? 0.8f : 0.4f));
            g.strokePath (path, juce::PathStrokeType (1.2f));
        }
    }
    void getIdealPopupMenuItemSizeWithOptions (const juce::String& text, bool isSeparator, int,
                                               int& idealWidth, int& idealHeight, const juce::PopupMenu::Options&) override
    {
        idealHeight = isSeparator ? 9 : juce::roundToInt (kSheetRowH);
        idealWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (sheetRowFont (true), text)
                                       + kSheetGutter + kSheetPadRight + 16.0f);
    }
    void drawPopupMenuUpDownArrowWithOptions (juce::Graphics& g, int width, int height, bool isScrollUpArrow,
                                              const juce::PopupMenu::Options&) override
    {
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).withSizeKeepingCentre (8.0f, 4.0f);
        juce::Path path;
        path.addTriangle (r.getX(), isScrollUpArrow ? r.getBottom() : r.getY(),
                          r.getRight(), isScrollUpArrow ? r.getBottom() : r.getY(),
                          r.getCentreX(), isScrollUpArrow ? r.getY() : r.getBottom());
        g.setColour (juce::Colour (kSheetInk));
        g.fillPath (path);
    }
};
}
