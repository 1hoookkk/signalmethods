#pragma once

#include <array>
#include <vector>

namespace ws
{
struct Triangle { int a, b, c; };

std::vector<Triangle> delaunay (const std::vector<std::array<double, 2>>& points);
}
