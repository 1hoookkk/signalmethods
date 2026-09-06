#pragma once

#include "app/Session.h"

namespace hs
{
struct PinMenu
{
    void show (const Session& session, int target, bool cube, juce::Rectangle<int> anchor, juce::Rectangle<int> bounds);
    int itemAt (const Session& session, juce::Point<int> p) const;
    int pinned (const Session& session) const;
    void wheel (const Session& session, int step);
    void paint (juce::Graphics& g, const Session& session) const;

    bool open = false, cube = false;
    int target = -1, scroll = 0;
    juce::Rectangle<int> rect;
};
}
