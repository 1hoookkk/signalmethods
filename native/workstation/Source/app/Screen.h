#pragma once

#include "Session.h"

namespace hs
{
class Screen : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit Screen (Session& session);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& k) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    juce::Image shot();

    juce::Point<float> puckPoint() const;
    int cornerAt (juce::Point<int> p) const;
    int cardAt (juce::Point<int> p, int rail) const;
    std::vector<int> railStars (int rail) const;

    juce::Rectangle<int> stage, presetBox;
    std::array<juce::Rectangle<int>, 2> rails;
    std::array<juce::Rectangle<int>, 4> cornerBox, cornerTag;
    juce::Rectangle<int> table;
    static constexpr int kLine = 16, kColumns = 5;
    juce::Rectangle<int> cell (int row, int column) const;
    std::array<juce::Rectangle<int>, 3> keys;
    struct Menu { bool open = false; int target = -1; juce::Rectangle<int> rect; int scroll = 0; } menu;
    static constexpr int kCard = 64;

private:
    void layout();
    juce::Rectangle<int> cardRect (int rail, int index) const;
    juce::Point<float> cornerPoint (int corner) const;
    int menuCount() const;
    juce::String menuItem (int i) const;
    int menuItemAt (juce::Point<int> p) const;
    void openMenu (int target, juce::Rectangle<int> anchor);
    void paintStage (juce::Graphics& g) const;
    void paintCorners (juce::Graphics& g) const;
    void paintRails (juce::Graphics& g) const;
    void paintMenu (juce::Graphics& g) const;
    void paintTable (juce::Graphics& g) const;
    void paintCurve (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour, float width, bool fill) const;
    Session& session;
    std::vector<double> hz;
    std::array<int, 2> scroll { 0, 0 };
    enum class Drag { none, puck, rail, card, row } dragging = Drag::none;
    int dragStar = -1, dragRail = -1, dragRow = -1, dragColumn = -1;
    Row dragBase;
    juce::Point<int> dragOrigin, dragPoint;
};
}
