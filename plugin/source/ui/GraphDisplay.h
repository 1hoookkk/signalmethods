#pragma once
#include "Theme.h"
#include "../parameters/TrenchParameters.h"
#include "../dsp/TrenchDspBridge.h"   // kUiCoeffCount: the cascade is 7x5, not 6x5
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
    void setBodyName (const juce::String& s) { if (bodyName != s) { bodyName = s; repaint(); } }
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
        // THE GLASS IS DISPLAY-ONLY. It takes no clicks at all.
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
        // X3 display law, FUN_1802c37a0. The SPAN is theirs: a hard 20 Hz -
        // 20 kHz that does NOT fold down to Nyquist at low sample rates (their
        // axis is 20 * 1000^(i/7999); the constant 0.00012501562 in the binary
        // IS 1/7999).
        //
        // The point COUNT is not theirs and deliberately so. They use a fixed
        // 8000 and compute the display on demand; we recompute on every
        // coefficient change, so 8000 measured 458 ms worst-case body-switch
        // idle against 39 ms adaptive - 12x, and past the 250 ms FaceShot gate.
        // At 4 samples per pixel the curve is already denser than the display
        // can show, so this costs nothing visible. Raise it only alongside a
        // recompute throttle.
        const double fLo = 20.0, fHi = 20000.0;
        // X3 decimation, FUN_1801376d0: they evaluate 8000 points and DISPLAY
        // 2000, keeping the value of LARGEST ABSOLUTE MAGNITUDE in each group
        // of four. Not an average, not a sample - the extremum, sign kept.
        // That is why their apexes stay needles and their notches stay deep:
        // with 4x oversample and peak-hold, no feature can be averaged away or
        // missed between samples.
        //
        // Same 4:1 here, one bin per pixel column. This is the crisp-needle
        // trace law (Tyson 2026-08-04) arrived at by measurement rather than
        // by eye - and one vertex per column is what makes the relock pass
        // below correct, since samples no longer fight over a column.
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
                // They evaluate at cos/sin of w and 2w rather than a complex
                // exponential, accumulate the POWER ratio |B|^2/|A|^2 across
                // the cascade, and take 10*log10 ONCE at the end. Held in
                // float32 as they hold it: a deep notch underflows the running
                // product to zero and the point drops off the bottom of the
                // plot, which is the notch depth reading correctly rather than
                // being floored.
                const float cw = (float) std::cos (w), sw = (float) std::sin (w);
                const float c2 = cw * cw - sw * sw, s2 = 2.0f * sw * cw;
                // ALL SEVEN sections. Plotting six drew a curve that was not
                // the cascade the audio runs - the seventh section's peak or
                // notch was simply absent from the display.
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
                // No denominator epsilon and no dB floor - the X3 has neither.
                // A zero product is -inf dB, whose magnitude wins its bin and
                // which the yt clamp parks on the plot floor. Only NaN is
                // refused, and only because it cannot draw.
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

            // Trace law (Tyson 2026-08-04): UNCLAMPED vertex spikes - a
            // resonance apex stays a crisp needle, never rounded into the
            // plot. The generous guard only rejects NaN, never softens peaks.
            const double yt = juce::jlimit (-0.25, 1.25, (dbTop - db) / (dbTop - dbBot));
            // Pixel-CENTRE lock on X: floor()+0.5 puts the stroke's axis down
            // the middle of a pixel column, which is the crispest a 1.1px line
            // can land.
            const float xRaw = plot.getX() + (float) frac * plot.getWidth();
            const float x = std::floor (xRaw) + 0.5f;
            rawXs.push_back (xRaw);
            // Y IS NOT LOCKED HERE (Tyson 2026-08-12, "fix it", off the zoomed
            // glass). Locking Y the same way snapped every vertex to a whole
            // pixel ROW, so a slope became a literal staircase in the PATH -
            // before any rasterising. The supersampled stroke was then
            // faithfully antialiasing a staircase, which is why more
            // supersampling never helped. Y is carried at full precision and
            // re-locked below, but only where the curve is actually flat.
            const float y = plot.getY() + (float) yt * plot.getHeight();
            rawYs.push_back (y);
            prevX = x;
            prevY = y;
            traceXs.push_back (x);
            traceDbs.push_back ((float) db);
        }
        // FLAT RUNS RE-LOCK, SLOPES DO NOT. A horizontal run wants its axis on
        // a pixel centre or it renders as two grey rows; a slope wants its true
        // position or it renders as steps. Locking is decided per vertex from
        // the curve's own local gradient, so the long unity shelf stays a crisp
        // hairline and the cutoff skirt comes out straight.
        // The window has to span a WHOLE PIXEL of travel, not one sample. That
        // used to need a 4-wide window, because 4 samples shared a column and
        // neighbours differed by a fraction of a pixel even down the steepest
        // skirt - so comparing neighbours called the whole path flat and
        // re-locked all of it, leaving the staircase untouched. Since the X3
        // decimation there is now exactly ONE vertex per column, so neighbours
        // ARE a whole pixel apart and the window is 1.
        const int perPixel = juce::jmax (1, juce::roundToInt ((float) bins / plot.getWidth()));
        const int n = (int) rawYs.size();
        for (int i = 0; i < n; ++i)
        {
            const float before = rawYs[(size_t) juce::jmax (0, i - perPixel)];
            const float after  = rawYs[(size_t) juce::jmin (n - 1, i + perPixel)];
            // 0.15px over two pixels of travel. The unity shelf is EXACTLY
            // flat, so it locks at any threshold above zero; anything looser
            // caught the shallow rise into the resonant peak as well and
            // stepped it (0.5 did exactly that).
            const bool flat = std::abs (after - before) < 0.15f;
            // On a flat run BOTH axes lock - that is what keeps the unity shelf
            // a single crisp row. On a slope NEITHER does: locking X alone
            // still stairsteps, because ~4 samples share one pixel column and
            // the path walks sideways then drops.
            const float y = flat ? std::floor (rawYs[(size_t) i]) + 0.5f : rawYs[(size_t) i];
            const float x = flat ? traceXs[(size_t) i] : rawXs[(size_t) i];
            if (! started)
            {
                path.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                // Hard-computed hardware look: linear multi-point vertex
                // connection, no smooth splines (fixed-resolution table).
                path.lineTo (x, y);
            }
        }
        responsePath = std::move (path);
        traceCache = {};   // the supersampled trace is stale
        if (! isTimerRunning()) startTimer (30);
        repaint();
    }
    // No mouse handlers: the glass is display-only and intercepts no clicks.
    // A hover highlight lived here "advertising" a screen drag — dead code
    // twice over (the drag was removed 2026-07-30, and with click-through on,
    // mouseEnter could never fire). Cut 2026-08-15: the face must never
    // advertise a gesture it does not have.
    void paint (juce::Graphics& g) override
    {
        // THE GLASS FOLLOWS THE RECESS'S OWN CORNERS (Tyson 2026-08-15 "Still
        // off", the circled top-right pinch). The plate's recess is hand-baked
        // and its four corners are UNEQUAL — traced off df2_panel_beige.png:
        // TL ~14, TR ~7, BL ~11, BR ~12 plate px. No single radius can sit
        // flush in all four; recessPath() carries each corner's measured
        // curve. The inner black border stays - it is what sells the LCD
        // being mounted BEHIND the chassis rather than printed on it.
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
            // TUBE GLASS (Tyson 2026-08-29 "bring that clean glass look back"):
            // a soft gloss across the top of the window and a faint vignette
            // pulling the corners down, the way the CRT face read.
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
            // Restraint law (Tyson 2026-07-30): the coral curve is pristine -
            // no SLAM/GRIT/ceiling effects ever touch it. The glass carries only
            // the filter's law and the boot signature.
            {
                // Boot sweep: the first curve draws itself on left-to-right like
                // a scope warming up, a bright scanline riding the reveal front.
                juce::Graphics::ScopedSaveState bootSave (g);
                const bool sweeping = bootStarted && bootReveal < 1.0f;
                const auto plot = plotBounds();
                const float e = bootReveal * bootReveal * (3.0f - 2.0f * bootReveal);
                const float frontX = plot.getX() + e * plot.getWidth();
                if (sweeping)
                    g.reduceClipRegion (juce::Rectangle<int> ((int) glass.getX(), (int) glass.getY(),
                                                              (int) (frontX - glass.getX()) + 1,
                                                              (int) glass.getHeight() + 2));
                // The cube followed the M/Q/T bars out (Tyson 2026-08-11: the
                // cube IS Morpheus's identity, and it was decorative telemetry
                // on a glass that should carry one curve). The wheels state
                // MORPH and Q physically, where the hand is.
                // DEPTH ORDER (Tyson 2026-08-12): black curve -> glass surface
                // -> grid. The graticule is under the glass; the trace is on
                // top of it. Painting the grid last put it ON the surface,
                // which is why the display had no depth - everything was in
                // one plane.
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
            // The gloss sweep and corner vignette that lived here were both
            // faded to alpha 0.0 over successive matte-glass verdicts - fills
            // that painted nothing. Deleted, not softened further.
        }
    }
