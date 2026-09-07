#include "Engine.h"
#include "Look.h"
#include <algorithm>
#include <cmath>

namespace hs
{
namespace
{
constexpr int kHop = 512, kSilenceMs = 300, kWriteMs = 600;
constexpr float kShort = 32767.0f;
}

Engine::Engine (Session& s, const hs::plot::Curves& c) : session (s), curves (c)
{
    outAnalysis.setParms (1024, 1024, kHop, 7);
    inAnalysis.setParms (1024, 1024, kHop, 7);
}

std::array<juce::String, 6> Engine::names() const
{
    const juce::String note = noteName (440.0 * std::pow (2.0, (session.note - 69) / 12.0));
    return { "PLAY", "PLUCK", "SAW " + note, "NOISE", "LOOP", "WRITE" };
}

std::array<juce::String, 3> Engine::routeWords() const
{
    const auto arrow = juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92"));
    return { "KEY " + arrow + " FREQUENCY", "VELOCITY " + arrow + " STRESS", "WHEEL " + arrow + " MORPH" };
}

void Engine::layout (juce::Rectangle<int> r)
{
    area = r;
    label = { area.getX(), area.getY(), area.getWidth(), 18 };
    plot = label;
    status = { area.getX(), area.getBottom() - 14, area.getWidth(), 14 };
    const int row = label.getBottom() + 8;
    const auto font = Look::font (10.0f);
    const auto words = names();
    int x = area.getX();
    for (int i = 0; i < 5; ++i)
    {
        const int w = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, words[(size_t) i]));
        keys[(size_t) i] = { x, row, w, 16 };
        x += w + 10;
    }
    const int w = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, words[5]));
    keys[5] = { area.getRight() - w, row, w, 16 };
    const auto lines = routeWords();
    int y = keys[0].getBottom() + 10;
    for (int i = 0; i < 3; ++i)
    {
        const int width = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, lines[(size_t) i]));
        routes[(size_t) i] = { area.getX(), y, width, 14 };
        if (i < 2) depths[(size_t) i] = { routes[(size_t) i].getRight() + 10, y, 30, 14 };
        y += 16;
    }
}

int Engine::sourceAt (juce::Point<int> p) const
{
    for (int i = 0; i < 6; ++i) if (keys[(size_t) i].contains (p)) return i;
    return -1;
}

int Engine::routeAt (juce::Point<int> p) const
{
    for (int i = 0; i < 3; ++i) if (routes[(size_t) i].contains (p)) return i;
    return -1;
}

int Engine::depthAt (juce::Point<int> p) const
{
    for (int i = 0; i < 2; ++i) if (depths[(size_t) i].contains (p)) return i;
    return -1;
}

bool Engine::live() const { return lastSound > 0 && juce::Time::currentTimeMillis() - lastSound < kSilenceMs; }

void Engine::silenceFor (int ms) { lastSound = juce::Time::currentTimeMillis() - ms; }

void Engine::analyse (Peevers& p, std::vector<float>& history, size_t& cursor, std::vector<float>& spectrum, const float* samples, int n)
{
    for (int i = 0; i < n; ++i) history.push_back (samples[i] * kShort);
    const size_t win = (size_t) p.winsize, hop = (size_t) std::max (1, p.stride);
    while (cursor + win <= history.size())
    {
        p.averagedFrame (history.data() + cursor);
        cursor += hop;
        spectrum.assign (p.fx.begin(), p.fx.begin() + p.nfft2);
    }
    if (cursor > 65536)
    {
        history.erase (history.begin(), history.begin() + (std::ptrdiff_t) cursor);
        cursor = 0;
    }
}

void Engine::feed (const float* out, const float* in, int n)
{
    if (n <= 0) return;
    bool sound = false;
    for (int i = 0; i < n && ! sound; ++i) sound = std::abs (out[i]) > 1.0e-6f || std::abs (in[i]) > 1.0e-6f;
    if (sound) lastSound = juce::Time::currentTimeMillis();
    analyse (outAnalysis, outHistory, outCursor, outSpectrum, out, n);
    analyse (inAnalysis, inHistory, inCursor, inSpectrum, in, n);
}

void Engine::spectrumCurve (juce::Graphics& g, const Peevers& p, const std::vector<float>& spectrum, juce::Colour colour, juce::Rectangle<int> into) const
{
    if (spectrum.size() < 2) return;
    juce::Graphics::ScopedSaveState saved (g);
    g.reduceClipRegion (into);
    juce::Path path;
    bool started = false;
    for (size_t i = 1; i < spectrum.size(); ++i)
    {
        const double hz = (double) i * rate / (double) p.nfft;
        if (hz < 20.0 || hz > 20000.0) continue;
        const double index = std::clamp ((double) spectrum[i], 0.0, 255.0);
        const double db = index * 60.0 / 255.0 - 30.0;
        const float x = (float) hs::plot::xOf (hz, into);
        const float y = (float) std::clamp (hs::plot::yOf (db, into), (double) into.getY(), (double) into.getBottom());
        if (! started) { path.startNewSubPath (x, y); started = true; }
        else path.lineTo (x, y);
    }
    if (! started) return;
    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

void Engine::paintLive (juce::Graphics& g, juce::Rectangle<int> into) const
{
    if (live()) spectrumCurve (g, outAnalysis, outSpectrum, Look::ink, into);
}

void Engine::paint (juce::Graphics& g)
{
    if (session.status.endsWith (".body240") && session.status != writeStamp)
    {
        writeStamp = session.status;
        writeTime = juce::Time::currentTimeMillis();
    }
    g.setFont (Look::font (11.0f));
    g.setColour (live() ? Look::ink : Look::text);
    g.drawText (session.playingLabel, label, juce::Justification::centredLeft);
    const auto words = names();
    const bool on[6] = { session.playing, session.source == 3, session.source == 0, session.source == 1, session.source == 2, false };
    const bool wrote = writeTime > 0 && juce::Time::currentTimeMillis() - writeTime < kWriteMs;
    g.setFont (Look::font (10.0f));
    for (int i = 0; i < 6; ++i)
    {
        juce::Colour colour = on[i] ? Look::ink : Look::dim;
        if (i == 4 && session.loopName.isEmpty()) colour = Look::faint;
        if (i == 5) colour = wrote ? Look::ink : Look::dim;
        g.setColour (colour);
        g.drawText (words[(size_t) i], keys[(size_t) i], juce::Justification::centredLeft);
    }
    const bool awake = session.live();
    const auto lines = routeWords();
    for (int i = 0; i < 3; ++i)
    {
        const auto r = session.route (i);
        g.setColour (! awake ? Look::faint : r.on ? Look::ink : Look::dim);
        g.drawText (lines[(size_t) i], routes[(size_t) i], juce::Justification::centredLeft);
        if (i > 1) continue;
        g.setColour (! awake ? Look::faint : r.on ? Look::text : Look::dim);
        g.drawText (juce::String (std::lround (r.depth * 100.0)), depths[(size_t) i], juce::Justification::centredLeft);
    }
    if (session.status.startsWith ("cannot") || session.status.startsWith ("no audio"))
    {
        g.setColour (Look::orange);
        g.drawText (session.status, status, juce::Justification::centredLeft);
    }
}
}
