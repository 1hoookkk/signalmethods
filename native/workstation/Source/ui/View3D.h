#pragma once

#include <juce_graphics/juce_graphics.h>
#include "../model/Stitch.h"

namespace ws
{
struct View3D
{
    double az = -37.5, el = 30.0, zoom = 1.0;
    double panX = 0.0, panY = 0.0;
    juce::Rectangle<float> rect;
    Vec3 lo { -0.65, -0.65, -0.2 }, hi { 0.65, 0.65, 6.6 };

    juce::Point<float> project (const Vec3& p) const;
    bool unproject (juce::Point<float> s, double z, double& x, double& y) const;
    double depth (const Vec3& p) const;
    double depth01 (const Vec3& p) const;
    bool farPlane (int axis) const;
    Vec3 right() const;
    Vec3 up() const;
    Vec3 forward() const;
    double scale() const;
};
}
