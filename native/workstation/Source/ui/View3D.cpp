#include "View3D.h"
#include <cmath>

namespace ws
{
namespace
{
double rad (double deg) { return deg * juce::MathConstants<double>::pi / 180.0; }
double dot (const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
}

Vec3 View3D::right() const { return { std::cos (rad (az)), -std::sin (rad (az)), 0.0 }; }
Vec3 View3D::up() const { return { std::sin (rad (az)) * std::sin (rad (el)), std::cos (rad (az)) * std::sin (rad (el)), std::cos (rad (el)) }; }
Vec3 View3D::forward() const { return { std::cos (rad (el)) * std::sin (rad (az)), std::cos (rad (el)) * std::cos (rad (az)), -std::sin (rad (el)) }; }

double View3D::scale() const
{
    const double span = std::max (hi.x - lo.x, hi.z - lo.z) * 1.35 + 0.8;
    return std::min (rect.getWidth(), rect.getHeight()) / span * zoom;
}

juce::Point<float> View3D::project (const Vec3& p) const
{
    const Vec3 c { (lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, (lo.z + hi.z) * 0.5 };
    const Vec3 q { p.x - c.x, p.y - c.y, p.z - c.z };
    const double s = scale();
    return { (float) (rect.getCentreX() + dot (q, right()) * s + panX), (float) (rect.getCentreY() - dot (q, up()) * s + panY) };
}

double View3D::depth (const Vec3& p) const { return dot (p, forward()); }

double View3D::depth01 (const Vec3& p) const
{
    double dmin = 1e18, dmax = -1e18;
    for (int i = 0; i < 8; ++i)
    {
        const Vec3 corner { (i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z };
        const double d = depth (corner);
        dmin = std::min (dmin, d);
        dmax = std::max (dmax, d);
    }
    return juce::jlimit (0.0, 1.0, (depth (p) - dmin) / std::max (1e-9, dmax - dmin));
}

bool View3D::farPlane (int axis) const
{
    const auto f = forward();
    const double component = axis == 0 ? f.x : axis == 1 ? f.y : f.z;
    return component > 0.0;
}
}
