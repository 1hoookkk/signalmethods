#include "Spectrogram.h"

#include "app/Library.h"
#include "dsp/VectorFit.h"
#include <cmath>
#include <complex>

namespace hs
{
namespace
{
enum Control { cFamily = 0, cName, cFft, cWin, cStride, cWindow, cLength, cEnv, cLogF, cFlat, cAxes, cPersp, cMesh, cClear, cPause };
const char* const kKeyRow = "zsxdcvgbhnjm,l.";
}

Spectrogram::Spectrogram (Audio* tap, Session* owner) : audio (tap), session (owner)
{
    formats.registerBasicFormats();
    pulled.assign (16384, 0.0f);
    raw.assign ((size_t) Peevers::kMax + 2, 0.0f);
    peevers.setParms (1024, 1024, 256, 7);
    setWantsKeyboardFocus (true);
    setSize (1100, 620);
    words();
    if (audio != nullptr) startTimer (30);
}

Spectrogram::~Spectrogram() { stopTimer(); }

void Spectrogram::clear()
{
    frames.clear();
    powers.clear();
    history.clear();
    cursor = 0;
    length = 0;
    pickedFrame = -1;
    peevers.reset();
}

void Spectrogram::trim()
{
    while ((int) frames.size() > kept)
    {
        frames.erase (frames.begin());
        if (! powers.empty()) powers.erase (powers.begin());
        if (pickedFrame >= 0) --pickedFrame;
    }
    if (pickedFrame >= (int) frames.size()) pickedFrame = -1;
}

void Spectrogram::feed (const float* samples, int n)
{
    history.insert (history.end(), samples, samples + n);
    const size_t win = (size_t) peevers.winsize, hop = (size_t) juce::jmax (1, peevers.stride);
    while (cursor + win <= history.size())
    {
        peevers.frame (history.data() + cursor);
        cursor += hop;
        frames.emplace_back (peevers.fx.begin(), peevers.fx.begin() + peevers.nfft2);
        const float* source = peevers.lpcenv == 0 ? peevers.arry.data() + 1 : peevers.synth.data();
        peevers.spectrum (source, peevers.nfft, raw.data(), peevers.nfft);
        powers.emplace_back (raw.begin(), raw.begin() + peevers.nfft2 + 1);
        trim();
    }
    if (cursor > 65536)
    {
        history.erase (history.begin(), history.begin() + (std::ptrdiff_t) cursor);
        cursor = 0;
    }
}

bool Spectrogram::load (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const int n = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (60 * 96000));
    juce::AudioBuffer<float> buffer ((int) juce::jmax (1u, reader->numChannels), n);
    reader->read (&buffer, 0, n, 0, true, true);
    rate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
    std::vector<float> mono ((size_t) n, 0.0f);
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        const auto* in = buffer.getReadPointer (c);
        for (int i = 0; i < n; ++i) mono[(size_t) i] += in[i];
    }
    const float scale = 32767.0f / (float) juce::jmax (1, buffer.getNumChannels());
    for (auto& v : mono) v *= scale;
    clear();
    fromFile = true;
    fileName = file.getFileNameWithoutExtension();
    length = n;
    feed (mono.data(), n);
    words();
    repaint();
    return true;
}

void Spectrogram::timerCallback()
{
    if (audio == nullptr) return;
    int got = audio->pullSampler (pulled.data(), (int) pulled.size());
    if (got <= 0 || paused) return;
    bool sound = false;
    for (int i = 0; i < got; ++i) sound = sound || std::abs (pulled[(size_t) i]) > 1.0e-6f;
    if (! sound && fromFile) return;
    if (fromFile) { clear(); fromFile = false; fileName.clear(); rate = 44100.0; words(); }
    for (int i = 0; i < got; ++i) pulled[(size_t) i] *= 32767.0f;
    feed (pulled.data(), got);
    repaint();
}

void Spectrogram::resized() { layout(); }

