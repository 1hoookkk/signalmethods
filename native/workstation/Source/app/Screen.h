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

    juce::Rectangle<int> plane, response, libraryList, columnList, statusLine;
    juce::Rectangle<int> playKey, sawKey, noiseKey, writeKey;
    int libraryScroll = 0, columnScroll = 0;
    static constexpr int kRow = 18;

private:
    void layout();
    double columnX (double column) const;
    void paintPlane (juce::Graphics& g) const;
    void paintResponse (juce::Graphics& g) const;
    void paintCurve (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour, float width) const;
    void paintWord (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool on, bool enabled) const;
    void paintList (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, const juce::StringArray& rows, const juce::StringArray& notes, int selected, int scroll) const;
    void placeAt (juce::Point<int> p, bool select);
    Session& session;
    std::vector<double> hz;
    bool dragging = false;
};
}
