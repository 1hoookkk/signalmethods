#include "Screen.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace hs
{
namespace
{
const juce::Colour kBack (0xffc7c5be), kGrid (0xffaaa8a1), kAccent (0xff252627), kDim (0xff666763), kText (0xff343633), kBox (0xffdedcd5), kFill (0x4a343633);
const juce::Colour kVowelInk (0xff445b6a), kCaptureInk (0xff805d32), kReadInk (0xff80534b);
const juce::Colour kPlotBack (0xff1b1d20), kPlotGrid (0xff35383b), kPlotInk (0xffe8e5dc);

juce::Font typeface (float height) { return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), height, juce::Font::plain)); }

double xOf (double hzValue, juce::Rectangle<int> r) { return r.getX() + r.getWidth() * std::log (hzValue / 20.0) / std::log (1000.0); }
double yOf (double db, juce::Rectangle<int> r) { return r.getBottom() - r.getHeight() * (db + 30.0) / 60.0; }

double sectionDb (const trench::core::PackedSection& words, double frequency)
{
    const std::array<trench::core::Biquad, 1> section { trench::core::section_words_to_biquad (words) };
    return trench::core::cascade_response_db (section, frequency, trench::core::kP2kDatumHz);
}

juce::String signedValue (double value, int decimals)
{
    return (value >= 0.0 ? "+" : "") + juce::String (value, decimals);
}

juce::String noteAndCents (double frequency)
{
    if (frequency <= 0.0) return "--";
    const double midi = 69.0 + 12.0 * std::log2 (frequency / 440.0);
    const int nearest = (int) std::lround (midi);
    return noteName (440.0 * std::pow (2.0, (nearest - 69) / 12.0)) + " " + signedValue (std::round ((midi - nearest) * 100.0), 0) + "c";
}

double bandwidthSt (const trench::core::PackedSection& words)
{
    const auto geometry = trench::core::geometry_from_words (words, trench::core::kP2kDatumHz);
    const auto* pole = std::get_if<trench::core::ConjugatePair> (&geometry.pole);
    if (pole == nullptr || pole->hz <= 0.0) return 0.0;
    const double width = -std::log (std::clamp (pole->radius, 1e-9, 1.0)) * trench::core::kP2kDatumHz / juce::MathConstants<double>::pi;
    return 24.0 * std::asinh (width / (2.0 * pole->hz)) / std::log (2.0);
}

juce::Colour inkOf (const Star& s)
{
    if (s.kind == "vowel") return kVowelInk;
    if (s.kind == "capture") return kCaptureInk;
    if (s.kind == "read") return kReadInk;
    return kAccent;
}

const juce::String kCaret (juce::CharPointer_UTF8 ("\xe2\x96\xbe"));
const char* const kRoomNames[4] = { "Picker", "Cube", "Stage", "Perform" };
const char* const kPaletteNames[3] = { "Vowels", "Reads", "Captures" };
const char* const kPaletteKinds[3] = { "vowel", "read", "capture" };
}

Screen::Screen (Session& s) : session (s), hz (curveHz())
{
    setSize (1120, 700);
    setWantsKeyboardFocus (true);
    layout();
    session.onChange = [this] {
        if (session.editing >= 0) view = View::stage;
        if (session.selected >= 0 && session.selected < (int) session.stars.size())
            for (int i = 0; i < 3; ++i) if (session.stars[(size_t) session.selected].kind == kPaletteKinds[i]) palette = i;
        layout(); repaint();
    };
}

void Screen::resized() { layout(); repaint(); }

void Screen::layout()
{
    const int w = std::max (600, getWidth()), h = std::max (400, getHeight());
    for (int i = 0; i < 4; ++i) navigation[(size_t) i] = { 20 + i * 96, 12, 88, 26 };
    advance = { w - 180, 12, 160, 26 };
    const int hudW = std::clamp (w / 5, 176, 232);
    hud = { w - 20 - hudW, h - 20 - 22 - hudW, hudW, 22 + hudW };
    hudHead = hud.withHeight (22);
    writeKey = hudHead.withTrimmedLeft (hudW - 64);
    const int cellSize = hudW / 2;
    for (int n = 0; n < 4; ++n)
    {
        cornerBox[(size_t) n] = { hud.getX() + (n & 1) * cellSize, hudHead.getBottom() + (n >> 1) * cellSize, cellSize, cellSize };
        cornerTag[(size_t) n] = cornerBox[(size_t) n].withHeight (18);
        cornerPlot[(size_t) n] = cornerBox[(size_t) n].withTrimmedTop (18).reduced (4);
    }
    stage = { 20, 54, hud.getX() - 16 - 20, h - 162 };
    keyboard = { 20, h - 66, stage.getWidth(), 50 };
    for (int i = 0; i < 4; ++i) keys[(size_t) i] = { 20 + i * 86, h - 98, i == 3 ? 150 : 80, 24 };
    for (int i = 0; i < 4; ++i) toKeys[(size_t) i] = { keys[3].getRight() + 12 + i * 46, h - 98, 42, 24 };
    status = { toKeys[3].getRight() + 12, h - 98, std::max (40, stage.getRight() - toKeys[3].getRight() - 12), 24 };

    const int pickerW = std::clamp (stage.getWidth() * 2 / 5, 220, 340);
    picker = { stage.getRight() - pickerW, stage.getY(), pickerW, stage.getHeight() };
    for (int i = 0; i < 3; ++i) paletteTabs[(size_t) i] = { picker.getX() + i * (pickerW / 3), picker.getY(), pickerW / 3, 24 };
    dropZone = picker.withTrimmedTop (picker.getHeight() - 28);
    chart = juce::Rectangle<int> (stage.getX(), stage.getY(), stage.getWidth() - pickerW - 24, stage.getHeight()).withTrimmedLeft (46).withTrimmedRight (70).withTrimmedTop (22).withTrimmedBottom (50);
    keepKey = { chart.getX(), chart.getBottom() + 22, 150, 22 };
    browserScroll = std::clamp (browserScroll, 0, std::max (0, (int) cards().size() * 38 - (picker.getHeight() - 28 - 28)));

    const int cubeSize = std::max (120, std::min (stage.getWidth() - 260, stage.getHeight() - 110));
    cubeArea = { stage.getCentreX() - cubeSize / 2, stage.getY() + 40, cubeSize, cubeSize };
    for (int n = 0; n < 8; ++n)
    {
        const auto p = cubePoint ((double) (n & 1), (double) ((n >> 1) & 1), (double) ((n >> 2) & 1));
        const int width = std::min (124, cubeArea.getWidth() / 2);
        cubeBox[(size_t) n] = { (int) p.x - ((n & 1) ? 0 : width), (int) p.y - 12, width, 24 };
        cubeTags[(size_t) n] = cubeBox[(size_t) n].withWidth (20);
    }
    depth = { cubeArea.getX(), cubeArea.getBottom() + 26, cubeArea.getWidth(), 20 };

    for (int i = 0; i < 4; ++i) stageTags[(size_t) i] = { stage.getX() + i * 40, stage.getY(), 34, 24 };
    const int th = kLine * (kRows + 1) + 8 + (showHardware ? 14 * kRows : 0);
    table = { stage.getX(), stage.getBottom() - th, stage.getWidth(), th };
    const int plotWidth = stage.getWidth() - 60, plotHeight = table.getY() - (stage.getY() + 36) - 26;
    const int unit = std::max (1, std::min (plotWidth / 3, plotHeight / 2));
    magnitude = { stage.getX() + 40, stage.getY() + 36, 3 * unit, 2 * unit };

    const int pad = std::max (80, std::min (stage.getWidth() - 200, stage.getHeight() - 80));
    morph = { stage.getCentreX() - pad / 2, stage.getY() + 40, pad, pad };
    for (int n = 0; n < 4; ++n)
    {
        const bool right = n == 1 || n == 3, bottom = n >= 2;
        padTags[(size_t) n] = { right ? morph.getRight() - 90 : morph.getX(), bottom ? morph.getBottom() + 6 : morph.getY() - 26, 90, 20 };
    }
}

