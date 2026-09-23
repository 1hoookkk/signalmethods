#pragma once
#include "ModulationChip.h"
#include "MixKnob.h"
#include "ValueReadout.h"
#include "../PluginProcessor.h"

namespace trench::ui
{
class ModulationBay final : public juce::Component
{
    struct BayLook final : juce::LookAndFeel_V4
    {
        explicit BayLook (const Theme& t) : theme (t) {}
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            if (b.getTitle() == "Movement")
            {
                drawMutedBoneReadout (g, b.getLocalBounds().toFloat(), 2.0f, over || down, theme);
                const float x = (float) b.getWidth() - 8, y = b.getHeight() * 0.5f;
                juce::Path arrow;
                arrow.addTriangle (x - 3, y - 1.5f, x + 3, y - 1.5f, x, y + 2);
                g.setColour (theme.labelInk()); g.fillPath (arrow);
            }
            else if (over || down || b.hasKeyboardFocus (true))
            {
                g.setColour (theme.labelInk().withAlpha (0.12f));
                g.fillRoundedRectangle (b.getLocalBounds().toFloat(), 2);
            }
        }
        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
        {
            g.setColour (theme.labelInk());
            const bool name = b.getTitle() == "Movement";
            const bool arrow = b.getTitle().contains ("movement") && ! name;
            g.setFont (displayFont (arrow ? 16.0f : name ? 11.5f : 10.5f));
            g.drawText (b.getButtonText(), name ? b.getLocalBounds().withTrimmedLeft (4).withTrimmedRight (17)
                                              : b.getLocalBounds(),
                        name ? juce::Justification::centredLeft : juce::Justification::centred);
        }
        Theme theme;
    };
    class Duration final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Duration (ModulationBay& b) : owner (b)
        {
            setTitle ("Movement duration"); setWantsKeyboardFocus (true);
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            setTooltip ("Drag up/right or use arrows: original length, 1, 2, 4 or 8 bars. Double-click for original timing.");
        }
        void paint (juce::Graphics& g) override
        {
            drawMutedBoneReadout (g, getLocalBounds().toFloat(), 2.0f,
                                 isMouseOverOrDragging() || hasKeyboardFocus (true), owner.look.theme);
            g.setColour (owner.look.theme.labelInk()); g.setFont (displayFont (10.5f));
            g.drawText (owner.durationText(), getLocalBounds().reduced (3, 0), juce::Justification::centred);
        }
        void mouseDown (const juce::MouseEvent&) override { start = owner.lengthChoice(); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            owner.selectLength (start + juce::roundToInt ((e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / 12.0f));
        }
        void mouseDoubleClick (const juce::MouseEvent&) override { owner.selectLength (0); }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            if (wheel.deltaY != 0) owner.selectLength (owner.lengthChoice() + (wheel.deltaY > 0 ? 1 : -1));
        }
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::upKey || k == juce::KeyPress::rightKey) { owner.selectLength (owner.lengthChoice() + 1); return true; }
            if (k == juce::KeyPress::downKey || k == juce::KeyPress::leftKey) { owner.selectLength (owner.lengthChoice() - 1); return true; }
            if (k == juce::KeyPress::homeKey) { owner.selectLength (0); return true; }
            if (k == juce::KeyPress::endKey) { owner.selectLength (4); return true; }
            return false;
        }
    private:
        ModulationBay& owner;
        int start = 0;
    };
