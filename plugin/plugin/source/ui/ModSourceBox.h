// REWIRED (2026-08-13). This file was restored from bb824a13 by a careless
// `git checkout HEAD --`, which also destroyed its uncommitted working state.
// It referenced ParamID::modOn / modShape / modTrigger - the MorphMod trio,
// deleted when Movement became a single curated-phrase parameter. It now
// drives ParamID::movePreset, where index 0 is OFF and every other index is
// an armed phrase, so "armed" is simply preset != 0 and there is no separate
// on/off or trigger to write.
#pragma once
#include "SectionRail.h"
#include "Theme.h"
#include "ParamInteraction.h"
#include "../parameters/TrenchParameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>
namespace trench::ui
{
// MOVEMENT's source: the one place modulation is chosen (Tyson 2026-08-04,
// "it doesn't make sense to have modulation chip and room"). OFF, or any shape
// the modShape parameter names — the base waves and the curated phrases, read
// from the parameter itself so the room can never disagree with the engine.
// Same row grammar as the rest of the bay: engraved caption, frosted select.
class ModSourceBox final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    ModSourceBox (juce::AudioProcessorValueTreeState& apvts, const Theme& theme)
        : t (theme)
    {
        modShapeParam   = apvts.getParameter (ParamID::movePreset);
        modOnParam      = modShapeParam;   // OFF is preset index 0
        modTriggerParam = nullptr;
        auto repaintOnChange = [this] (float) { repaint(); };
        for (auto* p : { modOnParam, modShapeParam })
            if (p != nullptr)
                atts.push_back (std::make_unique<juce::ParameterAttachment> (*p, repaintOnChange));
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Modulation");
        setHelpText ("Modulation preset - how MORPH travels on its own; click the name to punch it in/out, spinner or wheel to step, double-click for the list");
        setTooltip ("Modulation: click name = on/off (keeps the pattern), spinner/wheel = step, double-click = list");
    }

    /// The presets you can land on by stepping: the whole function-generator
    /// bank, GROWL, and LIVE (only while a phrase is actually being fed in).
    /// The old skip-six law covered the retired base waves; the bank swap
    /// (2026-08-15) left it hiding the first five E-mu patterns — only OFF
    /// at index 0 is excluded now.
    std::vector<int> selectable() const
    {
        std::vector<int> out;
        if (modShapeParam == nullptr)
            return out;
        const auto names = modShapeParam->getAllValueStrings();
        const bool liveReady = livePhraseProvider == nullptr || livePhraseProvider();
        for (int i = 1; i < names.size(); ++i)
            if (names[i] != "LIVE" || liveReady)
                out.push_back (i);
        return out;
    }

