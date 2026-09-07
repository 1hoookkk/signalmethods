#include "LensEditor.h"
#include <cmath>

namespace lens
{
namespace
{
const juce::Colour kGround (0xff161718), kPanel (0xff1f2124), kRule (0xffa8abb0), kGrid (0xff2e3136), kInk (0xffeceef0), kText (0xffd2d5d9), kDim (0xff8a8e94), kFaint (0xff45494f), kBlue (0xff4fa3e6), kOrange (0xfff08a45);
constexpr double kPi = 3.141592653589793;

juce::Point<float> armadillo (double hz, double radius, juce::Rectangle<int> r)
{
    const double R = std::min (r.getWidth() / 2.0 - 12.0, r.getHeight() - 20.0);
    const double cx = r.getCentreX(), cy = r.getBottom() - 12.0;
    const double octave = std::log2 (std::clamp (hz, 20.0, 20000.0) / 20.0);
    const double theta = kPi * (1.0 - octave / 10.0);
    const double resonance = 20.0 * std::log10 (1.0 / std::max (1.0e-5, 1.0 - std::min (0.99999, radius)));
    const double rho = R * std::min (1.0, resonance / 60.0);
    return { (float) (cx + rho * std::cos (theta)), (float) (cy - rho * std::sin (theta)) };
}
}

Editor::Editor (Processor& p) : AudioProcessorEditor (p), processor (p)
{
    setSize (320, 440);
    setWantsKeyboardFocus (true);
    startTimerHz (30);
}

Editor::~Editor() { stopTimer(); }

juce::Point<float> Editor::at (float x, float y) const
{
    const auto r = plane();
    return { r.getX() + r.getWidth() * x, r.getY() + r.getHeight() * (1.0f - y) };
}

int Editor::dotAt (juce::Point<int> p) const
{
    std::lock_guard<std::mutex> lock (processor.placedLock);
    for (int i = 0; i < (int) processor.placed.size(); ++i)
        if (at (processor.placed[(size_t) i].x, processor.placed[(size_t) i].y).getDistanceFrom (p.toFloat()) < 9.0f) return i;
    return -1;
}

void Editor::paintTrajectories (juce::Graphics& g, juce::Rectangle<int> r) const
{
    const double R = std::min (r.getWidth() / 2.0 - 12.0, r.getHeight() - 20.0);
    const double cx = r.getCentreX(), cy = r.getBottom() - 12.0;
    g.setColour (kGrid);
    for (int db = 20; db <= 60; db += 20) { juce::Path arcPath; arcPath.addCentredArc ((float) cx, (float) cy, (float) (R * db / 60.0), (float) (R * db / 60.0), 0.0f, (float) -kPi / 2.0f, (float) kPi / 2.0f, true); g.strokePath (arcPath, juce::PathStrokeType (1.0f)); }
    for (int oct = 0; oct <= 10; oct += 2) { const double t = kPi * (1.0 - oct / 10.0); g.drawLine ((float) cx, (float) cy, (float) (cx + R * std::cos (t)), (float) (cy - R * std::sin (t)), 1.0f); }
    g.setColour (kRule);
    g.drawLine ((float) (cx - R), (float) cy + 0.5f, (float) (cx + R), (float) cy + 0.5f, 1.0f);
    std::vector<Snapshot> dots;
    { std::lock_guard<std::mutex> lock (processor.placedLock); dots = processor.placed; }
    if (dots.size() < 2) return;
    const float px = processor.x->load(), py = processor.y->load();
    std::vector<std::pair<float, size_t>> near;
    for (size_t i = 0; i < dots.size(); ++i) near.push_back ({ std::hypot (px - dots[i].x, py - dots[i].y), i });
    std::sort (near.begin(), near.end());
    const auto& a = dots[near[0].second];
    const auto& b = dots[near[1].second];
    constexpr int steps = 24;
    for (size_t row = 0; row < trench::core::kSectionCount; ++row)
    {
        juce::Path poles, zeros;
        bool startedP = false, startedZ = false;
        for (int k = 0; k <= steps; ++k)
        {
            trench::core::PackedSection words {};
            for (size_t w = 0; w < 5; ++w) words[w] = trench::core::interpolate_word (a.words[row][w], b.words[row][w], (float) k / steps);
            const auto geometry = trench::core::geometry_from_words (words, trench::core::kP2kDatumHz);
            if (const auto* pole = std::get_if<trench::core::ConjugatePair> (&geometry.pole); pole != nullptr && pole->radius > 0.02)
            {
                const auto q = armadillo (pole->hz, pole->radius, r);
                if (! startedP) { poles.startNewSubPath (q); startedP = true; } else poles.lineTo (q);
            }
            if (const auto* zero = std::get_if<trench::core::ConjugatePair> (&geometry.zero); zero != nullptr && zero->radius > 0.02)
            {
                const auto q = armadillo (zero->hz, zero->radius, r);
                if (! startedZ) { zeros.startNewSubPath (q); startedZ = true; } else zeros.lineTo (q);
            }
        }
        g.setColour (kBlue.withAlpha (0.8f)); g.strokePath (poles, juce::PathStrokeType (1.0f));
        g.setColour (kOrange.withAlpha (0.7f)); g.strokePath (zeros, juce::PathStrokeType (1.0f));
    }
    const auto here = processor.blend (px, py);
    for (size_t row = 0; row < trench::core::kSectionCount; ++row)
    {
        const auto geometry = trench::core::geometry_from_words (here[row], trench::core::kP2kDatumHz);
        if (const auto* pole = std::get_if<trench::core::ConjugatePair> (&geometry.pole); pole != nullptr && pole->radius > 0.02)
        {
            const auto q = armadillo (pole->hz, pole->radius, r);
            g.setColour (kBlue); g.fillEllipse (q.x - 3.0f, q.y - 3.0f, 6.0f, 6.0f);
            g.setColour (kText); g.setFont (9.0f); g.drawText (juce::String ((int) row + 1), (int) q.x + 5, (int) q.y - 12, 12, 10, juce::Justification::left);
        }
        if (const auto* zero = std::get_if<trench::core::ConjugatePair> (&geometry.zero); zero != nullptr && zero->radius > 0.02)
        {
            const auto q = armadillo (zero->hz, zero->radius, r);
            g.setColour (kOrange); g.drawRect (juce::Rectangle<float> (q.x - 3.0f, q.y - 3.0f, 6.0f, 6.0f), 1.0f);
        }
    }
    g.setColour (kDim); g.setFont (9.0f);
    g.drawText (a.name + "  to  " + b.name, r.withHeight (12), juce::Justification::centredTop);
}

void Editor::paint (juce::Graphics& g)
{
    g.fillAll (kGround);
    const auto r = plane();
    g.setColour (kGrid);
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine (r.getX() + r.getWidth() * i / 4, (float) r.getY(), (float) r.getBottom());
        g.drawHorizontalLine (r.getY() + r.getHeight() * i / 4, (float) r.getX(), (float) r.getRight());
    }
    g.setColour (kFaint); g.drawRect (r);
    std::vector<Step> path;
    { std::lock_guard<std::mutex> lock (processor.pathLock); path = processor.path; }
    if (path.size() > 1)
    {
        juce::Path ribbon;
        ribbon.startNewSubPath (at (path[0].x, path[0].y));
        for (size_t i = 1; i < path.size(); ++i) ribbon.lineTo (at (path[i].x, path[i].y));
        g.setColour ((processor.playing.load() ? kBlue : kDim).withAlpha (0.55f));
        g.strokePath (ribbon, juce::PathStrokeType (1.2f));
    }
    std::vector<Snapshot> dots;
    { std::lock_guard<std::mutex> lock (processor.placedLock); dots = processor.placed; }
    const char* const letters = "ABCDEFGH";
    for (size_t i = 0; i < dots.size(); ++i)
    {
        const auto p = at (dots[i].x, dots[i].y);
        g.setColour (kText); g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
        g.setColour (kDim); g.setFont (10.0f);
        g.drawText (juce::String::charToString (letters[i % 8]) + "  " + dots[i].name, (int) p.x + 8, (int) p.y - 7, 200, 14, juce::Justification::left);
    }
    const auto puck = at (processor.x->load(), processor.y->load());
    g.setColour (kPanel); g.fillEllipse (puck.x - 7.0f, puck.y - 7.0f, 14.0f, 14.0f);
    g.setColour (processor.bypass->load() > 0.5f ? kDim : kBlue); g.drawEllipse (puck.x - 7.0f, puck.y - 7.0f, 14.0f, 14.0f, 1.8f);
    g.fillEllipse (puck.x - 2.0f, puck.y - 2.0f, 4.0f, 4.0f);
    g.setColour (kDim); g.setFont (10.0f);
    const juce::String mode = processor.playing.load() ? "PLAYING" : processor.recording.load() ? "RECORDING" : "";
    g.drawText (mode, r.withHeight (12).translated (0, -1), juce::Justification::centredRight);
    g.setColour (kFaint);
    g.drawText ("drag the puck   R record   P play   Backspace clear   right-click a dot for the next corner", getLocalBounds().removeFromBottom (130).removeFromTop (12), juce::Justification::centred);
    paintTrajectories (g, arc().withTrimmedTop (12));
}

