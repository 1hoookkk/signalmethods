#pragma once
#include "Theme.h"
#include "../parameters/TrenchParameters.h"
#include "../dsp/TrenchDspBridge.h"
#include "BinaryData.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
namespace trench::ui
{
class GraphDisplay : public juce::Component,
                     public juce::SettableTooltipClient,
                     private juce::Timer
{
public:
    void playSeedPulse()
    {
        if (traceXs.empty() || traceDbs.size() != traceXs.size())
            return;
        pulseOldDbs = traceDbs;
        pulsePhase = PulseCompress;
        pulseElapsedMs = 0.0;
        startTimer (30);
        repaint();
    }
    GraphDisplay (const Theme& theme,
                  juce::AudioProcessorValueTreeState& apvts, const juce::String& canvasParamId)
        : t (theme)
    {
        juce::ignoreUnused (apvts, canvasParamId);

        setInterceptsMouseClicks (false, false);
    }

    void announce (const juce::String& text)
    {
        amountCueText = text;
        amountCueAlpha = 1.0f;
        startTimer (30);
        repaint();
    }
    void updateFromCoeffs (const float coeffs[trench::kUiCoeffCount], float boost, double sr)
    {
        const float qNow = qParam != nullptr ? qParam->getValue() : 0.0f;
        bool same = juce::approximatelyEqual (sr, lastSr) && juce::approximatelyEqual (boost, lastBoost)
                    && juce::approximatelyEqual (qNow, lastQ);
        lastQ = qNow;
        for (int i = 0; same && i < trench::kUiCoeffCount; ++i)
            same = juce::approximatelyEqual (coeffs[i], lastCoeffs[i]);
        if (same && haveCurve)
            return;
        for (int i = 0; i < trench::kUiCoeffCount; ++i) lastCoeffs[i] = coeffs[i];
        lastBoost = boost; lastSr = sr; haveCurve = true;
        if (! bootStarted)
        {
            bootStarted = true;
            startTimer (30);
        }
        const auto plot = plotBounds();
        if (plot.isEmpty())
            return;
        const double dbTop = t.curveDbTop(), dbBot = t.curveDbBottom();

        const double fLo = 20.0, fHi = 20000.0;

        const int kOversample = 4;
        const int bins = juce::jmax (192, juce::roundToInt (plot.getWidth()));
        const int N = bins * kOversample;
        juce::Path path;
        bool started = false;
        float prevX = 0.0f, prevY = 0.0f;
        traceXs.clear();
        traceDbs.clear();
        traceXs.reserve (bins);
        traceDbs.reserve (bins);
        std::vector<float> rawYs, rawXs;
        rawYs.reserve (bins);
        rawXs.reserve (bins);
        for (int b = 0; b < bins; ++b)
        {
            double peakDb = 0.0;
            bool have = false;
            for (int k = 0; k < kOversample; ++k)
            {
                const int i = b * kOversample + k;
                const double frac = (double) i / (double) (N - 1);
                const double f = fLo * std::pow (fHi / fLo, frac);
                const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;

                const float cw = (float) std::cos (w), sw = (float) std::sin (w);
                const float c2 = cw * cw - sw * sw, s2 = 2.0f * sw * cw;

                float power = (float) boost * (float) boost;
                for (int s = 0; s < trench::kUiStageCount; ++s)
                {
                    const float b0 = (float) coeffs[s*5+0], b1 = (float) coeffs[s*5+1],
                                b2 = (float) coeffs[s*5+2];
                    const float a1 = (float) coeffs[s*5+3], a2 = (float) coeffs[s*5+4];
                    const float nr = b2*c2 + b1*cw + b0, ni = b2*s2 + b1*sw;
                    const float dr = a2*c2 + a1*cw + 1.0f, di = a2*s2 + a1*sw;
                    power *= (nr*nr + ni*ni) / (dr*dr + di*di);
                }

                if (std::isnan (power))
                    continue;
                const double db = 10.0 * std::log10 ((double) power);
                if (! have || std::abs (peakDb) < std::abs (db))
                {
                    peakDb = db;
                    have = true;
                }
            }
            if (! have)
                continue;
            const double db = peakDb;
            const double frac = (double) b / (double) (bins - 1);

            const double yt = juce::jlimit (-0.25, 1.25, (dbTop - db) / (dbTop - dbBot));

            const float xRaw = plot.getX() + (float) frac * plot.getWidth();
            const float x = std::floor (xRaw) + 0.5f;
            rawXs.push_back (xRaw);

            const float y = plot.getY() + (float) yt * plot.getHeight();
            rawYs.push_back (y);
            prevX = x;
            prevY = y;
            traceXs.push_back (x);
            traceDbs.push_back ((float) db);
        }

        const int perPixel = juce::jmax (1, juce::roundToInt ((float) bins / plot.getWidth()));
        const int n = (int) rawYs.size();
        for (int i = 0; i < n; ++i)
        {
            const float before = rawYs[(size_t) juce::jmax (0, i - perPixel)];
            const float after  = rawYs[(size_t) juce::jmin (n - 1, i + perPixel)];

            const bool flat = std::abs (after - before) < 0.15f;

            const float y = flat ? std::floor (rawYs[(size_t) i]) + 0.5f : rawYs[(size_t) i];
            const float x = flat ? traceXs[(size_t) i] : rawXs[(size_t) i];
            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {

                path.lineTo (x, y);
            }
        }
        responsePath = std::move (path);
        traceCache = {};
        if (! isTimerRunning()) startTimer (30);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {

        const auto aperture = getLocalBounds().toFloat();
        const auto glass = aperture;
        {
            g.setColour (juce::Colour (0xff14171a));
            g.fillPath (recessPath (aperture, 1.0f));
        }
        {
            juce::Path face = recessPath (glass, 0.0f);
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (face);
            if (displayPlate.isNull())
                displayPlate = juce::ImageCache::getFromMemory (BinaryData::display_bitmap4613_png,
                                                                BinaryData::display_bitmap4613_pngSize);
            {
                juce::Graphics::ScopedSaveState samplingState (g);
                g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                g.drawImage (displayPlate, glass.expanded (1.0f),
                             juce::RectanglePlacement::stretchToFit, false);
            }
            {
                juce::ColourGradient vig (juce::Colours::transparentBlack, glass.getCentreX(), glass.getCentreY(),
                                          juce::Colours::black.withAlpha (0.38f), glass.getX(), glass.getY(), true);
                g.setGradientFill (vig);
                g.fillRect (glass.expanded (1.0f));
                juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.13f), 0.0f, glass.getY(),
                                            juce::Colours::transparentWhite, 0.0f, glass.getY() + glass.getHeight() * 0.42f, false);
                gloss.addColour (0.55, juce::Colours::white.withAlpha (0.05f));
                g.setGradientFill (gloss);
                g.fillRect (glass.withHeight (glass.getHeight() * 0.42f));
            }
            {

                juce::Graphics::ScopedSaveState bootSave (g);
                const bool sweeping = bootStarted && bootReveal < 1.0f;
                const auto plot = plotBounds();
                const float e = bootReveal * bootReveal * (3.0f - 2.0f * bootReveal);
                const float frontX = plot.getX() + e * plot.getWidth();
                if (sweeping)
                    g.reduceClipRegion (juce::Rectangle<int> ((int) glass.getX(), (int) glass.getY(),
                                                              (int) (frontX - glass.getX()) + 1,
                                                              (int) glass.getHeight() + 2));

                drawGraticule (g);
                drawVignette (g);
                drawResponseTrace (g);
                if (sweeping)
                {
                    g.setColour (t.curveColour().withAlpha (0.10f));
                    g.fillRect (frontX - 7.0f, plot.getY(), 7.0f, plot.getHeight());
                    g.setColour (t.curveColour().brighter (0.35f).withAlpha (0.75f));
                    g.drawLine (frontX - 1.0f, plot.getY(), frontX - 1.0f, plot.getBottom(), 1.2f);
                }
            }
            if (amountCueAlpha > 0.01f)
            {
                g.setFont (telemetryFont (9.8f, false));
                g.setColour (juce::Colour (0xffcfe8de).withAlpha (0.94f * amountCueAlpha));
                g.drawText (amountCueText,
                            juce::Rectangle<float> (glass.getX() + 8.0f, glass.getY() + 4.0f, 190.0f, 14.0f),
                            juce::Justification::centredLeft, false);
            }

        }
    }
private:

