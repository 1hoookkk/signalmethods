#include "Layout.h"
#include <cmath>

namespace ws
{
void Layout::compute (float W, float H)
{
    const float left = 200.0f, right = 300.0f, tlH = 76.0f, top = 36.0f;
    groups = { 0.0f, 0.0f, left, 106.0f };
    tray = { 0.0f, 106.0f, left, H - tlH - 106.0f };
    keyRow = { left, 0.0f, W - left - right, top };
    field = { left, top, W - left - right, H - tlH - top };
    resp = { W - right, 0.0f, right, 190.0f };
    info = { W - right, 190.0f, right, H - tlH - 190.0f };
    tl = { 0.0f, H - tlH, W, tlH };
    tlAx = { 70.0f, H - tlH + 12.0f, W - 80.0f, tlH - 30.0f };
}

std::array<double, 2> Layout::toField (juce::Point<float> p) const
{
    return { ((double) p.x - field.getX() - pan[0]) / (field.getWidth() * zoom), (field.getBottom() - (double) p.y + pan[1]) / (field.getHeight() * zoom) };
}

juce::Point<float> Layout::fromField (std::array<double, 2> u) const
{
    return { (float) (field.getX() + u[0] * field.getWidth() * zoom + pan[0]), (float) (field.getBottom() - u[1] * field.getHeight() * zoom + pan[1]) };
}

float Layout::rx (double hz) const { return resp.getX() + 34.0f + (float) (std::log10 (hz / 20.0) / 3.0) * (resp.getWidth() - 44.0f); }
float Layout::ry (double db) const { return resp.getY() + 12.0f + (float) ((30.0 - db) / 60.0) * (resp.getHeight() - 32.0f); }
float Layout::tx (double t) const { return tlAx.getX() + (float) (t / duration) * tlAx.getWidth(); }
double Layout::tAt (float x) const { return juce::jlimit (0.0, duration, (x - tlAx.getX()) / tlAx.getWidth() * duration); }
}