void Screen::showView (View next)
{
    view = next;
    menu.open = false;
    dragging = Drag::none;
    if (next == View::cube) session.setCubePoint (session.cube.x, session.cube.y, session.cube.z);
    else if (next == View::perform) session.setPuck (session.quad.morph, session.quad.q);
    else if (next == View::stage) { if (session.editing < 0) session.edit (0); }
    else if (session.selected >= 0) session.select (session.selected);
    else session.setPuck (session.quad.morph, session.quad.q);
    layout(); repaint();
}

void Screen::advanceRoom()
{
    if (view == View::picker) showView (View::cube);
    else if (view == View::cube) { if (session.takeSlice()) showView (View::perform); }
    else if (view == View::stage) showView (View::perform);
    else session.write();
}

std::vector<int> Screen::cards() const
{
    std::vector<int> out;
    for (int k = 0; k < (int) session.stars.size(); ++k)
        if (session.stars[(size_t) k].kind == kPaletteKinds[palette]) out.push_back (k);
    return out;
}

juce::Point<float> Screen::cubePoint (double x, double y, double z) const
{
    return { (float) (cubeArea.getX() + cubeArea.getWidth() * (0.72 * x + 0.28 * z)),
             (float) (cubeArea.getBottom() - cubeArea.getHeight() * (0.72 * y + 0.28 * z)) };
}

std::pair<double, double> Screen::planeAt (juce::Point<int> p) const
{
    const double z = session.cube.z;
    const double x = ((p.x - cubeArea.getX()) / (double) cubeArea.getWidth() - 0.28 * z) / 0.72;
    const double y = ((cubeArea.getBottom() - p.y) / (double) cubeArea.getHeight() - 0.28 * z) / 0.72;
    return { std::clamp (x, 0.0, 1.0), std::clamp (y, 0.0, 1.0) };
}

bool Screen::inPlane (juce::Point<int> p) const
{
    const auto f = planeAt (p);
    return cubePoint (f.first, f.second, session.cube.z).getDistanceFrom (p.toFloat()) < 14.0f;
}

juce::Rectangle<int> Screen::cell (int row, int column) const
{
    static const int widths[kColumns] = { 9, 7, 15, 12, 11, 11 };
    int total = 0;
    for (int w : widths) total += w;
    int x = table.getX() + 6;
    for (int c = 0; c < column; ++c) x += (table.getWidth() - 12) * widths[c] / total;
    return { x, table.getY() + 4 + (row + 1) * kLine + std::max (0, row) * (showHardware ? 14 : 0), (table.getWidth() - 12) * widths[column] / total, kLine };
}

juce::Point<float> Screen::peakPoint (int row) const
{
    const auto words = session.editWords();
    const double frequency = rowHz (words[(size_t) row]);
    const double db = responseDb (words, { frequency })[0];
    return { (float) xOf (frequency, magnitude), (float) std::clamp (yOf (db, magnitude), (double) magnitude.getY(), (double) magnitude.getBottom()) };
}

int Screen::peakAt (juce::Point<int> p) const
{
    if (session.editing < 0 || ! magnitude.expanded (8).contains (p)) return -1;
    const auto words = session.editWords();
    int best = -1;
    float distance = 10.0f;
    for (int row = 0; row < kRows - 1; ++row)
    {
        if (rowOf (words[(size_t) row]).type != RowType::peak || rowDb (words[(size_t) row]) <= 0.0) continue;
        const float d = peakPoint (row).getDistanceFrom (p.toFloat());
        if (d < distance) { best = row; distance = d; }
    }
    return best;
}

juce::Point<float> Screen::cornerPoint (int corner) const
{
    const bool right = corner == 1 || corner == 3, bottom = corner >= 2;
    return { (float) (right ? morph.getRight() : morph.getX()), (float) (bottom ? morph.getBottom() : morph.getY()) };
}

juce::Point<float> Screen::puckPoint() const
{
    return { (float) (morph.getX() + session.quad.morph / 100.0 * morph.getWidth()), (float) (morph.getBottom() - session.quad.q / 100.0 * morph.getHeight()) };
}

juce::Rectangle<int> Screen::card (int index) const
{
    return { picker.getX(), picker.getY() + 28 + index * 38 - browserScroll, picker.getWidth(), 36 };
}

int Screen::cardAt (juce::Point<int> p) const
{
    if (! picker.withTrimmedTop (28).withTrimmedBottom (28).contains (p)) return -1;
    const auto shown = cards();
    const int index = (p.y - picker.getY() - 28 + browserScroll) / 38;
    return index >= 0 && index < (int) shown.size() ? shown[(size_t) index] : -1;
}

