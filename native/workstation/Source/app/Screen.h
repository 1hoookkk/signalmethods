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

    enum class View { picker, cube, stage, perform };
    View view = View::picker;
    void showView (View next);

    juce::Point<float> puckPoint() const;
    juce::Point<float> chartPoint (double f1, double f2) const;
    std::pair<double, double> formantsAt (juce::Point<int> p) const;
    int cornerAt (juce::Point<int> p) const;
    int pointAt (juce::Point<int> p) const;
    juce::Point<float> cubePoint (double x, double y, double z) const;
    juce::Rectangle<int> cell (int row, int column) const;
    juce::Point<float> peakPoint (int row) const;
    int peakAt (juce::Point<int> p) const;
    juce::Rectangle<int> pianoKey (int midi) const;
    int noteAt (juce::Point<int> p) const;
    juce::Rectangle<int> card (int index) const;

    juce::Rectangle<int> stage, picker, chart, hud, hudHead, writeKey, advance;
    std::array<juce::Rectangle<int>, 4> navigation, cornerBox, cornerTag, cornerPlot, keys, toKeys, stageTags, padTags;
    std::array<juce::Rectangle<int>, 8> cubeBox, cubeTags;
    std::array<juce::Rectangle<int>, 3> paletteTabs;
    juce::Rectangle<int> table, magnitude, morph, keyboard, status, cubeArea, depth, dropZone, keepKey;
    bool showHardware = false;
    int palette = 0;
    std::vector<int> cards() const;
    static constexpr int kLine = 20, kColumns = 6;
    static constexpr double kF1Low = 150.0, kF1High = 1200.0, kF2Low = 450.0, kF2High = 3400.0;
    struct Menu { bool open = false, cube = false; int target = -1; juce::Rectangle<int> rect; int scroll = 0; } menu;

private:
    void layout();
    juce::Point<float> cornerPoint (int corner) const;
    int menuCount() const;
    juce::String menuItem (int i) const;
    int menuItemAt (juce::Point<int> p) const;
    int menuPinned() const;
    void openMenu (int target, bool cube, juce::Rectangle<int> anchor);
    void advanceRoom();
    int cardAt (juce::Point<int> p) const;
    bool inPlane (juce::Point<int> p) const;
    std::pair<double, double> planeAt (juce::Point<int> p) const;
    void paintTabs (juce::Graphics& g) const;
    void paintPicker (juce::Graphics& g) const;
    void paintCube (juce::Graphics& g) const;
    void paintEditor (juce::Graphics& g) const;
    void paintMagnitude (juce::Graphics& g) const;
    void paintTable (juce::Graphics& g) const;
    void paintPerform (juce::Graphics& g) const;
    void paintHud (juce::Graphics& g) const;
    void paintStrip (juce::Graphics& g) const;
    void paintKeyboard (juce::Graphics& g) const;
    void paintMenu (juce::Graphics& g) const;
    void paintGhost (juce::Graphics& g) const;
    void paintCurve (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour, float width, bool fill) const;
    Session& session;
    std::vector<double> hz;
    enum class Drag { none, puck, made, card, row, peak, keyboard, cube, depth, slice, transpose } dragging = Drag::none;
    int dragStar = -1, dragRow = -1, dragColumn = -1;
    Row dragBase;
    Words dragWords {};
    bool peakEditStarted = false;
    int browserScroll = 0;
    juce::Point<int> dragOrigin, dragPoint;
};
}