public:
    ModulationBay (PluginProcessor& p, ModulationChip& control, const Theme& theme,
                   juce::File directory = trench::MotionLibrary::directory())
        : processor (p), chip (control), look (theme), libraryDirectory (std::move (directory)), duration (*this),
          notch (p.apvts, theme, ParamID::distortion, "Distortion")
    {
        setTitle ("MODULATION"); setLookAndFeel (&look); setWantsKeyboardFocus (true);
        selection.setTitle ("Movement");
        selection.setTooltip ("Choose a movement. Left/right arrows audition the previous or next. Choose it again to restart.");
        selection.onClick = [this] { edit(); };
        addAndMakeVisible (selection);
        addAndMakeVisible (duration);
        notch.setTooltip ("Distortion: internal filter distortion. The body's resonant bloom follows the signal level. 0 leaves the cascade exactly linear.");
        addAndMakeVisible (notch);
        for (const auto* id : { ParamID::movePreset, ParamID::moveLength, ParamID::movePlayback })
        {
            auto attachment = std::make_unique<juce::ParameterAttachment> (*processor.apvts.getParameter (id),
                [this] (float) { refreshMotion (true); });
            attachment->sendInitialUpdate();
            attachments.push_back (std::move (attachment));
        }
    }
    ~ModulationBay() override { setLookAndFeel (nullptr); }
    void selectPattern (int index)
    {
        chip.selectPlayback (0); chip.selectPattern (index);
        processor.restartMovement(); refreshMotion (true);
    }
    void selectLength (int index) { chip.selectLength (juce::jlimit (0, 4, index)); refreshMotion (true); }
    void selectSaved (int index)
    {
        library = trench::MotionLibrary::load (libraryDirectory);
        if (index < 0 || index >= (int) library.size()) return;
        processor.applyUserMotion (library[(size_t) index], true); refreshMotion (true);
    }
    void stepPattern (int delta)
    {
        library = trench::MotionLibrary::load (libraryDirectory);
        const int count = trench::kNumFuncGenPatterns + (int) library.size();
        int index = chip.selectedPattern() - 1;
        if (processor.usingUserMotion())
        {
            index = -1;
            const auto name = processor.userMotion.get().name;
            for (size_t i = 0; i < library.size(); ++i)
                if (library[i].name == name) index = trench::kNumFuncGenPatterns + (int) i;
        }
        index = index < 0 ? (delta > 0 ? 0 : count - 1) : (index + delta + count) % count;
        if (index < trench::kNumFuncGenPatterns) selectPattern (index + 1);
        else selectSaved (index - trench::kNumFuncGenPatterns);
    }
    void edit()
    {
        library = trench::MotionLibrary::load (libraryDirectory);
        juce::PopupMenu menu;
        menu.addItem (1, "Off", true, ! active());
        for (int i = 0; i < trench::kNumFuncGenPatterns; ++i)
            menu.addItem (i + 2, trench::kFuncGenPatterns[i].name, true,
                          ! processor.usingUserMotion() && chip.selectedPattern() == i + 1);
        if (! library.empty()) menu.addSeparator();
        for (size_t i = 0; i < library.size(); ++i)
            menu.addItem (100 + (int) i, library[i].name, true,
                          processor.usingUserMotion() && processor.userMotion.get().name == library[i].name);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&selection),
            [safe = juce::Component::SafePointer<ModulationBay> (this)] (int result)
            {
                if (safe == nullptr || result == 0) return;
                if (result >= 100) safe->selectSaved (result - 100);
                else safe->selectPattern (result - 1);
            });
    }
    bool active() const { return processor.usingUserMotion() || chip.selectedPattern() > 0; }
    void setRadiusActivity (float a) { notch.setActivity (a); }
    int lengthChoice() const { return (int) processor.apvts.getRawParameterValue (ParamID::moveLength)->load(); }
    bool isOnce() const
    {
        const int choice = (int) processor.apvts.getRawParameterValue (ParamID::movePlayback)->load();
        const auto motion = processor.motionForEditing();
        return choice == 2 || (choice == 0 && motion.direction == 5);
    }
    juce::String durationText() const
    {
        if (lengthChoice() > 0)
        {
            const int bars = 1 << (lengthChoice() - 1);
            return juce::String (bars) + " BAR";
        }
        const auto motion = processor.motionForEditing();
        const int loopSteps = motion.loopSteps > 0 ? motion.loopSteps
                             : (motion.direction == 2 ? 2 * motion.steps - 2 : motion.steps);
        const double time = loopSteps * (motion.rateHz > 0 ? 1.0 / motion.rateHz : motion.stepBeats);
        if (motion.rateHz > 0)
            return juce::String (time, std::abs (time - std::round (time)) < 0.001 ? 0 : 2) + " S";
        const double bars = time / 4.0;
        if (bars >= 1.0 && std::abs (bars - std::round (bars)) < 0.001)
            return juce::String (juce::roundToInt (bars)) + " BAR";
        return juce::String (time, std::abs (time - std::round (time)) < 0.001 ? 0 : 2) + " BEAT";
    }
    void refreshMotion (bool force = false)
    {
        const auto revision = processor.userMotion.revision();
        const bool custom = processor.usingUserMotion();
        const int preset = (int) processor.apvts.getRawParameterValue (ParamID::movePreset)->load();
        const int length = lengthChoice();
        const int play = (int) processor.apvts.getRawParameterValue (ParamID::movePlayback)->load();
        if (! force && revision == lastRevision && custom == lastCustom
            && preset == lastPreset && length == lastLength && play == lastPlayback) return;
        lastRevision = revision; lastCustom = custom;
        lastPreset = preset; lastLength = length; lastPlayback = play;
        const auto motion = processor.motionForEditing();
        selection.setButtonText (active() ? motion.name + (custom && motion.dirty ? " *" : "") : "Off");
        duration.setVisible (active());
        resized(); repaint(); duration.repaint();
    }
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::leftKey) { stepPattern (-1); return true; }
        if (key == juce::KeyPress::rightKey) { stepPattern (1); return true; }
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey) { edit(); return true; }
        return false;
    }
    void resized() override
    {
        const int durationWidth = juce::jlimit (40, 75, juce::roundToInt (
            juce::GlyphArrangement::getStringWidth (displayFont (10.5f), durationText())) + 12);
        const int available = getParentComponent()
            ? juce::jmin (kFaceLockedWidth, getParentComponent()->getWidth()) - getX() - 28 : 240;
        const int wanted = juce::roundToInt (
            juce::GlyphArrangement::getStringWidth (displayFont (11.5f), selection.getButtonText())) + 22;
        const int width = juce::jlimit (42, juce::jmax (42, juce::jmin (165, available)), wanted);
        selection.setBounds (0, 14, width, 20);
        duration.setBounds (0, 37, durationWidth, 20);
        notch.setBounds (0, 63, 112, 36);
        setSize (juce::jmax (juce::jmax (width, durationWidth), 112), 99);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (look.theme.labelInk()); g.setFont (displayFont (10.0f));
        g.drawText ("MODULATION", 0, 0, juce::jmax (selection.getWidth(), 112), 12,
                    juce::Justification::centredLeft);
    }
private:
    PluginProcessor& processor;
    ModulationChip& chip;
    BayLook look;
    juce::File libraryDirectory;
    juce::TextButton selection;
    Duration duration;
    MixKnob notch;
    std::vector<trench::UserMotion> library;
    std::vector<std::unique_ptr<juce::ParameterAttachment>> attachments;
    unsigned lastRevision = 0;
    bool lastCustom = false;
    int lastPreset = -1, lastLength = -1, lastPlayback = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationBay)
};
}