juce::Rectangle<int> Screen::pianoKey (int midi) const
{
    if (midi < 36 || midi > 71) return {};
    static const int offsets[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    const int pitch = midi % 12, white = (midi - 36) / 12 * 7 + offsets[pitch];
    const bool black = pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10;
    const int left = keyboard.getX() + keyboard.getWidth() * white / 21;
    const int right = keyboard.getX() + keyboard.getWidth() * (white + 1) / 21;
    if (black) return { right - (right - left) / 3, keyboard.getY(), std::max (4, (right - left) * 2 / 3), keyboard.getHeight() * 3 / 5 };
    return { left, keyboard.getY(), right - left, keyboard.getHeight() };
}

int Screen::noteAt (juce::Point<int> p) const
{
    for (int midi = 36; midi <= 71; ++midi)
        if (pianoKey (midi).getHeight() < keyboard.getHeight() && pianoKey (midi).contains (p)) return midi;
    for (int midi = 36; midi <= 71; ++midi)
        if (pianoKey (midi).contains (p)) return midi;
    return -1;
}

juce::Point<float> Screen::chartPoint (double f1, double f2) const
{
    const double u = std::log (std::clamp (f1, kF1Low, kF1High) / kF1Low) / std::log (kF1High / kF1Low);
    const double v = std::log (std::clamp (f2, kF2Low, kF2High) / kF2Low) / std::log (kF2High / kF2Low);
    return { (float) (chart.getX() + u * chart.getWidth()), (float) (chart.getBottom() - v * chart.getHeight()) };
}

std::pair<double, double> Screen::formantsAt (juce::Point<int> p) const
{
    const double u = std::clamp ((p.x - chart.getX()) / (double) chart.getWidth(), 0.0, 1.0);
    const double v = std::clamp ((chart.getBottom() - p.y) / (double) chart.getHeight(), 0.0, 1.0);
    return { kF1Low * std::pow (kF1High / kF1Low, u), kF2Low * std::pow (kF2High / kF2Low, v) };
}

int Screen::cornerAt (juce::Point<int> p) const
{
    for (int n = 0; n < 4; ++n) if (cornerBox[(size_t) n].expanded (4).contains (p)) return n;
    return -1;
}

int Screen::pointAt (juce::Point<int> p) const
{
    int best = -1;
    double bestD = 10.0;
    for (int k = 0; k < (int) session.stars.size(); ++k)
    {
        const auto f = formantsOf (session.stars[(size_t) k].words);
        if (f[0] <= 0.0 || f[1] <= 0.0) continue;
        const double d = chartPoint (f[0], f[1]).getDistanceFrom (p.toFloat());
        if (d < bestD) { bestD = d; best = k; }
    }
    return best;
}

int Screen::menuCount() const { return (int) session.stars.size(); }

juce::String Screen::menuItem (int i) const
{
    return i >= 0 && i < (int) session.stars.size() ? session.stars[(size_t) i].name : juce::String();
}

int Screen::menuItemAt (juce::Point<int> p) const
{
    if (! menu.open || ! menu.rect.contains (p)) return -1;
    const int i = (p.y - menu.rect.getY() + menu.scroll) / 18;
    return i >= 0 && i < menuCount() ? i : -1;
}

int Screen::menuPinned() const
{
    if (menu.target < 0) return -1;
    return menu.cube ? session.cube.pins[(size_t) menu.target] : session.quad.pins[(size_t) Session::kCornerPin[menu.target]];
}

void Screen::openMenu (int target, bool cube, juce::Rectangle<int> anchor)
{
    menu.open = true; menu.target = target; menu.cube = cube; menu.scroll = 0;
    const int h = std::min (18 * menuCount(), std::max (90, getHeight() - anchor.getBottom() - 30));
    int y = anchor.getBottom() + 2;
    if (y + h > getHeight() - 8) y = std::max (8, anchor.getY() - h - 2);
    const int width = std::max (anchor.getWidth(), 200);
    int x = anchor.getX();
    if (x + width > getWidth() - 8) x = std::max (8, getWidth() - 8 - width);
    menu.rect = { x, y, width, h };
    const int pinned = menuPinned();
    if (pinned > 4) menu.scroll = std::min (pinned * 18 - 36, std::max (0, 18 * menuCount() - h));
    repaint();
}

void Screen::paintCurve (juce::Graphics& g, juce::Rectangle<int> r, const Words& words, juce::Colour colour, float width, bool fill) const
{
    juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (r);
    const auto db = responseDb (words, hz);
    juce::Path p;
    for (size_t i = 0; i < hz.size(); ++i)
    {
        const float x = (float) xOf (hz[i], r), y = (float) std::clamp (yOf (db[i], r), (double) r.getY(), (double) r.getBottom());
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    if (fill)
    {
        juce::Path area (p);
        area.lineTo ((float) xOf (hz.back(), r), (float) r.getBottom());
        area.lineTo ((float) xOf (hz.front(), r), (float) r.getBottom());
        area.closeSubPath();
        g.setColour (kFill); g.fillPath (area);
    }
    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (width));
}

void Screen::paint (juce::Graphics& g)
{
    g.fillAll (kBack);
    paintTabs (g);
    if (view == View::picker) paintPicker (g);
    else if (view == View::cube) paintCube (g);
    else if (view == View::stage) paintEditor (g);
    else paintPerform (g);
    paintHud (g);
    paintStrip (g);
    paintKeyboard (g);
    paintGhost (g);
    paintMenu (g);
}

void Screen::paintTabs (juce::Graphics& g) const
{
    g.setFont (typeface (13.0f));
    for (int i = 0; i < 4; ++i)
    {
        const auto r = navigation[(size_t) i];
        g.setColour ((int) view == i ? kAccent : kDim);
        g.drawText ("F" + juce::String (i + 1) + "  " + kRoomNames[i], r, juce::Justification::centredLeft);
        if ((int) view == i) g.fillRect (r.withY (r.getBottom()).withHeight (2));
    }
    const bool live = view != View::cube || session.cube.complete();
    g.setColour (live ? kAccent : kGrid);
    const char* label = view == View::picker ? "Cube >" : view == View::cube ? "Slice into body >" : view == View::stage ? "Perform >" : "Write  W";
    g.drawText (label, advance, juce::Justification::centredRight);
}

void Screen::paintPicker (juce::Graphics& g) const
{
    g.setFont (typeface (12.0f));
    for (int i = 0; i < 3; ++i)
    {
        const auto r = paletteTabs[(size_t) i];
        g.setColour (palette == i ? kAccent : kDim);
        g.drawText (kPaletteNames[i], r, juce::Justification::centredLeft);
        if (palette == i) g.fillRect (r.withY (r.getBottom() - 2).withHeight (2).withWidth (r.getWidth() - 12));
    }
    g.setColour (kDim);
    g.drawText ("Vowel space   F1 across, F2 up", juce::Rectangle<int> (chart.getX(), stage.getY(), chart.getWidth(), 20), juce::Justification::centredLeft);
    {
        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (picker.withTrimmedTop (28).withTrimmedBottom (28));
        const auto shown = cards();
        for (int index = 0; index < (int) shown.size(); ++index)
        {
            const auto r = card (index);
            if (! r.intersects (picker)) continue;
            const int k = shown[(size_t) index];
            const auto& star = session.stars[(size_t) k];
            const bool selected = k == session.selected || k == session.auditioning;
            if (selected) { g.setColour (kBox); g.fillRect (r); }
            g.setColour (selected ? kAccent : kText);
            g.setFont (typeface (15.0f));
            g.drawText (star.name, r.reduced (8, 0).withTrimmedRight (170), juce::Justification::centredLeft);
            g.setFont (typeface (10.0f));
            g.setColour (kDim);
            g.drawText (formantName (star.words), r.withTrimmedLeft (r.getWidth() - 170).withWidth (66), juce::Justification::centredRight);
            paintCurve (g, r.withTrimmedLeft (r.getWidth() - 96).reduced (4, 5), star.words, inkOf (star), 1.0f, false);
            g.setColour (kGrid); g.drawHorizontalLine (r.getBottom(), (float) r.getX(), (float) r.getRight());
        }
        if (shown.empty())
        {
            g.setFont (typeface (12.0f));
            g.setColour (kDim);
            g.drawText (palette == 1 ? "no reads yet" : "no captures yet", picker.withTrimmedTop (28).withHeight (36).reduced (8, 0), juce::Justification::centredLeft);
        }
    }
    g.setFont (typeface (11.0f));
    g.setColour (kGrid); g.drawRect (dropZone);
    g.setColour (kDim);
    g.drawText ("drop a .wav   order-12 LPC at 11,025 Hz", dropZone, juce::Justification::centred);
    g.setColour (kBox); g.fillRect (keepKey);
    g.setColour (session.placeable() ? kAccent : kGrid);
    g.drawText ("Keep as card   Ctrl+S", keepKey, juce::Justification::centred);
    g.setFont (typeface (10.0f));
    for (double f : { 200.0, 300.0, 500.0, 700.0, 1000.0 })
    {
        const int x = (int) std::round (chartPoint (f, kF2Low).x);
        g.setColour (kGrid); g.fillRect (x, chart.getY(), 1, chart.getHeight());
        g.setColour (kDim); g.drawText (juce::String ((int) f), x - 20, chart.getBottom() + 4, 40, 12, juce::Justification::centred);
    }
    for (double f : { 500.0, 700.0, 1000.0, 1500.0, 2000.0, 3000.0 })
    {
        const int y = (int) std::round (chartPoint (kF1Low, f).y);
        g.setColour (kGrid); g.fillRect (chart.getX(), y, chart.getWidth(), 1);
        g.setColour (kDim); g.drawText (juce::String ((int) f), chart.getX() - 42, y - 6, 38, 12, juce::Justification::centredRight);
    }
    g.setColour (kDim);
    g.drawText ("F1", chart.getRight() + 6, chart.getBottom() + 4, 20, 12, juce::Justification::centredLeft);
    g.drawText ("F2", chart.getX() - 42, chart.getY() - 16, 38, 12, juce::Justification::centredRight);
    const auto origin = chartPoint (kSchwaF1, kSchwaF2);
    g.setColour (kDim.withAlpha (0.8f));
    g.drawLine (origin.x, (float) chart.getY(), origin.x, (float) chart.getBottom(), 1.0f);
    g.drawLine ((float) chart.getX(), origin.y, (float) chart.getRight(), origin.y, 1.0f);
    g.setFont (typeface (11.0f));
    for (int k = 0; k < (int) session.stars.size(); ++k)
    {
        const auto& s = session.stars[(size_t) k];
        const auto f = formantsOf (s.words);
        if (f[0] <= 0.0 || f[1] <= 0.0) continue;
        const auto p = chartPoint (f[0], f[1]);
        const bool lit = k == session.selected || k == session.hovered || k == session.auditioning;
        bool pinned = false;
        for (int pin : session.quad.pins) pinned = pinned || pin == k;
        const auto ink = inkOf (s);
        g.setColour (lit ? ink : ink.withAlpha (pinned ? 0.9f : 0.55f));
        if (pinned) g.fillEllipse (p.x - 4.5f, p.y - 4.5f, 9.0f, 9.0f);
        else g.drawEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f, lit ? 2.0f : 1.0f);
        g.setColour (lit ? ink : kText.withAlpha (0.8f));
        g.drawText (s.name, (int) p.x + 8, (int) p.y - 7, 90, 14, juce::Justification::centredLeft);
    }
    if (session.madeLive)
    {
        const auto f = formantsOf (session.made.words);
        const auto p = chartPoint (f[0], f[1]);
        g.setColour (session.inMade() ? kAccent : kDim);
        g.drawEllipse (p.x - 6.0f, p.y - 6.0f, 12.0f, 12.0f, 1.5f);
        g.drawLine (p.x - 10.0f, p.y, p.x + 10.0f, p.y, 1.0f);
        g.drawLine (p.x, p.y - 10.0f, p.x, p.y + 10.0f, 1.0f);
        g.drawText (session.made.name, (int) p.x + 10, (int) p.y + 6, 90, 14, juce::Justification::centredLeft);
    }
}

