#pragma once

#include "Session.h"

namespace hs
{
class Screen : public juce::Component
{
public:
    explicit Screen (Session& session);
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& k) override;
    juce::Image shot();
    int nearestStar (juce::Point<float> p, float within) const;
    juce::Point<float> starPoint (int k) const;
    juce::Point<float> puckPoint (double morph, double q) const;

    juce::Rectangle<int> map, response, statusLine;
    juce::Rectangle<int> playKey, sawKey, noiseKey, writeKey;
    std::array<juce::Rectangle<int>, 4> chips;

private:
    void layout();
    juce::Point<float> vowelPoint (double f1, double f2) const;
    juce::Point<float> pinPoint (int n) const;
    void paintMap (juce::Graphics& g);
    void paintQuad (juce::Graphics& g);
    void paintResponse (juce::Graphics& g) const;
    void paintCurve (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour, float width) const;
    void paintWord (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool on, bool enabled) const;
    void puckFrom (juce::Point<float> p);
    void refreshHeat();
    Session& session;
    std::vector<double> hz;
    std::vector<juce::Point<float>> points;
    std::vector<double> heat;
    std::array<int, 4> heatPins { -2, -2, -2, -2 };
    static constexpr int kHeat = 9;
    enum class Drag { none, pin, puck, frame } dragging = Drag::none;
    int dragPin = -1;
    juce::Point<float> dragOffset, dragStart;
    std::array<juce::Point<float>, 4> dragPoints;
};
}