    static juce::Path recessPath (juce::Rectangle<float> r, float e)
    {
        constexpr float rTL = 5.5f, rTR = 2.8f, rBL = 4.3f, rBR = 4.7f;
        const float x0 = r.getX() - e, y0 = r.getY() - e;
        const float x1 = r.getRight() + e, y1 = r.getBottom() + e;
        const auto k = [e] (float rad) { return juce::jmax (0.5f, rad + e); };
        juce::Path p;
        p.startNewSubPath (x0 + k (rTL), y0);
        p.lineTo (x1 - k (rTR), y0);
        p.quadraticTo (x1, y0, x1, y0 + k (rTR));
        p.lineTo (x1, y1 - k (rBR));
        p.quadraticTo (x1, y1, x1 - k (rBR), y1);
        p.lineTo (x0 + k (rBL), y1);
        p.quadraticTo (x0, y1, x0, y1 - k (rBL));
        p.lineTo (x0, y0 + k (rTL));
        p.quadraticTo (x0, y0, x0 + k (rTL), y0);
        p.closeSubPath();
        return p;
    }

    void drawVignette (juce::Graphics& g)
    {
        const auto b = getLocalBounds().toFloat().reduced (1.15f);

        constexpr float depth = 0.06f;
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (b, 7.8f);
        g.reduceClipRegion (clip);
        juce::ColourGradient v (juce::Colours::transparentBlack,
                                b.getCentreX(), b.getY() + b.getHeight() * 0.40f,
                                juce::Colours::black.withAlpha (depth),
                                b.getX(), b.getBottom(), true);
        v.addColour (0.55, juce::Colours::black.withAlpha (depth * 0.14f));
        g.setGradientFill (v);
        g.fillRect (b);
    }

