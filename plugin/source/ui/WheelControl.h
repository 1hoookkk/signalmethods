#pragma once

#include "Theme.h"
#include "ParamInteraction.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>
#include <memory>

namespace trench::ui
{

class WheelControl : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    // Locked DF2 cycle-10 wheel. The retained frame is authored at the
    // logical aperture's native 139x31 size; the 129th cell is a wrap sentinel.
    // This is the SESSIONBAK_1784628088 disc-rolloff seating, restored verbatim.
    // Sizing and seating were taken further on 2026-08-07 (144 wide, seated
    // 1px down and 1px right, component widened to hold it) and REVERTED on
    // Tyson's verdict "the wheels are off". Back to the SESSIONBAK_1784628088
    // numbers verbatim. Measured facts kept for whoever tries again: the drum
    // silhouette fills only 403 of the frame's 417px across and 81 of its 93
    // down, so at 139 the visible wheel is 134.3 wide inside a 137.4 aperture.
    // Frame size is NOT wheel size.
    // 2026-08-07, after the wells became REAL cutouts (wheel behind the
    // plate, punch_wheel_wells.py): Tyson on the seated render — "Good.
    // Slightly bigger, and slightly lower." 139x31 -> 144x32 with a 1px
    // drop. The earlier 144-wide attempt failed only because the wheel was
    // composited OVER the plate; under the cutout the lip hides the spill.
    // 2026-08-07, wheel drawn ON TOP of the plate: "render the wheel just
    // slightly bigger than the bounds so it reads flush" (Tyson). Measured —
    // the well opening is 352x67 plate px = 139x26 editor px, and the drum
    // silhouette fills only 403 of the frame's 417 across and 79 of its 93
    // down. At 144x32 the visible drum was 139.2x27.2: dead flush with the
    // opening, so the cut's own ragged edge still showed at the rim. 148x33
    // (aspect 4.485 vs the strip's 4.484) puts the visible drum at 143x28 —
    // 2px proud each side, 1px top and bottom. Frame size is NOT wheel size.
    // Five Point aperture restored (Tyson 2026-08-09): 139x31, the
    // canonical scale of plugin/trench_face.png at a6f80568.
    // Grown a step (Tyson 2026-08-11 "make the wheel slightly bigger"). The
    // drum draws at THIS size, centred - not fitted to the component - so the
    // element rect's re-measure against the definitive master did not move it.
    // Same 4.484 aspect the strip is authored at.
    // THE WELL SHOWS AT BOTH ENDS (Tyson 2026-08-12: "it should appear on
    // either side to show that it's embedded"). Every previous size drew the
    // drum WIDER than the opening - 147, then 143 against a 137px aperture -
    // so it covered the well completely and the wheel read as filling a slot
    // rather than sitting down inside one. It has to be narrower than the hole.
    //
    // The frame's disc fills 403 of its 417px, so a drawn width W shows a wheel
    // of 0.966 * W. At 137 that is 132.3 inside a 137 opening: about 2.3px of
    // well floor still visible past each cap, so it stays embedded while
    // filling more of the slot (Tyson 2026-08-12 "slightly bigger"). Height
    // follows the strip's 4.484 aspect.
    static constexpr int kStripFrameWidth = 417;
    // The operator live-tune subsystem (Ctrl+drag nudge / Ctrl+wheel resize,
    // persisted to Documents/TRENCH/wheel_tune.json) was calibration-only,
    // shipped permanently disabled, and could silently displace the wheel art
    // if its stale json ever resurfaced. Deleted 2026-08-07; the landed
    // numbers ARE the constants above.

