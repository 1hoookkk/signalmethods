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
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }

private:
    void timerCallback() override;
    juce::String svg() const;
    Processor& processor;
    std::unique_ptr<juce::Drawable> drawing;
    unsigned int seen = 0;
};
}
