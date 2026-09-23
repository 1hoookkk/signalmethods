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
        juce::ignoreUnused (canvasParamId);
        juce::ignoreUnused (apvts);
        setInterceptsMouseClicks (false, false);
        setTitle ("Filter response");
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
        if (! isTimerRunning()) startTimer (30);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {

        const auto aperture = getLocalBounds().toFloat();
        const auto glass = aperture;
        {
            g.setColour (juce::Colour (0xff0a0c0d));
            g.fillPath (recessPath (aperture, 1.8f));
            juce::ColourGradient rim (juce::Colours::white.withAlpha (0.38f),
                                      aperture.getX(), aperture.getY(),
                                      juce::Colours::white.withAlpha (0.06f),
                                      aperture.getRight(), aperture.getBottom(), false);
            rim.addColour (0.55, juce::Colours::white.withAlpha (0.16f));
            g.setGradientFill (rim);
            g.strokePath (recessPath (aperture, 0.0f), juce::PathStrokeType (1.2f));
        }
        {
            juce::Path face = recessPath (glass, 0.0f);
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (face);
            {
                juce::ColourGradient phosphorBed (t.glassTop(), 0.0f, glass.getY(),
                                                  t.glassBottom(), 0.0f, glass.getBottom(), false);
                g.setGradientFill (phosphorBed);
                g.fillRect (glass.expanded (1.0f));
            }
            if (gridPlate.isNull())
            {
                gridPlate = juce::ImageCache::getFromMemory (BinaryData::trench_display_grid_png,
                                                             BinaryData::trench_display_grid_pngSize);
                const juce::Colour tint = t.gridTint();
                const float boost = t.gridBoost();
                if (gridPlate.isValid())
                {
                    gridPlate = gridPlate.createCopy();
                    juce::Image::BitmapData data (gridPlate, juce::Image::BitmapData::readWrite);
                    for (int y = 0; y < data.height; ++y)
                        for (int x = 0; x < data.width; ++x)
                        {
                            const juce::Colour c = data.getPixelColour (x, y);
                            data.setPixelColour (x, y, juce::Colour::fromFloatRGBA (c.getFloatRed() * tint.getFloatRed(),
                                                                                    c.getFloatGreen() * tint.getFloatGreen(),
                                                                                    c.getFloatBlue() * tint.getFloatBlue(),
                                                                                    juce::jmin (1.0f, c.getFloatAlpha() * boost)));
                        }
                }
            }
            {
                juce::Graphics::ScopedSaveState samplingState (g);
                g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                g.setOpacity (1.0f);
                g.drawImage (gridPlate, glass, juce::RectanglePlacement::stretchToFit, false);
                if (t.themeParam ("zeroLine", 0.0) > 0.0)
                {
                    const double top = t.themeParam ("curveDbTop", 40.0);
                    const double bottom = t.themeParam ("curveDbBottom", -40.0);
                    const auto plot = plotBounds();
                    const float y = (float) (plot.getY() + top / (top - bottom) * plot.getHeight());
                    g.setColour (t.accent().withAlpha (0.55f));
                    for (float x = glass.getX() + 3.0f; x < glass.getRight() - 3.0f; x += 4.0f)
                        g.fillRect (x, y, 2.0f, 1.0f);
                }
                if (t.themeParam ("graticule", 0.0) > 0.0)
                {
                    const double top = t.themeParam ("curveDbTop", 40.0);
                    const double bottom = t.themeParam ("curveDbBottom", -40.0);
                    const auto plot = plotBounds();
                    const auto yFor = [&] (double db) { return (float) (plot.getY() + (top - db) / (top - bottom) * plot.getHeight()); };
                    const juce::Colour lines[] = { juce::Colour (0xffe0463c), juce::Colour (0xff62d64a), juce::Colour (0xff4a7fe6) };
                    const double levels[] = { 18.0, 0.0, -18.0 };
                    for (int i = 0; i < 3; ++i)
                    {
                        g.setColour (lines[i].withAlpha (0.85f));
                        const float y = yFor (levels[i]);
                        for (float x = glass.getX() + 3.0f; x < glass.getRight() - 3.0f; x += 4.0f)
                            g.fillRect (x, y, 2.0f, 1.0f);
                    }
                }
            }
            {
                juce::ColourGradient vig (juce::Colours::transparentBlack, glass.getCentreX(), glass.getCentreY(),
                                          juce::Colours::black.withAlpha (0.38f), glass.getX(), glass.getY(), true);
                g.setGradientFill (vig);
                g.fillRect (glass.expanded (1.0f));
                juce::ColourGradient lip (juce::Colours::black.withAlpha (0.34f), 0.0f, glass.getY(),
                                          juce::Colours::transparentBlack, 0.0f, glass.getY() + 6.0f, false);
                g.setGradientFill (lip);
                g.fillRect (glass.withHeight (6.0f));
                juce::ColourGradient lipL (juce::Colours::black.withAlpha (0.34f), glass.getX(), 0.0f,
                                           juce::Colours::transparentBlack, glass.getX() + 6.0f, 0.0f, false);
                g.setGradientFill (lipL);
                g.fillRect (glass.withWidth (6.0f));
                juce::ColourGradient lipR (juce::Colours::black.withAlpha (0.22f), glass.getRight(), 0.0f,
                                           juce::Colours::transparentBlack, glass.getRight() - 5.0f, 0.0f, false);
                g.setGradientFill (lipR);
                g.fillRect (glass.withLeft (glass.getRight() - 5.0f));
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
        constexpr float rTL = 1.3f, rTR = 1.3f, rBL = 1.3f, rBR = 1.3f;
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
        if (getLocalBounds().isEmpty() || path.isEmpty())
            return;
        const juce::PathStrokeType::JointStyle joint = juce::PathStrokeType::curved;
        const juce::PathStrokeType::EndCapStyle cap = juce::PathStrokeType::butt;
        const float px = 1.0f / juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        g.setColour (colour.withMultipliedAlpha (0.22f));
        g.strokePath (path, { 2.0f * px, joint, cap });
        g.setColour (colour.interpolatedWith (t.curveHighlight(), 0.30f));
        g.strokePath (path, { px, joint, cap });
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
        if (t.themeParam ("pixelTrace", 0.0) > 0.0)
        {
            drawPixelTrace (g, phos);
            return;
        }
        strokeTrace (g, responsePath, phos);
    }
    void drawPixelTrace (juce::Graphics& g, juce::Colour colour) const
    {
        const auto plot = plotBounds();
        const float cell = (float) t.themeParam ("pixelCell", 2.0);
        const double dbTop = t.curveDbTop(), dbBot = t.curveDbBottom();
        const int n = (int) traceXs.size();
        const auto yAt = [&] (float x) -> float
        {
            if (x <= traceXs.front()) return traceDbs.front();
            if (x >= traceXs.back()) return traceDbs.back();
            int lo = 0, hi = n - 1;
            while (hi - lo > 1)
            {
                const int mid = (lo + hi) / 2;
                if (traceXs[(size_t) mid] <= x) lo = mid; else hi = mid;
            }
            const float span = juce::jmax (1.0e-4f, traceXs[(size_t) hi] - traceXs[(size_t) lo]);
            const float f = (x - traceXs[(size_t) lo]) / span;
            return traceDbs[(size_t) lo] + (traceDbs[(size_t) hi] - traceDbs[(size_t) lo]) * f;
        };
        const int rows = juce::jmax (1, (int) std::floor (plot.getHeight() / cell));
        int prevRow = -1;
        g.setColour (colour);
        for (float cx = plot.getX(); cx + cell <= plot.getRight() + 0.01f; cx += cell)
        {
            const float db = yAt (cx + cell * 0.5f);
            const float yt = (float) ((dbTop - db) / (dbTop - dbBot));
            const int row = juce::jlimit (0, rows - 1, (int) std::floor (yt * (float) rows));
            const float y = plot.getY() + (float) row * cell;
            g.fillRect (cx, y, cell, cell);
            if (prevRow >= 0 && std::abs (row - prevRow) > 1)
            {
                const int a = juce::jmin (row, prevRow) + 1, b = juce::jmax (row, prevRow) - 1;
                for (int r = a; r <= b; ++r)
                    g.fillRect (cx, plot.getY() + (float) r * cell, cell, cell);
            }
            prevRow = row;
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

        g.setColour (col.withMultipliedAlpha (0.05f));
        g.strokePath (path, { kTraceWidth * 1.9f, juce::PathStrokeType::curved,
                              juce::PathStrokeType::butt });
        g.setColour (col.withMultipliedAlpha (0.10f));
        g.strokePath (path, { kTraceWidth * 1.1f, juce::PathStrokeType::curved,
                              juce::PathStrokeType::butt });
        g.setColour (col);
        g.strokePath (path, { kTraceWidth, juce::PathStrokeType::curved,
                              juce::PathStrokeType::butt });
    }
    Theme t;
    mutable juce::Image gridPlate;
    juce::Path responsePath;

    static constexpr float kTraceWidth = 1.0f;
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
