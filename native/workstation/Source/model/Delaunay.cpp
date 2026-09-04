#include "Delaunay.h"
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>

namespace ws
{
std::vector<Triangle> delaunay (const std::vector<std::array<double, 2>>& pts)
{
    struct P { double x, y; };
    std::vector<P> p;
    juce::Random rng (7);
    for (const auto& q : pts) p.push_back ({ q[0] + (rng.nextDouble() - 0.5) * 2e-6, q[1] + (rng.nextDouble() - 0.5) * 2e-6 });
    const int n = (int) p.size();
    if (n < 3) return {};
    p.push_back ({ -10.0, -10.0 });
    p.push_back ({ 10.0, -10.0 });
    p.push_back ({ 0.0, 10.0 });
    struct T { int a, b, c; double cx, cy, r2; bool bad; };
    const auto circum = [&] (int a, int b, int c) -> T
    {
        const double ax = p[(size_t) a].x, ay = p[(size_t) a].y, bx = p[(size_t) b].x, by = p[(size_t) b].y, cx = p[(size_t) c].x, cy = p[(size_t) c].y;
        const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
        if (std::abs (d) < 1e-18) return { a, b, c, 0.0, 0.0, -1.0, false };
        const double ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d;
        const double uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d;
        return { a, b, c, ux, uy, (ax - ux) * (ax - ux) + (ay - uy) * (ay - uy), false };
    };
    std::vector<T> tris { circum (n, n + 1, n + 2) };
    for (int i = 0; i < n; ++i)
    {
        std::vector<std::pair<int, int>> edges;
        for (auto& t : tris)
        {
            const double dx = p[(size_t) i].x - t.cx, dy = p[(size_t) i].y - t.cy;
            t.bad = t.r2 >= 0.0 && dx * dx + dy * dy < t.r2;
            if (t.bad)
            {
                edges.push_back ({ t.a, t.b });
                edges.push_back ({ t.b, t.c });
                edges.push_back ({ t.c, t.a });
            }
        }
        tris.erase (std::remove_if (tris.begin(), tris.end(), [] (const T& t) { return t.bad; }), tris.end());
        for (size_t e = 0; e < edges.size(); ++e)
        {
            bool shared = false;
            for (size_t f = 0; f < edges.size(); ++f)
                if (e != f && ((edges[e].first == edges[f].first && edges[e].second == edges[f].second) || (edges[e].first == edges[f].second && edges[e].second == edges[f].first)))
                { shared = true; break; }
            if (! shared) tris.push_back (circum (edges[e].first, edges[e].second, i));
        }
    }
    std::vector<Triangle> out;
    for (const auto& t : tris)
        if (t.a < n && t.b < n && t.c < n) out.push_back ({ t.a, t.b, t.c });
    return out;
}
}
