#pragma once

#include "Theme.h"

namespace trench::ui
{

// The faceplate: Tyson's beige plate art, drawn 1:1. The art's baked recesses
// ARE the wells — and the MORPH/Q wheel wells are CUT OUT (alpha holes,
// tools/punch_wheel_wells.py; Tyson 2026-08-07 "cut out the well so its
// transparent. Not bodge the gap"). The wheels sit BEHIND this plate in
// z-order and show through the holes, lip overlapping the wheel — a flat-
// laying pitchwheel seating in a slot. The holes' top-edge feather is
// translucent BLACK in the asset: the plate's cast shadow on the wheel.
// NOT opaque any more (the holes must reveal the wheels), still buffered.
class FaceplateView : public juce::Component
{
public:
    FaceplateView (juce::Image panel, const Theme& theme)
        : panelImage (std::move (panel)), t (theme)
    {
        // The wheel wells are punched out again (Tyson 2026-08-11), so the
        // plate is NOT opaque: declaring it so lets JUCE skip painting the
        // wheels sitting behind it, and the holes come through as bare
        // background.
        setOpaque (false);
        setBufferedToImage (true);
        setInterceptsMouseClicks (false, false);
    }

    /// The open room's carved frame (mock_sel comp): the plate's own groove,
    /// broken at the top-left so the selector can seat in it. Editor px.
    void setRoomFrame (juce::Rectangle<float> frame, float gapX0, float gapX1)
    {
        if (frame != roomFrame || gapX0 != roomGapX0 || gapX1 != roomGapX1)
        {
            roomFrame = frame;
            roomGapX0 = gapX0;
            roomGapX1 = gapX1;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        // NO fillAll (Tyson 2026-08-11): with the wheel wells punched out, an
        // opaque wash here is what the holes show - it paints over the wheels
        // sitting behind the plate. The plate art covers the component edge to
        // edge except at the openings, which is exactly where it must not.
        if (panelImage.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (panelImage, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        }

        if (! roomFrame.isEmpty())
        {
            g.setColour (juce::Colour (0xffa6967d).withAlpha (0.59f));
            g.strokePath (roomFramePath (roomFrame.reduced (0.8f), 4.7f, roomGapX0, roomGapX1),
                          juce::PathStrokeType (1.2f));
            g.setColour (juce::Colour (0xff453424).withAlpha (0.67f));
            g.strokePath (roomFramePath (roomFrame, 5.5f, roomGapX0, roomGapX1),
                          juce::PathStrokeType (1.2f));
        }
        // The wheels sit BEHIND the punched plate again, so this cast lands ON
        // TOP of the drum - the plate's own shadow falling into the opening,
        // which is what shows the wheel's edge.
        drawWheelContactShadow (g, t.rect ("morphWheel"));
        drawWheelContactShadow (g, t.rect ("qWheel"));
        // HISTORICAL NOTE - the cast was briefly deleted here on a wrong reading
        // ("shadows on shadows"). It is VITAL: it is what seats the drum in the
        // plate. Restored verbatim from 4db42c24 below. (Tyson 2026-08-12: "the
        // way it sits ... the framing")
        /* removed reasoning kept for the record: (Tyson 2026-08-12: "the wheel is looking
        // more and more sunken, which makes me think you're putting shadows on
        // shadows"). Correct, and measured: the plate art ALREADY carries this
        // cast - the well interior is baked dark and tools/bake_wheel_plate_shadow.py
        // multiplied a drop shadow into the plate pixels below each well. This
        // ellipse then drew a SECOND one over the top of it, 11.4px deep at 0.62
        // alpha, in the same place. WheelControl adds its own seating gradients
        // on top of that, so the drum was carrying three casts.
        // The baked one stays because it shades the plate's own grain; a drawn
        // pass over the art cannot (this file's own note: "code-drawn shadow
        // passes over the art stay banned - they read as fog"). */
        // NO DRAWN READOUT SEAT. Every attempt at one today - inside the
        // component, then on the plate - came out as a gap above and below the
        // box, because the box already carries its own keyline and a second
        // dark edge outside it is just a second dark edge (Tyson 2026-08-12:
        // "they still have a top and bottom gap"). The readouts at 4db42c24 had
        // no seat at all and that is the version that was judged good.

        // NO REPAINT ON THE PLATE (Tyson 2026-08-12). The carved room frame was
        // stroked here in code - a light catch line and a dark cut line - over
        // art that already carries its own carving. Two grooves on one plate.
        // The casts below stay; they are shadows the controls throw, not
        // geometry drawn on top of geometry.

        // No code-drawn vignette or shadow bands over the art — they read as a
        // fake layer (Tyson 2026-07-11). The rack seating shadow is BAKED into
        // the plate asset itself (df2_panel_beige_shadow.png), so the grain
        // shades with it like the X3 reference.

        // Wheel-well shadows live IN the punched plate asset: translucent
        // black inside the cutouts (a contact ring hugging the walls plus a
        // soft top cast), falling on the wheel behind, PLUS the drop shadow
        // each wheel throws on the plate below its well — restored 2026-08-08
        // on Tyson's ask, baked by tools/bake_wheel_plate_shadow.py
        // (multiplied into the plate pixels, so the grain shades with it).
        // Code-drawn shadow passes over the art stay banned — they covered
        // the grain and read as fog (Tyson 2026-08-07: "your shadow update
        // was just a spray").

        // The outer rim rect went with it - the plate art has its own edge.
    }

    static void drawWheelContactShadow (juce::Graphics& g, juce::Rectangle<float> well)
    {
        if (well.isEmpty())
            return;
        // EMBEDDED (Tyson 2026-08-14: "add the shadow at the corners to make it
        // feel like its slightly embedded", then "dont render shadows at the
        // top. Only on the sides to show its sitting in the well nicely").
        // The lip casts INTO the opening at the SIDES only — the tread runs
        // under the side lips, so a few px of cast at each end seats it; the
        // top stays open. Clipped to the well's rounded opening so nothing
        // lands on the plate.
        {
            // The element rect is a 139x31-editor-px box; the punched opening
            // measured off the plate (2026-08-15 re-measure) is 137.4x25.6,
            // centred - 0.8px inside horizontally, 2.7px vertically. Cast
            // into the OPENING, not the box.
            const auto opening_r = well.reduced (0.8f, 2.7f);
            juce::Graphics::ScopedSaveState save (g);
            juce::Path opening;
            opening.addRoundedRectangle (opening_r, opening_r.getHeight() * 0.34f);
            g.reduceClipRegion (opening);
            const float sideD = 7.0f;
            juce::ColourGradient left (juce::Colours::black.withAlpha (0.50f),
                                       opening_r.getX(), opening_r.getCentreY(),
                                       juce::Colours::transparentBlack,
                                       opening_r.getX() + sideD, opening_r.getCentreY(), false);
            g.setGradientFill (left);
            g.fillRect (opening_r.withWidth (sideD));
            juce::ColourGradient right (juce::Colours::black.withAlpha (0.50f),
                                        opening_r.getRight(), opening_r.getCentreY(),
                                        juce::Colours::transparentBlack,
                                        opening_r.getRight() - sideD, opening_r.getCentreY(), false);
            g.setGradientFill (right);
            g.fillRect (opening_r.withLeft (opening_r.getRight() - sideD));
        }
        const float castH = 7.0f;
        const float cx = well.getCentreX();
        const float overlap = 1.5f;
        const float cy = well.getBottom() - overlap;
        const float rx = well.getWidth() * 0.48f;   // broad half-ellipse edge
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (juce::Rectangle<int> ((int) well.getX(), (int) std::floor (cy),
                                                  (int) well.getWidth(), (int) (castH + overlap)));
        g.addTransform (juce::AffineTransform::scale (1.0f, (castH + overlap) / rx, cx, cy));
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.74f), cx, cy,
                                 juce::Colours::transparentBlack, cx + rx, cy, true);
        sh.addColour (0.50, juce::Colours::black.withAlpha (0.50f));
        sh.addColour (0.82, juce::Colours::black.withAlpha (0.18f));
        g.setGradientFill (sh);
        g.fillEllipse (cx - rx, cy - rx, rx * 2.0f, rx * 2.0f);
    }
private:
    // One open outline: clockwise from the gap's right end all the way round
    // to its left end, so the top edge stays broken for the selector.
    static juce::Path roomFramePath (juce::Rectangle<float> r, float rad,
                                     float gapX0, float gapX1)
    {
        constexpr float q = juce::MathConstants<float>::halfPi;
        juce::Path p;
        p.startNewSubPath (juce::jlimit (r.getX() + rad, r.getRight() - rad, gapX1), r.getY());
        p.lineTo (r.getRight() - rad, r.getY());
        p.addCentredArc (r.getRight() - rad, r.getY() + rad, rad, rad, 0.0f, 0.0f, q);
        p.lineTo (r.getRight(), r.getBottom() - rad);
        p.addCentredArc (r.getRight() - rad, r.getBottom() - rad, rad, rad, 0.0f, q, 2.0f * q);
        p.lineTo (r.getX() + rad, r.getBottom());
        p.addCentredArc (r.getX() + rad, r.getBottom() - rad, rad, rad, 0.0f, 2.0f * q, 3.0f * q);
        p.lineTo (r.getX(), r.getY() + rad);
        p.addCentredArc (r.getX() + rad, r.getY() + rad, rad, rad, 0.0f, 3.0f * q, 4.0f * q);
        p.lineTo (juce::jlimit (r.getX() + rad, r.getRight() - rad, gapX0), r.getY());
        return p;
    }



    juce::Image panelImage;
    Theme t;
    juce::Rectangle<float> roomFrame;
    float roomGapX0 = 0.0f, roomGapX1 = 0.0f;
};

} // namespace trench::ui
