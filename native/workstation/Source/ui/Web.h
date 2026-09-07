#pragma once

#include "app/Bridge.h"
#include "dsp/Peevers.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace hs
{
struct Web : public juce::Component, private juce::Timer
{
    Web (Session& session, juce::File webDir, juce::File interopJs);
    ~Web() override;
    void resized() override;
    std::function<void()> onSpectrogram;

    Bridge bridge;

private:
    void timerCallback() override;
    std::optional<juce::WebBrowserComponent::Resource> resource (const juce::String& path) const;
    juce::int64 newestStamp() const;
    juce::File webDir, interopJs;
    juce::int64 stamp = 0;
    int ticks = 0;
    std::atomic<bool> dirty { true };
    Peevers lpc;
    std::vector<float> tap = std::vector<float> (16384, 0.0f);
    std::unique_ptr<juce::WebBrowserComponent> view;
};
}
