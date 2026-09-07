#include "Spectrogram.h"

#include <cmath>

namespace hs
{
Spectrogram::Spectrogram (Audio* tap) : audio (tap)
{
    formats.registerBasicFormats();
    pulled.assign (16384, 0.0f);
    setSize (900, 560);
    words();
    if (audio != nullptr) startTimer (30);
}

Spectrogram::~Spectrogram() { stopTimer(); }

void Spectrogram::clear()
{
    frames.clear();
    history.clear();
    cursor = 0;
    length = 0;
    peevers.reset();
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
        if ((int) frames.size() > kFrames) frames.erase (frames.begin());
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
    length = n;
    feed (mono.data(), n);
    repaint();
    return true;
}

void Spectrogram::timerCallback()
{
    if (audio == nullptr) return;
    int got = audio->pull (pulled.data(), (int) pulled.size());
    if (got <= 0) return;
    bool sound = false;
    for (int i = 0; i < got; ++i) sound = sound || std::abs (pulled[(size_t) i]) > 1.0e-6f;
    if (! sound && fromFile) return;
    if (fromFile) { clear(); fromFile = false; rate = 44100.0; }
    for (int i = 0; i < got; ++i) pulled[(size_t) i] *= 32767.0f;
    feed (pulled.data(), got);
    repaint();
}

void Spectrogram::resized() { layout(); }

void Spectrogram::layout()
{
    auto r = getLocalBounds().reduced (10);
    strip = r.removeFromBottom (18);
    surface = r;
    words();
}

void Spectrogram::words()
{
    const juce::String names[8] = {
        "FFT " + juce::String (peevers.nfft),
        "Win " + juce::String (peevers.winsize),
        "Stride " + juce::String (peevers.stride),
        juce::String (Peevers::windowNames[peevers.wintype]),
        "Env", "LogF", "2D", "Axes"
    };
    const auto font = Look::font (11.0f);
    int x = strip.getX();
    for (int i = 0; i < 8; ++i)
    {
        labels[(size_t) i] = names[i];
        const int w = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, names[i]));
        hits[(size_t) i] = { x, strip.getY(), w, strip.getHeight() };
        x += w + 18;
    }
}

juce::Point<float> Spectrogram::project (float bin, float value, float depth) const
{
    const float x = bin / (float) juce::jmax (1, peevers.nfft2) - 0.5f;
    const float y = value / 255.0f / 3.0f;
    const float z = depth - 0.5f;
    const float ca = std::cos (azimuth), sa = std::sin (azimuth);
    const float cd = std::cos (declination), sd = std::sin (declination);
    const float rx = x * ca + z * sa;
    const float rz = -x * sa + z * ca;
    const float sy = y * cd + rz * sd;
    const float spanX = (float) surface.getWidth() * 0.52f;
    const float spanY = (float) surface.getHeight() * 0.78f;
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
    for (int f = 0; f <= count; f += 20)
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
    for (int f = count; f >= 0; f -= 20)
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
        const int n2 = peevers.nfft2;
        for (int i = 0; i < count; ++i)
        {
            const auto& fr = frames[(size_t) i];
            const float depth = (float) (count - 1 - i) / (float) juce::jmax (1, count - 1);
            juce::Path path;
            int last = -1;
            juce::Point<float> prev;
            const int bins = juce::jmin (n2, (int) fr.size());
            for (int bin = 0; bin < bins; ++bin)
            {
                const float px = logF ? peevers.zlogpos[(size_t) bin] : (float) bin;
                const auto p = project (px, fr[(size_t) bin], depth);
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
            if (last >= 0)
            {
                g.setColour (tint (last));
                g.strokePath (path, juce::PathStrokeType (1.0f));
            }
        }
    }
    g.setFont (Look::font (11.0f));
    for (int i = 0; i < 8; ++i)
    {
        const bool toggle = i >= 4;
        const bool on = i == 4 ? peevers.lpcenv != 0 : i == 5 ? logF : i == 6 ? twoD : axes;
        g.setColour (toggle ? (on ? Look::ink : Look::dim) : Look::text);
        g.drawText (labels[(size_t) i], hits[(size_t) i].toFloat(), juce::Justification::centredLeft);
    }
}

void Spectrogram::mouseDown (const juce::MouseEvent& e)
{
    words();
    for (int i = 4; i < 8; ++i)
    {
        if (! hits[(size_t) i].contains (e.getPosition())) continue;
        if (i == 4) { peevers.lpcenv = peevers.lpcenv == 0 ? 1 : 0; peevers.reset(); }
        else if (i == 5) logF = ! logF;
        else if (i == 6) twoD = ! twoD;
        else axes = ! axes;
        words();
        repaint();
        return;
    }
    from = e.getPosition();
    fromAzimuth = azimuth;
    fromDeclination = declination;
}

void Spectrogram::mouseDrag (const juce::MouseEvent& e)
{
    if (strip.contains (from)) return;
    azimuth = fromAzimuth + (float) (e.getPosition().x - from.x) * 0.006f;
    declination = juce::jlimit (-1.4f, 1.4f, fromDeclination + (float) (e.getPosition().y - from.y) * 0.006f);
    repaint();
}

void Spectrogram::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    words();
    const int step = wheel.deltaY > 0.0f ? 1 : wheel.deltaY < 0.0f ? -1 : 0;
    if (step == 0) return;
    int nfft = peevers.nfft, win = peevers.winsize, stride = peevers.stride, type = peevers.wintype;
    if (hits[0].contains (e.getPosition()))
    {
        nfft = juce::jlimit (64, 4096, step > 0 ? nfft * 2 : nfft / 2);
        win = juce::jmin (win, nfft);
    }
    else if (hits[1].contains (e.getPosition()))
    {
        win = juce::jlimit (64, juce::jmin (4096, nfft), step > 0 ? win * 2 : win / 2);
    }
    else if (hits[2].contains (e.getPosition()))
    {
        stride = juce::jlimit (16, win, stride + step * juce::jmax (1, win / 8));
    }
    else if (hits[3].contains (e.getPosition()))
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
