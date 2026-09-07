#pragma once

#include "app/Bridge.h"
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
    juce::File webDir, interopJs;
    std::unique_ptr<juce::WebBrowserComponent> view;
};
}
