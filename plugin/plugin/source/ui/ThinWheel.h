#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include "BinaryData.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
namespace trench::ui
{
class ThinWheel : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    // The earlier thin vertical MIX control, restored: the authored
    // thin_wheel_strip asset exactly as it is, never regenerated, never
    // recoloured. It is drawn narrow at its own proportion and it is the one
    // control on the face that takes no part in the palette pass.
    static constexpr int kFrameW = 12, kFrameH = 94, kNumFrames = 64;
    // Full travel in one drag of a little over the drum's own height.
    static constexpr float kDragRange = 140.0f;
    ThinWheel (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        strip = juce::ImageCache::getFromMemory (BinaryData::thin_wheel_strip_png,
                                                 BinaryData::thin_wheel_strip_pngSize);
        param = apvts.getParameter (paramID);
        jassert (param != nullptr);
        if (param != nullptr)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { repaint(); });
            defaultDenorm = param->convertFrom0to1 (param->getDefaultValue());
            const auto name = param->getName (32);
            setTitle (name);
            setHelpText (name + " - drag to dial; Shift for fine; double-click to reset");
            setTooltip (name + ": drag to dial, Shift fine, double-click reset");
            attachment->sendInitialUpdate();
        }
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setInterceptsMouseClicks (true, false);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, param);
            return;
        }
        if (attachment != nullptr)
            attachment->beginGesture();
        // A thumbwheel turns under the finger. Pressing it does not move it,
        // wherever on the drum you press - only the drag turns it.
        // One gesture law with the wheels (2026-08-15): cursor hides for the
        // drag; back at the press point on release.
        e.source.enableUnboundedMouseMovement (true, false);
        dragStartY   = e.position.y;
        valueAtStart = currentNormalised();
        fineActive   = e.mods.isShiftDown();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || attachment == nullptr || param == nullptr)
            return;
        // Re-anchor when Shift is taken or released mid-drag, so changing gear
        // never jumps the value.
        if (e.mods.isShiftDown() != fineActive)
        {
            fineActive   = e.mods.isShiftDown();
            dragStartY   = e.position.y;
            valueAtStart = currentNormalised();
        }
        const float next = juce::jlimit (0.0f, 1.0f,
                                         valueAtStart
                                             + (dragStartY - e.position.y) / kDragRange
                                                   * (fineActive ? 0.25f : 1.0f));
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (next));
        if (onValueGesture)
            onValueGesture (next);
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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (attachment == nullptr || param == nullptr)
            return;
        const float next = juce::jlimit (0.0f, 1.0f, currentNormalised() + w.deltaY * 0.08f);
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (next));
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        if (! strip.isValid())
            return;
        // Narrow drum, centred in a hit box that is deliberately wider than it.
        // The component's extra width is grab area only - nothing is drawn in
        // it, and the cast shadow on the plate is keyed off this same rect so
        // the two cannot drift apart.
        const auto drum = mixDrumRect (getLocalBounds().toFloat());
        g.setOpacity (1.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const int frame = currentFrame();
        g.drawImage (strip,
                     juce::roundToInt (drum.getX()), juce::roundToInt (drum.getY()),
                     juce::roundToInt (drum.getWidth()), juce::roundToInt (drum.getHeight()),
                     frame * kFrameW, 0, kFrameW, kFrameH);
    }
private:
public:
    // Which rotation frame the drum is showing. Read-only, so a proof can
    // check that a value change actually reaches the picture.
    int currentFrame() const
    {
        // DIRECT MAPPING, and leave it alone. The cross-correlation says
        // frame k's row (i+2) matches frame k+1's row i: the authored tread
        // moves UP as frames ascend, so value-up -> frame-up already follows
        // the finger. The 2026-08-15 inversion mis-signed that measurement
        // and spun the drum backwards ("mix roll sucks") - reverted same day.
        return juce::jlimit (0, kNumFrames - 1,
                             juce::roundToInt (currentNormalised() * (kNumFrames - 1)));
    }
    std::function<void (float)> onValueGesture;
private:
    float currentNormalised() const
    {
        return param != nullptr ? juce::jlimit (0.0f, 1.0f, param->getValue()) : 0.0f;
    }
    juce::Image strip;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float defaultDenorm = 1.0f;
    float dragStartY = 0.0f;
    float valueAtStart = 0.0f;
    bool  fineActive = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThinWheel)
};
}
