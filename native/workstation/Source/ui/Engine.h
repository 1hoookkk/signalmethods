#pragma once

#include "Plot.h"
#include "app/Session.h"
#include "dsp/Peevers.h"

namespace hs
{
struct Engine
{
    Engine (Session& session, const hs::plot::Curves& curves);
    void layout (juce::Rectangle<int> area);
    void paint (juce::Graphics& g);
    void paintLive (juce::Graphics& g, juce::Rectangle<int> into) const;
    void feed (const float* out, const float* in, int n);
    void silenceFor (int ms);
    bool live() const;
    int sourceAt (juce::Point<int> p) const;
    int routeAt (juce::Point<int> p) const;
    int depthAt (juce::Point<int> p) const;
    std::array<juce::String, 7> names() const;
    std::array<juce::String, 4> routeWords() const;
    static constexpr int kRouteOf[4] = { 3, 0, 1, 2 };

    juce::Rectangle<int> area, plot, label, status;
    std::array<juce::Rectangle<int>, 7> keys;
    std::array<juce::Rectangle<int>, 4> routes;
    std::array<juce::Rectangle<int>, 3> depths;
    double rate = 44100.0;

private:
    void analyse (Peevers& p, std::vector<float>& history, size_t& cursor, std::vector<float>& spectrum, const float* samples, int n);
    void spectrumCurve (juce::Graphics& g, const Peevers& p, const std::vector<float>& spectrum, juce::Colour colour, juce::Rectangle<int> into) const;
    Session& session;
    const hs::plot::Curves& curves;
    Peevers outAnalysis, inAnalysis;
    std::vector<float> outHistory, inHistory, outSpectrum, inSpectrum;
    size_t outCursor = 0, inCursor = 0;
    juce::int64 lastSound = 0, writeTime = 0;
    juce::String writeStamp;
};
}
