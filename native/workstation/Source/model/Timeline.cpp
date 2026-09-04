#include "Timeline.h"
#include <algorithm>

namespace ws
{
std::vector<Key> Timeline::sorted() const
{
    auto ks = keys;
    std::sort (ks.begin(), ks.end(), [] (const Key& a, const Key& b) { return a.t < b.t; });
    return ks;
}

std::optional<std::array<double, 2>> Timeline::pathAt (double t) const
{
    if (keys.empty()) return std::nullopt;
    const auto ks = sorted();
    if (t <= ks.front().t) return ks.front().p;
    if (t >= ks.back().t) return ks.back().p;
    for (size_t i = 0; i + 1 < ks.size(); ++i)
        if (t >= ks[i].t && t <= ks[i + 1].t)
        {
            const double f = (t - ks[i].t) / std::max (1e-9, ks[i + 1].t - ks[i].t);
            return std::array<double, 2> { ks[i].p[0] + (ks[i + 1].p[0] - ks[i].p[0]) * f, ks[i].p[1] + (ks[i + 1].p[1] - ks[i].p[1]) * f };
        }
    return std::nullopt;
}
}
