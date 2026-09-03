#pragma once
#include "Theme.h"
#include "../dsp/WheelLoop.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace trench::ui
{
inline constexpr int kDevPanelWidth = 210;

class DevPanel final : public juce::Component,
                       private juce::Timer
{
public:
    DevPanel (const Theme& theme, WheelLoop& loopRef, juce::File loopDir)
        : t (theme), loop (loopRef), dir (std::move (loopDir))
    {
        for (auto* b : { &armButton, &stopButton, &playButton, &saveButton, &loadButton })
        {
            addAndMakeVisible (*b);
            b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2f2d));
            b->setColour (juce::TextButton::textColourOffId, t.curveColour());
        }
        armButton.setButtonText ("REC AT NEXT BAR");
        stopButton.setButtonText ("STOP");
        playButton.setButtonText ("LOOP");
        saveButton.setButtonText ("SAVE");
        loadButton.setButtonText ("LOAD");
        bars.addItemList ({ "1 bar", "2 bars", "4 bars", "8 bars" }, 1);
        bars.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (bars);
        grid.addItemList ({ "RAW", "1/8 STEPS", "1/16 STEPS", "1/32 STEPS" }, 1);
        grid.setSelectedId (3, juce::dontSendNotification);
        addAndMakeVisible (grid);
        glide.setButtonText ("GLIDE");
        glide.setColour (juce::ToggleButton::textColourId, t.curveColour());
        glide.setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (glide);
        grid.onChange = [this] { requantize(); };
        glide.onClick = [this] { requantize(); };
        addAndMakeVisible (name);
        name.setText ("loop", juce::dontSendNotification);
        name.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff11151a));
        name.setColour (juce::TextEditor::textColourId, t.curveColour());
        armButton.onClick = [this] { loop.arm (barsSelected()); };
        stopButton.onClick = [this] { loop.stop(); };
        playButton.onClick = [this] { loop.play(); };
        saveButton.onClick = [this] { save(); };
        loadButton.onClick = [this] { load(); };
        startTimer (60);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        r.removeFromTop (28);
        r.removeFromTop (22);
        bars.setBounds (r.removeFromTop (24)); r.removeFromTop (6);
        armButton.setBounds (r.removeFromTop (26)); r.removeFromTop (4);
        auto row = r.removeFromTop (26);
        stopButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 2));
        row.removeFromLeft (4);
        playButton.setBounds (row);
        r.removeFromTop (6);
        row = r.removeFromTop (24);
        grid.setBounds (row.removeFromLeft (row.getWidth() * 3 / 5));
        row.removeFromLeft (4);
        glide.setBounds (row);
        r.removeFromTop (10);
        r.removeFromTop (60);
        name.setBounds (r.removeFromTop (24)); r.removeFromTop (4);
        row = r.removeFromTop (26);
        saveButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 2));
        row.removeFromLeft (4);
        loadButton.setBounds (row);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff181c1b));
        g.setColour (juce::Colour (0xff2b302e));
        g.fillRect (0, 0, 1, getHeight());
        auto r = getLocalBounds().reduced (10);
        g.setFont (displayFont (13.0f, true));
        g.setColour (t.curveColour());
        g.drawText ("DEV", r.removeFromTop (28), juce::Justification::centredLeft, false);
        g.setFont (displayFont (10.5f, true));
        g.setColour (t.curveColour().withAlpha (0.7f));
        g.drawText ("WHEEL LOOP", r.removeFromTop (22), juce::Justification::centredLeft, false);
        r.removeFromTop (24 + 6 + 26 + 4 + 26 + 6 + 24 + 10);
        auto status = r.removeFromTop (60);
        g.setFont (displayFont (10.5f, false));
        g.setColour (t.curveColour().withAlpha (0.85f));
        g.drawFittedText (statusText(), status, juce::Justification::topLeft, 3);
        drawLoopStrip (g, status.removeFromBottom (18).toFloat());
    }
private:
    int barsSelected() const { return 1 << juce::jmax (0, bars.getSelectedId() - 1); }
    juce::String statusText() const
    {
        const auto m = loop.currentMode();
        const int beats = loop.beatsRecorded();
        const double phase = loop.phaseBeats();
        switch (m)
        {
            case WheelLoop::Mode::Armed: return "armed: move the wheel, recording starts on the bar";
            case WheelLoop::Mode::Recording: return "recording  beat " + juce::String (phase, 1);
            case WheelLoop::Mode::Playing: return "looping " + juce::String (beats / 4) + " bar(s)  beat " + juce::String (phase, 1);
            default: return beats > 0 ? juce::String (beats / 4) + " bar(s) held" : "no loop";
        }
    }
    void drawLoopStrip (juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const int beats = loop.beatsRecorded();
        if (beats <= 0) return;
        const auto ticks = loop.snapshot();
        g.setColour (juce::Colour (0xff0c1210));
        g.fillRect (r);
        g.setColour (t.curveColour().withAlpha (0.9f));
        juce::Path p;
        for (size_t i = 0; i < ticks.size(); ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) ticks.size();
            const float y = r.getBottom() - r.getHeight() * ticks[i];
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.strokePath (p, juce::PathStrokeType (1.0f));
        if (loop.currentMode() == WheelLoop::Mode::Playing)
        {
            const float x = r.getX() + r.getWidth() * (float) (loop.phaseBeats() / (double) beats);
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        }
    }
    void save()
    {
        const int beats = loop.beatsRecorded();
        if (beats <= 0) return;
        dir.createDirectory();
        auto safe = juce::File::createLegalFileName (name.getText().trim());
        if (safe.isEmpty()) safe = "loop";
        const auto ticks = loop.snapshot();
        juce::var arr;
        for (float v : ticks) arr.append (v);
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("name", name.getText().trim());
        obj->setProperty ("ticksPerBeat", WheelLoop::kTicksPerBeat);
        obj->setProperty ("beats", beats);
        obj->setProperty ("values", arr);
        dir.getChildFile (safe + ".wheelloop").replaceWithText (juce::JSON::toString (juce::var (obj)));
    }
    void load()
    {
        juce::PopupMenu menu;
        juce::Array<juce::File> files;
        dir.findChildFiles (files, juce::File::findFiles, false, "*.wheelloop");
        for (int i = 0; i < files.size(); ++i)
            menu.addItem (i + 1, files[i].getFileNameWithoutExtension());
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&loadButton), [this, files] (int choice)
        {
            if (choice <= 0) return;
            const auto v = juce::JSON::parse (files[choice - 1]);
            const int beats = (int) v.getProperty ("beats", 0);
            std::vector<float> ticks;
            if (auto* arr = v.getProperty ("values", juce::var()).getArray())
                for (const auto& x : *arr) ticks.push_back ((float) (double) x);
            if (loop.load (ticks, beats))
                name.setText (v.getProperty ("name", files[choice - 1].getFileNameWithoutExtension()).toString(), juce::dontSendNotification);
        });
    }
    void requantize()
    {
        const int id = grid.getSelectedId();
        const int stepsPerBeat = id <= 1 ? 0 : (id == 2 ? 2 : id == 3 ? 4 : 8);
        loop.quantize (stepsPerBeat, glide.getToggleState());
        repaint();
    }
    void timerCallback() override
    {
        if (loop.takeRawDirty())
            requantize();
        repaint();
    }
    Theme t;
    WheelLoop& loop;
    juce::File dir;
    juce::TextButton armButton, stopButton, playButton, saveButton, loadButton;
    juce::ComboBox bars;
    juce::ComboBox grid;
    juce::ToggleButton glide;
    juce::TextEditor name;
};
}
