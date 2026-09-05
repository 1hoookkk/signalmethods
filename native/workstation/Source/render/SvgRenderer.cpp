#include "SvgRenderer.h"

namespace ws
{
namespace
{
juce::String rgba (const Vertex& v)
{
    return "rgba(" + juce::String ((int) std::round (v.r * 255)) + "," + juce::String ((int) std::round (v.g * 255)) + "," + juce::String ((int) std::round (v.b * 255)) + "," + juce::String (v.a, 3) + ")";
}
juce::String num (float x) { return juce::String (x, 1); }
}

juce::String sceneToSvg (const std::vector<Batch>& batches, float width, float height)
{
    juce::String s;
    s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << num (width) << "\" height=\"" << num (height) << "\" viewBox=\"0 0 " << num (width) << " " << num (height) << "\">";
    s << "<rect width=\"" << num (width) << "\" height=\"" << num (height) << "\" fill=\"#000\"/>";
    for (const auto& b : batches)
    {
        if (b.kind == Batch::points)
        {
            for (const auto& v : b.v)
            {
                if (b.round) s << "<circle cx=\"" << num (v.x) << "\" cy=\"" << num (v.y) << "\" r=\"" << num (v.size / 2) << "\" fill=\"" << rgba (v) << "\"/>";
                else s << "<rect x=\"" << num (v.x - v.size / 2) << "\" y=\"" << num (v.y - v.size / 2) << "\" width=\"" << num (v.size) << "\" height=\"" << num (v.size) << "\" fill=\"" << rgba (v) << "\"/>";
            }
        }
        else if (b.kind == Batch::lines)
        {
            for (size_t i = 0; i + 1 < b.v.size(); i += 2)
                s << "<line x1=\"" << num (b.v[i].x) << "\" y1=\"" << num (b.v[i].y) << "\" x2=\"" << num (b.v[i + 1].x) << "\" y2=\"" << num (b.v[i + 1].y) << "\" stroke=\"" << rgba (b.v[i]) << "\" stroke-width=\"" << num (b.v[i].size) << "\"/>";
        }
        else if (b.v.size() > 1)
        {
            s << "<polyline fill=\"none\" stroke=\"" << rgba (b.v[0]) << "\" stroke-width=\"" << num (b.v[0].size) << "\" points=\"";
            for (const auto& v : b.v) s << num (v.x) << "," << num (v.y) << " ";
            s << "\"/>";
        }
    }
    s << "</svg>";
    return s;
}

void drawSvg (juce::Graphics& g, const juce::String& svg, float width, float height)
{
    if (auto drawable = juce::Drawable::createFromSVGString (svg))
        drawable->drawWithin (g, { 0.0f, 0.0f, width, height }, juce::RectanglePlacement::stretchToFit, 1.0f);
}
}
