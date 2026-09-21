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
class MixKnob final : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    MixKnob (juce::AudioProcessorValueTreeState& apvts, const Theme& theme,
             const juce::String& paramID,
             juce::String displayLabel = "MIX")
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
    // Engine-measured coloration (0..1): how hard the pole push is actually
    // working on the audio right now. Drives the glint breathing; the number
    // and pointer stay the AMOUNT the user set.
    void setActivity (float a) noexcept
    {
        a = juce::jlimit (0.0f, 1.0f, a);
        if (std::abs (a - activity) > 0.01f)
        {
            activity = a;
            repaint();
        }
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
        const float d = kBayKnobDiameter;
        const auto knob = juce::Rectangle<float> (d, d)
                              .withCentre ({ b.getX() + d * 0.5f, b.getCentreY() });
        const float blockX = b.getX() + d + 4.0f;
        // The numeric box is centred on ITS OWN KNOB's centre line; the caption
        // sits directly above it (verdict 2026-08-05: the value used to hang
        // 6px below the knob it belongs to).
        const float boxTop = b.getCentreY() - (float) kBayValueHeight * 0.5f;
        const float capW = juce::jmin ((float) kBayValueWidth + 26.0f,
                                       b.getRight() - blockX);
        drawBayCaption (g, juce::Rectangle<float> (blockX, boxTop - 11.0f, capW, 11.0f),
                        label, t);
        const auto c = knob.getCentre();
        const auto teal = t.rollerIllumination();
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
                static juce::Image* machinedPtr = new juce::Image();
                juce::Image& machined = *machinedPtr;
                if (machined.isNull())
                {
                    machined = juce::ImageCache::getFromMemory (BinaryData::trench_knob_strip_png,
                                                                BinaryData::trench_knob_strip_pngSize)
                                   .createCopy();
                    juce::Image::BitmapData bd (machined, juce::Image::BitmapData::readWrite);
                    const auto deepen = [] (juce::uint8 v)
                    {
                        return (juce::uint8) juce::roundToInt (
                            std::pow ((float) v / 255.0f, 1.45f) * 255.0f);
                    };
                    for (int py = 0; py < bd.height; ++py)
                        for (int px = 0; px < bd.width; ++px)
                        {
                            const auto colr = bd.getPixelColour (px, py);
                            if (colr.getAlpha() > 0)
                                bd.setPixelColour (px, py,
                                    juce::Colour::fromRGBA (deepen (colr.getRed()),
                                                            deepen (colr.getGreen()),
                                                            deepen (colr.getBlue()),
                                                            colr.getAlpha()));
                        }
                }
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
            g.drawImage (strip,
                         (int) (c.x - frameD * 0.5f), (int) (c.y - frameD * 0.5f),
                         (int) frameD, (int) frameD,
                         frame * frameSize, 0, frameSize, frameSize,
                         false);
        }
        {
            // X3 tick-ring meter is BAKED into the filmstrip (Tyson
            // 2026-07-31 "bake it into the blender"): each frame carries its
            // lit teeth - none at 0, full ring at 100. Only GRIT's
            // engine-measured activity stays a live overlay: a soft flare
            // over the lit arc, invisible when the engine is idle.
            if (activity > 0.01f && value > 0.001f)
            {
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, d * 0.36f, d * 0.36f, 0.0f,
                                   -2.36f, juce::jmap (value, -2.36f, 2.36f), true);
                g.setColour (teal.withAlpha (0.40f * activity));
                g.strokePath (arc, { 5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
            }
        }
        {
            // small readout centred under the knob: the SAME bone family as
            // every readout on the face, scaled down so it never rivals the
            // MORPH/Q primaries.
            // sized to stay legible at the small face (verdict 2026-07-31
            // "secondary readouts need to be more visible")
            const float boxW = juce::jmin ((float) kBayValueWidth, b.getRight() - blockX - 1.0f);
            // the box hangs off its own knob, centred under its caption - not
            // centred in the leftover width, which detached it from the control
            const auto box = juce::Rectangle<float> (boxW, (float) kBayValueHeight)
                                 .withCentre ({ blockX + capW * 0.5f, b.getCentreY() });
            // Same material and same text voice as the MORPH/Q primaries:
            // ValueReadout draws drawMutedBoneReadout + drawCrispText emphasis.
            drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
            drawCrispText (g, box.reduced (4.0f, 1.0f),
                           choice != nullptr ? choice->getCurrentChoiceName()
                                             : juce::String (juce::roundToInt (value * 100.0f)),
                           kBayValuePt, t.textColour ("morphReadout", juce::Colour (0xff2a2722)), true);
        }
    }
private:
    float activity = 0.0f;
    float currentNormalised() const noexcept
    {
        return param != nullptr ? param->getValue() : 0.0f;
    }
    Theme t;
    juce::String label;
    juce::Image strip;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float defaultDenorm = 0.0f;
    float dragStartY = 0.0f;
    float valueAtStart = 0.0f;
    bool hover = false;
};
}
