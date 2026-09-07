#pragma once

#include "Look.h"
#include "app/Audio.h"
#include "dsp/Peevers.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace hs
{
class Spectrogram : public juce::Component, public juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit Spectrogram (Audio* tap = nullptr);
    ~Spectrogram() override;

    void feed (const float* samples, int n);
    bool load (const juce::File& file);
    juce::Image shot();

    int frameCount() const { return (int) frames.size(); }
    const std::vector<float>& frameAt (int index) const { return frames[(size_t) index]; }
    const std::vector<float>& latest() const { return frames.back(); }
    void clear();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void timerCallback() override;

    Peevers peevers;
    bool logF = false, twoD = false, axes = true;
    double rate = 44100.0;
    int length = 0;
    juce::Rectangle<int> surface, strip;
    std::array<juce::Rectangle<int>, 8> hits {};
    static constexpr int kFrames = 500;

private:
    void layout();
    void words();
    void fit();
    juce::Point<float> project (float bin, float value, float depth) const;
    juce::Colour tint (int index) const;
    void grid (juce::Graphics& g) const;
    void flat (juce::Graphics& g) const;

    Audio* audio = nullptr;
    std::vector<std::vector<float>> frames;
    std::vector<float> history, pulled;
    size_t cursor = 0;
    bool fromFile = false;
    float azimuth = 0.50f, declination = 0.46f;
    juce::Point<float> centre;
    juce::Point<int> from;
    float fromAzimuth = 0.0f, fromDeclination = 0.0f;
    std::array<juce::String, 8> labels;
    juce::AudioFormatManager formats;
};
}