    /// One step through the preset list, wrapping. Stepping ARMS modulation —
    /// walking the list with the sound off would be a silent control.
    void step (int dir)
    {
        const auto list = selectable();
        if (list.empty())
            return;
        int at = 0;
        if (armed())
        {
            const auto found = std::find (list.begin(), list.end(), shapeIndex());
            if (found != list.end())
                at = (int) std::distance (list.begin(), found);
        }
        else
        {
            // stepping up from OFF starts at the top of the list, not past it
            at = dir > 0 ? -1 : 0;
        }
        const int n = (int) list.size();
        const int next = ((at + dir) % n + n) % n;
        write (modShapeParam, (float) list[(size_t) next]);
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        // The face's gesture law: the mouse wheel always works.
        if (! juce::approximatelyEqual (w.deltaY, 0.0f))
            step (w.deltaY > 0 ? 1 : -1);
    }
    /// LIVE is only real while a phrase is being fed in.
    std::function<bool()> livePhraseProvider;
    bool armed() const { return modShapeParam != nullptr
                             && modShapeParam->getCurrentValueAsText() != "OFF"; }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition()))
            return;
        // Right-click means "automate this" everywhere else on the face
        // (WheelControl, MixKnob, ThinWheel, ValueReadout all do it). Without
        // this guard it opened the preset list instead, so the one gesture the
        // host expects on a parameter did something else here.
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, modShapeParam);
            return;
        }
        // Stepping first: changing the modulation preset used to cost a click, a
        // popup and a hunt down a list, for what is really a walk through a
        // handful of phrases (Tyson 2026-08-07: "make the modulation preset much
        // easier to change"). The arrows step it in place; the name still opens
        // the full list for jumping straight to one.
        const auto boxArea = boxBounds();
        if (boxArea.contains (e.position))
        {
            if (e.position.x > boxArea.getRight() - kStepperW)
            {
                step (e.position.y < boxArea.getCentreY() ? +1 : -1);
                return;
            }
            // PUNCH IN / PUNCH OUT (Tyson 2026-08-15 "not make it lose the
            // users creative flow"): a single click on the name bypasses
            // movement WITHOUT losing the pattern — the name stays, dimmed —
            // and a second click re-arms the same pattern. The full list
            // moved to double-click. (A double fires two toggles first: net
            // no state change, then the list opens.)
            if (armed())
            {
                lastArmed = shapeIndex();
                write (modShapeParam, 0.0f);
            }
            else
            {
                const auto list = selectable();
                int target = lastArmed;
                if (target <= 0 && ! list.empty())
                    target = list.front();
                if (target > 0)
                    write (modShapeParam, (float) target);
            }
            repaint();
        }
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || ! boxBounds().contains (e.position))
            return;
        juce::PopupMenu m;
        m.setLookAndFeel (&lightMenu);
        const bool on = armed();
        m.addItem (1, "OFF", true, ! on);
        m.addSeparator();
        if (modShapeParam != nullptr)
        {
            const auto names = modShapeParam->getAllValueStrings();
            const int cur = shapeIndex();
            const bool liveReady = livePhraseProvider == nullptr || livePhraseProvider();
            for (int i = 1; i < names.size(); ++i)
                m.addItem (i + 2, names[i], names[i] == "LIVE" ? liveReady : true,
                           on && cur == i);
        }
        juce::Component::SafePointer<ModSourceBox> self (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [self] (int id) { if (self != nullptr && id > 0) self->apply (id); });
    }
    void paint (juce::Graphics& g) override
    {
        juce::Graphics::ScopedSaveState s (g);
        g.setOpacity (isEnabled() ? 1.0f : 0.32f);
        // NO CAPTION (Tyson 2026-08-15 bay doors): the box IS the preset — a
        // word above it spent a row saying so. The component keeps the caption
        // band's height so the box's seat on the plate did not move; the band
        // is simply blank plate now. ("PRESET" not "SOURCE" history: 2026-08-07.)
        const auto box = boxBounds();
        drawMutedBoneReadout (g, box, box.getHeight() * 0.17f, hover, t);
        // Name centred BETWEEN the steppers, so stepping never shifts it, and
        // SIZED TO FIT: drawCrispText clips rather than ellipsing, and the
        // preset names run to 24 characters ("1 Oct Random - Chromatic"), which
        // no fixed size holds at bay width. Shrinking a long name a point or
        // two is invisible; losing its tail is not.
        auto nameArea = box.reduced (2.0f, 0.5f);
        nameArea.removeFromRight (kStepperW);
        nameArea.removeFromLeft (6.0f);
        // Punched out: the held name sits back at 45% - present, not playing.
        drawCrispText (g, nameArea, stateWord(), fittedNamePt (stateWord(), nameArea.getWidth()),
                       juce::Colour (0xff2a2722).withAlpha (armed() ? 1.0f : 0.45f), true);

        // The E-MU spinner, the same construction the room selector wears:
        // up = next, down = previous, the name still opens the whole list.
        drawEmuSpinner (g, juce::Rectangle<float> (box.getRight() - kStepperW, box.getY(),
                                                   kStepperW, box.getHeight()),
                        ! selectable().empty(), t);
    }
private:
    static constexpr float kStepperW = 13.0f;   // hit target for one arrow
    static constexpr float kNamePt = 9.5f;      // the bay's select voice
    static constexpr float kNameMinPt = 8.0f;   // below this it stops being legible
    static float fittedNamePt (const juce::String& text, float available)
    {
        const float wide = juce::GlyphArrangement::getStringWidth (displayFont (kNamePt, true), text);
        if (wide <= available || wide <= 0.0f)
            return kNamePt;
        return juce::jmax (kNameMinPt, kNamePt * available / wide);
    }
    juce::Rectangle<float> boxBounds() const
    {
        return getLocalBounds().toFloat().withTrimmedTop (12.5f);
    }
    int shapeIndex() const noexcept
    {
        if (modShapeParam == nullptr) return 0;
        return juce::jlimit (0, juce::jmax (0, modShapeParam->getAllValueStrings().size() - 1),
                             juce::roundToInt (modShapeParam->convertFrom0to1 (modShapeParam->getValue())));
    }
    juce::String stateWord() const
    {
        if (armed() && modShapeParam != nullptr)
            return modShapeParam->getCurrentValueAsText();
        // Punched out but a pattern is held: the name stays on the box,
        // dimmed - the selection is never lost, one click brings it back.
        if (lastArmed > 0 && modShapeParam != nullptr)
        {
            const auto names = modShapeParam->getAllValueStrings();
            if (lastArmed < names.size())
                return names[lastArmed];
        }
        return "OFF";
    }
    static void write (juce::RangedAudioParameter* p, float denorm)
    {
        if (p == nullptr) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (denorm));
        p->endChangeGesture();
    }
    void apply (int id)
    {
        if (id == 1)
        {
            write (modOnParam, 0.0f);
        }
        else
        {
            // Shape only reaches MorphMod on the SYNC leg — picking one arms it,
            // or the choice would be silent under the ENV default.
            write (modOnParam, 1.0f);
            write (modTriggerParam, 1.0f);   // SYNC
            write (modShapeParam, (float) (id - 2));
        }
        repaint();
    }
    Theme t;
    SectionRail::LightMenuLnF lightMenu;
    juce::RangedAudioParameter* modOnParam = nullptr;
    juce::RangedAudioParameter* modShapeParam = nullptr;
    juce::RangedAudioParameter* modTriggerParam = nullptr;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> atts;
    bool hover = false;
    // The punch-out memory: which pattern a single click restores.
    int lastArmed = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModSourceBox)
};
}
