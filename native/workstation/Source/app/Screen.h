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

    juce::Rectangle<int> header, navigator, squares, live, libraryList, columnList, statusLine;
    juce::Rectangle<int> playKey, sawKey, noiseKey, writeKey, pad;
    std::array<std::array<juce::Rectangle<int>, 2>, 2> square;
    int libraryScroll = 0, columnScroll = 0;
    static constexpr int kRow = 18;

private:
    void layout();
    void paintGrid (juce::Graphics& g, juce::Rectangle<int> r, bool wide) const;
    void paintCurve (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour) const;
    void paintKey (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool on, bool enabled) const;
    void paintList (juce::Graphics& g, juce::Rectangle<int> r, const juce::StringArray& rows, int selected, int scroll, bool focused) const;
    juce::Rectangle<int> plotArea (juce::Rectangle<int> r) const;
    Session& session;
    std::vector<double> hz;
    bool dragging = false;
};
}
