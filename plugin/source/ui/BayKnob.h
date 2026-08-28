#pragma once
#include "ParamInteraction.h"
#include "Theme.h"
#include "../parameters/TrenchParameters.h"
#include "BinaryData.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>
#include <memory>
namespace trench::ui
{
class BayKnob final : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    BayKnob (juce::AudioProcessorValueTreeState& apvts, const Theme& theme,
             const juce::String& paramID, juce::String displayLabel)
        : t (theme), label (std::move (displayLabel)),
          param (apvts.getParameter (paramID))
    {
        setInterceptsMouseClicks (true, false);
        // the filmstrip frame is drawn oversized so its DISC lands at true
        // size; the baked-shadow margin may spill past the bounds
        setPaintingIsUnclipped (true);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setTitle (label);
        setHelpText (label + " - drag up/down; Shift for fine; double-click to reset");
        setTooltip (label + ": drag/wheel, Shift fine, double-click reset");
        if (param != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { repaint(); });
            attachment->sendInitialUpdate();
            defaultDenorm = param->convertFrom0to1 (param->getDefaultValue());
        }
    }
    /// Row-cell presentation (Tyson 2026-08-28 "2 rows of knobs ... keep
    /// controls to an absolute minimum"): the knob over its caption, the value
    /// taking the caption's seat while the hand is on it - no bone box.
    void setCompact (bool c) { if (compact != c) { compact = c; repaint(); } }
    /// Dimmed = the control is real but has nothing to act on right now
    /// (Tyson 2026-08-09 bay refactor): with PRESET at OFF the MOVEMENT room
    /// stays on screen, and Depth/Follow go quiet and stop taking the mouse
    /// rather than pretending to work. Not the same as setEnabled(false),
    /// which the No-filter body already owns.
    void setDimmed (bool d)
    {
        if (dimmed == d)
            return;
        dimmed = d;
        // The COMPONENT's alpha, not Graphics::setOpacity: every setColour
        // inside paint() replaces the fill and takes the opacity with it, so
        // the only dim that survives a whole draw is this one.
        setAlpha (d ? 0.35f : (isEnabled() ? 1.0f : 0.32f));
        setInterceptsMouseClicks (! d, false);
        setMouseCursor (d ? juce::MouseCursor::NormalCursor
                          : juce::MouseCursor::UpDownResizeCursor);
        repaint();
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, param);
            return;
        }
        if (attachment != nullptr)
            attachment->beginGesture();
        // One gesture law with the wheels (2026-08-15): cursor hides for the
        // drag and is unbound from the screen edge; back at the press point
        // on release.
        e.source.enableUnboundedMouseMovement (true, false);
        dragStartY = e.position.y;
        valueAtStart = currentNormalised();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (attachment == nullptr || param == nullptr || e.mods.isPopupMenu())
            return;
        const float travel = 72.0f;
        const float scale = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const float next = juce::jlimit (0.0f, 1.0f,
                                         valueAtStart + (dragStartY - e.position.y) / travel * scale);
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (next));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (attachment != nullptr)
            attachment->endGesture();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (attachment != nullptr)
            attachment->setValueAsCompleteGesture (defaultDenorm);
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (attachment == nullptr || param == nullptr)
            return;
        const float next = juce::jlimit (0.0f, 1.0f,
                                         currentNormalised() + wheel.deltaY * 0.08f);
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (next));
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (isEnabled() ? 1.0f : 0.32f);
        // ONE horizontal unit (verdict 2026-08-01 "knobs look miniature -
        // scale the whole idea up"): the knob at full size on the left,
        // engraved caption over the frosted readout beside it. Stacked units
        // capped the knob at 26px inside the upper band; this doesn't.
        const auto b = getLocalBounds().toFloat();
        const float value = currentNormalised();
        const auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (param);
        const float d = compact ? 26.0f : kBayKnobDiameter;
        // The knob + caption/value block is ONE unit, centred in the row - the
        // carve's margins stay even on both sides (2026-08-06 proportion verdict).
        const float blockW = (float) kBayValueWidth + 26.0f;
        const float startX = b.getX() + juce::jmax (0.0f, (b.getWidth() - (d + 4.0f + blockW)) * 0.5f);
        const auto knob = compact
            ? juce::Rectangle<float> (d, d).withCentre ({ b.getCentreX(), b.getY() + d * 0.5f + 1.0f })
            : juce::Rectangle<float> (d, d).withCentre ({ startX + d * 0.5f, b.getCentreY() });
        const float blockX = startX + d + 4.0f;
        // The compact row carries one 10px caption, a 1px gap and the 17px
        // readout as a single centred block. This keeps all four rows inside
        // the restored 503px face without crowding the knob shadows.
        constexpr float captionH = 10.0f;
        constexpr float captionGap = 1.0f;
        const float contentH = captionH + captionGap + (float) kBayValueHeight;
        const float contentTop = b.getCentreY() - contentH * 0.5f;
        const float boxTop = contentTop + captionH + captionGap;
        const float capW = juce::jmin (blockW,
                                       b.getRight() - blockX);
        if (! compact)
            drawBayCaption (g, juce::Rectangle<float> (blockX, contentTop, capW, captionH),
                            label, t);
        const auto c = knob.getCentre();
        // NO code-drawn knob shadow. Tried and rejected three times now
        // (two drop-shadow methods 2026-08-01, a contact shadow 2026-08-05:
        // "those contact shadows gotta go"). The filmstrip's baked Blender
        // shadow is the only one. Do not add a fourth.
        {
            // Blender-rendered filmstrip (61 frames, alpha + baked contact
            // shadow) - the knob is photographed, not drawn. The strip's
            // rotation runs mirrored, hence (1 - value).
            if (strip.isNull())
            {
                // darker machined bakelite (verdict 2026-08-01): one shared
                // gamma pass - the body deepens, the machined glints and the
                // ivory pointer keep their brightness
                // HOST-EXIT SAFETY: this was a `static juce::Image`. Function-local
                // statics are destroyed during CRT teardown - after JUCE has shut
                // down and while the DLL is detaching - so freeing the image's
                // pixel data hits an allocator that is already gone. That is a
                // crash as the host closes, and no destructor of ours can prevent
                // it. Leaked on purpose: never destroyed, reclaimed by the OS.
                // The gamma-1.45 deepen that used to run here is BAKED into the
                // strip now (tools/neutralise_knob_material.py). It could never
                // have fixed what it was aimed at: a power curve scales all
                // three channels together, so the strip's warm cast survived it
                // proportionally and the knob just went dark AND brown (Tyson
                // 2026-08-07: "the knobs are still brown in my render",
                // measured R-B +7.0 before, +5.0 after). The asset is neutral
                // graphite at source now. Do not re-add a load-time pass.
                static juce::Image* machinedPtr = new juce::Image();
                juce::Image& machined = *machinedPtr;
                if (machined.isNull())
                    machined = juce::ImageCache::getFromMemory (BinaryData::trench_knob_strip_png,
                                                                BinaryData::trench_knob_strip_pngSize);
                strip = machined;
            }
            constexpr int frameSize = 96, frameCount = 61;
            const int frame = juce::jlimit (0, frameCount - 1,
                                            juce::roundToInt ((1.0f - value) * (float) (frameCount - 1)));
            // the opaque disc measures 76 of the 96px frame - draw the frame
            // oversized so the KNOB is d, not the margin (verdict 2026-08-01
            // "you scaled the knobs, now they are transparent")
            const float frameD = d * (96.0f / 76.0f);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            // CONTACT, first: the knob sits ON the plate and threw nothing, so
            // it read as a hole rather than a part. Plate-tinted, never black
            // (the E-mu art dims its panel ~10%, it does not composite black).
            {
                const auto shade = juce::Colour (0xff2a1f12);
                juce::ColourGradient cast (shade.withAlpha (0.42f), c.x + d * 0.06f, c.y + d * 0.10f,
                                           shade.withAlpha (0.0f), c.x + d * 0.06f, c.y + d * 0.62f, true);
                g.setGradientFill (cast);
                g.fillEllipse (c.x - d * 0.54f + d * 0.06f, c.y - d * 0.54f + d * 0.10f,
                               d * 1.08f, d * 1.08f);
            }

            g.drawImage (strip,
                         (int) (c.x - frameD * 0.5f), (int) (c.y - frameD * 0.5f),
                         (int) frameD, (int) frameD,
                         frame * frameSize, 0, frameSize, frameSize,
                         false);

            // FORM (Tyson 2026-08-11: "edit the knob directly in the code").
            // The 61-frame art is authored at 96px and drawn at 36 - at that
            // size its modelling averages away and the cap reads as a flat
            // black disc. These two passes put the form back and are ROTATION
            // INDEPENDENT, so they cannot disagree with whichever frame is up:
            // a dome lit from the top-left, and a hard specular catch on the
            // upper rim with a dark return on the lower one. Nothing here
            // touches the pointer or the tick ring - those stay the art's.
            {
                juce::Graphics::ScopedSaveState save (g);
                juce::Path cap;
                cap.addEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d);
                g.reduceClipRegion (cap);

                // THE DOME PASS IS DELETED (Tyson 2026-08-12: "why do the knobs
                // have a smudge on them"). It was a radial white-0.16 to
                // black-0.30 wash meant to put modelling back at 36px; at that
                // size it does not read as a lit dome, it reads as a thumbprint
                // on the cap. The rim catch below does the same job with an edge
                // instead of a smear, and the filmstrip already carries the form.
                const float rimD = d - 1.2f;
                juce::Path rim;
                rim.addEllipse (c.x - rimD * 0.5f, c.y - rimD * 0.5f, rimD, rimD);
                juce::ColourGradient edge (juce::Colours::white.withAlpha (0.55f),
                                           c.x, c.y - d * 0.5f,
                                           juce::Colour (0xff100c07).withAlpha (0.75f),
                                           c.x, c.y + d * 0.5f, false);
                edge.addColour (0.5, juce::Colours::transparentBlack);
                g.setGradientFill (edge);
                g.strokePath (rim, juce::PathStrokeType (1.1f));
            }
        }
        // X3 tick-ring meter is BAKED into the filmstrip (Tyson 2026-07-31
        // "bake it into the blender"): each frame carries its lit teeth -
        // none at 0, full ring at 100. The live GRIT-activity flare that
        // overlaid it was never fed by anyone and is deleted.
        if (compact)
        {
            const auto cap = juce::Rectangle<float> (b.getX(), knob.getBottom() + 2.0f,
                                                     b.getWidth(), 10.0f);
            drawBayCaption (g, cap,
                            hover ? (choice != nullptr ? choice->getCurrentChoiceName()
                                                       : juce::String (juce::roundToInt (value * 100.0f)))
                                  : label, t);
        }
        else
        {
            // small readout centred under the knob: the SAME bone family as
            // every readout on the face, scaled down so it never rivals the
            // MORPH/Q primaries.
            // sized to stay legible at the small face (verdict 2026-07-31
            // "secondary readouts need to be more visible")
            const float boxW = juce::jmin ((float) kBayValueWidth, b.getRight() - blockX - 1.0f);
            // the box hangs off its own knob, centred under its caption - not
            // centred in the leftover width, which detached it from the control
            const auto box = juce::Rectangle<float> (blockX + (capW - boxW) * 0.5f,
                                                     boxTop, boxW,
                                                     (float) kBayValueHeight);
            // Same material and same text voice as the MORPH/Q primaries:
            // ValueReadout draws drawMutedBoneReadout + drawCrispText emphasis.
            drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
            drawCrispText (g, box.reduced (4.0f, 1.0f),
                           choice != nullptr ? choice->getCurrentChoiceName()
                                             : juce::String (juce::roundToInt (value * 100.0f)),
                           kBayValuePt, t.textColour ("morphReadout", juce::Colour (0xff2a2722)));
        }
    }
private:
    float currentNormalised() const noexcept
    {
        return param != nullptr ? param->getValue() : 0.0f;
    }
    Theme t;
    juce::String label;
    bool compact = false;
    juce::Image strip;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float defaultDenorm = 0.0f;
    float dragStartY = 0.0f;
    float valueAtStart = 0.0f;
    bool hover = false;
    bool dimmed = false;
};
}