void Editor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const int dot = dotAt (e.getPosition());
    if (dot >= 0)
    {
        if (e.mods.isRightButtonDown()) { processor.placeNext ((size_t) dot); return; }
        dragDot = dot;
        return;
    }
    if (plane().contains (e.getPosition())) { dragPuck = true; mouseDrag (e); }
}

void Editor::mouseDrag (const juce::MouseEvent& e)
{
    const auto r = plane();
    const float nx = (float) (e.x - r.getX()) / (float) r.getWidth(), ny = 1.0f - (float) (e.y - r.getY()) / (float) r.getHeight();
    if (dragDot >= 0) { processor.moveDot ((size_t) dragDot, nx, ny); return; }
    if (dragPuck) processor.setPuck (nx, ny);
}

void Editor::mouseUp (const juce::MouseEvent&) { dragDot = -1; dragPuck = false; }

bool Editor::keyPressed (const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'r' || key.getTextCharacter() == 'R')
    {
        const bool on = ! processor.recording.load();
        if (on) { processor.playing = false; processor.clearPath(); }
        processor.recording = on;
        return true;
    }
    if (key.getTextCharacter() == 'p' || key.getTextCharacter() == 'P')
    {
        processor.recording = false;
        processor.playing = ! processor.playing.load();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::backspaceKey) { processor.playing = false; processor.recording = false; processor.clearPath(); return true; }
    return false;
}
}
