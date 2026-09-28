#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include "../dsp/KeySnap.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <functional>
#include <memory>
namespace trench::ui
{
class KeySnapBox final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    KeySnapBox (juce::AudioProcessorValueTreeState& apvts, const Theme& theme)
        : t (theme), param (apvts.getParameter (ParamID::keySnap))
    {
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Key Snap");
        setHelpText ("KEY OFF leaves the body as authored. AUTO follows the key heard in the input and shifts the whole body by up to a semitone so its strongest resonance sits on a note of the key.");
        setTooltip ("KEY: click for AUTO (follows the key it hears) or OFF");
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { repaint(); });
    }
    void setSuggestionProviders (std::function<int()> primary)
    {
        primarySuggestion = std::move (primary);
    }
    void setListeningProvider (std::function<bool()> provider)
    {
        listeningProvider = std::move (provider);
    }
    void refreshSuggestion()
    {
        const int first = suggestion (primarySuggestion);
        const bool listeningNow = isListening();
        if (first != lastSuggestion)
        {
            lastSuggestion = first;
            if (currentChoice() == 0 && first >= 0)
            {
                arrivalStartedMs = juce::Time::getMillisecondCounterHiRes();
                arriving = true;
            }
            repaint();
        }
        else if (arriving || listeningNow)
        {
            if (juce::Time::getMillisecondCounterHiRes() - arrivalStartedMs >= kArrivalDurationMs)
                arriving = false;
            repaint();
        }
        else if (listeningNow != lastListening)
        {
            repaint();
        }
        lastListening = listeningNow;
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override
    {
        hover = false;
        hoveredCandidate = -1;
        repaint();
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int candidate = candidateAt (e.position);
        if (candidate != hoveredCandidate)
        {
            hoveredCandidate = candidate;
            repaint();
        }
    }
    void mouseDown (const juce::MouseEvent&) override
    {
        down = true;
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        down = false;
        repaint();
        if (param == nullptr || ! getLocalBounds().contains (e.position.toInt()))
            return;

        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, param);
            return;
        }
        const int next = currentChoice() == 0 ? trench::KeySnap::kAutoChoice : 0;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) next));
        param->endChangeGesture();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (param == nullptr || wheel.deltaY == 0.0f)
            return;
        const int last = juce::jmax (0, param->getNumSteps() - 1);
        const int current = juce::jlimit (
            0, last, juce::roundToInt (param->convertFrom0to1 (param->getValue())));
        const int direction = wheel.deltaY > 0.0f ? 1 : -1;
        const int next = (current + direction + last + 1) % (last + 1);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) next));
        param->endChangeGesture();
    }
    void paint (juce::Graphics& g) override
    {
        const int first = suggestion (primarySuggestion);
        const bool showingSuggestion = currentChoice() == 0 && first >= 0;

        if (getHeight() < 26)
        {

            const auto ink = juce::Colour (0xff2a2722);
            const bool locked = currentChoice() != 0;
            const auto b = getLocalBounds().toFloat();
            const auto bigFont   = displayFont (11.8f, false);
            compactAlt = {};
            float x = b.getRight();
            const float boxH = juce::jmin (17.0f, b.getHeight());
            const float boxY = b.getY() + (b.getHeight() - boxH) * 0.5f;
            const auto valueBox = [&] (const juce::String& text, const juce::Font& f)
            {
                const float w = juce::jmax (26.0f,
                                            juce::GlyphArrangement::getStringWidth (f, text) + 13.0f);
                x -= w;
                const auto box = juce::Rectangle<float> (x, boxY, w, boxH);
                drawMutedBoneReadout (g, box, boxH * 0.17f, hover, t);
                return box;
            };

            if (locked)
            {
                const auto text = currentChoice() == trench::KeySnap::kAutoChoice
                    ? (first >= 0 ? "AUTO " + shortSuggestionText (first) : juce::String ("AUTO"))
                    : shortChoiceText (currentChoice());
                const auto box = valueBox (text, bigFont);
                g.setFont (bigFont);
                g.setColour (ink);
                g.drawText (text, box.toNearestInt(), juce::Justification::centred, false);
            }
            if (! locked && showingSuggestion)
            {
                x -= 4.0f;
                const auto text = shortSuggestionText (first);
                const auto box = valueBox (text, bigFont);
                compactAlt = box;
                g.setFont (bigFont);
                g.setColour (ink.withAlpha (hoveredCandidate >= 0 ? 0.95f : 0.45f));
                g.drawText (text, box.toNearestInt(), juce::Justification::centred, false);
            }
            g.setFont (displayFont (11.5f, true));
            g.setColour (juce::Colour (0xff0d0b09).withAlpha (
                (locked || showingSuggestion) ? 0.92f : 0.80f));
            g.drawText (locked ? "KEY" : "KEY: OFF", juce::Rectangle<float> (b.getX(), b.getY(),
                                                       x - b.getX() - 4.0f, b.getHeight()).toNearestInt(),
                        juce::Justification::centredRight, false);
            if (! locked && ! showingSuggestion && isListening())
                drawListeningHairline (g);
            return;
        }

    }
