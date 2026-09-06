#pragma once

#include "Session.h"

namespace hs
{
class Screen : public juce::Component
{
public:
    explicit Screen (Session& session);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& k) override;
    juce::Image shot();
    int nearestEntry (juce::Point<int> p) const;
    int nearestAnchor (juce::Point<int> p) const;
    juce::Point<float> chartPoint (const Words& words) const;

    juce::Rectangle<int> chart, response, libraryList, stripList, statusLine, qBar;
    juce::Rectangle<int> playKey, sawKey, noiseKey, writeKey;
    int libraryScroll = 0, stripScroll = 0;
    static constexpr int kRow = 18;

private:
    void layout();
    juce::Point<float> vowelPoint (double f1, double f2) const;
    void paintChart (juce::Graphics& g) const;
    void paintResponse (juce::Graphics& g) const;
    void paintCurve (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour, float width) const;
    void paintWord (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool on, bool enabled) const;
    void paintList (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::StringArray& rows, const juce::StringArray& notes, int selected, int scroll) const;
    void dragAlong (juce::Point<int> p);
    void keepVisible();
    Session& session;
    std::vector<double> hz;
    std::vector<std::array<double, 4>> libraryFormants;
    enum class Drag { none, path, q } dragging = Drag::none;
};
}