void Screen::paintCube (juce::Graphics& g) const
{
    g.setFont (typeface (12.0f));
    g.setColour (kDim);
    g.drawText ("Eight corners, a plane at depth Z, the point is what plays", stage.withHeight (24), juce::Justification::centredLeft);
    for (int n = 0; n < 8; ++n)
        for (int bit : { 1, 2, 4 })
            if ((n & bit) == 0)
            {
                const auto a = cubePoint ((double) (n & 1), (double) ((n >> 1) & 1), (double) ((n >> 2) & 1));
                const int other = n | bit;
                const auto b = cubePoint ((double) (other & 1), (double) ((other >> 1) & 1), (double) ((other >> 2) & 1));
                g.setColour (kDim); g.drawLine ({ a, b }, 1.0f);
            }
    const auto a = cubePoint (0.0, 0.0, session.cube.z), b = cubePoint (1.0, 0.0, session.cube.z);
    const auto c = cubePoint (1.0, 1.0, session.cube.z), d = cubePoint (0.0, 1.0, session.cube.z);
    juce::Path plane;
    plane.addQuadrilateral (a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
    g.setColour (kVowelInk.withAlpha (0.09f)); g.fillPath (plane);
    g.setColour (kVowelInk.withAlpha (0.7f)); g.strokePath (plane, juce::PathStrokeType (1.3f));
    if (session.cube.complete())
    {
        const auto p = cubePoint (session.cube.x, session.cube.y, session.cube.z);
        juce::Path diamond;
        diamond.addQuadrilateral (p.x, p.y - 9.0f, p.x + 9.0f, p.y, p.x, p.y + 9.0f, p.x - 9.0f, p.y);
        g.setColour (kBack); g.fillPath (diamond);
        g.setColour (kAccent); g.strokePath (diamond, juce::PathStrokeType (1.6f));
        g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);
    }
    g.setFont (typeface (12.0f));
    for (int n = 0; n < 8; ++n)
    {
        const int pin = session.cube.pins[(size_t) n];
        const auto r = cubeBox[(size_t) n];
        g.setColour (kBox); g.fillRect (r);
        g.setColour (session.editingCube && session.editing == n ? kVowelInk : kDim);
        g.drawText (juce::String (n + 1), cubeTags[(size_t) n], juce::Justification::centred);
        g.setColour (kText);
        g.drawText (pin >= 0 ? session.stars[(size_t) pin].name : juce::String ("+"), r.withTrimmedLeft (22).withTrimmedRight (16), juce::Justification::centredLeft);
        g.drawText (kCaret, r.withTrimmedLeft (r.getWidth() - 14), juce::Justification::centred);
    }
    g.setColour (kGrid); g.fillRect (depth.withY (depth.getCentreY()).withHeight (1));
    g.setColour (kVowelInk); g.fillEllipse ((float) (depth.getX() + depth.getWidth() * session.cube.z) - 5, (float) depth.getCentreY() - 5, 10, 10);
    g.setColour (kDim);
    g.drawText ("Z  " + juce::String (session.cube.z * 100.0, 0), depth.withX (depth.getX() - 70).withWidth (60), juce::Justification::centredRight);
}

