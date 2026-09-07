#pragma once

#include "Look.h"
#include "app/Session.h"
#include "dsp/Peevers.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <set>
#include <vector>

namespace hs
{
class Spectrogram : public juce::Component, public juce::Timer, public juce::FileDragAndDropTarget
{
public:
    explicit Spectrogram (Audio* tap = nullptr, Session* owner = nullptr);
    ~Spectrogram() override;

    void feed (const float* samples, int n);
    bool load (const juce::File& file);
    juce::Image shot();

    int frameCount() const { return (int) frames.size(); }
    const std::vector<float>& frameAt (int index) const { return frames[(size_t) index]; }
    const std::vector<float>& latest() const { return frames.back(); }
    void clear();
    int pickAt (juce::Point<int> p) const;
    juce::String frameName (int index) const;
    bool takeFrame();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& k) override;
    bool keyStateChanged (bool isKeyDown) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void timerCallback() override;

    Peevers peevers;
    bool logF = false, twoD = false, axes = true, persp = false, mesh = true, paused = false;
    double rate = 44100.0;
    int length = 0, kept = 500, pickedFrame = -1;
    juce::Rectangle<int> surface, panel;
    static constexpr int kControls = 15;
    static constexpr int kFrames = 500;
    std::array<juce::Rectangle<int>, kControls> hits {};

private:
    void layout();
    void words();
    void fit();
    void trim();
    void chooseFamily();
    juce::Point<float> project (float bin, float value, float depth) const;
    juce::Colour tint (int index) const;
    void grid (juce::Graphics& g) const;
    void flat (juce::Graphics& g) const;
    void slice (juce::Graphics& g, int index, int count, juce::Colour colour) const;
    void controls (juce::Graphics& g) const;

    Audio* audio = nullptr;
    Session* session = nullptr;
    std::vector<std::vector<float>> frames, powers;
    std::vector<float> history, pulled, raw;
    size_t cursor = 0;
    bool fromFile = false;
    juce::String fileName;
    float azimuth = 0.22f, declination = 0.62f;
    juce::Point<float> centre;
    juce::Point<int> from;
    float fromAzimuth = 0.0f, fromDeclination = 0.0f;
    std::array<juce::String, kControls> labels;
    std::set<int> held;
    juce::AudioFormatManager formats;
};
}
