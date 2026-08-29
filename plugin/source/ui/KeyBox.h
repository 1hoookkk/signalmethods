#pragma once
#include "Theme.h"
#include "ParamInteraction.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
namespace trench::ui
{
class KeyBox final : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    static constexpr float kWordW = 30.0f;
    KeyBox (juce::AudioProcessorValueTreeState& apvts, const Theme& theme, juce::LookAndFeel& menuLnF)
        : t (theme), lnf (menuLnF), param (apvts.getParameter (ParamID::keySnap))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Key");
        setTooltip ("Key: OFF leaves the body as authored. Pick a key to shift the whole filter to it. The dim name is the key the input sounds like.");
        if (param != nullptr)
            attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
    }
    static juce::String nameFor (int label)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (label < 0 || label >= 24) return juce::String::charToString (0x2014);
        return juce::String (names[label % 12]) + (label >= 12 ? " min" : " maj");
    }
    static int choiceForDetection (int label)
    {
        if (label < 0 || label >= 24) return 0;
        return label < 12 ? 13 + label : 1 + (label - 12);
    }
    void setState (int detectedLabel, bool isListening)
    {
        if (detectedLabel == detected && isListening == listening) return;
        detected = detectedLabel; listening = isListening; repaint();
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (param == nullptr || juce::approximatelyEqual (w.deltaY, 0.0f)) return;
        const int n = param->getAllValueStrings().size();
        const int next = ((choice() + (w.deltaY > 0 ? 1 : -1)) % n + n) % n;
        setChoice (next);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (param == nullptr) return;
        if (e.mods.isPopupMenu()) { showParamContextMenu (*this, param); return; }
        if (! boxRect().contains (e.position)) return;
        const auto names = param->getAllValueStrings();
        const int cur = choice();
        const int heard = choiceForDetection (detected);
        juce::PopupMenu m;
        m.setLookAndFeel (&lnf);
        m.addItem (1, "OFF", true, cur == 0);
        if (heard > 0)
            m.addItem (heard + 1, names[heard] + "   (heard)", true, cur == heard);
        m.addSeparator();
        for (int i = 1; i < names.size(); ++i)
            m.addItem (i + 1, names[i], true, cur == i);
        juce::Component::SafePointer<KeyBox> self (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [self] (int id) { if (self != nullptr && id > 0) self->setChoice (id - 1); });
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        drawBayCaption (g, b.withWidth (kWordW), "KEY", t);
        const auto box = boxRect();
        drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
        const int cur = choice();
        const juce::Rectangle<float> lamp { box.getX() + 5.0f, box.getCentreY() - 2.5f, 5.0f, 5.0f };
        g.setColour (listening ? t.modulationLamp() : t.labelInk().withAlpha (0.30f));
        g.fillEllipse (lamp);
        const auto ink = t.textColour ("morphReadout", juce::Colour (0xff2a2722));
        if (cur == 0)
            drawCrispText (g, box.withTrimmedLeft (12.0f).reduced (2.0f, 1.0f), nameFor (detected), kBayValuePt, ink.withAlpha (0.45f));
        else
            drawCrispText (g, box.withTrimmedLeft (12.0f).reduced (2.0f, 1.0f), param->getAllValueStrings()[cur], kBayValuePt, ink);
        juce::Path arrow;
        const float ax = box.getRight() - 8.0f, ay = box.getCentreY();
        arrow.addTriangle (ax - 3.0f, ay - 1.5f, ax + 3.0f, ay - 1.5f, ax, ay + 2.5f);
        g.setColour (ink.withAlpha (0.75f));
        g.fillPath (arrow);
    }
private:
    juce::Rectangle<float> boxRect() const
    {
        return getLocalBounds().toFloat().withTrimmedLeft (kWordW + 4.0f).reduced (0.0f, 1.0f);
    }
    int choice() const
    {
        return param != nullptr ? juce::roundToInt (param->convertFrom0to1 (param->getValue())) : 0;
    }
    void setChoice (int index)
    {
        if (attachment != nullptr && param != nullptr)
            attachment->setValueAsCompleteGesture ((float) index);
    }
    Theme t;
    juce::LookAndFeel& lnf;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int detected = -1;
    bool listening = false;
    bool hover = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBox)
};
}
