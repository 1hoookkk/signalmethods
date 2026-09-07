#pragma once

#include "Session.h"

namespace hs
{
struct Bridge
{
    explicit Bridge (Session& session);
    juce::var state() const;
    bool dispatch (const juce::String& name, const juce::Array<juce::var>& args);
    juce::Array<juce::var> curveOf (const Words& words) const;

    std::vector<double> hz;
    Session& session;
};
}