    void drawGraticule (juce::Graphics&) {}

    juce::Rectangle<float> plotBounds() const
    {
        return getLocalBounds().toFloat().reduced (6.0f, 5.0f);
    }

    juce::Colour responseColour() const
    {
        return t.curveColour();
    }

    void strokeTrace (juce::Graphics& g, const juce::Path& path, juce::Colour colour) const
    {

        const int kSS = juce::jmax (3, (int) std::ceil (
            g.getInternalContext().getPhysicalPixelScaleFactor()));
        const auto area = getLocalBounds();
        if (area.isEmpty() || path.isEmpty())
            return;
        if (! traceCache.isValid()
            || traceCache.getWidth() != area.getWidth() * kSS
            || traceCache.getHeight() != area.getHeight() * kSS
            || cachedColour != colour)
        {
            traceCache = juce::Image (juce::Image::ARGB,
                                      area.getWidth() * kSS, area.getHeight() * kSS, true);
            cachedColour = colour;
            juce::Graphics ig (traceCache);
            ig.addTransform (juce::AffineTransform::scale ((float) kSS));

            ig.setColour (colour);
            ig.strokePath (path, { kTraceWidth, juce::PathStrokeType::curved,
                                   juce::PathStrokeType::butt });
        }
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (traceCache,
                                juce::AffineTransform::scale (1.0f / (float) kSS), false);
    }
    void drawResponseTrace (juce::Graphics& g) const
    {
        if (pulsePhase != PulseIdle)
        {
            drawSeedPulseTrace (g);
            return;
        }
        const auto phos = responseColour();
        const auto plot = plotBounds();
        if (traceXs.empty() || traceXs.size() != traceDbs.size() || plot.isEmpty())
        {
            if (! responsePath.isEmpty())
            {

                strokeTrace (g, responsePath, phos);
            }
            return;
        }

        g.setOpacity (1.0f);
        strokeTrace (g, responsePath, phos);

        {
            const double dbTop = t.curveDbTop(), dbBot = t.curveDbBottom();
            const int N = (int) traceDbs.size(), win = juce::jmax (3, N / 40);
            g.setColour (phos);
            for (int i = win; i < N - win; ++i)
            {
                const float v = traceDbs[(size_t) i];
                bool peak = true;
                float floor = v;
                for (int k = i - win; k <= i + win && peak; ++k)
                {
                    if (k != i && traceDbs[(size_t) k] >= v) peak = false;
                    floor = juce::jmin (floor, traceDbs[(size_t) k]);
                }
                if (! peak || v - floor < 2.5f) continue;
                const float yt = (float) juce::jlimit (0.0, 1.0, (dbTop - v) / (dbTop - dbBot));
                const float x = traceXs[(size_t) i], y = plot.getY() + yt * plot.getHeight();
                g.drawLine (x - 3.0f, y, x + 3.0f, y, 1.0f);
                g.drawLine (x, y - 3.0f, x, y + 3.0f, 1.0f);
            }
        }
    }
    void drawSeedPulseTrace (juce::Graphics& g) const
    {
        if (traceXs.empty() || traceXs.size() != traceDbs.size() || traceXs.size() != pulseOldDbs.size())
            return;
        const auto plot = plotBounds();
        if (plot.isEmpty())
            return;
        const double dbTop = t.curveDbTop(), dbBot = t.curveDbBottom();
        const float centreY = plot.getY() + plot.getHeight() * 0.5f;
        const size_t N = traceXs.size();
        float squash = 1.0f;
        float progress = 0.0f;
        bool jitter = false;
        if (pulsePhase == PulseCompress)
        {
            progress = (float) juce::jlimit (0.0, 1.0, pulseElapsedMs / kPulseCompressMs);
            squash = juce::jmap (progress, 1.0f, 0.06f);
        }
        else if (pulsePhase == PulseStatic)
        {
            squash = 0.06f;
            jitter = true;
        }
        else
        {
            progress = (float) juce::jlimit (0.0, 1.0, pulseElapsedMs / kPulseRedrawMs);
            squash = juce::jmap (progress, 0.06f, 1.0f);
        }
        juce::Path path;
        for (size_t i = 0; i < N; ++i)
        {
            double db = pulseOldDbs[i];
            if (pulsePhase == PulseRedraw)
                db = pulseOldDbs[i] + (traceDbs[i] - pulseOldDbs[i]) * progress;
            double yt = juce::jlimit (-0.06, 1.06, (dbTop - db) / (dbTop - dbBot));
            float y = plot.getY() + (float) yt * plot.getHeight();
            y = centreY + (y - centreY) * squash;
            if (jitter)
                y += pulseRng.nextFloat() * 3.0f - 1.5f;
            if (i == 0) path.startNewSubPath (traceXs[i], y);
            else        path.lineTo (traceXs[i], y);
        }
        const float heat = pulsePhase == PulseRedraw ? (1.0f - progress) : 1.0f;
        const auto hot = juce::Colour (0xffe9dfc7).interpolatedWith (juce::Colour (0xffc9853f), 0.38f);
        const auto col = t.curveColour().interpolatedWith (hot, heat);

        g.setColour (col);
        g.strokePath (path, { 1.0f, juce::PathStrokeType::curved,
                              juce::PathStrokeType::butt });
    }
    Theme t;
    mutable juce::Image displayPlate;
    mutable juce::Image gridPlate;
    juce::Path responsePath;

