#include "Canvas.h"

namespace ws
{
Batch& Canvas::open (Batch::Kind kind, bool round)
{
    if (batches.empty() || batches.back().kind != kind || batches.back().round != round || kind == Batch::image)
        batches.push_back ({ kind, round, {}, {}, {}, {} });
    return batches.back();
}

void Canvas::fillRect (juce::Rectangle<float> r)
{
    if (r.isEmpty()) return;
    auto& b = open (Batch::rects, false);
    b.v.push_back (vertex (r.getTopLeft(), colour, 0.0f));
    b.v.push_back (vertex (r.getBottomRight(), colour, 0.0f));
}

void Canvas::drawRect (juce::Rectangle<int> r, int t)
{
    const auto f = r.toFloat();
    const float k = (float) t;
    fillRect (f.getX(), f.getY(), f.getWidth(), k);
    fillRect (f.getX(), f.getBottom() - k, f.getWidth(), k);
    fillRect (f.getX(), f.getY() + k, k, f.getHeight() - 2.0f * k);
    fillRect (f.getRight() - k, f.getY() + k, k, f.getHeight() - 2.0f * k);
}

void Canvas::drawHorizontalLine (int y, float x1, float x2) { fillRect (x1, (float) y, x2 - x1, 1.0f); }
void Canvas::drawVerticalLine (int x, float y1, float y2) { fillRect ((float) x, y1, 1.0f, y2 - y1); }

void Canvas::fillEllipse (float x, float y, float w, float h)
{
    auto& b = open (Batch::points, true);
    b.v.push_back (vertex ({ x + w * 0.5f, y + h * 0.5f }, colour, (w + h) * 0.5f));
}

void Canvas::drawText (const juce::String& s, int x, int y, int w, int h, juce::Justification just)
{
    if (s.isEmpty()) return;
    auto& b = open (Batch::text, false);
    b.texts.push_back ({ juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h), s, colour, just, fontH });
}

void Canvas::drawImageAt (const juce::Image& img, int x, int y)
{
    if (! img.isValid()) return;
    auto& b = open (Batch::image, false);
    b.img = img;
    b.box = { (float) x, (float) y, (float) img.getWidth(), (float) img.getHeight() };
}
}
