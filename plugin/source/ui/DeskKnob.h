#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include "../parameters/TrenchParameters.h"
#include "BinaryData.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace trench::ui
{
class DeskKnob final : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    DeskKnob (juce::AudioProcessorValueTreeState& apvts, const Theme& theme,
              const juce::String& paramID, juce::String caption)
        : t (theme),
          driveParam (apvts.getParameter (paramID)), label (std::move (caption))
    {
        setTitle (label);
        setWantsKeyboardFocus (true);
        setRepaintsOnMouseActivity (true);
        strip = juce::ImageCache::getFromMemory (BinaryData::trench_knob_black_strip_png, BinaryData::trench_knob_black_strip_pngSize);
        if (driveParam != nullptr)
            driveAttachment = std::make_unique<juce::ParameterAttachment> (*driveParam, [this] (float) { updateTooltip(); repaint(); });
        updateTooltip();
    }

    float getDrive() const noexcept
    {
        return driveParam != nullptr ? driveParam->getValue() : 0.0f;
    }

    void setLegendVisible (bool visible) { legendVisible = visible; repaint(); }

    bool keyPressed (const juce::KeyPress& key) override { return adjustParamFromKey (driveParam, key); }

    void updateTooltip()
    {
        const auto role = label == "INPUT" ? "gain before the filter; drives Mackity when SLAM is on" : "final gain after processing";
        const auto help = juce::String (role) + ". Shift for fine adjustment; double-click to reset to 0 dB.";
        setHelpText (help);
        setTooltip (label + ": " + (driveParam != nullptr ? driveParam->getCurrentValueAsText() : juce::String()) + " dB. " + help);
    }

    juce::Rectangle<float> getLegendArea() const
    {
        const auto b = getLocalBounds().toFloat();
        const float legendH = juce::jmax (12.0f, b.getHeight() * 0.28f);
        return { b.getX(), b.getBottom() - legendH, b.getWidth(), legendH };
    }

    juce::Rectangle<float> getKnobArea() const
    {
        const auto b = getLocalBounds().toFloat();
        const float legendH = juce::jmax (12.0f, b.getHeight() * 0.28f);
        return b.withTrimmedBottom (legendH);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto legend = getLegendArea();
        const float d = juce::jmin (kBayKnobDiameter, b.getWidth() * 0.9f);
        const juce::Point<float> c { b.getCentreX(), getKnobArea().getCentreY() };
        const float drive = getDrive();

        if (strip.isValid())
        {
            {
                const auto shade = juce::Colour (0xff2a1f12);
                juce::ColourGradient cast (shade.withAlpha (0.42f), c.x + d * 0.06f, c.y + d * 0.10f,
                                           shade.withAlpha (0.0f), c.x + d * 0.06f, c.y + d * 0.62f, true);
                g.setGradientFill (cast);
                g.fillEllipse (c.x - d * 0.54f + d * 0.06f, c.y - d * 0.54f + d * 0.10f, d * 1.08f, d * 1.08f);
            }
            constexpr int frameSize = 96, frameCount = 61;
            const int frame = juce::jlimit (0, frameCount - 1,
                                            juce::roundToInt ((1.0f - drive) * (float) (frameCount - 1)));
            const float frameD = d * (96.0f / 76.0f);
            g.setOpacity (1.0f);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
            const auto snap = [scale] (float v) { return std::round (v * scale) / scale; };
            const float x0 = snap (c.x - frameD * 0.5f), y0 = snap (c.y - frameD * 0.5f);
            const float side = snap (c.x + frameD * 0.5f) - x0;
            g.drawImage (strip.getClippedImage ({ frame * frameSize, 0, frameSize, frameSize }),
                         { x0, y0, side, side }, juce::RectanglePlacement::stretchToFit);
        }

        const float fontSize = juce::jlimit (8.0f, 11.0f, legend.getHeight() * 0.75f);
        g.setFont (displayFont (fontSize, true));

        const auto driveBox = legend.toNearestInt();
        g.setColour (t.labelInk());
        if (legendVisible)
            g.drawText (draggingKnob && driveParam != nullptr ? driveParam->getCurrentValueAsText() + " dB" : label,
                    driveBox, juce::Justification::centred, false);
    }

    void mouseMove (const juce::MouseEvent&) override
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, driveParam);
            return;
        }

        if (driveAttachment != nullptr && driveParam != nullptr)
        {
            driveAttachment->beginGesture();
            gestureOpen = true;
            dragStartY = e.position.y;
            dragValueDb = driveParam->convertFrom0to1 (getDrive());
            draggingKnob = true;
            e.source.enableUnboundedMouseMovement (true, false);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! draggingKnob || driveAttachment == nullptr || driveParam == nullptr || e.mods.isPopupMenu())
            return;

        const float dy = dragStartY - e.position.y;
        dragStartY = e.position.y;
        if (dy == 0.0f) return;
        driveAttachment->setValueAsPartOfGesture (gainDragValue (*driveParam, dragValueDb, dy, e.mods.isShiftDown()));
        updateTooltip();
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (driveAttachment != nullptr && gestureOpen)
        {
            driveAttachment->endGesture();
            gestureOpen = false;
        }
        draggingKnob = false;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (driveAttachment != nullptr && driveParam != nullptr)
        {
            driveAttachment->setValueAsCompleteGesture (driveParam->convertFrom0to1 (driveParam->getDefaultValue()));
            updateTooltip();
            repaint();
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (driveAttachment != nullptr && driveParam != nullptr)
        {
            adjustParamFromWheel (driveParam, e, wheel);
            updateTooltip();
            repaint();
        }
    }

private:
    bool legendVisible = true;
    juce::Image strip;
    Theme t;
    juce::RangedAudioParameter* driveParam = nullptr;
    juce::String label;
    std::unique_ptr<juce::ParameterAttachment> driveAttachment;
    float dragStartY = 0.0f;
    float dragValueDb = 0.0f;
    bool draggingKnob = false;
    bool gestureOpen = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeskKnob)
};
}