    static constexpr float kTraceWidth = 1.1f;
    mutable juce::Image traceCache;
    mutable juce::Colour cachedColour;
    std::vector<float> traceXs;
    std::vector<float> traceDbs;
    float lastCoeffs[trench::kUiCoeffCount] = {};
    float lastBoost = -1.0f;
    double lastSr = 0.0;
    bool haveCurve = false;
    juce::RangedAudioParameter* qParam     = nullptr;
    float lastQ = -1.0f;
    enum PulsePhase { PulseIdle, PulseCompress, PulseStatic, PulseRedraw };
    PulsePhase pulsePhase = PulseIdle;
    double pulseElapsedMs = 0.0;
    std::vector<float> pulseOldDbs;
    mutable juce::Random pulseRng;
    static constexpr double kPulseCompressMs = 60.0;
    static constexpr double kPulseStaticMs   = 80.0;
    static constexpr double kPulseRedrawMs   = 100.0;
    juce::String amountCueText;
    float amountCueAlpha = 0.0f;

    bool bootStarted = false;
    float bootReveal = 0.0f;
    void timerCallback() override
    {
        if (bootStarted && bootReveal < 1.0f)
            bootReveal = juce::jmin (1.0f, bootReveal + 30.0f / 550.0f);
        amountCueAlpha = juce::jmax (0.0f, amountCueAlpha - 0.04f);
        if (pulsePhase != PulseIdle)
        {
            pulseElapsedMs += 30.0;
            if (pulsePhase == PulseCompress && pulseElapsedMs >= kPulseCompressMs)
            {
                pulsePhase = PulseStatic;
                pulseElapsedMs = 0.0;
            }
            else if (pulsePhase == PulseStatic && pulseElapsedMs >= kPulseStaticMs)
            {
                pulsePhase = PulseRedraw;
                pulseElapsedMs = 0.0;
            }
            else if (pulsePhase == PulseRedraw && pulseElapsedMs >= kPulseRedrawMs)
            {
                pulsePhase = PulseIdle;
                pulseOldDbs.clear();
            }
        }
        const bool bootSweeping = bootStarted && bootReveal < 1.0f;
        if (! bootSweeping && amountCueAlpha <= 0.01f && pulsePhase == PulseIdle)
            stopTimer();
        repaint();
    }
};
}
