#pragma once
#include "Theme.h"
#include "SelectorLookAndFeel.h"
#include "ParamInteraction.h"
#include "../PluginProcessor.h"
#include "../TrenchBodyRoster.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <memory>
#include <vector>
namespace trench::ui
{
class TypeSelectorView : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::ComboBox::Listener
{
public:
    TypeSelectorView (juce::AudioProcessorValueTreeState& apvts, const Theme& theme)
        : t (theme)
    {
        setInterceptsMouseClicks (true, true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Type");
        setHelpText ("Type - choose the filter body.");
        setTooltip ("TYPE: choose the filter body");
        selector.setLookAndFeel (&menuLookAndFeel);
        selector.setInterceptsMouseClicks (false, false);
        selector.setWantsKeyboardFocus (false);
        for (auto colourId : { juce::ComboBox::backgroundColourId, juce::ComboBox::outlineColourId,
                               juce::ComboBox::buttonColourId, juce::ComboBox::arrowColourId,
                               juce::ComboBox::textColourId })
            selector.setColour (colourId, juce::Colours::transparentBlack);
        selector.setTextWhenNothingSelected ({});
        populate();
        // NOT a ComboBoxAttachment: that maps item <-> parameter by NORMALISED
        // position across the item count, so it only agrees with the roster
        // index while the parameter's range ends at numItems-1. The range is
        // frozen and the roster grows with the user's bodies folder, so the two
        // drift apart and the glass loads a different body than it names. Drive
        // the integer index directly instead.
        param = apvts.getParameter (ParamID::body);
        if (auto* bodyParam = param)
        {
            attachment = std::make_unique<juce::ParameterAttachment> (
                *bodyParam,
                [this] (float value)
                {
                    const int index = juce::roundToInt (value);
                    if (index < 0 || index >= selector.getNumItems()
                        || index == selector.getSelectedItemIndex())
                        return;
                    const juce::ScopedValueSetter<bool> guard (writingParameter, true);
                    selector.setSelectedItemIndex (index, juce::sendNotificationSync);
                },
                apvts.undoManager);
            attachment->sendInitialUpdate();
        }
        selector.addListener (this);
        addAndMakeVisible (selector);
    }
    ~TypeSelectorView() override
    {
        selector.setLookAndFeel (nullptr);
        selector.removeListener (this);
    }
    std::function<void()> onSeed;
    std::function<void()> onExportBody;
    /// Audition a body by EAR without touching the parameter: no automation, no
    /// undo step, nothing committed until a click. Set by the editor.
    void resized() override { selector.setBounds (getLocalBounds().reduced (3, 2)); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }
    // The BODY bar is a ROLLER, like the wheels: drag across it and the filters
    // ONE-LAW GESTURES (Tyson 2026-08-09 UX contract: "I would remove
    // BODY's hidden horizontal-flick behaviour"). Choice controls: click
    // opens the list, wheel/arrows step. The drag-flick drum is gone - a
    // hidden gesture on the most-used control on the plate.
    void mouseUp (const juce::MouseEvent& e) override
    {
        // Right-click is the host-automation gesture across the whole face. It
        // used to fall through and open the body browser, so the one thing a
        // host user expects on a parameter did something else on the BODY
        // selector — the most-used control on the plate.
        if (e.mods.isPopupMenu())
        {
            showParamContextMenu (*this, param);
            repaint();
            return;
        }
        refreshFromDisk();
        if (onOpenBrowser != nullptr)
            onOpenBrowser (selector.getSelectedItemIndex());
        repaint();
    }
    /// The face opens its own body list (BodyBrowser) instead of a system menu.
    std::function<void (int)> onOpenBrowser;
    void setSelectedBody (int idx) { commitBody (idx); }
    int selectedBody() const { return selector.getSelectedItemIndex(); }
    void refreshFromDisk()
    {
        // Keep the selected BODY, not the selected slot: a new file in the
        // user's bodies folder sorts in and shifts every index after it. If the
        // body moved, write the new index through so the parameter and the glass
        // still name the same thing.
        const int before = selector.getSelectedItemIndex();
        const auto keepBase = trench::bodyBaseForIndex (before);
        trench::rescanBodyRoster();
        populate();
        const int now = keepBase.isNotEmpty() ? trench::bodyIndexForBase (keepBase) : before;
        if (now >= 0 && now < selector.getNumItems())
            selector.setSelectedItemIndex (now, now == before ? juce::dontSendNotification
                                                             : juce::sendNotificationSync);
    }
    juce::RangedAudioParameter* param = nullptr;
    void commitBody (int idx)
    {
        selector.setSelectedItemIndex (idx, juce::sendNotificationSync);
    }
    void paint (juce::Graphics& g) override
    {
        const auto recess = getLocalBounds().toFloat();
        const auto bar = recess;
        const bool hot = isMouseOverOrDragging (true) || selector.isPopupActive();
        drawIvoryWell (g, bar, bar.getHeight() * 0.18f, hot, t);
        auto inner = bar.reduced (13.0f, 2.0f);
        juce::Rectangle<float> box;
        const auto arrowSrc = t.layout.sourceRectFor ("typeArrow");
        if (arrowSrc.getWidth() > 1.0f)
            box = sourceRectToEditor (arrowSrc).translated (-(float) getX(), -(float) getY());
        else
        {
            const float w = bar.getHeight() + t.typeArrowExtra();
            box = inner.removeFromRight (w).withSizeKeepingCentre (w - 8.0f, bar.getHeight() - 8.0f);
        }
        const auto textArea = juce::Rectangle<float> (inner.getX(), inner.getY(),
                                                      juce::jmax (10.0f, box.getX() - inner.getX() - 16.0f),
                                                      inner.getHeight());
        const auto selectedIndex = selector.getSelectedId() - 1;
        // E-mu choice values are regular UI text; only their field labels are
        // bold. Making the body name bold flattened that hierarchy.
        g.setFont (displayFont (t.fontSize ("typeName", 17.0f), false));
        // ONE ink, hot or not (Tyson 2026-08-08: "keep them simple like the
        // preset selector"). The PRESET box states its name in a single ink and
        // lets the frosted panel's own active hairline carry the hover; this bar
        // additionally darkened its text to pure black, so the name changed
        // weight under the cursor for no reason a user could name.
        const auto ink = t.textColour ("typeName", juce::Colour (0xff1a1713));
        const auto typeText = selectedIndex >= 0 ? trench::bodyDisplayName (selectedIndex) : juce::String();
        g.setColour (ink);
        g.drawText (typeText, textArea.toNearestInt(),
                    juce::Justification::centredLeft, false);
        // A SOLID TRIANGLE, and no divider (Tyson 2026-08-08, off the four
        // reference panels: "here's your visual language"). Measured on their
        // TYPE bar: the mark is 7x4 px filled - row widths 7, 5, 3, 1, shrinking
        // monotonically - sitting 3px off the right edge of a 14px bar, with no
        // rule separating it from the name. Ours was a 1.5px stroked V behind a
        // hairline: two marks where the language has one. Same 7:4 aspect here.
        const auto arrow = box.withSizeKeepingCentre (9.0f, 5.0f).translated (0.0f, 0.5f);
        juce::Path arrowPath;
        arrowPath.addTriangle (arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
        g.setColour (juce::Colours::white.withAlpha (0.40f));
        g.fillPath (arrowPath, juce::AffineTransform::translation (0.0f, 1.0f));
        g.setColour (ink);
        g.fillPath (arrowPath);
    }
private:
    void comboBoxChanged (juce::ComboBox*) override
    {
        if (! writingParameter && attachment != nullptr)
            attachment->setValueAsCompleteGesture ((float) selector.getSelectedItemIndex());
        if (onAnnounce)
            onAnnounce (selector.getText());
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int n = selector.getNumItems();
        if (n == 0 || w.deltaY == 0.0f)
            return;
        const int step = w.deltaY > 0.0f ? -1 : 1;
        const int idx = juce::jlimit (0, n - 1, selector.getSelectedItemIndex() + step);
        selector.setSelectedItemIndex (idx, juce::sendNotificationSync);
    }
public:
    std::function<void (const juce::String&)> onAnnounce;
private:
    void populate()
    {
        selector.clear (juce::dontSendNotification);
        int count = 0;
        const auto* entries = trench::bodyRoster (count);
        bool userHeading = false;
        for (int i = 0; i < count; ++i)
        {
            if (! userHeading && juce::String (entries[i].category) == "USER")
            {
                selector.addSeparator();
                selector.addSectionHeading ("Your bodies");
                userHeading = true;
            }
            selector.addItem (entries[i].displayName, i + 1);
        }
    }
    Theme t;
    SelectorLookAndFeel menuLookAndFeel;
    juce::ComboBox selector;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool writingParameter = false;
};
}
