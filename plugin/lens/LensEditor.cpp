#include "LensEditor.h"
#include <cmath>
#include <complex>

namespace lens
{
namespace
{
constexpr double kPi = 3.141592653589793;
constexpr int kW = 360, kH = 300;

juce::String num (double v) { return juce::String (v, 1); }

double cascadeDb (const std::array<std::atomic<double>, 12>& shown, double hz)
{
    double total = 0.0;
    for (size_t row = 0; row < 6; ++row)
    {
        const double f = shown[row * 2].load(), bw = shown[row * 2 + 1].load();
        if (f <= 0.0) continue;
        const double r = std::exp (-kPi * bw / trench::core::kP2kDatumHz), theta = 2.0 * kPi * f / trench::core::kP2kDatumHz;
        const std::complex<double> z = std::polar (1.0, -2.0 * kPi * hz / trench::core::kP2kDatumHz);
        const std::complex<double> den = 1.0 - 2.0 * r * std::cos (theta) * z + r * r * z * z;
        const double dc = std::abs (1.0 - 2.0 * r * std::cos (theta) + r * r);
        total += 20.0 * std::log10 (std::max (1e-9, dc / std::abs (den)));
    }
    return total;
}
}

Editor::Editor (Processor& p) : AudioProcessorEditor (p), processor (p)
{
    setSize (kW, kH);
    startTimerHz (30);
}

Editor::~Editor() { stopTimer(); }

void Editor::timerCallback()
{
    const auto now = processor.frames.load();
    if (now == seen && drawing != nullptr) return;
    seen = now;
    drawing = juce::Drawable::createFromSVGString (svg());
    repaint();
}

juce::String Editor::svg() const
{
    const double R = 120.0, cx = kW / 2.0, cy = 150.0;
    juce::String s;
    s << "<svg xmlns='http://www.w3.org/2000/svg' width='" << kW << "' height='" << kH << "' viewBox='0 0 " << kW << " " << kH << "'>";
    s << "<rect width='" << kW << "' height='" << kH << "' fill='#161718'/>";
    for (int db = 20; db <= 60; db += 20)
    {
        const double rho = R * db / 60.0;
        s << "<path d='M" << num (cx - rho) << " " << num (cy) << " A" << num (rho) << " " << num (rho) << " 0 0 1 " << num (cx + rho) << " " << num (cy) << "' fill='none' stroke='#2e3136' stroke-width='1'/>";
    }
    for (int oct = 0; oct <= 10; oct += 2)
    {
        const double t = kPi * (1.0 - oct / 10.0);
        s << "<line x1='" << num (cx) << "' y1='" << num (cy) << "' x2='" << num (cx + R * std::cos (t)) << "' y2='" << num (cy - R * std::sin (t)) << "' stroke='#2e3136' stroke-width='1'/>";
    }
    s << "<line x1='" << num (cx - R) << "' y1='" << num (cy) << "' x2='" << num (cx + R) << "' y2='" << num (cy) << "' stroke='#a8abb0' stroke-width='1'/>";
    s << "<text x='" << num (cx - R) << "' y='" << num (cy + 14) << "' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='9' text-anchor='middle'>20</text>";
    s << "<text x='" << num (cx + R) << "' y='" << num (cy + 14) << "' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='9' text-anchor='middle'>20k</text>";
    const juce::Rectangle<double> plot (20.0, 176.0, kW - 40.0, 100.0);
    s << "<rect x='" << num (plot.getX()) << "' y='" << num (plot.getY()) << "' width='" << num (plot.getWidth()) << "' height='" << num (plot.getHeight()) << "' fill='none' stroke='#45494f'/>";
    s << "<line x1='" << num (plot.getX()) << "' y1='" << num (plot.getCentreY()) << "' x2='" << num (plot.getRight()) << "' y2='" << num (plot.getCentreY()) << "' stroke='#45494f'/>";
    juce::String path;
    for (int i = 0; i < 96; ++i)
    {
        const double hz = 20.0 * std::pow (1000.0, i / 95.0);
        const double db = std::clamp (cascadeDb (processor.shown, hz), -30.0, 30.0);
        const double x = plot.getX() + plot.getWidth() * i / 95.0, y = plot.getBottom() - (db + 30.0) / 60.0 * plot.getHeight();
        path << (i == 0 ? "M" : "L") << num (x) << " " << num (y) << " ";
    }
    s << "<path d='" << path << "' fill='none' stroke='#4fa3e6' stroke-width='1.6'/>";
    for (size_t row = 0; row < 6; ++row)
    {
        const double hz = processor.shown[row * 2].load(), bw = processor.shown[row * 2 + 1].load();
        if (hz <= 0.0) continue;
        const double radius = std::exp (-kPi * bw / trench::core::kP2kDatumHz);
        const double octave = std::log2 (std::clamp (hz, 20.0, 20000.0) / 20.0), theta = kPi * (1.0 - octave / 10.0);
        const double resonance = 20.0 * std::log10 (1.0 / std::max (1e-5, 1.0 - std::min (0.99999, radius)));
        const double rho = R * std::min (1.0, resonance / 60.0);
        const double x = cx + rho * std::cos (theta), y = cy - rho * std::sin (theta);
        s << "<circle cx='" << num (x) << "' cy='" << num (y) << "' r='3.5' fill='#4fa3e6'/>";
        s << "<text x='" << num (x + 6) << "' y='" << num (y - 5) << "' fill='#d2d5d9' font-family='Segoe UI, sans-serif' font-size='9'>" << (int) row + 1 << "</text>";
    }
    s << "<text x='12' y='16' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='10' letter-spacing='1'>FOLLOW</text>";
    s << "<text x='" << kW - 12 << "' y='16' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='10' text-anchor='end'>SMOOTH " << (int) processor.smooth->load() << " ms</text>";
    s << "</svg>";
    return s;
}

void Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161718));
    if (drawing != nullptr) drawing->drawAt (g, 0.0f, 0.0f, 1.0f);
}

void Editor::mouseDrag (const juce::MouseEvent& e)
{
    if (e.y > 24) return;
    const float t = juce::jlimit (0.0f, 1.0f, (float) e.x / (float) kW);
    if (auto* p = processor.state.getParameter ("smooth")) p->setValueNotifyingHost (t);
}
}
