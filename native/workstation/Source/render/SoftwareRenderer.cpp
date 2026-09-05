#include "SoftwareRenderer.h"
#include "../ui/Style.h"

namespace ws
{
namespace
{
juce::Colour colourOf (const Vertex& v) { return juce::Colour::fromFloatRGBA (v.r, v.g, v.b, v.a); }
}

void drawScene (juce::Graphics& g, const std::vector<Batch>& batches)
{
    for (const auto& b : batches)
    {
        switch (b.kind)
        {
            case Batch::points:
                for (const auto& v : b.v)
                {
                    g.setColour (colourOf (v));
                    if (b.round) g.fillEllipse (v.x - v.size * 0.5f, v.y - v.size * 0.5f, v.size, v.size);
                    else g.fillRect (v.x - v.size * 0.5f, v.y - v.size * 0.5f, v.size, v.size);
                }
                break;
            case Batch::lines:
                for (size_t i = 0; i + 1 < b.v.size(); i += 2)
                {
                    g.setColour (colourOf (b.v[i]));
                    g.drawLine (b.v[i].x, b.v[i].y, b.v[i + 1].x, b.v[i + 1].y, b.v[i].size);
                }
                break;
            case Batch::strip:
                if (b.v.size() > 1)
                {
                    juce::Path p;
                    p.startNewSubPath (b.v[0].x, b.v[0].y);
                    for (size_t i = 1; i < b.v.size(); ++i) p.lineTo (b.v[i].x, b.v[i].y);
                    g.setColour (colourOf (b.v[0]));
                    g.strokePath (p, juce::PathStrokeType (b.v[0].size));
                }
                break;
            case Batch::rects:
                for (size_t i = 0; i + 1 < b.v.size(); i += 2)
                {
                    g.setColour (colourOf (b.v[i]));
                    g.fillRect (juce::Rectangle<float> (b.v[i].x, b.v[i].y, b.v[i + 1].x - b.v[i].x, b.v[i + 1].y - b.v[i].y));
                }
                break;
            case Batch::text:
                for (const auto& t : b.texts)
                {
                    g.setFont (mono (t.size));
                    g.setColour (t.colour);
                    g.drawText (t.s, t.box.toNearestInt(), t.just, false);
                }
                break;
            case Batch::image:
                g.drawImageAt (b.img, (int) b.box.getX(), (int) b.box.getY());
                break;
        }
    }
}
}
