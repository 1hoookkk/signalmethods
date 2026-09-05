#include "Layout.h"
#include <cmath>

namespace ws
{
void Layout::compute (float Wf, float Hf)
{
    const float W = std::floor (Wf), H = std::floor (Hf);
    const float left = 208.0f, right = 432.0f, tlH = 80.0f, top = 40.0f, m = 8.0f;
    groups = { 0.0f, 0.0f, left, 8.0f + 7.0f * 16.0f + 8.0f };
    tray = { 0.0f, groups.getBottom(), left, H - tlH - groups.getBottom() };
    keyRow = { left, 0.0f, W - left - right, top };
    field = { left + m, top, W - left - right - 2.0f * m, H - tlH - top - m };
    resp = { W - right, 0.0f, right, 152.0f };
    body = { W - right, resp.getBottom(), right, 152.0f };
    square = { W - right + m + 48.0f, body.getY() + 20.0f, 112.0f, 112.0f };
    info = { W - right, body.getBottom(), right, 144.0f };
    arma = { W - right, info.getBottom(), right, H - tlH - info.getBottom() };
    tl = { 0.0f, H - tlH, W, tlH };
    tlAx = { 80.0f, H - tlH + 20.0f, W - 80.0f - m, tlH - 40.0f };
}

std::array<double, 2> Layout::toField (juce::Point<float> p) const
{
    return { ((double) p.x - field.getX() - pan[0]) / (field.getWidth() * zoom), (field.getBottom() - (double) p.y + pan[1]) / (field.getHeight() * zoom) };
}

juce::Point<float> Layout::fromField (std::array<double, 2> u) const
{
    return { (float) (field.getX() + u[0] * field.getWidth() * zoom + pan[0]), (float) (field.getBottom() - u[1] * field.getHeight() * zoom + pan[1]) };
}

float Layout::rx (double hz) const { return resp.getX() + 40.0f + (float) (std::log10 (hz / 20.0) / 3.0) * (resp.getWidth() - 56.0f); }
float Layout::ry (double db) const { return resp.getY() + 24.0f + (float) ((30.0 - db) / 60.0) * (resp.getHeight() - 48.0f); }
float Layout::tx (double t) const { return tlAx.getX() + (float) (t / duration) * tlAx.getWidth(); }
double Layout::tAt (float x) const { return juce::jlimit (0.0, duration, (x - tlAx.getX()) / tlAx.getWidth() * duration); }

juce::Rectangle<float> Layout::cascadeRect() const { return { field.getX() + 40.0f, field.getY() + 24.0f, field.getWidth() - 48.0f, 120.0f }; }
juce::Rectangle<float> Layout::stageRect (int s) const
{
    const float top = field.getY() + 168.0f, h = std::floor ((field.getHeight() - 184.0f) / 6.0f);
    return { field.getX() + 304.0f, top + s * h, field.getWidth() - 312.0f, h - 8.0f };
}
float Layout::sx (juce::Rectangle<float> r, double hz) const { return r.getX() + (float) (std::log10 (hz / 20.0) / 3.0) * r.getWidth(); }
float Layout::sy (juce::Rectangle<float> r, double db) const { return r.getY() + (float) ((30.0 - db) / 60.0) * r.getHeight(); }
double Layout::hzAt (juce::Rectangle<float> r, float x) const { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) (x - r.getX()) / r.getWidth())); }
double Layout::dbAt (juce::Rectangle<float> r, float y) const { return 30.0 - juce::jlimit (0.0, 1.0, (double) (y - r.getY()) / r.getHeight()) * 60.0; }

juce::Point<float> Layout::armaXY (double hz, double r) const
{
    const double oct = juce::jlimit (0.0, 10.0, std::log2 (std::max (hz, 20.0) / 20.0));
    const double th = juce::MathConstants<double>::pi - juce::MathConstants<double>::pi * oct / 10.0;
    const double db = std::min (60.0, 20.0 * std::log10 (1.0 / std::max (1.0 - r, 1e-3)));
    const float R = std::floor (std::min (arma.getWidth() * 0.5f - 40.0f, arma.getHeight() - 40.0f));
    const float cx = std::floor (arma.getCentreX()), cy = std::floor (arma.getY() + 24.0f + R);
    return { cx + (float) (db / 60.0 * R * std::cos (th)), cy - (float) (db / 60.0 * R * std::sin (th)) };
}
}