void Spectrogram::layout()
{
    auto r = getLocalBounds().reduced (10);
    panel = r.removeFromLeft (200);
    surface = r;
    auto inner = panel.reduced (6, 4);
    int y = inner.getY();
    for (int i = 0; i < kControls; ++i)
    {
        if (i == cFft || i == cEnv) y += 10;
        hits[(size_t) i] = { inner.getX(), y, inner.getWidth(), 16 };
        y += 18;
    }
    words();
}

void Spectrogram::words()
{
    juce::String heading = fromFile ? fileName : (session != nullptr ? session->familyName : juce::String());
    if (heading.isEmpty()) heading = "no source";
    labels[cFamily] = "FAMILY";
    labels[cName] = heading.toUpperCase();
    labels[cFft] = "FFT SIZE  " + juce::String (peevers.nfft);
    labels[cWin] = "WIN SIZE  " + juce::String (peevers.winsize);
    labels[cStride] = "STRIDE  " + juce::String (peevers.stride);
    labels[cWindow] = juce::String (Peevers::windowNames[peevers.wintype]).toUpperCase();
    labels[cLength] = "LENGTH  " + juce::String (kept);
    labels[cEnv] = "ENV";
    labels[cLogF] = "LOGF";
    labels[cFlat] = "2D";
    labels[cAxes] = "AXES";
    labels[cPersp] = "PERSP";
    labels[cMesh] = mesh ? "MESH" : "LINE";
    labels[cClear] = "CLEAR";
    labels[cPause] = "PAUSE";
}

juce::Point<float> Spectrogram::project (float bin, float value, float depth) const
{
    const float x = bin / (float) juce::jmax (1, peevers.nfft2) - 0.5f;
    const float y = value / 255.0f / 2.0f;
    const float z = depth - 0.5f;
    const float ca = std::cos (azimuth), sa = std::sin (azimuth);
    const float cd = std::cos (declination), sd = std::sin (declination);
    const float rx = x * ca + z * sa;
    const float rz = -x * sa + z * ca;
    const float sy = y * cd + rz * sd;
    const float shrink = persp ? 1.0f / (1.0f + 0.6f * depth) : 1.0f;
    const float spanX = (float) surface.getWidth() * 0.52f * shrink;
    const float spanY = (float) surface.getHeight() * 0.78f * shrink;
    return { centre.x + rx * spanX, centre.y - sy * spanY };
}

void Spectrogram::fit()
{
    centre = { 0.0f, 0.0f };
    const float right = logF ? peevers.zlogpos[(size_t) peevers.nfft2] : (float) peevers.nfft2;
    float lox = 1.0e9f, hix = -1.0e9f, loy = 1.0e9f, hiy = -1.0e9f;
    for (int k = 0; k < 8; ++k)
    {
        const auto p = project ((k & 1) != 0 ? right : 0.0f, (k & 2) != 0 ? 255.0f : -20.0f, (k & 4) != 0 ? 1.0f : 0.0f);
        lox = juce::jmin (lox, p.x);
        hix = juce::jmax (hix, p.x);
        loy = juce::jmin (loy, p.y);
        hiy = juce::jmax (hiy, p.y);
    }
    const auto box = surface.reduced (48, 8).withTrimmedBottom (14).withTrimmedRight (18);
    centre = { (float) box.getCentreX() - (lox + hix) * 0.5f, (float) box.getCentreY() - (loy + hiy) * 0.5f };
}

juce::Colour Spectrogram::tint (int index) const
{
    return Look::blue.interpolatedWith (Look::orange, juce::jlimit (0.0f, 1.0f, (float) index / 255.0f));
}

