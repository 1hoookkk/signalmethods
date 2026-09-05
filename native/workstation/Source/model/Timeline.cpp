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

std::optional<Spot> Timeline::pathAt (double t, const Stitch& st) const
{
    if (keys.empty()) return std::nullopt;
    const auto ks = sorted();
    if (t <= ks.front().t) return ks.front().spot;
    if (t >= ks.back().t) return ks.back().spot;
    for (size_t i = 0; i + 1 < ks.size(); ++i)
        if (t >= ks[i].t && t <= ks[i + 1].t)
            return st.lerp (ks[i].spot, ks[i + 1].spot, (t - ks[i].t) / std::max (1e-9, ks[i + 1].t - ks[i].t));
    return std::nullopt;
}
}
