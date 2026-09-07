#include "LensEditor.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <complex>

namespace lens
{
namespace
{
constexpr double kPi = 3.141592653589793;
constexpr int kW = 480, kH = 300;

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
    setWantsKeyboardFocus (false);
    startTimerHz (30);
}

Editor::~Editor() { stopTimer(); }

void Editor::timerCallback()
{
    const auto now = processor.frameSeq.load();
    const bool stale = juce::Time::currentTimeMillis() - keptAt > 1500 && kept.isNotEmpty();
    if (stale) kept.clear();
    if (now == seen && drawing != nullptr && ! stale && processor.quiet.load() == quietShown) return;
    seen = now; quietShown = processor.quiet.load();
    drawing = juce::Drawable::createFromSVGString (svg());
    repaint();
}

juce::String Editor::svg() const
{
    const double fs = processor.lpcRate(), top = fs * 0.5;
    const double low = 40.0;
    const juce::Rectangle<double> plot (36.0, 28.0, kW - 48.0, kH - 64.0);
    auto xOf = [&] (double hz) { return plot.getX() + plot.getWidth() * std::log (std::clamp (hz, low, top) / low) / std::log (top / low); };
    auto yOf = [&] (double db) { return plot.getBottom() - (std::clamp (db, -60.0, 30.0) + 60.0) / 90.0 * plot.getHeight(); };
    juce::String s;
    s << "<svg xmlns='http://www.w3.org/2000/svg' width='" << kW << "' height='" << kH << "' viewBox='0 0 " << kW << " " << kH << "'>";
    s << "<rect width='" << kW << "' height='" << kH << "' fill='#161718'/>";
    for (int db = -60; db <= 30; db += 10)
    {
        s << "<line x1='" << num (plot.getX()) << "' y1='" << num (yOf (db)) << "' x2='" << num (plot.getRight()) << "' y2='" << num (yOf (db)) << "' stroke='" << (db == 0 ? "#45494f" : "#2e3136") << "'/>";
        s << "<text x='" << num (plot.getX() - 4) << "' y='" << num (yOf (db) + 3) << "' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='9' text-anchor='end'>" << (db > 0 ? "+" : "") << db << "</text>";
    }
    for (double hz : { 100.0, 1000.0, 5000.0 })
    {
        if (hz > top) continue;
        s << "<line x1='" << num (xOf (hz)) << "' y1='" << num (plot.getY()) << "' x2='" << num (xOf (hz)) << "' y2='" << num (plot.getBottom()) << "' stroke='#2e3136'/>";
        s << "<text x='" << num (xOf (hz)) << "' y='" << num (plot.getBottom() + 12) << "' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='9' text-anchor='middle'>" << (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String ((int) hz)) << "</text>";
    }
    s << "<rect x='" << num (plot.getX()) << "' y='" << num (plot.getY()) << "' width='" << num (plot.getWidth()) << "' height='" << num (plot.getHeight()) << "' fill='none' stroke='#a8abb0'/>";

    std::array<float, Processor::kWindow> frame {};
    std::array<double, Processor::kOrder + 1> coefficients {};
    double error = 0.0;
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const auto before = processor.frameSeq.load();
        if (before & 1u) continue;
        frame = processor.frameCopy;
        coefficients = processor.coefficientCopy;
        error = processor.errorCopy;
        if (processor.frameSeq.load() == before) break;
    }
    juce::dsp::FFT fft (9);
    std::array<float, Processor::kWindow * 2> bins {};
    for (int i = 0; i < Processor::kWindow; ++i) bins[(size_t) i] = frame[(size_t) i];
    fft.performFrequencyOnlyForwardTransform (bins.data());
    double peak = -120.0;
    std::array<double, Processor::kWindow / 2> spectrumDb {};
    for (int k = 1; k < Processor::kWindow / 2; ++k)
    {
        spectrumDb[(size_t) k] = 20.0 * std::log10 (std::max (1e-7, (double) bins[(size_t) k] / (Processor::kWindow / 4.0)));
        peak = std::max (peak, spectrumDb[(size_t) k]);
    }
    const double lift = -peak;
    juce::String spectrumPath;
    bool started = false;
    for (int k = 1; k < Processor::kWindow / 2; ++k)
    {
        const double hz = (double) k * fs / Processor::kWindow;
        if (hz < low || hz > top) continue;
        spectrumPath << (started ? "L" : "M") << num (xOf (hz)) << " " << num (yOf (spectrumDb[(size_t) k] + lift)) << " ";
        started = true;
    }
    s << "<path d='" << spectrumPath << "' fill='none' stroke='#8a8e94' stroke-width='1' opacity='0.7'/>";
    if (coefficients[0] == 1.0 && error > 0.0)
    {
        juce::String envelopePath;
        double envelopePeak = -120.0;
        std::array<double, 128> envelope {};
        for (int i = 0; i < 128; ++i)
        {
            const double hz = low * std::pow (top / low, i / 127.0);
            const std::complex<double> z = std::polar (1.0, -2.0 * kPi * hz / fs);
            std::complex<double> a = 1.0;
            std::complex<double> zk = 1.0;
            for (int k = 1; k <= Processor::kOrder; ++k) { zk *= z; a += coefficients[(size_t) k] * zk; }
            envelope[(size_t) i] = 20.0 * std::log10 (std::max (1e-9, std::sqrt (error) / std::abs (a)));
            envelopePeak = std::max (envelopePeak, envelope[(size_t) i]);
        }
        for (int i = 0; i < 128; ++i)
        {
            const double hz = low * std::pow (top / low, i / 127.0);
            envelopePath << (i == 0 ? "M" : "L") << num (xOf (hz)) << " " << num (yOf (envelope[(size_t) i] - envelopePeak)) << " ";
        }
        s << "<path d='" << envelopePath << "' fill='none' stroke='#eceef0' stroke-width='1.2'/>";
    }
    juce::String cascadePath;
    double cascadePeak = -120.0;
    std::array<double, 128> cascade {};
    for (int i = 0; i < 128; ++i)
    {
        const double hz = low * std::pow (top / low, i / 127.0);
        cascade[(size_t) i] = cascadeDb (processor.shown, hz);
        cascadePeak = std::max (cascadePeak, cascade[(size_t) i]);
    }
    for (int i = 0; i < 128; ++i)
    {
        const double hz = low * std::pow (top / low, i / 127.0);
        cascadePath << (i == 0 ? "M" : "L") << num (xOf (hz)) << " " << num (yOf (cascade[(size_t) i] - cascadePeak)) << " ";
    }
    s << "<path d='" << cascadePath << "' fill='none' stroke='#4fa3e6' stroke-width='1.8'/>";
    for (size_t row = 0; row < 6; ++row)
    {
        const double hz = processor.shown[row * 2].load(), bw = processor.shown[row * 2 + 1].load();
        if (hz <= 0.0) continue;
        s << "<line x1='" << num (xOf (hz)) << "' y1='" << num (plot.getY()) << "' x2='" << num (xOf (hz)) << "' y2='" << num (plot.getY() + 6) << "' stroke='#4fa3e6'/>";
        s << "<text x='" << num (xOf (hz)) << "' y='" << num (plot.getY() + 16) << "' fill='#d2d5d9' font-family='Segoe UI, sans-serif' font-size='9' text-anchor='middle'>" << (int) std::lround (hz) << "</text>";
        s << "<text x='" << num (xOf (hz)) << "' y='" << num (plot.getY() + 26) << "' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='8' text-anchor='middle'>" << (int) std::lround (bw) << "</text>";
    }
    const bool holding = processor.hold->load() > 0.5f;
    s << "<text x='12' y='16' fill='" << (holding ? "#eceef0" : "#8a8e94") << "' font-family='Segoe UI, sans-serif' font-size='10' letter-spacing='1'>" << (holding ? "HOLD" : processor.quiet.load() ? "FOLLOW  quiet, keeping the last fit" : "FOLLOW") << "</text>";
    s << "<text x='" << kW - 12 << "' y='16' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='10' text-anchor='end'>SMOOTH " << (int) processor.smooth->load() << " ms   GATE " << (int) processor.gate->load() << " dB</text>";
    s << "<text x='12' y='" << kH - 6 << "' fill='" << (holding ? "#eceef0" : "#d2d5d9") << "' font-family='Segoe UI, sans-serif' font-size='10' letter-spacing='1'>HOLD</text>";
    s << "<text x='60' y='" << kH - 6 << "' fill='#d2d5d9' font-family='Segoe UI, sans-serif' font-size='10' letter-spacing='1'>KEEP</text>";
    s << "<text x='" << kW - 12 << "' y='" << kH - 6 << "' fill='" << (kept.isNotEmpty() ? "#eceef0" : "#45494f") << "' font-family='Segoe UI, sans-serif' font-size='10' text-anchor='end'>" << (kept.isNotEmpty() ? "kept " + kept : "grey the input   white its LPC-12 envelope   blue the six sections   drag the top edge for SMOOTH") << "</text>";
    s << "</svg>";
    return s;
}

void Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161718));
    if (drawing != nullptr) drawing->drawAt (g, 0.0f, 0.0f, 1.0f);
}

void Editor::doHold()
{
    if (auto* p = processor.state.getParameter ("hold")) p->setValueNotifyingHost (processor.hold->load() > 0.5f ? 0.0f : 1.0f);
}

void Editor::doKeep()
{
    const auto file = processor.keep();
    kept = file == juce::File() ? juce::String ("nothing to keep") : file.getFileName();
    keptAt = juce::Time::currentTimeMillis();
    drawing = juce::Drawable::createFromSVGString (svg());
    repaint();
}

void Editor::mouseDown (const juce::MouseEvent& e)
{
    if (e.y >= kH - 18)
    {
        if (e.x < 52) doHold();
        else if (e.x < 100) doKeep();
        return;
    }
    mouseDrag (e);
}

void Editor::mouseDrag (const juce::MouseEvent& e)
{
    if (e.y > 24) return;
    const float t = juce::jlimit (0.0f, 1.0f, (float) e.x / (float) kW);
    if (auto* p = processor.state.getParameter ("smooth")) p->setValueNotifyingHost (t);
}
}