void Spectrogram::grid (juce::Graphics& g) const
{
    const int n2 = peevers.nfft2;
    const int count = juce::jmax (1, (int) frames.size());
    const auto font = Look::font (10.0f);
    g.setFont (font);
    g.setColour (Look::grid);
    for (int bin = 0; bin <= n2; bin += 20)
    {
        const float px = logF ? peevers.zlogpos[(size_t) bin] : (float) bin;
        const auto a = project (px, 0.0f, 0.0f), b = project (px, 0.0f, 1.0f);
        g.drawLine (a.x, a.y, b.x, b.y, 0.6f);
    }
    const int every = juce::jmax (20, (count / 5 / 20) * 20);
    for (int f = 0; f <= count; f += every)
    {
        const float depth = (float) (count - f) / (float) juce::jmax (1, count);
        const auto a = project (0.0f, 0.0f, depth);
        const auto b = project (logF ? peevers.zlogpos[(size_t) n2] : (float) n2, 0.0f, depth);
        g.drawLine (a.x, a.y, b.x, b.y, 0.6f);
    }
    g.setColour (Look::dim);
    float taken = -1000.0f;
    for (int bin = 0; bin <= n2; bin += 20)
    {
        const float px = logF ? peevers.zlogpos[(size_t) bin] : (float) bin;
        const auto a = project (px, 0.0f, 0.0f);
        if (a.x - taken < 34.0f) continue;
        taken = a.x;
        const double khz = ((double) (rate * bin) * 0.001) / (double) peevers.nfft;
        g.drawText (juce::String (khz, 2), juce::Rectangle<float> (a.x - 22.0f, a.y + 3.0f, 44.0f, 12.0f), juce::Justification::centred);
    }
    taken = 1.0e9f;
    for (int f = count; f >= 0; f -= every)
    {
        const float depth = (float) (count - f) / (float) juce::jmax (1, count);
        const auto a = project (0.0f, 0.0f, depth);
        if (taken - a.y < 13.0f) continue;
        taken = a.y;
        const double seconds = ((double) f * (double) peevers.stride) / rate;
        g.drawText (juce::String (seconds, 2), juce::Rectangle<float> (a.x - 44.0f, a.y - 6.0f, 40.0f, 12.0f), juce::Justification::centredRight);
    }
    g.setColour (Look::ink);
    const float right = logF ? peevers.zlogpos[(size_t) n2] : (float) n2;
    juce::Path box;
    box.startNewSubPath (project (0.0f, 0.0f, 1.0f));
    box.lineTo (project (right, 0.0f, 1.0f));
    box.lineTo (project (right, 255.0f, 1.0f));
    box.lineTo (project (0.0f, 255.0f, 1.0f));
    box.closeSubPath();
    g.strokePath (box, juce::PathStrokeType (0.8f));
    g.setColour (Look::faint);
    g.setFont (Look::font (9.0f));
    for (int i = 0; i < 13; ++i)
    {
        const float p = std::pow (10.0f, (float) (12 - i));
        const float v = (float) ((double) std::log10 ((float) (((double) p * peevers.largest) / 1000000000000.0)) * peevers.m + peevers.b);
        const auto a = project (right - 3.0f, v, 1.0f), b = project (right, v, 1.0f);
        g.drawLine (a.x, a.y, b.x, b.y, 0.6f);
        g.drawText (juce::String (i * -5), juce::Rectangle<float> (b.x + 3.0f, b.y - 6.0f, 26.0f, 12.0f), juce::Justification::centredLeft);
    }
}