    WheelControl (juce::AudioProcessorValueTreeState& apvts, juce::String paramID,
                  juce::Image filmstrip, const Theme& theme)
        : strip (std::move (filmstrip)), t (theme)
    {
        numFrames = juce::jmax (1, strip.getWidth() / kStripFrameWidth);
        jassert (! strip.isValid() || strip.getWidth() % kStripFrameWidth == 0);
        isQControl = paramID.containsIgnoreCase ("q") || paramID.containsIgnoreCase ("slam");
        param = apvts.getParameter (paramID);
        jassert (param != nullptr);
        if (param != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { repaint(); });
            defaultDenorm = param->convertFrom0to1 (param->getDefaultValue());
            const auto name = param->getName (32);
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            setTitle (name);
            setHelpText (name + " - drag left/right or mouse-wheel; double-click to reset");
            setTooltip (name + ": drag/wheel, double-click reset");
            attachment->sendInitialUpdate();
        }
        setInterceptsMouseClicks (true, false);
        // The 148x33 destination is larger than the 140x31 component, so the
        // spill would otherwise be clipped away at the very edges that have to
        // cover the opening. The wheel is behind the plate; the spill is hidden.
        setPaintingIsUnclipped (true);
    }

    void setParameter (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        attachment.reset();
        param = apvts.getParameter (paramID);
        isQControl = paramID.containsIgnoreCase ("q") || paramID.containsIgnoreCase ("slam");
        if (param != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { repaint(); });
            defaultDenorm = param->convertFrom0to1 (param->getDefaultValue());
            const auto name = param->getName (32);
            setTitle (name);
            setHelpText (name + " - drag left/right or mouse-wheel; double-click to reset");
            setTooltip (name + ": drag/wheel, double-click reset");
            attachment->sendInitialUpdate();
        }
        repaint();
    }

    void setDisplayOverride (bool active, float normalised)
    {
        normalised = juce::jlimit (0.0f, 1.0f, normalised);
        if (displayOverrideActive != active || ! juce::approximatelyEqual (displayOverrideValue, normalised))
        {
            displayOverrideActive = active;
            displayOverrideValue = normalised;
            repaint();
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hovering = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovering = false; repaint(); }

    std::function<void()> onGestureEnd;
    std::function<void()> onAltDragStart;
    std::function<void (float)> onAltDragSample;
    std::function<void()> onAltDragEnd;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())   // right-click -> Reset + host MIDI-learn/automation menu
        {
            showParamContextMenu (*this, param);
            return;
        }
        pressing = true;
        altRecording = e.mods.isAltDown() && onAltDragSample != nullptr;
        if (attachment != nullptr)
            attachment->beginGesture();
        // Hardware feel (2026-08-15 fidelity pass): the hand becomes the
        // wheel. Hiding the cursor for the drag and unbinding it from the
        // screen edge makes a throw feel like pushing tread instead of
        // steering a pointer. JUCE puts the cursor back at the press point
        // on release.
        e.source.enableUnboundedMouseMovement (true, false);
        dragStartX   = e.position.x;
        valueAtStart = currentNormalised();
        // GRAB, DON'T JUMP (Tyson 2026-08-15 "why does it also feel weird to
        // drag"): pressing used to snap the value to the cursor's x — a flat
        // pitchwheel teleporting under the finger, and the opposite of the
        // proven press-inert law. Now the press only takes hold; motion is
        // the drag's.
        if (altRecording)
        {
            if (onAltDragStart != nullptr) onAltDragStart();
            onAltDragSample (currentNormalised());
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            return;
        if (e.mods.isShiftDown() && attachment != nullptr && param != nullptr)
        {
            // Fine, precise drag: scaled delta from the press point.
            const float w    = juce::jmax (1.0f, (float) getWidth());
            const float next = juce::jlimit (0.0f, 1.0f,
                                             valueAtStart + (e.position.x - dragStartX) / w * 0.25f);
            attachment->setValueAsPartOfGesture (param->convertFrom0to1 (next));
            repaint();
        }
        else
        {
            dragRelative (e);
        }
        if (altRecording && onAltDragSample != nullptr)
            onAltDragSample (currentNormalised());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        pressing = false;
        if (attachment != nullptr)
            attachment->endGesture();
        if (onGestureEnd != nullptr)
            onGestureEnd();
        if (altRecording)
        {
            altRecording = false;
            if (onAltDragEnd != nullptr) onAltDragEnd();
        }
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (attachment != nullptr)
            attachment->setValueAsCompleteGesture (defaultDenorm);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        juce::ignoreUnused (e);
        if (attachment == nullptr || param == nullptr)
            return;

        const float next = juce::jlimit (0.0f, 1.0f, currentNormalised() + wheel.deltaY * 0.08f);
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (next));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (! strip.isValid())
            return;

        const int fw = strip.getWidth() / numFrames;
        const int fh = strip.getHeight();
        if (fw <= 0 || fh <= 0)
            return;

        // A 129-cell E-mu strip contains 128 usable states plus a byte-identical
        // frame-0 wrap sentinel. Parameter 1.0 must still show usable frame 127.
        const int usableFrames = numFrames == 129 ? 128 : numFrames;
        const int last = usableFrames - 1;
        const int frame = juce::jlimit (0, last, juce::roundToInt (displayNormalised() * (float) last));

        // SUPERSEDED LAWS, for the record: WINE_WHEEL ("full silhouette, never
        // clipped") died with the flat-pitchwheel verdict (Tyson 2026-08-14) —
        // the frame now draws WIDER than the punched slot and the plate's hole
        // cuts the tread, so the baked housing, caps and notches never show.
        // "Nothing painted behind the wheel" stands for THIS component, but
        // the editor paints its background as the well floor (PluginEditor::
        // paint) — the void behind the punched plate is no longer bare black.
        // The position lamp stays baked into the filmstrip as one smooth
        // packet — no code-drawn lamp and no segmented diode pass.
        // Supersampled frame minified into the fixed logical aperture, centred.
        // NOT fit-to-component scaling (that un-seats it from the baked well).
        // SESSIONBAK_1784628088 seating (the approved 07-21 disc-rolloff
        // reference): supersampled frame minified into the logical aperture,
        // centred.
        // Seated a touch BELOW the opening's centre line, not on it (Tyson
        // 2026-08-07: "make the wheel slightly further down, make it sit inside
        // the well... tuck it"). Dead-centre framing put the drum's top edge
        // level with the aperture's top lip, so the lip had nothing to overlap
        // and the wheel read as laid on the plate. Dropping it tucks the top
        // under the lip while the extra spill at the bottom hides behind the
        // bevel — the same 2.5px of invisible spill the seating law allows.
        // Measured at the wheel's centre row, the drum's left cap lands on the
        // opening's left edge with nothing between them while ~1.2px of bevel
        // still shows on the right — dead-centre by the numbers, but it reads
        // left because the plate's lit bevel runs down the RIGHT side, so the
        // eye takes that lit band as clearance the left side does not have
        // (Tyson 2026-08-07: "too far left").
        // Centred, not dropped (Tyson 2026-08-12 "there's a gap in the top").
        // The frame's disc fills about 0.86 of its height, so at a drawn 31 the
        // visible wheel is 26.6 against a 26px opening - a third of a pixel of
        // cover at each edge. Pushing it down 1px spent all of that on the
        // bottom and left the well floor showing along the top lip.
        static constexpr int kSeatDrop  = 0;
        static constexpr int kSeatShift = 0;
        // Native locked framing: 139x31, centred. The component is authored to
        // that same size; any half-pixel rounding spill is symmetric under the
        // plate lip. Treating this as a 417x93 SS3 frame was the old seating
        // regression.
        const auto wheelRect = getLocalBounds().toFloat()
                                   .translated ((float) kSeatShift, (float) kSeatDrop);
        // FLAT PITCH WHEEL (Tyson 2026-08-14: "not a drum — a flat laying
        // pitchwheel, left to right 0 to 100"; supersedes the WINE_WHEEL
        // full-silhouette law). The wheel is drawn WIDER than the plate's
        // punched slot so the window cuts the tread: the frame's baked housing,
        // rounded caps and side notches all fall under the plate lip, and what
        // shows is tread running edge-to-edge with the lamp riding the throw.
        static constexpr float kThrowOverscan = 1.12f;
        const auto frameRect = juce::Rectangle<float> ((float) fw * kThrowOverscan / 3.0f,
                                                       (float) fh / 3.0f)
                                   .withCentre (wheelRect.getCentre());
        // No cavity paint: the Five Point plate is opaque and sits BEHIND
        // this component - its own drawn well art frames the drum.
        // THE WELL FLOOR (Tyson 2026-08-12 "what's with the sides"). The wheel
        // sits BEHIND a punched plate, and the aperture is fractionally wider
        // than the wheel's own silhouette - the frame's disc fills only 403 of
        // its 417px across. At the rounded caps that left the hole showing
        // through to the editor background as hard black notches.
        //
        // The old "never paint a cavity here" rule was written when the wheel
        // was composited in FRONT of an opaque plate, where a drawn rectangle
        // had nowhere to hide. Behind a real hole it is only ever visible in
        // the slivers the drum does not cover, and without it those slivers
        // are a hole in the plugin.
        // NO CAVITY FILL. This existed to plug the slivers the drum did not
        // cover while the wheel sat BEHIND the punched plate. Rendered OVER the
        // plate (2026-08-13) it is just a black rectangle laid on the art,
        // hiding the well's own lip.
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        // 3x SUPERSAMPLE, then squash to plugin size (Tyson 2026-08-07). Going
        // 417 -> 148 in ONE resampler pass is a 2.8x minification: the frame's
        // tooth edges and lower rim fall between output samples and break up
        // into the jagged speckle marked on the render. Landing on 3x the
        // destination first, then halving that down, box-filters the detail
        // instead of point-sampling it. Cached per (frame, size) so the two
        // passes cost nothing on repaint — a wheel drag reuses the image until
        // the frame index actually changes.
        // Five Point draw: the 417px frame minified into the aperture in ONE
        // resampler pass, exactly as the canonical face was rendered — but
        // cached per frame index. Modulation animates this component at UI
        // rate, and an uncached high-quality minify per repaint is real CPU
        // on the paint thread for pixels that have not changed.
        // AT THE DEVICE'S OWN RESOLUTION. The cache used to be built at the
        // component's LOGICAL size and blitted 1:1, so on a scaled face (a host
        // DPI transform, or the 3x proof render) a 143px image was stretched to
        // 429 - the drum was the only soft thing on a face whose text, curve and
        // readouts were all redrawn crisp. Every other filmstrip on the plate
        // (the bay knobs) hands the source straight to drawImage and lands
        // sharp. Cache at the physical pixel count instead and draw into the
        // logical rect: identical work at 1x, a real 3x frame at 3x.
        const float pixelScale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int pw = juce::jmax (1, juce::roundToInt (frameRect.getWidth()  * pixelScale));
        const int ph = juce::jmax (1, juce::roundToInt (frameRect.getHeight() * pixelScale));
        if (scaledIndex != frame || scaledFrame.getWidth() != pw
            || scaledFrame.getHeight() != ph)
        {
            scaledFrame = strip.getClippedImage ({ frame * fw, 0, fw, fh })
                              .rescaled (pw, ph, juce::Graphics::highResamplingQuality);
            scaledIndex = frame;
        }
        g.drawImage (scaledFrame, frameRect, juce::RectanglePlacement::stretchToFit);

        // THE SEATING SHADOW (Tyson 2026-08-11: "the shadows that make the
        // wheel look embedded on either side... don't add it on the faceplate,
        // draw it over the wheel"). It supersedes the older "nothing is painted
        // on the drum" law below, and it belongs here rather than baked into
        // the plate: the plate is opaque and drawn BEHIND this component, so a
        // baked shadow cannot fall on the drum at all - which is why the plate
        // recolour lost the effect in the first place.
        //
        // THE SIDE RETURN (Tyson 2026-08-12: "the shadow needs to return on the
        // sides of the wheels"). The punched asset's contact ring hugs the
        // opening, but the drum is drawn larger than the opening, so at the
        // ends there was nothing closing the surface off - the lamp packet ran
        // straight to the caps and read as sticking out past the wheel rather
        // than turning away from you.
        //
        // ENDS ONLY. A top or bottom return would shade it like something lying
        // the other way; this turns about a vertical axis and scrolls left to
        // right, so it is square-on to the viewer everywhere except its sides.
        // Warm near-black, not black - the plate's own shadow family.
        {
            const auto wheel = frameRect;
            const auto shade = juce::Colour (0xff17110a);
            juce::Graphics::ScopedSaveState save (g);
            // The silhouette mask is the cached frame, which now lives at the
            // DEVICE's pixel count over the FRAME's rect - map it back.
            g.reduceClipRegion (scaledFrame,
                                juce::AffineTransform::scale (frameRect.getWidth()  / (float) pw,
                                                              frameRect.getHeight() / (float) ph)
                                    .translated (frameRect.getX(), frameRect.getY()));

            const float endW = wheel.getWidth() * 0.13f;
            juce::ColourGradient left (shade.withAlpha (0.80f), wheel.getX(), wheel.getCentreY(),
                                       shade.withAlpha (0.0f), wheel.getX() + endW, wheel.getCentreY(), false);
            left.addColour (0.40, shade.withAlpha (0.34f));
            g.setGradientFill (left);
            g.fillRect (wheel.withWidth (endW));

            juce::ColourGradient right (shade.withAlpha (0.80f), wheel.getRight(), wheel.getCentreY(),
                                        shade.withAlpha (0.0f), wheel.getRight() - endW, wheel.getCentreY(), false);
            right.addColour (0.40, shade.withAlpha (0.34f));
            g.setGradientFill (right);
            g.fillRect (wheel.withLeft (wheel.getRight() - endW));
        }

        // Crown band killed twice (operator: "grey line in the middle") and the
        // code sheen retired with the slate-charcoal strip trial — the ART owns
        // the wheel's light. If the wheel reads too black, fix the STRIP, not
        // paint over it here.

        // NOTHING is painted on the drum. Tyson 2026-08-07, against the DF2
        // reference: "this is a wheel sticking out laying on its side
        // horizontally" — it PROTRUDES through the slot, so there are no cavity
        // walls above or beside it and nothing to cast an edge or corner
        // shadow onto it. Every attempt to add one has been killed: the end-wall
        // strips and bottom sill ("too brown and muddy"), the top band ("why is
        // there a weird ceiling shadow"), and the corner-gathered recess
        // vignette. The strip's own render carries the cylinder's light — lit
        // top rim, dark belly, lit bottom rim. Fix the STRIP, never paint here.

        // Hover/drag feedback: the drum catches a touch more light under
        // the cursor — state feedback as light on the object, not a ring.
        if (hovering || pressing)
        {
            const auto drumF = wheelRect;
            juce::ColourGradient lift (juce::Colours::white.withAlpha (pressing ? 0.10f : 0.06f),
                                       0.0f, drumF.getY() + drumF.getHeight() * 0.30f,
                                       juce::Colours::transparentBlack, 0.0f, drumF.getBottom(), false);
            g.setGradientFill (lift);
            g.fillRect (drumF);
        }

        // The faceplate art owns the well edge. Do not draw an extra software
        // border over the bitmap; it reads as a rectangular artifact.
    }

