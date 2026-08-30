#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
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
        setHelpText ("KEY OFF leaves the body as authored. A heard key is offered beside it; click it to lock, click the locked key to return to OFF.");
        setTooltip ("KEY: OFF by default. Click the heard key to lock it; click again for OFF");
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (
                *param, [this] (float) { repaint(); });
    }
    void setSuggestionProviders (std::function<int()> primary, std::function<int()> secondary)
    {
        primarySuggestion = std::move (primary);
        secondarySuggestion = std::move (secondary);
    }
    void setListeningProvider (std::function<bool()> provider)
    {
        listeningProvider = std::move (provider);
    }
    void refreshSuggestion()
    {
        const int first = suggestion (primarySuggestion);
        const int second = suggestion (secondarySuggestion);
        const bool listeningNow = isListening();
        if (first != lastSuggestion || second != lastAltSuggestion)
        {
            lastSuggestion = first;
            lastAltSuggestion = second;
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
        const int current = currentChoice();
        if (current == 0)
        {
            const int candidate = candidateAt (e.position);
            if (candidate == 0)
                applySuggestedChoice (suggestion (primarySuggestion));
            else if (candidate == 1)
                applySuggestedChoice (suggestion (secondarySuggestion));
            return;
        }

        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (0.0f));
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
        const int second = suggestion (secondarySuggestion);
        const bool showingSuggestion = currentChoice() == 0 && first >= 0;

        if (getHeight() < 26)
        {

            const auto ink = juce::Colour (0xff2a2722);
            const bool locked = currentChoice() != 0;
            const auto b = getLocalBounds().toFloat();
            const auto bigFont   = displayFont (11.8f, false);
            const auto smallFont = displayFont (10.5f, false);
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

            {
                const auto text = locked ? shortChoiceText (currentChoice()) : juce::String ("OFF");
                const auto box = valueBox (text, bigFont);
                g.setFont (bigFont);
                g.setColour (ink.withAlpha (locked ? 1.0f : 0.80f));
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
            g.setFont (t.smallLabel());
            g.setColour (juce::Colour (0xff0d0b09).withAlpha (
                (locked || showingSuggestion) ? 0.92f : 0.80f));
            g.drawText ("KEY", juce::Rectangle<float> (b.getX(), b.getY(),
                                                       x - b.getX() - 4.0f, b.getHeight()).toNearestInt(),
                        juce::Justification::centredRight, false);
            if (! locked && ! showingSuggestion && isListening())
                drawListeningHairline (g);
            return;
        }

        const auto chip = getLocalBounds().toFloat().reduced (1.5f);
        const bool alive = showingSuggestion || currentChoice() != 0 || isListening();
        if (! alive)
        {

            g.setFont (displayFont (6.4f, true));
            g.setColour (juce::Colour (0xffc7d3cd).withAlpha (0.30f));
            g.drawText ("KEY", getLocalBounds().removeFromTop (13),
                        juce::Justification::centred, false);
            return;
        }
        g.setColour (juce::Colour (0xff101614).withAlpha (0.88f));
        g.fillRoundedRectangle (chip, 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (chip, 8.0f, 1.0f);
        g.setColour (juce::Colour (0xffc7d3cd).withAlpha (0.22f));
        g.drawRoundedRectangle (chip.reduced (1.0f), 7.0f, 0.7f);
        g.setFont (displayFont (6.4f, true));
        g.setColour (juce::Colour (0xffc7d3cd).withAlpha (0.62f));
        g.drawText ("KEY", 8, 4, 24, 9, juce::Justification::centredLeft, false);
        if (showingSuggestion)
        {
            const float elapsed = (float) (juce::Time::getMillisecondCounterHiRes()
                                            - arrivalStartedMs);
            drawCandidate (g, first, 8.0f, 14.0f, arrivalProgress (elapsed, 0.0f),
                           0.94f, hoveredCandidate == 0);
            drawCandidate (g, second, 47.0f, 15.0f, arrivalProgress (elapsed, 85.0f),
                           0.72f, hoveredCandidate == 1);
        }
        else if (currentChoice() == 0)
        {
            if (isListening())
                drawListeningHairline (g);
        }
        else
        {
            drawSelected (g, shortChoiceText (currentChoice()));
        }
    }
private:
    static constexpr double kArrivalDurationMs = 420.0;
    static float arrivalProgress (float elapsedMs, float delayMs)
    {
        const float linear = juce::jlimit (0.0f, 1.0f, (elapsedMs - delayMs) / 250.0f);
        const float remaining = 1.0f - linear;
        return 1.0f - remaining * remaining * remaining;
    }
    void drawCandidate (juce::Graphics& g, int label, float x, float targetY,
                        float progress, float strength, bool highlighted) const
    {
        const float y = targetY - (1.0f - progress) * 9.0f + (down ? 0.5f : 0.0f);
        const auto tile = juce::Rectangle<float> (x, y, 33.0f, 21.0f);
        const float alpha = 0.30f + progress * 0.70f;
        g.setColour (juce::Colours::black.withAlpha (alpha * 0.13f * strength));
        g.fillRoundedRectangle (tile.translated (0.0f, 1.0f), 2.0f);
        g.setColour (juce::Colour (0xffaebbb4).withAlpha (alpha * 0.19f * strength));
        g.fillRoundedRectangle (tile, 2.0f);
        if (highlighted)
        {
            g.setColour (juce::Colour (0xffd5dfd8).withAlpha (alpha * 0.10f));
            g.fillRoundedRectangle (tile, 3.0f);
        }
        g.setColour (juce::Colour (0xffc7d3cd).withAlpha (
            alpha * strength * (highlighted ? 0.94f : 0.68f)));
        g.drawRoundedRectangle (tile.reduced (0.5f), 1.8f, highlighted ? 0.9f : 0.65f);
        g.setFont (displayFont (10.4f, true));
        g.setColour (juce::Colour (0xffe3ebe6).withAlpha (alpha * strength));
        g.drawText (shortSuggestionText (label), tile.toNearestInt(),
                    juce::Justification::centred, false);
    }
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
    static int snapChoiceForSuggestion (int label)
    {
        if (label < 0 || label >= 24)
            return -1;
        return label < 12 ? 13 + label : 1 + (label - 12);
    }
    int currentChoice() const
    {
        if (param == nullptr)
            return 0;
        const int last = juce::jmax (0, param->getNumSteps() - 1);
        return juce::jlimit (0, last,
                             juce::roundToInt (param->convertFrom0to1 (param->getValue())));
    }
    void applySuggestedChoice (int label)
    {
        const int choice = snapChoiceForSuggestion (label);
        if (choice < 0 || param == nullptr)
            return;
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) choice));
        param->endChangeGesture();
    }
    Theme t;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    std::function<int()> primarySuggestion;
    std::function<int()> secondarySuggestion;
    std::function<bool()> listeningProvider;
    int lastSuggestion = -2;
    int lastAltSuggestion = -2;
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