void Spectrogram::flat (juce::Graphics& g) const
{
    const auto box = surface.reduced (30, 20).toFloat();
    g.setColour (Look::ink);
    g.drawRect (box, 0.8f);
    if (frames.empty()) return;
    const auto& fr = frames.back();
    const int n2 = juce::jmin (peevers.nfft2, (int) fr.size());
    const float right = logF ? peevers.zlogpos[(size_t) juce::jmax (1, n2 - 1)] : (float) juce::jmax (1, n2 - 1);
    juce::Path path;
    int last = -1;
    juce::Point<float> prev;
    for (int bin = 0; bin < n2; ++bin)
    {
        const float px = logF ? peevers.zlogpos[(size_t) bin] : (float) bin;
        const float x = box.getX() + box.getWidth() * (px / right);
        const float y = box.getBottom() - box.getHeight() * juce::jlimit (0.0f, 1.0f, fr[(size_t) bin] / 255.0f);
        const juce::Point<float> p (x, y);
        const int index = (int) std::floor (Peevers::lutlimit (fr[(size_t) bin]));
        if (bin == 0) { path.startNewSubPath (p); last = index; }
        else
        {
            if (index != last)
            {
                g.setColour (tint (last));
                g.strokePath (path, juce::PathStrokeType (1.0f));
                path.clear();
                path.startNewSubPath (prev);
                last = index;
            }
            path.lineTo (p);
        }
        prev = p;
    }
    g.setColour (tint (last));
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

void Spectrogram::slice (juce::Graphics& g, int index, int count, juce::Colour colour) const
{
    if (index < 0 || index >= (int) frames.size()) return;
    const auto& fr = frames[(size_t) index];
    const int bins = juce::jmin (peevers.nfft2, (int) fr.size());
    if (bins < 2) return;
    const float depth = (float) (count - 1 - index) / (float) juce::jmax (1, count - 1);
    juce::Path line;
    for (int bin = 0; bin < bins; ++bin)
    {
        const float px = logF ? peevers.zlogpos[(size_t) bin] : (float) bin;
        const auto p = project (px, fr[(size_t) bin], depth);
        if (bin == 0) line.startNewSubPath (p); else line.lineTo (p);
    }
    juce::Path fill (line);
    fill.lineTo (project (logF ? peevers.zlogpos[(size_t) (bins - 1)] : (float) (bins - 1), -20.0f, depth));
    fill.lineTo (project (0.0f, -20.0f, depth));
    fill.closeSubPath();
    g.setColour (Look::ground);
    g.fillPath (fill);
    g.setColour (colour);
    if (mesh) g.strokePath (line, juce::PathStrokeType (1.0f));
    else
        for (int bin = 0; bin < bins; ++bin)
        {
            const float px = logF ? peevers.zlogpos[(size_t) bin] : (float) bin;
            const auto p = project (px, fr[(size_t) bin], depth);
            g.fillRect (p.x - 0.5f, p.y - 0.5f, 1.2f, 1.2f);
        }
}

void Spectrogram::controls (juce::Graphics& g) const
{
    g.setColour (Look::grid);
    g.drawLine ((float) panel.getRight(), (float) panel.getY(), (float) panel.getRight(), (float) panel.getBottom(), 0.8f);
    g.setFont (Look::font (10.0f));
    for (int i = 0; i < kControls; ++i)
    {
        const bool toggle = i >= cEnv && i <= cMesh;
        const bool on = i == cEnv ? peevers.lpcenv != 0
                      : i == cLogF ? logF
                      : i == cFlat ? twoD
                      : i == cAxes ? axes
                      : i == cPersp ? persp
                      : i == cMesh ? mesh
                      : i == cPause ? paused
                      : true;
        g.setColour (toggle || i == cPause ? (on ? Look::ink : Look::dim) : i == cName ? Look::dim : Look::text);
        auto box = hits[(size_t) i];
        g.drawText (labels[(size_t) i], box.toFloat(), juce::Justification::centredLeft);
        if (i == cFamily && Look::hasGlyph (Look::Glyph::caret)) Look::glyph (g, Look::Glyph::caret, box.removeFromRight (12), Look::text);
    }
}

void Spectrogram::paint (juce::Graphics& g)
{
    if (surface.isEmpty()) layout();
    fit();
    g.fillAll (Look::ground);
    if (twoD) flat (g);
    else
    {
        if (axes) grid (g);
        const int count = (int) frames.size();
        const int step = juce::jmax (1, count / 80);
        for (int i = (count - 1) % step; i < count; i += step)
        {
            const float depth = (float) (count - 1 - i) / (float) juce::jmax (1, count - 1);
            slice (g, i, count, i == count - 1 ? Look::ink : Look::blue.withAlpha (0.25f + 0.65f * (1.0f - depth)));
        }
        if (pickedFrame >= 0 && pickedFrame < count) slice (g, pickedFrame, count, Look::orange);
    }
    controls (g);
}

int Spectrogram::pickAt (juce::Point<int> p) const
{
    const int count = (int) frames.size();
    if (count == 0) return -1;
    const float right = logF ? peevers.zlogpos[(size_t) peevers.nfft2] : (float) peevers.nfft2;
    const juce::Point<float> q ((float) p.x, (float) p.y);
    int best = -1;
    float nearest = 1.0e9f;
    for (int i = 0; i < count; ++i)
    {
        const float depth = (float) (count - 1 - i) / (float) juce::jmax (1, count - 1);
        const juce::Line<float> base (project (0.0f, 0.0f, depth), project (right, 0.0f, depth));
        juce::Point<float> foot;
        const float d = base.getDistanceFromPoint (q, foot);
        if (d < nearest) { nearest = d; best = i; }
    }
    return best;
}

juce::String Spectrogram::frameName (int index) const
{
    juce::String head;
    if (fromFile) head = fileName;
    else
    {
        if (session != nullptr) head = session->familyName;
        const int midi = session != nullptr ? session->samplerNote : -1;
        if (midi >= 0)
        {
            const auto note = juce::MidiMessage::getMidiNoteName (midi, true, true, 4);
            head = head.isNotEmpty() ? head + " " + note : note;
        }
    }
    const double seconds = (double) index * (double) peevers.stride / (rate > 0.0 ? rate : 44100.0);
    return (head.isNotEmpty() ? head + " " : juce::String()) + "@ " + juce::String (seconds, 2);
}

bool Spectrogram::takeFrame()
{
    if (session == nullptr || pickedFrame < 0 || pickedFrame >= (int) powers.size()) return false;
    const auto& power = powers[(size_t) pickedFrame];
    if (power.size() < 8) return false;
    double top = 0.0;
    for (float v : power) top = std::max (top, (double) v);
    if (! (top > 0.0)) return false;
    std::vector<double> magnitudeDb (power.size(), 0.0);
    for (size_t i = 0; i < power.size(); ++i) magnitudeDb[i] = 10.0 * std::log10 (std::max ((double) power[i] / top, 1.0e-14));
    std::vector<std::complex<double>> response;
    minimumPhaseResponse (magnitudeDb, response);
    if (response.empty()) return false;
    const auto fitted = vectorFit (response, rate, 5, 8);
    if (fitted.poles.empty()) return false;
    Star s;
    s.kind = "capture";
    s.corner = "frame";
    s.words = fittedWords (fitted, rate);
    if (! admit (s.words)) return false;
    s.name = frameName (pickedFrame);
    s.parentA = fromFile ? fileName : session->familyName;
    const int index = session->addFrame (s);
    if (index < 0) return false;
    session->placeInTarget (index);
    repaint();
    return true;
}

void Spectrogram::chooseFamily()
{
    if (session == nullptr) return;
    const auto names = session->families();
    if (names.empty()) return;
    juce::PopupMenu menu;
    for (int i = 0; i < (int) names.size(); ++i) menu.addItem (i + 1, names[(size_t) i], true, names[(size_t) i] == session->familyName);
    auto* self = this;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (hits[cFamily])),
        [self, names] (int chosen) {
            if (chosen <= 0 || chosen > (int) names.size()) return;
            self->session->loadFamily (names[(size_t) (chosen - 1)]);
            self->clear();
            self->words();
            self->repaint();
        });
}