void Screen::paintEditor (juce::Graphics& g) const
{
    g.setFont (typeface (12.0f));
    for (int i = 0; i < 4; ++i)
    {
        const bool lit = ! session.editingCube && session.editing == i;
        g.setColour (lit ? kBox : kBack);
        g.fillRect (stageTags[(size_t) i]);
        g.setColour (lit ? kAccent : kDim);
        g.drawText (juce::String::charToString (Session::kCornerLetters[i]), stageTags[(size_t) i], juce::Justification::centred);
    }
    g.setColour (kDim);
    const juce::String title = session.editing < 0 ? juce::String ("what plays")
        : session.editingCube ? "cube " + juce::String (session.editing + 1) + "  " + session.stars[(size_t) session.cube.pins[(size_t) session.editing]].name
        : juce::String::charToString (Session::kCornerLetters[session.editing]) + "  " + session.cornerName (session.editing);
    g.drawText (title, juce::Rectangle<int> (stageTags[3].getRight() + 16, stage.getY(), std::max (40, stage.getWidth() - 200), 24), juce::Justification::centredLeft);
    paintMagnitude (g);
    paintTable (g);
}

void Screen::paintMagnitude (juce::Graphics& g) const
{
    const auto r = magnitude;
    g.setColour (kPlotBack); g.fillRect (r.expanded (2));
    g.setFont (typeface (11.0f));
    for (int db = -30; db <= 30; db += 10)
    {
        const int y = (int) std::round (yOf (db, r));
        g.setColour (db == 0 ? kDim : kPlotGrid);
        g.drawHorizontalLine (std::min (y, r.getBottom() - 1), (float) r.getX(), (float) r.getRight());
        g.setColour (kText);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), r.getX() - 38, y - 7, 30, 14, juce::Justification::centredRight);
    }
    for (double f : { 20.0, 100.0, 1000.0, 10000.0, 20000.0 })
    {
        const int x = (int) std::round (xOf (f, r));
        g.setColour (kPlotGrid); g.drawVerticalLine (std::min (x, r.getRight() - 1), (float) r.getY(), (float) r.getBottom());
        g.setColour (kDim);
        g.drawText (f < 1000.0 ? juce::String ((int) f) : juce::String ((int) (f / 1000.0)) + "k", x - 18, r.getBottom() + 4, 36, 14, juce::Justification::centred);
    }
    if (session.sounding) paintCurve (g, r, session.words, kPlotInk, 2.0f, false);
    g.setColour (kDim); g.drawRect (r);
    if (session.editing < 0) return;
    const auto words = session.editWords();
    for (int row = 0; row < kRows - 1; ++row)
    {
        if (rowOf (words[(size_t) row]).type != RowType::peak || rowDb (words[(size_t) row]) <= 0.0) continue;
        const auto p = peakPoint (row);
        g.setColour (kPlotBack); g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
        g.setColour (kPlotInk); g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 1.5f);
        g.drawText (juce::String (row + 1), (int) p.x + 7, (int) p.y - 16, 16, 14, juce::Justification::centredLeft);
    }
}

void Screen::paintTable (juce::Graphics& g) const
{
    g.setColour (kBack.withAlpha (0.92f)); g.fillRect (table);
    g.setColour (kDim); g.drawRect (table);
    g.setFont (typeface (table.getWidth() < 450 ? 11.0f : 12.0f));
    const juce::String heads[kColumns] = { "#", "Type", "Note", noteName (440.0 * std::pow (2.0, (session.note - 69) / 12.0)) + " +st", "BW st", "Peak dB" };
    g.setColour (kDim);
    for (int c = 0; c < kColumns; ++c) g.drawText (heads[c], cell (-1, c), juce::Justification::centredLeft);
    if (session.editing < 0) return;
    const auto words = session.editWords();
    for (int r = 0; r < kRows; ++r)
    {
        const auto& w = words[(size_t) r];
        const auto row = rowOf (w);
        const double hzValue = rowHz (w), db = rowDb (w);
        const bool rest = row.type == RowType::rest;
        const bool lit = (dragging == Drag::row || dragging == Drag::peak) && dragRow == r;
        g.setColour (lit ? kAccent : rest ? kDim : kText);
        g.drawText (r == kRows - 1 ? "CEILING" : juce::String (r + 1), cell (r, 0), juce::Justification::centredLeft);
        g.drawText (rest ? "Rest" : row.type == RowType::notch ? "Notch" : "Peak", cell (r, 1), juce::Justification::centredLeft);
        g.drawText (noteAndCents (hzValue), cell (r, 2), juce::Justification::centredLeft);
        g.drawText (rest ? "--" : signedValue (69.0 + 12.0 * std::log2 (hzValue / 440.0) - session.note, 1), cell (r, 3), juce::Justification::centredLeft);
        g.drawText (rest ? "--" : juce::String (bandwidthSt (w), 1), cell (r, 4), juce::Justification::centredLeft);
        g.drawText (rest ? "--" : signedValue (db, 1), cell (r, 5), juce::Justification::centredLeft);
        if (showHardware)
        {
            g.setColour (kDim);
            const auto raw = juce::String (hzValue, 1) + " Hz   radius words Z 0x" + juce::String::toHexString ((int) w[1]).paddedLeft ('0', 4).toUpperCase()
                + "  P 0x" + juce::String::toHexString ((int) w[3]).paddedLeft ('0', 4).toUpperCase();
            g.drawText (raw, table.getX() + 6, cell (r, 0).getBottom(), table.getWidth() - 12, 14, juce::Justification::centredLeft);
        }
    }
}

