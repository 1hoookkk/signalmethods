#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>

namespace ws
{
enum class Room { frames, edit, sound };

struct Layout
{
    juce::Rectangle<float> tray, field, resp, body, square, arma, cascade, tl, tlAx, sortRow, rooms;
    double zoom = 1.0;
    double pan[2] { 0.0, 0.0 };
    double duration = 8.0;
    Room room = Room::frames;
    bool timelineOpen = false;

    void compute (float width, float height);
    std::array<double, 2> toField (juce::Point<float> p) const;
    juce::Point<float> fromField (std::array<double, 2> u) const;
    float rx (double hz) const;
    float ry (double db) const;
    float tx (double t) const;
    double tAt (float x) const;
    juce::Point<float> armaXY (double hz, double r) const;
    juce::Rectangle<float> stageRect (int s) const;
    float sx (juce::Rectangle<float> r, double hz) const;
    float sy (juce::Rectangle<float> r, double db) const;
    double hzAt (juce::Rectangle<float> r, float x) const;
    double dbAt (juce::Rectangle<float> r, float y) const;
};
}