void Spectrogram::mouseDown (const juce::MouseEvent& e)
{
    layout();
    const auto p = e.getPosition();
    from = p;
    fromAzimuth = azimuth;
    fromDeclination = declination;
    for (int i = 0; i < kControls; ++i)
    {
        if (! hits[(size_t) i].contains (p)) continue;
        if (i == cFamily) chooseFamily();
        else if (i == cEnv) { peevers.lpcenv = peevers.lpcenv == 0 ? 1 : 0; peevers.reset(); }
        else if (i == cLogF) logF = ! logF;
        else if (i == cFlat) twoD = ! twoD;
        else if (i == cAxes) axes = ! axes;
        else if (i == cPersp) persp = ! persp;
        else if (i == cMesh) mesh = ! mesh;
        else if (i == cClear) clear();
        else if (i == cPause) paused = ! paused;
        words();
        repaint();
        return;
    }
    if (panel.contains (p)) return;
    fit();
    pickedFrame = pickAt (p);
    repaint();
}

void Spectrogram::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (panel.contains (e.getPosition())) return;
    fit();
    pickedFrame = pickAt (e.getPosition());
    takeFrame();
    repaint();
}

void Spectrogram::mouseDrag (const juce::MouseEvent& e)
{
    if (panel.contains (from)) return;
    azimuth = fromAzimuth + (float) (e.getPosition().x - from.x) * 0.006f;
    declination = juce::jlimit (-1.4f, 1.4f, fromDeclination + (float) (e.getPosition().y - from.y) * 0.006f);
    repaint();
}

