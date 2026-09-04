#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>

namespace ws
{
struct Layout
{
    juce::Rectangle<float> groups, tray, field, resp, body, square, info, arma, tl, tlAx, keyRow;
    double zoom = 1.0;
    double pan[2] { 0.0, 0.0 };
    double duration = 8.0;

    void compute (float width, float height);
    std::array<double, 2> toField (juce::Point<float> p) const;
    juce::Point<float> fromField (std::array<double, 2> u) const;
    float rx (double hz) const;
    float ry (double db) const;
    float tx (double t) const;
    double tAt (float x) const;
    juce::Point<float> armaXY (double hz, double r) const;
};
}