void Screen::paintPerform (juce::Graphics& g) const
{
    g.setFont (typeface (12.0f));
    g.setColour (kDim);
    g.drawText ("MORPH across, Q up", stage.withHeight (24), juce::Justification::centredLeft);
    g.setColour (kBox); g.fillRect (morph);
    g.setColour (kGrid);
    for (int i = 1; i < 4; ++i)
    {
        g.fillRect (morph.getX() + morph.getWidth() * i / 4, morph.getY(), 1, morph.getHeight());
        g.fillRect (morph.getX(), morph.getY() + morph.getHeight() * i / 4, morph.getWidth(), 1);
    }
    g.drawRect (morph);
    g.setFont (typeface (11.0f));
    for (int n = 0; n < 4; ++n)
    {
        const bool right = n == 1 || n == 3;
        g.setColour (kDim);
        g.drawText (juce::String::charToString (Session::kCornerLetters[n]) + "  " + session.cornerName (n), padTags[(size_t) n], right ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }
    if (session.quad.complete() && session.auditioning == -1)
    {
        const auto pk = puckPoint();
        g.setColour (kDim.withAlpha (0.9f));
        for (int n = 0; n < 4; ++n) { const auto c = cornerPoint (n); g.drawLine (pk.x, pk.y, c.x, c.y, 1.0f); }
        juce::Path diamond;
        diamond.addQuadrilateral (pk.x, pk.y - 9.0f, pk.x + 9.0f, pk.y, pk.x, pk.y + 9.0f, pk.x - 9.0f, pk.y);
        g.setColour (kBack); g.fillPath (diamond);
        g.setColour (kAccent); g.strokePath (diamond, juce::PathStrokeType (1.6f));
        g.fillEllipse (pk.x - 2.5f, pk.y - 2.5f, 5.0f, 5.0f);
        g.setFont (typeface (12.0f));
        g.setColour (kText);
        g.drawText ("MORPH " + juce::String (session.quad.morph, 0) + "   Q " + juce::String (session.quad.q, 0), morph.withY (morph.getBottom() + 30).withHeight (20), juce::Justification::centred);
    }
}

void Screen::paintHud (juce::Graphics& g) const
{
    g.setColour (kBox); g.fillRect (hud);
    g.setFont (typeface (11.0f));
    g.setColour (kDim);
    g.drawText ("BODY  240 bytes", hudHead.reduced (6, 0), juce::Justification::centredLeft);
    g.setColour (session.quad.complete() ? kAccent : kGrid);
    g.drawText ("W  write", writeKey.reduced (6, 0), juce::Justification::centredRight);
    const auto corners = cornersOf (session.quad, session.stars);
    for (int n = 0; n < 4; ++n)
    {
        const auto box = cornerBox[(size_t) n];
        const bool lit = ! session.editingCube && session.editing == n;
        const bool target = (dragging == Drag::card || dragging == Drag::slice) && cornerAt (dragPoint) == n;
        g.setColour (kGrid); g.drawRect (box);
        const auto tag = cornerTag[(size_t) n].reduced (5, 0);
        g.setColour (lit ? kAccent : kDim);
        g.drawText (juce::String::charToString (Session::kCornerLetters[n]), tag, juce::Justification::centredLeft);
        g.setColour (kText);
        const auto name = session.cornerName (n);
        g.drawText (name.isNotEmpty() ? name : juce::String ("+"), tag.withTrimmedLeft (14).withTrimmedRight (12), juce::Justification::centredLeft);
        g.drawText (kCaret, tag.withTrimmedLeft (tag.getWidth() - 12), juce::Justification::centredRight);
        const auto plot = cornerPlot[(size_t) n];
        g.setColour (kPlotBack); g.fillRect (plot);
        g.setColour (kPlotGrid);
        for (int db = -20; db <= 20; db += 10) if (db != 0) g.drawHorizontalLine ((int) std::round (yOf (db, plot)), (float) plot.getX(), (float) plot.getRight());
        g.setColour (kDim); g.drawHorizontalLine ((int) std::round (yOf (0.0, plot)), (float) plot.getX(), (float) plot.getRight());
        if (session.quad.pins[(size_t) Session::kCornerPin[n]] >= 0)
            paintCurve (g, plot, corners[(size_t) Session::kCornerPin[n]], lit ? kPlotInk : kPlotInk.withAlpha (0.8f), lit ? 1.6f : 1.2f, false);
        if (target) { g.setColour (kAccent); g.drawRect (box.reduced (1), 2); }
    }
}

void Screen::paintStrip (juce::Graphics& g) const
{
    g.setFont (typeface (11.0f));
    const juce::String names[] = { "Play", "Saw " + noteName (440.0 * std::pow (2.0, (session.note - 69) / 12.0)), "Noise", session.loopName.isNotEmpty() ? session.loopName : "Loop" };
    const bool on[] = { session.playing, session.source == 0, session.source == 1, session.source == 2 };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (on[i] ? kAccent : kDim);
        g.drawText (names[i], keys[(size_t) i], juce::Justification::centredLeft);
    }
    const bool placeable = session.placeable();
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (placeable ? kBox : kBack); g.fillRect (toKeys[(size_t) i]);
        g.setColour (placeable ? kAccent : kGrid);
        g.drawText ("> " + juce::String::charToString (Session::kCornerLetters[i]), toKeys[(size_t) i], juce::Justification::centred);
    }
    g.setColour (kDim);
    g.drawText (session.status, status, juce::Justification::centredRight);
}

void Screen::paintKeyboard (juce::Graphics& g) const
{
    g.setFont (typeface (10.0f));
    for (int layer = 0; layer < 2; ++layer)
        for (int midi = 36; midi <= 71; ++midi)
        {
            const auto r = pianoKey (midi);
            const bool black = r.getHeight() < keyboard.getHeight();
            if (black != (layer == 1)) continue;
            g.setColour (midi == session.note ? kVowelInk : black ? kAccent : kBox);
            g.fillRect (r);
            g.setColour (kBack); g.drawRect (r);
            if (midi % 12 == 0)
            {
                g.setColour (midi == session.note ? kBox : kDim);
                g.drawText ("C" + juce::String (midi / 12 - 1), r.withTrimmedTop (r.getHeight() - 16), juce::Justification::centred);
            }
        }
}

