#pragma once

#include "LensProcessor.h"
#include "dsp/Peevers.h"

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
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void analyse();
    juce::Rectangle<int> surface() const { return getLocalBounds().withTrimmedBottom (getHeight() - 250).reduced (10).withTrimmedTop (10); }
    juce::Rectangle<int> response() const { return juce::Rectangle<int> (44, 262, getWidth() - 60, 200); }
    juce::Rectangle<int> controls() const { return getLocalBounds().removeFromBottom (getHeight() - 474).reduced (10, 6); }
    double hzOfY (int y, juce::Rectangle<int> r) const;
    float yOfHz (double hz, juce::Rectangle<int> r) const;
    float xOfHz (double hz, juce::Rectangle<int> r) const;
    double hzOfX (int x, juce::Rectangle<int> r) const;
    int rowNear (double hz) const;
    int poleRowAt (juce::Point<int> p) const;
    void paintSurface (juce::Graphics& g, juce::Rectangle<int> r);
    void paintResponse (juce::Graphics& g, juce::Rectangle<int> r);
    void paintControls (juce::Graphics& g, juce::Rectangle<int> r);

    Processor& processor;
    hs::Peevers spec, envelope;
    std::vector<float> history, tap;
    size_t cursor = 0;
    juce::Image image;
    int column = 0;
    std::vector<float> latest, latestEnvelope;
    std::vector<double> grid;
    int dragRow = -1;
    bool dragSlider = false;
    int lastStruck = 0;
    juce::int64 struckAt = 0;
};
}
