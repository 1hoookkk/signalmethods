#include "LensEditor.h"
#include <cmath>

namespace lens
{
namespace
{
constexpr int kW = 520, kH = 340;
juce::String num (double v) { return juce::String (v, 1); }
juce::String escape (const std::string& s) { return juce::String (s).replace ("&", "&amp;").replace ("<", "&lt;").replace (">", "&gt;"); }
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
    const auto now = processor.frames.load() * 4u + (processor.quiet.load() ? 1u : 0u) + (unsigned) selectedSlot * 2u;
    if (now == seen && drawing != nullptr) return;
    seen = now;
    drawing = juce::Drawable::createFromSVGString (svg());
    repaint();
}

juce::String Editor::svg() const
{
    const auto& nodes = processor.locator.nodes();
    const juce::Rectangle<double> plot (36.0, 24.0, kW - 48.0, 170.0);
    auto xOf = [&] (double hz) { return plot.getX() + plot.getWidth() * std::log (std::clamp (hz, kLowHz, kHighHz) / kLowHz) / std::log (kHighHz / kLowHz); };
    auto yOf = [&] (double db) { return plot.getBottom() - (std::clamp (db, -40.0, 40.0) + 40.0) / 80.0 * plot.getHeight(); };
    juce::String s;
    s << "<svg xmlns='http://www.w3.org/2000/svg' width='" << kW << "' height='" << kH << "' viewBox='0 0 " << kW << " " << kH << "'>";
    s << "<rect width='" << kW << "' height='" << kH << "' fill='#161718'/>";
    for (int db = -40; db <= 40; db += 20)
    {
        s << "<line x1='" << num (plot.getX()) << "' y1='" << num (yOf (db)) << "' x2='" << num (plot.getRight()) << "' y2='" << num (yOf (db)) << "' stroke='" << (db == 0 ? "#45494f" : "#2e3136") << "'/>";
        s << "<text x='" << num (plot.getX() - 4) << "' y='" << num (yOf (db) + 3) << "' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='9' text-anchor='end'>" << (db > 0 ? "+" : "") << db << "</text>";
    }
    for (double hz : { 100.0, 1000.0 })
    {
        s << "<line x1='" << num (xOf (hz)) << "' y1='" << num (plot.getY()) << "' x2='" << num (xOf (hz)) << "' y2='" << num (plot.getBottom()) << "' stroke='#2e3136'/>";
        s << "<text x='" << num (xOf (hz)) << "' y='" << num (plot.getBottom() + 12) << "' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='9' text-anchor='middle'>" << (hz >= 1000.0 ? "1k" : "100") << "</text>";
    }
    s << "<rect x='" << num (plot.getX()) << "' y='" << num (plot.getY()) << "' width='" << num (plot.getWidth()) << "' height='" << num (plot.getHeight()) << "' fill='none' stroke='#a8abb0'/>";
    Descriptor d {};
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const auto before = processor.descriptorSeq.load();
        if (before & 1u) continue;
        d = processor.shownDescriptor;
        if (processor.descriptorSeq.load() == before) break;
    }
    juce::String envelopePath;
    for (int b = 0; b < kBins; ++b) envelopePath << (b == 0 ? "M" : "L") << num (xOf (Locator::gridHz (b))) << " " << num (yOf (d[(size_t) b])) << " ";
    s << "<path d='" << envelopePath << "' fill='none' stroke='#eceef0' stroke-width='1.2'/>";
    const int match = processor.matched.load();
    if (match >= 0 && match < (int) nodes.size())
    {
        const auto& node = nodes[(size_t) match];
        std::array<double, kBins> response {};
        double mean = 0.0;
        for (int b = 0; b < kBins; ++b) { response[(size_t) b] = Locator::responseDb (node.words, node.datum, Locator::gridHz (b)); mean += response[(size_t) b]; }
        mean /= kBins;
        juce::String path;
        for (int b = 0; b < kBins; ++b) path << (b == 0 ? "M" : "L") << num (xOf (Locator::gridHz (b))) << " " << num (yOf (response[(size_t) b] - mean)) << " ";
        s << "<path d='" << path << "' fill='none' stroke='#4fa3e6' stroke-width='1.8'/>";
    }
    s << "<text x='12' y='14' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='10' letter-spacing='1'>" << (processor.loaded ? (processor.quiet.load() ? "LOCATE  quiet, holding the last match" : "LOCATE") : "LOCATE  no corpus index found") << "</text>";
    s << "<text x='" << kW - 12 << "' y='14' fill='#8a8e94' font-family='Segoe UI, sans-serif' font-size='10' text-anchor='end'>SMOOTH " << (int) processor.smooth->load() << " ms</text>";
    const double listY = plot.getBottom() + 30.0;
    for (int i = 0; i < Processor::kRanked; ++i)
    {
        const int node = processor.ranked[(size_t) i].load();
        if (node < 0 || node >= (int) nodes.size()) continue;
        const bool first = i == 0;
        s << "<text x='" << num (plot.getX()) << "' y='" << num (listY + i * 14) << "' fill='" << (first ? "#eceef0" : "#8a8e94") << "' font-family='Segoe UI, sans-serif' font-size='" << (first ? 11 : 10) << "'>" << escape (nodes[(size_t) node].name) << "</text>";
        s << "<text x='" << num (plot.getX() + 260.0) << "' y='" << num (listY + i * 14) << "' fill='#45494f' font-family='Segoe UI, sans-serif' font-size='9'>" << juce::String (processor.rankedDistance[(size_t) i].load(), 3) << "</text>";
    }
    const double slotsY = kH - 12.0;
    const char* const letters = "ABCD";
    for (int i = 0; i < 4; ++i)
    {
        const int node = processor.slots[(size_t) i].load();
        const bool selected = i == selectedSlot;
        s << "<text x='" << 12 + i * 118 << "' y='" << num (slotsY) << "' fill='" << (selected ? "#eceef0" : "#8a8e94") << "' font-family='Segoe UI, sans-serif' font-size='10' letter-spacing='1'>" << letters[i] << "</text>";
        if (selected) s << "<line x1='" << 12 + i * 118 << "' y1='" << num (slotsY + 3) << "' x2='" << 20 + i * 118 << "' y2='" << num (slotsY + 3) << "' stroke='#4fa3e6'/>";
        if (node >= 0 && node < (int) nodes.size())
            s << "<text x='" << 26 + i * 118 << "' y='" << num (slotsY) << "' fill='#d2d5d9' font-family='Segoe UI, sans-serif' font-size='9'>" << escape (nodes[(size_t) node].name.substr (0, 20)) << "</text>";
    }
    s << "<text x='" << kW - 12 << "' y='" << num (slotsY) << "' fill='" << (match >= 0 ? "#eceef0" : "#45494f") << "' font-family='Segoe UI, sans-serif' font-size='10' letter-spacing='1' text-anchor='end'>CAPTURE</text>";
    s << "</svg>";
    return s;
}

void Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161718));
    if (drawing != nullptr) drawing->drawAt (g, 0.0f, 0.0f, 1.0f);
}

void Editor::mouseDown (const juce::MouseEvent& e)
{
    if (e.y >= kH - 22)
    {
        if (e.x >= kW - 80) { processor.capture (selectedSlot); seen = 0; return; }
        for (int i = 0; i < 4; ++i) if (e.x >= 12 + i * 118 && e.x < 12 + (i + 1) * 118) { selectedSlot = i; seen = 0; return; }
        return;
    }
    mouseDrag (e);
}

void Editor::mouseDrag (const juce::MouseEvent& e)
{
    if (e.y > 22) return;
    const float t = juce::jlimit (0.0f, 1.0f, (float) e.x / (float) kW);
    if (auto* p = processor.state.getParameter ("smooth")) p->setValueNotifyingHost (t);
}
}