private:
    juce::Image scaledFrame;      // supersampled frame at destination size
    int scaledIndex = -1;         // which strip frame scaledFrame holds


    float currentNormalised() const
    {
        return param != nullptr ? juce::jlimit (0.0f, 1.0f, param->getValue()) : 0.0f;
    }

    float displayNormalised() const
    {
        // While the user is dragging, the glow tracks THEIR hand (the real
        // parameter), never the modulated display value — otherwise motion
        // yanks the packet away from the cursor mid-gesture.
        return (displayOverrideActive && ! pressing) ? displayOverrideValue : currentNormalised();
    }

    // THE LIGHT LANDS UNDER THE FINGER (Tyson 2026-08-11: "it's not a drum and
    // it follows the glow"). Mapping the cursor across the component's full
    // width assumed the packet also travels the full width. Measured on the
    // shipping strip, its centre only runs from 5.3% to 82.7% of the frame, so
    // at the ends the light sat up to 17% of the width away from the hand -
    // which is exactly what made the wheel feel disconnected.
    //
    // Solving for the value whose packet lands on the cursor instead:
    // packet_x = (P0 + v * (P1 - P0)) * width. Invert it.
    // Re-based onto the VISIBLE wheel (2026-08-13). The two stops were measured
    // as fractions of the padded 417px FRAME; the component is the ink now, so
    // they carry through as (P * 417 - kInkX) / kInkW.
    static constexpr float kPacketX0 = 0.0398f;  // packet centre at value 0
    static constexpr float kPacketX1 = 0.8367f;  // packet centre at value 1

    void dragRelative (const juce::MouseEvent& e)
    {
        if (attachment == nullptr || param == nullptr)
            return;

        // Grab-and-push: the value moves by how far the hand moved, scaled to
        // the lamp packet's own travel so the lit position tracks the cursor
        // 1:1 along the tread. Absolute cursor-position mapping is gone — it
        // jumped on press and drifted from the lamp near the ends.
        const float throwPx = juce::jmax (1.0f, (kPacketX1 - kPacketX0) * (float) getWidth());
        const float next = juce::jlimit (0.0f, 1.0f,
                                         valueAtStart + (e.position.x - dragStartX) / throwPx);
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (next));
        repaint();
    }

    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::Image strip;
    int numFrames = 1;
    Theme t;
    bool hovering = false;
    bool pressing = false;
    bool altRecording = false;
    bool isQControl = false;
    bool displayOverrideActive = false;
    float displayOverrideValue = 0.0f;
    float defaultDenorm = 0.0f;
    float dragStartX = 0.0f;
    float valueAtStart = 0.0f;
};

} // namespace trench::ui