private:
    static constexpr double kArrivalDurationMs = 420.0;
    void drawListeningHairline (juce::Graphics& g) const
    {
        const double seconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float phase = (float) std::fmod (seconds, 1.25) / 1.25f;
        const float x = 20.0f + phase * ((float) getWidth() - 40.0f);
        const float y = (float) getHeight() * 0.62f;
        juce::ColourGradient scan (juce::Colours::transparentBlack, x - 6.0f, y,
                                   juce::Colour (0xff7f948c).withAlpha (0.70f), x, y, false);
        scan.addColour (0.78, juce::Colour (0xff7f948c).withAlpha (0.30f));
        scan.addColour (1.0, juce::Colours::transparentBlack);
        g.setGradientFill (scan);
        g.fillRect (juce::Rectangle<float> (x - 6.0f, y - 0.5f, 12.0f, 1.0f));
    }
    int candidateAt (juce::Point<float> point) const
    {
        if (currentChoice() != 0 || suggestion (primarySuggestion) < 0)
            return -1;
        if (getHeight() < 26)
            return ! compactAlt.isEmpty() && compactAlt.contains (point) ? 0 : -1;
        if (juce::Rectangle<float> (8.0f, 11.0f, 33.0f, 26.0f).contains (point))
            return 0;
        if (juce::Rectangle<float> (47.0f, 12.0f, 33.0f, 25.0f).contains (point))
            return 1;
        return -1;
    }
    void drawSelected (juce::Graphics& g, const juce::String& text) const
    {
        const auto tile = juce::Rectangle<float> ((float) getWidth() * 0.5f - 17.0f, 13.0f, 34.0f, 21.0f);
        g.setColour (juce::Colour (0xffaebbb4).withAlpha (hover ? 0.24f : 0.15f));
        g.fillRoundedRectangle (tile, 2.0f);
        g.setColour (juce::Colour (0xffc7d3cd).withAlpha (hover ? 0.82f : 0.60f));
        g.drawRoundedRectangle (tile.reduced (0.5f), 1.8f, 0.7f);
        g.setFont (displayFont (10.4f, true));

        g.setColour (t.accent().withAlpha (0.95f));
        g.drawText (text, tile.toNearestInt(), juce::Justification::centred, false);
    }
    bool isListening() const
    {
        return listeningProvider && listeningProvider();
    }
    static int suggestion (const std::function<int()>& provider)
    {
        return provider ? juce::jlimit (-1, 23, provider()) : -1;
    }
    static juce::String shortSuggestionText (int label)
    {
        if (label < 0 || label >= 24)
            return "--";
        static constexpr const char* notes[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        return juce::String (notes[label % 12]) + (label >= 12 ? "m" : "");
    }
    static juce::String shortChoiceText (int choice)
    {
        if (choice >= 1 && choice <= 12)
            return shortSuggestionText (12 + choice - 1);
        if (choice >= 13 && choice <= 24)
            return shortSuggestionText (choice - 13);
        return "--";
    }
    int currentChoice() const
    {
        if (param == nullptr)
            return 0;
        const int last = juce::jmax (0, param->getNumSteps() - 1);
        return juce::jlimit (0, last,
                             juce::roundToInt (param->convertFrom0to1 (param->getValue())));
    }
    Theme t;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    std::function<int()> primarySuggestion;
    std::function<bool()> listeningProvider;
    int lastSuggestion = -2;
    double arrivalStartedMs = 0.0;
    bool arriving = false;
    bool hover = false;
    bool down = false;
    int hoveredCandidate = -1;
    bool lastListening = false;
    mutable juce::Rectangle<float> compactAlt;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeySnapBox)
};
}