void Screen::paintGhost (juce::Graphics& g) const
{
    if (dragging != Drag::card && dragging != Drag::slice) return;
    if (dragging == Drag::card && dragStar < 0) return;
    const Words words = dragging == Drag::card ? session.stars[(size_t) dragStar].words : session.words;
    const auto ink = dragging == Drag::card ? inkOf (session.stars[(size_t) dragStar]) : kVowelInk;
    juce::Rectangle<int> ghost (dragPoint.x - 40, dragPoint.y - 18, 80, 36);
    g.setColour (kBack.withAlpha (0.9f)); g.fillRect (ghost);
    g.setColour (ink); g.drawRect (ghost);
    paintCurve (g, ghost.reduced (3, 3), words, ink, 1.0f, false);
}

void Screen::paintMenu (juce::Graphics& g) const
{
    if (! menu.open) return;
    g.setColour (kBox); g.fillRect (menu.rect);
    g.setColour (kDim); g.drawRect (menu.rect);
    g.setFont (typeface (11.0f));
    const int first = menu.scroll / 18, last = std::min (menuCount(), first + menu.rect.getHeight() / 18 + 2);
    const int pinned = menuPinned();
    juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (menu.rect);
    for (int i = first; i < last; ++i)
    {
        juce::Rectangle<int> line (menu.rect.getX(), menu.rect.getY() + i * 18 - menu.scroll, menu.rect.getWidth(), 18);
        if (pinned == i) { g.setColour (kDim); g.fillRect (line); }
        g.setColour (inkOf (session.stars[(size_t) i]));
        g.drawText (menuItem (i), line.reduced (8, 0), juce::Justification::centredLeft);
    }
}

void Screen::mouseMove (const juce::MouseEvent& e)
{
    if (dragging != Drag::none) return;
    session.hover (view == View::picker && chart.contains (e.getPosition()) ? pointAt (e.getPosition()) : -1);
}

void Screen::mouseExit (const juce::MouseEvent&) { if (dragging == Drag::none) session.unhover(); }

void Screen::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (isShowing()) grabKeyboardFocus();
    if (menu.open)
    {
        const int i = menuItemAt (p);
        const int target = menu.target;
        const bool cube = menu.cube;
        menu.open = false;
        if (i >= 0) { if (cube) session.pinCube (target, i); else session.pinCorner (target, i); }
        repaint();
        return;
    }
    for (int i = 0; i < 4; ++i) if (navigation[(size_t) i].contains (p)) { showView ((View) i); return; }
    if (advance.contains (p)) { advanceRoom(); return; }
    for (int i = 0; i < 4; ++i)
        if (keys[(size_t) i].contains (p))
        {
            if (i == 0) session.setPlaying (! session.playing);
            else if (i == 3) { if (session.loopName.isNotEmpty()) session.setSource (2); }
            else session.setSource (i == 1 ? 0 : 1);
            return;
        }
    for (int i = 0; i < 4; ++i) if (toKeys[(size_t) i].contains (p)) { session.toCorner (i); return; }
    if (writeKey.contains (p)) { session.write(); return; }
    for (int n = 0; n < 4; ++n) if (cornerTag[(size_t) n].contains (p)) { openMenu (n, false, cornerTag[(size_t) n]); return; }
    for (int n = 0; n < 4; ++n) if (cornerBox[(size_t) n].contains (p)) { session.edit (n); return; }
    if (keyboard.contains (p))
    {
        if (const int midi = noteAt (p); midi >= 0) { dragging = Drag::keyboard; session.setNote (midi); }
        return;
    }
    if (view == View::picker)
    {
        for (int i = 0; i < 3; ++i) if (paletteTabs[(size_t) i].contains (p)) { palette = i; browserScroll = 0; layout(); repaint(); return; }
        if (keepKey.contains (p)) { session.keep(); return; }
        if (const int k = cardAt (p); k >= 0)
        {
            session.select (k);
            dragging = Drag::card; dragStar = k; dragOrigin = p; dragPoint = p;
            return;
        }
        if (chart.expanded (8, 8).contains (p))
        {
            if (const int k = pointAt (p); k >= 0)
            {
                session.select (k);
                dragging = Drag::card; dragStar = k; dragOrigin = p; dragPoint = p;
                return;
            }
            dragging = Drag::made;
            const auto f = formantsAt (p);
            session.setMade (f.first, f.second);
        }
        return;
    }
    if (view == View::cube)
    {
        for (int n = 0; n < 8; ++n) if (cubeTags[(size_t) n].contains (p)) { session.editCube (n); return; }
        for (int n = 0; n < 8; ++n) if (cubeBox[(size_t) n].contains (p)) { openMenu (n, true, cubeBox[(size_t) n]); return; }
        if (depth.expanded (0, 8).contains (p))
        {
            dragging = Drag::depth;
            mouseDrag (e);
            return;
        }
        if (! session.cube.complete()) return;
        if (cubePoint (session.cube.x, session.cube.y, session.cube.z).getDistanceFrom (p.toFloat()) < 12.0f)
        {
            dragging = Drag::slice; dragOrigin = p; dragPoint = p;
            if (session.auditioning != Session::kCube) session.setCubePoint (session.cube.x, session.cube.y, session.cube.z);
            return;
        }
        if (inPlane (p))
        {
            dragging = Drag::cube;
            mouseDrag (e);
        }
        return;
    }
    if (view == View::stage)
    {
        for (int i = 0; i < 4; ++i) if (stageTags[(size_t) i].contains (p)) { if (session.editingCube || session.editing != i) session.edit (i); return; }
        if (const int row = peakAt (p); row >= 0)
        {
            dragging = Drag::peak; dragRow = row; dragOrigin = p; dragPoint = p;
            peakEditStarted = false;
            dragWords = session.editWords(); dragBase = rowOf (dragWords[(size_t) row]);
            return;
        }
        if (session.editing < 0 || ! table.contains (p)) return;
        for (int r = 0; r < kRows; ++r)
            for (int c = 1; c < kColumns; ++c)
                if (cell (r, c).contains (p))
                {
                    if (c == 4) return;
                    const auto words = session.editWords();
                    Row row = rowOf (words[(size_t) r]);
                    session.beginRowEdit();
                    if (c == 1)
                    {
                        row.type = row.type == RowType::rest ? RowType::peak : row.type == RowType::peak ? RowType::notch : RowType::rest;
                        if (row.type == RowType::peak && r == kRows - 1 && rowHz (words[(size_t) r]) <= 0.0) { row.type = RowType::notch; row.f = kFreqCodes - 1; row.g = 0; }
                        session.setRow (session.editing, r, row);
                        return;
                    }
                    if (row.type == RowType::rest) { row.type = RowType::peak; session.setRow (session.editing, r, row); }
                    dragging = Drag::row; dragRow = r; dragColumn = c; dragOrigin = p; dragPoint = p; dragBase = row;
                    return;
                }
        return;
    }
    if (morph.expanded (12, 12).contains (p) && session.quad.complete())
    {
        dragging = Drag::puck;
        mouseDrag (e);
    }
}