bool Spectrogram::keyPressed (const juce::KeyPress& k)
{
    if (k.getKeyCode() == juce::KeyPress::returnKey) return takeFrame();
    const bool control = k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown();
    if (control && k.getKeyCode() == 'G')
    {
        if (auto* window = getTopLevelComponent()) window->setVisible (false);
        return true;
    }
    if (session == nullptr) return false;
    const bool used = session->key (k);
    if (used)
    {
        const auto lower = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
        for (int i = 0; kKeyRow[i] != 0; ++i)
            if (lower == (juce::juce_wchar) kKeyRow[i]) held.insert (session->keyOctave + i);
    }
    return used;
}

bool Spectrogram::keyStateChanged (bool)
{
    if (session == nullptr) return false;
    bool any = false;
    for (int i = 0; kKeyRow[i] != 0; ++i)
    {
        const int code = juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) kKeyRow[i]);
        const int midi = session->keyOctave + i;
        if (! juce::KeyPress::isKeyCurrentlyDown (code) && held.count (midi) > 0)
        {
            held.erase (midi);
            session->keyNoteOff (midi);
            any = true;
        }
    }
    return any;
}

void Spectrogram::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    layout();
    const int step = wheel.deltaY > 0.0f ? 1 : wheel.deltaY < 0.0f ? -1 : 0;
    if (step == 0) return;
    const auto p = e.getPosition();
    if (hits[cLength].contains (p))
    {
        kept = juce::jlimit (100, kFrames, kept + step * 50);
        trim();
        words();
        repaint();
        return;
    }
    int nfft = peevers.nfft, win = peevers.winsize, stride = peevers.stride, type = peevers.wintype;
    if (hits[cFft].contains (p))
    {
        nfft = juce::jlimit (64, 4096, step > 0 ? nfft * 2 : nfft / 2);
        win = juce::jmin (win, nfft);
    }
    else if (hits[cWin].contains (p))
    {
        win = juce::jlimit (64, juce::jmin (4096, nfft), step > 0 ? win * 2 : win / 2);
    }
    else if (hits[cStride].contains (p))
    {
        stride = juce::jlimit (16, win, stride + step * juce::jmax (1, win / 8));
    }
    else if (hits[cWindow].contains (p))
    {
        type = (type + step + Peevers::kWindows) % Peevers::kWindows;
    }
    else return;
    stride = juce::jlimit (16, win, stride);
    clear();
    peevers.setParms (nfft, win, stride, type);
    words();
    repaint();
}

bool Spectrogram::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".aif")) return true;
    return false;
}

void Spectrogram::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
        if (isInterestedInFileDrag ({ f })) { load (juce::File (f)); return; }
}

juce::Image Spectrogram::shot()
{
    juce::Image image (juce::Image::RGB, getWidth(), getHeight(), true);
    juce::Graphics g (image);
    paintEntireComponent (g, false);
    return image;
}
}