private:
    /// The recess's outline with its four MEASURED, unequal corner curves
    /// (plate px TL 14 / TR 7 / BL 11 / BR 12 -> editor 5.5 / 2.8 / 4.3 / 4.7
    /// at the 326/828 map). `e` expands (+) or insets (-) the rect, radii
    /// following, so the dark backing ring, the glass clip and the hover
    /// stroke all track the same geometry.
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
    // The cover over the panel: a sheen falling from the top edge and a bright
    // catch on the top and left cut edge. Drawn HERE, between the graticule and
    // the trace, so the grid reads as lying under glass and the curve as
    // sitting on it. It used to be baked into the glass bitmap, which forced
    // the grid above it.
    // THE COVER SHEET. A cover has THICKNESS: it catches a low sheen on its
    // face and its cut edge catches brighter still, with a dark returning edge
    // opposite. Drawn between the graticule and the trace so the rules read as
    // lying UNDER the sheet and the curve as sitting on it.

    // The corners of a lit panel fall away. Kept SHALLOW: a pale reflective
    // panel takes far less falloff than smoked glass before the corners read as
    // dirty rather than deep (0.38 suited the smoked build and looked like a
    // smudge here).
    void drawVignette (juce::Graphics& g)
    {
        const auto b = getLocalBounds().toFloat().reduced (1.15f);
        // Back to a real aperture depth with the smoked panel (0.15 was the
        // pale-LCD value, where anything more read as a smudge).
        // FLAT (Tyson 2026-08-12: "the glass has too much depth"). The goal
        // build's field falls to only 0.86 of centre at the corner; ours was
        // driving a 0.34 vignette on top of an already-shaded bitmap.
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

    // THE GRATICULE LIVES IN THE GLASS ASSET, and nothing is drawn here. One
    // ruling, authored where the field is authored: tools/make_glass.py bakes
    // the rules into trench_glass.png with the theme's stated markings colour
    // (DRAW_GRID; the 2026-08-15 face rules in near-black ink #0B0E16 on the
    // #202A40 field). Drawing a second graticule in code over the baked one is
    // the doubling caught on 2026-08-12 — if the ruling needs to change, the
    // change goes through make_glass.py, never through a stroke pass here.
    // tools/make_display_grid.py (the 6x crisp bitmap) stays on disk for the
    // day a code-drawn ruling over a ruleless field is wanted instead.
    void drawGraticule (juce::Graphics&) {}


    // THE CELL STACK IS GONE (Tyson 2026-08-11). The glass was split into a
    // state cell on top and a trace cell below, which made the response one
    // piece of telemetry among several. With the cube and the axis bars both
    // out, the curve takes the whole aperture: one clean line on smoked slate.
    juce::Rectangle<float> plotBounds() const
    {
        return getLocalBounds().toFloat().reduced (6.0f, 5.0f);
    }
    // The M/Q/T axis bars were KILLED 2026-08-10 ("the biggest giveaway"):
    // Morpheus needs bars because knobs cannot show position — our wheels
    // glow 0..100, so the face already carries its axis bars in hardware.
    // Never re-add a screen duplicate of what the wheels state.
    juce::Colour responseColour() const
    {
        return t.curveColour();
    }
    /// The trace, drawn at 3x into an offscreen buffer and area-averaged down.
    /// A 1px stroke on a slope steeper than 45 degrees MUST cover two whole
    /// rows to stay connected - that is correct rasterising, but it reads as a
    /// staircase. Supersampling gives the slope real partial coverage while the
    /// flat runs still land on a single row. Cached: rebuilt only when the path
    /// or the size changes, never per frame.
    void strokeTrace (juce::Graphics& g, const juce::Path& path, juce::Colour colour) const
    {
        // Supersample at least 3x for the 1x face, and never below the real
        // device scale (HiDPI audit 2026-08-09) - a fixed 3x cache was fine
        // at 1x and softens on a 4x monitor. The cache key includes the
        // scale via its own dimensions.
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
            // Stroke in the REAL colour. Rendering white and then using the
            // buffer as an alpha mask (drawImageTransformed's fill-alpha path)
            // bypassed the resampler entirely - the downscale came out pixel
            // identical to the 1x stroke, measured.
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
                // Trace law (2026-08-04): single 1.0 px flat opaque stroke.
                strokeTrace (g, responsePath, phos);
            }
            return;
        }
        // Trace law (lock 2026-08-06): the curve is ALWAYS one flat 1.0 px
        // opaque stroke. No understroke, no second pass, no bloom.
        g.setOpacity (1.0f);
        strokeTrace (g, responsePath, phos);
        // PEAKS MARKED (Tyson 2026-08-29, from the first faces): a small cross
        // on every resonance so the player sees where the vowels sit.
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
        if (bodyName.isNotEmpty())
        {
            g.setFont (telemetryFont (9.8f, false));
            g.setColour (phos.withAlpha (0.70f));
            g.drawText (bodyName.toUpperCase(),
                        juce::Rectangle<float> (plot.getRight() - 190.0f, plot.getBottom() - 16.0f, 182.0f, 12.0f),
                        juce::Justification::centredRight, false);
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
        // Same trace law: flat 1.0 px core, no aura.
        g.setColour (col);
        g.strokePath (path, { 1.0f, juce::PathStrokeType::curved,
                              juce::PathStrokeType::butt });
    }
    Theme t;
    mutable juce::Image displayPlate;
    mutable juce::Image gridPlate;
    juce::Path responsePath;
    // One logical pixel, antialiased by the supersampled cache. No companion
    // stroke or glow: the curve is a precise mint hairline on dark glass.
    static constexpr float kTraceWidth = 1.1f;
    mutable juce::Image traceCache;
    mutable juce::Colour cachedColour;
    std::vector<float> traceXs;
    std::vector<float> traceDbs;
    juce::String bodyName;
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