void Screen::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    dragPoint = p;
    if (dragging == Drag::row)
    {
        Row row = dragBase;
        const int steps = (p.x - dragOrigin.x) / 4;
        if (dragColumn == 5) row.g = std::clamp (dragBase.g + steps, kGainMin, kGainMax);
        else row.f = std::clamp (dragBase.f + steps, 0, kFreqCodes - 1);
        session.setRow (session.editing, dragRow, row);
        return;
    }
    if (dragging == Drag::peak)
    {
        if (session.editing < 0 || (p == dragOrigin && ! peakEditStarted)) return;
        const auto& original = dragWords[(size_t) dragRow];
        const double baseHz = rowHz (original);
        const double targetHz = std::clamp (baseHz * std::pow (1000.0, (p.x - dragOrigin.x) / (double) magnitude.getWidth()), 20.0, 20000.0);
        const double baseDb = responseDb (dragWords, { baseHz })[0];
        const double targetDb = std::clamp (baseDb + (dragOrigin.y - p.y) * 60.0 / magnitude.getHeight(), -30.0, 30.0);
        double best = std::numeric_limits<double>::max();
        Row chosen = dragBase;
        for (int gain = kGainMin; gain <= kGainMax; ++gain)
        {
            Row candidate { RowType::peak, 0, gain };
            double pitchError = std::numeric_limits<double>::max();
            for (int f = 0; f < kFreqCodes; ++f)
            {
                const double frequency = rowHz (rowWords ({ RowType::peak, f, gain }, original[4]));
                if (frequency <= 0.0) continue;
                const double error = std::abs (12.0 * std::log2 (frequency / targetHz));
                if (error < pitchError) { pitchError = error; candidate.f = f; }
            }
            const auto words = rowWords (candidate, original[4]);
            const double frequency = rowHz (words);
            if (frequency <= 0.0) continue;
            double db = sectionDb (words, frequency);
            for (int r = 0; r < kRows; ++r) if (r != dragRow) db += sectionDb (dragWords[(size_t) r], frequency);
            const double error = std::abs (db - targetDb) + 12.0 * pitchError;
            if (error < best) { best = error; chosen = candidate; }
        }
        if (rowWords (chosen, original[4]) != session.editWords()[(size_t) dragRow])
        {
            if (! peakEditStarted) { session.beginRowEdit(); peakEditStarted = true; }
            session.setRow (session.editing, dragRow, chosen);
        }
        return;
    }
    if (dragging == Drag::puck)
    {
        session.setPuck ((p.x - morph.getX()) * 100.0 / morph.getWidth(), (morph.getBottom() - p.y) * 100.0 / morph.getHeight());
        return;
    }
    if (dragging == Drag::made)
    {
        const auto f = formantsAt (p);
        session.setMade (f.first, f.second);
        return;
    }
    if (dragging == Drag::cube)
    {
        const auto f = planeAt (p);
        session.setCubePoint (f.first, f.second, session.cube.z);
        return;
    }
    if (dragging == Drag::depth)
    {
        session.setCubePoint (session.cube.x, session.cube.y, (p.x - depth.getX()) / (double) depth.getWidth());
        return;
    }
    if (dragging == Drag::keyboard)
    {
        if (const int midi = noteAt (p); midi >= 0 && midi != session.note) session.setNote (midi);
        return;
    }
    if (dragging == Drag::card || dragging == Drag::slice) repaint();
}

void Screen::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (dragging == Drag::card)
    {
        if (const int n = cornerAt (p); n >= 0 && dragStar >= 0) session.pinCorner (n, dragStar);
    }
    if (dragging == Drag::slice)
    {
        if (const int n = cornerAt (p); n >= 0) session.toCorner (n);
    }
    dragging = Drag::none; dragStar = -1; dragRow = -1; dragColumn = -1;
    peakEditStarted = false;
    repaint();
}

void Screen::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int step = wheel.deltaY > 0 ? -54 : 54;
    if (menu.open && menu.rect.contains (e.getPosition()))
    {
        menu.scroll = std::clamp (menu.scroll + step, 0, std::max (0, 18 * menuCount() - menu.rect.getHeight()));
        repaint();
        return;
    }
    if (view == View::picker && picker.contains (e.getPosition()))
    {
        browserScroll += step;
        layout(); repaint();
    }
}

bool Screen::keyPressed (const juce::KeyPress& k)
{
    const auto m = k.getModifiers();
    const bool plain = ! m.isCommandDown() && ! m.isCtrlDown() && ! m.isAltDown();
    for (int i = 0; i < 4; ++i)
        if (k.getKeyCode() == juce::KeyPress::F1Key + i) { showView ((View) i); return true; }
    if (k.getKeyCode() == 'H' && plain) { showHardware = ! showHardware; layout(); repaint(); return true; }
    if (menu.open && k.getKeyCode() == juce::KeyPress::escapeKey) { menu.open = false; repaint(); return true; }
    const bool used = session.key (k);
    if (used) repaint();
    return used;
}

bool Screen::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files) if (f.endsWithIgnoreCase (".wav")) return true;
    return false;
}

void Screen::filesDropped (const juce::StringArray& files, int x, int y)
{
    const int corner = cornerAt ({ x, y });
    const bool asLoop = corner < 0 && view != View::picker;
    for (const auto& f : files)
    {
        if (! f.endsWithIgnoreCase (".wav")) continue;
        if (asLoop) { session.setLoop (juce::File (f)); continue; }
        const int k = session.addRead (juce::File (f));
        if (k >= 0 && corner >= 0) session.pinCorner (corner, k);
    }
}

juce::Image Screen::shot()
{
    juce::Image image (juce::Image::RGB, getWidth(), getHeight(), true);
    juce::Graphics g (image);
    paintEntireComponent (g, false);
    return image;
}
}
