#pragma once

#include "Body.h"
#include "Engine.h"
#include "Keyboard.h"
#include "Mother.h"
#include "Palette.h"
#include "PinMenu.h"
#include "Plot.h"
#include "Stage.h"
#include "app/Session.h"
#include <set>

namespace hs
{
class Screen : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit Screen (Session& session);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& k) override;
    bool keyStateChanged (bool isKeyDown) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    juce::Image shot();

    std::function<void()> onSpectrogram;
    plot::Curves curves;
    Palette palette;
    Mother mother;
    Stage stage;
    Body body;
    Engine engine;
    Keyboard keyboard;
    PinMenu menu;
    juce::Rectangle<int> bottom;

private:
    void layout();
    void paintGhost (juce::Graphics& g) const;
    void timerCallback() override;
    Session& session;
    std::vector<float> tapOut, tapIn;
    enum class Drag { none, puck, made, card, peak, zero, blade, transpose, carve, keyboard, rail, sound } dragging = Drag::none;
    int dragStar = -1, dragRow = -1;
    Words dragWords {};
    bool editStarted = false;
    juce::int64 wheelTime = 0;
    juce::Point<int> dragOrigin, dragPoint;
    std::set<int> heldKeys;
};
}
