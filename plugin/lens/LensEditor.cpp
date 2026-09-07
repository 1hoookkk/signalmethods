#include "LensEditor.h"
#include "dsp/Formants.h"
#include <cmath>
#include <complex>

namespace lens
{
namespace
{
const juce::Colour kGround (0xff161718), kPanel (0xff1f2124), kRule (0xffa8abb0), kGrid (0xff2e3136), kInk (0xffeceef0), kText (0xffd2d5d9), kDim (0xff8a8e94), kFaint (0xff45494f), kBlue (0xff4fa3e6), kOrange (0xfff08a45);
constexpr double kLow = 20.0, kHigh = 20000.0, kPi = 3.141592653589793;

double sectionDb (const trench::core::PackedSection& words, double hz)
{
    const auto q = trench::core::section_words_to_biquad (words);
    const std::complex<double> z = std::polar (1.0, -2.0 * kPi * hz / trench::core::kP2kDatumHz);
    const std::complex<double> num = q[0] + q[1] * z + q[2] * z * z;
    const std::complex<double> den = 1.0 + q[3] * z + q[4] * z * z;
    return 20.0 * std::log10 (std::max (1e-9, std::abs (num / den)));
}
}

Editor::Editor (Processor& p) : AudioProcessorEditor (p), processor (p)
{
    setSize (560, 560);
    setWantsKeyboardFocus (true);
    spec.setParms (512, 512, 128, 7);
    envelope.setParms (512, 512, 128, 7);
    envelope.lpcenv = 1;
    tap.resize (16384);
    for (int i = 0; i < 128; ++i) grid.push_back (kLow * std::pow (kHigh / kLow, i / 127.0));
    image = juce::Image (juce::Image::RGB, 540, 200, true);
    image.clear (image.getBounds(), kGround);
    startTimerHz (30);
}

Editor::~Editor() { stopTimer(); }

double Editor::hzOfY (int y, juce::Rectangle<int> r) const { return kLow * std::pow (kHigh / kLow, 1.0 - std::clamp ((double) (y - r.getY()) / r.getHeight(), 0.0, 1.0)); }
float Editor::yOfHz (double hz, juce::Rectangle<int> r) const { return (float) (r.getY() + r.getHeight() * (1.0 - std::log (std::clamp (hz, kLow, kHigh) / kLow) / std::log (kHigh / kLow))); }
float Editor::xOfHz (double hz, juce::Rectangle<int> r) const { return (float) (r.getX() + r.getWidth() * std::log (std::clamp (hz, kLow, kHigh) / kLow) / std::log (kHigh / kLow)); }
double Editor::hzOfX (int x, juce::Rectangle<int> r) const { return kLow * std::pow (kHigh / kLow, std::clamp ((double) (x - r.getX()) / r.getWidth(), 0.0, 1.0)); }

void Editor::timerCallback()
{
    analyse();
    const int now = processor.struck.load();
    if (now != lastStruck) { lastStruck = now; struckAt = juce::Time::currentTimeMillis(); }
    repaint();
}

void Editor::analyse()
{
    const int n = processor.pullInput (tap.data(), (int) tap.size());
    if (n <= 0) return;
    history.insert (history.end(), tap.begin(), tap.begin() + n);
    const size_t win = (size_t) spec.winsize, hop = (size_t) spec.stride;
    const double rate = processor.sampleRate();
    while (cursor + win <= history.size())
    {
        spec.frame (history.data() + cursor);
        envelope.frame (history.data() + cursor);
        cursor += hop;
        latest.assign (spec.fx.begin(), spec.fx.begin() + spec.nfft2);
        latestEnvelope.assign (envelope.fx.begin(), envelope.fx.begin() + envelope.nfft2);
        const int h = image.getHeight();
        for (int y = 0; y < h; ++y)
        {
            const double hz = kLow * std::pow (kHigh / kLow, 1.0 - (double) y / (h - 1));
            const int bin = std::clamp ((int) std::lround (hz / rate * spec.nfft), 1, spec.nfft2 - 1);
            const float level = std::clamp (latest[(size_t) bin] / 255.0f, 0.0f, 1.0f);
            image.setPixelAt (column, y, kGround.interpolatedWith (kBlue.brighter (0.4f), level * level));
        }
        column = (column + 1) % image.getWidth();
    }
    if (cursor > 65536) { history.erase (history.begin(), history.begin() + (std::ptrdiff_t) cursor); cursor = 0; }
}

int Editor::rowNear (double hz) const
{
    const auto w = processor.words();
    int best = -1;
    double bestSt = 3.0;
    int empty = -1;
    for (int row = 0; row < (int) trench::core::kSectionCount; ++row)
    {
        const auto g = trench::core::geometry_from_words (w[(size_t) row], trench::core::kP2kDatumHz);
        const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
        if (pole == nullptr || pole->radius < 0.05) { if (empty < 0) empty = row; continue; }
        const double st = std::abs (12.0 * std::log2 (pole->hz / hz));
        if (st < bestSt) { bestSt = st; best = row; }
    }
    return best >= 0 ? best : empty;
}

int Editor::poleRowAt (juce::Point<int> p) const
{
    const auto r = response();
    const auto w = processor.words();
    for (int row = 0; row < (int) trench::core::kSectionCount; ++row)
    {
        const auto g = trench::core::geometry_from_words (w[(size_t) row], trench::core::kP2kDatumHz);
        const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
        if (pole == nullptr || pole->radius < 0.05) continue;
        double total = 0.0;
        for (const auto& s : w) total += sectionDb (s, pole->hz);
        const juce::Point<float> q (xOfHz (pole->hz, r), (float) std::clamp (r.getBottom() - (total + 30.0) / 60.0 * r.getHeight(), (double) r.getY(), (double) r.getBottom()));
        if (q.getDistanceFrom (p.toFloat()) < 9.0f) return row;
    }
    return -1;
}

void Editor::paintSurface (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (kGround); g.fillRect (r);
    const int w = image.getWidth();
    const int tail = w - column;
    g.drawImage (image, r.getX(), r.getY(), tail * r.getWidth() / w, r.getHeight(), column, 0, tail, image.getHeight());
    g.drawImage (image, r.getX() + tail * r.getWidth() / w, r.getY(), column * r.getWidth() / w, r.getHeight(), 0, 0, column, image.getHeight());
    g.setColour (kGrid);
    for (double hz : { 100.0, 1000.0, 10000.0 }) g.drawHorizontalLine ((int) yOfHz (hz, r), (float) r.getX(), (float) r.getRight());
    const auto words = processor.words();
    for (int row = 0; row < (int) trench::core::kSectionCount; ++row)
    {
        const auto geometry = trench::core::geometry_from_words (words[(size_t) row], trench::core::kP2kDatumHz);
        const auto* pole = std::get_if<trench::core::ConjugatePair> (&geometry.pole);
        if (pole == nullptr || pole->radius < 0.05) continue;
        const float y = yOfHz (pole->hz, r);
        g.setColour (row == dragRow ? kInk : kOrange.withAlpha (0.85f));
        g.drawHorizontalLine ((int) y, (float) r.getRight() - 40.0f, (float) r.getRight());
        g.setFont (9.0f);
        g.drawText (juce::String (row + 1), r.getRight() - 52, (int) y - 6, 12, 12, juce::Justification::centredRight);
    }
    g.setColour (kFaint); g.drawRect (r);
    g.setColour (kDim); g.setFont (10.0f);
    g.drawText ("SPECTROGRAM   click a ridge to place a pole, drag it, right-click to clear", r.getX() + 6, r.getY() + 2, r.getWidth() - 12, 12, juce::Justification::left);
    if (juce::Time::currentTimeMillis() - struckAt < 150) { g.setColour (kInk); g.fillEllipse ((float) r.getRight() - 14.0f, (float) r.getY() + 4.0f, 8.0f, 8.0f); }
}

void Editor::paintResponse (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setColour (kGrid);
    for (int db = -30; db <= 30; db += 10)
    {
        const float y = (float) (r.getBottom() - (db + 30.0) / 60.0 * r.getHeight());
        g.drawHorizontalLine ((int) y, (float) r.getX(), (float) r.getRight());
        g.setColour (kDim); g.setFont (9.0f); g.drawText ((db > 0 ? "+" : "") + juce::String (db), r.getX() - 34, (int) y - 6, 30, 12, juce::Justification::right); g.setColour (kGrid);
    }
    for (double hz : { 100.0, 1000.0, 10000.0 }) g.drawVerticalLine ((int) xOfHz (hz, r), (float) r.getY(), (float) r.getBottom());
    g.setColour (kFaint); g.drawHorizontalLine ((int) (r.getBottom() - r.getHeight() / 2), (float) r.getX(), (float) r.getRight());
    g.setColour (kRule); g.drawRect (r);
    const double rate = processor.sampleRate();
    auto frameCurve = [&] (const std::vector<float>& frame, juce::Colour colour, float width)
    {
        if (frame.size() < 4) return;
        juce::Path path; bool started = false;
        for (size_t i = 1; i < frame.size(); ++i)
        {
            const double hz = (double) i * rate / (double) spec.nfft;
            if (hz < kLow || hz > kHigh) continue;
            const double db = frame[i] * 60.0 / 255.0 - 30.0;
            const juce::Point<float> q (xOfHz (hz, r), (float) std::clamp (r.getBottom() - (db + 30.0) / 60.0 * r.getHeight(), (double) r.getY(), (double) r.getBottom()));
            if (! started) { path.startNewSubPath (q); started = true; } else path.lineTo (q);
        }
        g.setColour (colour); g.strokePath (path, juce::PathStrokeType (width));
    };
    frameCurve (latest, kDim.withAlpha (0.5f), 1.0f);
    frameCurve (latestEnvelope, kInk.withAlpha (0.9f), 1.2f);
    const auto words = processor.words();
    auto curve = [&] (auto dbAt, juce::Colour colour, float width)
    {
        juce::Path path;
        for (size_t i = 0; i < grid.size(); ++i)
        {
            const double db = dbAt (grid[i]);
            const juce::Point<float> q (xOfHz (grid[i], r), (float) std::clamp (r.getBottom() - (db + 30.0) / 60.0 * r.getHeight(), (double) r.getY(), (double) r.getBottom()));
            if (i == 0) path.startNewSubPath (q); else path.lineTo (q);
        }
        g.setColour (colour); g.strokePath (path, juce::PathStrokeType (width));
    };
    for (size_t row = 0; row < trench::core::kSectionCount; ++row)
        curve ([&] (double hz) { return sectionDb (words[row], hz); }, kBlue.withAlpha (row == (size_t) dragRow ? 0.9f : 0.35f), 1.0f);
    curve ([&] (double hz) { double t = 0.0; for (const auto& s : words) t += sectionDb (s, hz); return t; }, kBlue, 1.8f);
    for (int row = 0; row < (int) trench::core::kSectionCount; ++row)
    {
        const auto geometry = trench::core::geometry_from_words (words[(size_t) row], trench::core::kP2kDatumHz);
        const auto* pole = std::get_if<trench::core::ConjugatePair> (&geometry.pole);
        if (pole == nullptr || pole->radius < 0.05) continue;
        double total = 0.0;
        for (const auto& s : words) total += sectionDb (s, pole->hz);
        const juce::Point<float> q (xOfHz (pole->hz, r), (float) std::clamp (r.getBottom() - (total + 30.0) / 60.0 * r.getHeight(), (double) r.getY(), (double) r.getBottom()));
        g.setColour (kPanel); g.fillEllipse (q.x - 5.0f, q.y - 5.0f, 10.0f, 10.0f);
        g.setColour (row == dragRow ? kInk : kBlue); g.drawEllipse (q.x - 5.0f, q.y - 5.0f, 10.0f, 10.0f, 1.5f);
        g.setColour (kText); g.setFont (9.0f); g.drawText (juce::String (row + 1), (int) q.x + 7, (int) q.y - 13, 12, 10, juce::Justification::left);
    }
    g.setColour (kDim); g.setFont (10.0f);
    g.drawText ("RESPONSE   grey the input, white its LPC envelope, thin blue each section, blue the cascade   wheel on a pole for resonance   F fits from the envelope", r.getX() + 6, r.getY() + 2, r.getWidth() - 12, 12, juce::Justification::left);
}

void Editor::paintControls (juce::Graphics& g, juce::Rectangle<int> r)
{
    g.setFont (11.0f);
    const auto captureA = r.removeFromLeft (90), captureB = r.removeFromRight (90);
    g.setColour (processor.haveA ? kInk : kText); g.drawText ("CAPTURE A", captureA, juce::Justification::centredLeft);
    g.setColour (processor.haveB ? kInk : kText); g.drawText ("CAPTURE B", captureB, juce::Justification::centredRight);
    const auto slider = r.reduced (16, 0);
    const bool live = processor.haveA && processor.haveB;
    g.setColour (kFaint); g.drawHorizontalLine (slider.getCentreY(), (float) slider.getX(), (float) slider.getRight());
    const float x = slider.getX() + slider.getWidth() * processor.morph->load(), y = (float) slider.getCentreY();
    juce::Path diamond; diamond.addQuadrilateral (x, y - 8.0f, x + 8.0f, y, x, y + 8.0f, x - 8.0f, y);
    g.setColour (kPanel); g.fillPath (diamond);
    g.setColour (live ? kBlue : kDim); g.strokePath (diamond, juce::PathStrokeType (1.8f));
    g.setColour (kDim); g.setFont (9.0f);
    g.drawText (live ? "A to B through the chip's words" : "capture A and B to sweep", slider.withY (slider.getY() - 2).withHeight (12), juce::Justification::centredTop);
}

void Editor::paint (juce::Graphics& g)
{
    g.fillAll (kGround);
    paintSurface (g, surface());
    paintResponse (g, response());
    paintControls (g, controls());
}

void Editor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto p = e.getPosition();
    if (surface().contains (p))
    {
        const double hz = hzOfY (p.y, surface());
        const int row = rowNear (hz);
        if (row < 0) return;
        if (e.mods.isRightButtonDown()) { processor.clearRow (row); return; }
        const auto g = trench::core::geometry_from_words (processor.words()[(size_t) row], trench::core::kP2kDatumHz);
        const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
        const double radius = pole != nullptr && pole->radius >= 0.05 ? pole->radius : std::exp (-kPi * 80.0 / trench::core::kP2kDatumHz);
        processor.setPole (row, hz, radius);
        dragRow = row;
        return;
    }
    if (response().contains (p))
    {
        const int row = poleRowAt (p);
        if (row >= 0) { if (e.mods.isRightButtonDown()) processor.clearRow (row); else dragRow = row; }
        return;
    }
    auto c = controls();
    const auto captureA = c.removeFromLeft (90), captureB = c.removeFromRight (90);
    if (captureA.contains (p)) { processor.capture (0); return; }
    if (captureB.contains (p)) { processor.capture (1); return; }
    if (c.contains (p)) { dragSlider = true; mouseDrag (e); }
}

