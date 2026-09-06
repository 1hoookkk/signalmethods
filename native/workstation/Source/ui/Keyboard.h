#pragma once

#include "app/Session.h"

namespace hs
{
struct Keyboard
{
    explicit Keyboard (Session& session);
    void layout (juce::Rectangle<int> area);
    void paint (juce::Graphics& g) const;
    juce::Rectangle<int> pianoKey (int midi) const;
    int noteAt (juce::Point<int> p) const;

    juce::Rectangle<int> area;

private:
    Session& session;
};
}
