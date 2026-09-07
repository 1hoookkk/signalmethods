#pragma once

#include "LensProcessor.h"

namespace lens
{
class Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Editor (Processor& p);
    ~Editor() override;
    void paint (juce::Graphics& g) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<int> plane() const { return getLocalBounds().withTrimmedBottom (130).reduced (10); }
    juce::Rectangle<int> arc() const { return getLocalBounds().removeFromBottom (130).reduced (10, 6); }
    juce::Point<float> at (float x, float y) const;
    int dotAt (juce::Point<int> p) const;
    void paintTrajectories (juce::Graphics& g, juce::Rectangle<int> r) const;
    Processor& processor;
    int dragDot = -1;
    bool dragPuck = false;
};
}