void Editor::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (dragRow >= 0)
    {
        const auto g = trench::core::geometry_from_words (processor.words()[(size_t) dragRow], trench::core::kP2kDatumHz);
        const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
        const double radius = pole != nullptr && pole->radius >= 0.05 ? pole->radius : 0.99;
        const double hz = surface().contains (p) || p.y < response().getY() ? hzOfY (p.y, surface()) : hzOfX (p.x, response());
        processor.setPole (dragRow, hz, radius);
        return;
    }
    if (dragSlider)
    {
        auto c = controls(); c.removeFromLeft (90); c.removeFromRight (90);
        const auto slider = c.reduced (16, 0);
        processor.setMorph ((float) (p.x - slider.getX()) / (float) slider.getWidth());
    }
}

void Editor::mouseUp (const juce::MouseEvent&) { dragRow = -1; dragSlider = false; }

void Editor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int row = poleRowAt (e.getPosition());
    if (row < 0) return;
    const auto g = trench::core::geometry_from_words (processor.words()[(size_t) row], trench::core::kP2kDatumHz);
    const auto* pole = std::get_if<trench::core::ConjugatePair> (&g.pole);
    if (pole == nullptr) return;
    const double bandwidth = -std::log (std::max (pole->radius, 1e-9)) * trench::core::kP2kDatumHz / kPi;
    const double next = std::clamp (bandwidth * (wheel.deltaY > 0 ? 0.85 : 1.18), 10.0, 4000.0);
    processor.setPole (row, pole->hz, std::exp (-kPi * next / trench::core::kP2kDatumHz));
}

bool Editor::keyPressed (const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F')
    {
        const auto found = hs::lpcResonances (envelope.lpc.k, processor.sampleRate(), (int) trench::core::kSectionCount);
        processor.seed (found);
        return true;
    }
    if (key.getTextCharacter() == 'a' || key.getTextCharacter() == 'A') { processor.capture (0); return true; }
    if (key.getTextCharacter() == 'b' || key.getTextCharacter() == 'B') { processor.capture (1); return true; }
    return false;
}
}
