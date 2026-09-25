#pragma once
#include "../source/PluginProcessor.h"
#include "../source/ui/Theme.h"
#include <cmath>
#include <functional>
#include <vector>

namespace trench::ui
{
class MotionCanvas final : public juce::Component
{
public:
    explicit MotionCanvas (trench::UserMotion& motion) : data (motion)
    {
        setTitle ("Motion points");
        setWantsKeyboardFocus (true);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }
    std::function<void()> onBegin, onChange;
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (12, 16); }
    bool loops() const { return data.playback == 1 || (data.playback == 0 && data.direction != 5); }
    juce::Point<float> point (int i) const
    {
        const auto r = plot();
        return { r.getX() + r.getWidth() * (float) i / (float) (loops() ? data.steps : data.steps - 1),
                 r.getBottom() - r.getHeight() * (data.values[(size_t) i] + 1.0f) * 0.5f };
    }
    void paint (juce::Graphics& g) override
    {
        const auto r = plot();
        g.fillAll (juce::Colour (0xff0c1213));
        g.setColour (juce::Colour (0xff82c8bc).withAlpha (0.12f));
        for (int i = 0; i <= 4; ++i)
        {
            const float x = r.getX() + r.getWidth() * i / 4;
            const float y = r.getY() + r.getHeight() * i / 4;
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        }
        juce::Path path;
        auto previous = point (0);
        path.startNewSubPath (previous);
        for (int i = 1; i <= data.steps; ++i)
        {
            if (i == data.steps && ! loops()) break;
            const auto next = i == data.steps ? juce::Point<float> (r.getRight(), point (0).y) : point (i);
            if (! data.smooth) path.lineTo (next.x, previous.y);
            path.lineTo (next);
            previous = next;
        }
        g.setColour (juce::Colour (0xff82c8bc));
        g.strokePath (path, juce::PathStrokeType (1.5f));
        for (int i = 0; i < data.steps; ++i)
        {
            const auto p = point (i);
            const float radius = i == selected ? 4.5f : 3.3f;
            g.setColour (i == selected ? juce::Colour (0xffd8ece8) : juce::Colour (0xff82c8bc));
            g.fillEllipse (p.x - radius, p.y - radius, radius * 2, radius * 2);
        }
        g.setFont (displayFont (10.0f));
        g.setColour (juce::Colour (0xff9eb4ad));
        g.drawText ("LOW", 4, getHeight() - 13, 40, 12, juce::Justification::centredLeft);
        g.drawText ("HIGH", 4, 1, 40, 12, juce::Justification::centredLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto r = plot();
        selected = juce::jlimit (0, data.steps - 1, juce::roundToInt (
            (e.position.x - r.getX()) / r.getWidth() * (loops() ? data.steps : data.steps - 1)));
        if (onBegin) onBegin();
        dragPoint (e.position.y);
    }
    void mouseDrag (const juce::MouseEvent& e) override { dragPoint (e.position.y); }
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey)
        {
            selected = juce::jlimit (0, data.steps - 1, selected + (key == juce::KeyPress::leftKey ? -1 : 1));
            repaint(); return true;
        }
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            if (onBegin) onBegin();
            const float amount = key.getModifiers().isShiftDown() ? 0.01f : 0.05f;
            data.values[(size_t) selected] = juce::jlimit (-1.0f, 1.0f, data.values[(size_t) selected]
                + (key == juce::KeyPress::upKey ? amount : -amount));
            if (onChange) onChange();
            repaint(); return true;
        }
        return false;
    }
private:
    void dragPoint (float y)
    {
        const auto r = plot();
        data.values[(size_t) selected] = juce::jlimit (-1.0f, 1.0f, 2.0f * (r.getBottom() - y) / r.getHeight() - 1.0f);
        if (onChange) onChange();
        repaint();
    }
    trench::UserMotion& data;
    int selected = 0;
};

class MotionEditor final : public juce::Component
{
    struct EditorLook final : juce::LookAndFeel_V4
    {
        juce::Font getComboBoxFont (juce::ComboBox&) override { return displayFont (11.5f); }
        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (4, 0, box.getWidth() - 21, box.getHeight());
            label.setFont (getComboBoxFont (box));
        }
    };
public:
    MotionEditor (PluginProcessor& p, const Theme& t, juce::File directory = trench::MotionLibrary::directory())
        : processor (p), theme (t), root (std::move (directory)), draft (p.motionForEditing()), canvas (draft)
    {
        setTitle ("Edit movement");
        setLookAndFeel (&look);
        addAndMakeVisible (canvas);
        canvas.onBegin = [this] { remember(); };
        canvas.onChange = [this] { publish(); };
        configure (smooth, "Motion smoothing", { "Smooth", "Step" });
        configure (duration, "Motion duration", { "1/4 bar", "1/2 bar", "1 bar", "2 bars", "4 bars" });
        configure (rate, "Motion rate", { "Sync", "0.5 Hz", "1 Hz", "2 Hz", "3 Hz", "4 Hz", "6 Hz", "8 Hz", "12 Hz", "16 Hz" });
        configure (play, "Motion playback", { "Loop", "Once" });
        smooth.onChange = [this] { remember(); draft.smooth = smooth.getSelectedId() == 1; publish(); };
        duration.onChange = [this] { remember(); draft.length = duration.getSelectedItemIndex(); publish(); };
        rate.onChange = [this] { remember(); draft.rateHz = kRateHz[(size_t) juce::jlimit (0, kNumRates - 1, rate.getSelectedItemIndex())]; publish(); };
        play.onChange = [this] { remember(); draft.playback = play.getSelectedId(); draft.direction = draft.playback == 2 ? 5 : 0; publish(); };
        button (done, "Done", [this] { if (onDone) onDone(); });
        button (undo, "Undo", [this] { undoEdit(); });
        button (fresh, "New", [this] { remember(); draft = trench::UserMotion {}; sync(); publish (true); });
        button (restart, "Restart", [this] { processor.restartMovement(); });
        button (save, "Save As", [this] { saveAs (name.getText()); });
        name.setTitle ("Movement name");
        name.setInputRestrictions (32);
        name.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xffd8e2e9));
        name.setColour (juce::TextEditor::textColourId, theme.labelInk());
        name.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff777d80));
        name.setFont (displayFont (13.0f));
        name.onReturnKey = [this] { saveAs (name.getText()); };
        addAndMakeVisible (name);
        sync();
        message = "Drag a point. Hear it immediately.";
    }
    ~MotionEditor() override { setLookAndFeel (nullptr); }
    std::function<void()> onDone, onSaved;
    void undoEdit()
    {
        if (history.empty()) return;
        draft = history.back(); history.pop_back();
        sync(); publish();
    }
    bool saveAs (const juce::String& title)
    {
        auto saved = draft;
        const auto result = trench::MotionLibrary::save (saved, title, root);
        if (result.failed()) { message = result.getErrorMessage(); repaint(); return false; }
        draft = saved;
        processor.applyUserMotion (draft);
        name.setText (draft.name, false);
        message = "Saved to MOVE.";
        if (onSaved) onSaved();
        repaint();
        return true;
    }
    void resized() override
    {
        done.setBounds (getWidth() - 55, 10, 45, 22);
        fresh.setBounds (12, 42, 42, 22);
        undo.setBounds (62, 42, 45, 22);
        restart.setBounds (getWidth() - 74, 42, 62, 22);
        canvas.setBounds (12, 76, getWidth() - 24, 160);
        smooth.setBounds (12, 258, 64, 22);
        duration.setBounds (78, 258, 64, 22);
        rate.setBounds (144, 258, 50, 22);
        play.setBounds (198, 258, 52, 22);
        name.setBounds (12, 321, getWidth() - 94, 25);
        save.setBounds (getWidth() - 75, 321, 63, 25);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xffc2bcad));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
        g.setColour (juce::Colour (0xff5e5c53));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
        g.setColour (theme.labelInk());
        g.setFont (displayFont (13.0f, true));
        g.drawText ("EDIT MOVE", 12, 10, 130, 22, juce::Justification::centredLeft);
        g.setFont (displayFont (10.0f));
        g.drawText ("SHAPE", 12, 241, 64, 15, juce::Justification::centredLeft);
        g.drawText ("TIME", 78, 241, 64, 15, juce::Justification::centredLeft);
        g.drawText ("RATE", 144, 241, 50, 15, juce::Justification::centredLeft);
        g.drawText ("PLAY", 198, 241, 52, 15, juce::Justification::centredLeft);
        g.drawText ("NAME", 12, 302, 80, 15, juce::Justification::centredLeft);
        g.setFont (displayFont (11.0f));
        g.drawFittedText (message, 12, 358, getWidth() - 24, 38, juce::Justification::topLeft, 2);
        g.setFont (displayFont (10.0f));
        g.drawText ("Points set travel above the MORPH wheel.", 12, getHeight() - 26,
                    getWidth() - 24, 18, juce::Justification::centredLeft);
    }
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { if (onDone) onDone(); return true; }
        if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z') { undoEdit(); return true; }
        return false;
    }
private:
    void configure (juce::ComboBox& c, const char* title, const juce::StringArray& entries)
    {
        c.setTitle (title); c.addItemList (entries, 1);
        c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffd8e2e9));
        c.setColour (juce::ComboBox::textColourId, theme.labelInk());
        addAndMakeVisible (c);
    }
    void button (juce::TextButton& b, const char* text, std::function<void()> action)
    {
        b.setButtonText (text); b.setTitle (text); b.onClick = std::move (action);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffd8e2e9));
        b.setColour (juce::TextButton::textColourOffId, theme.labelInk());
        addAndMakeVisible (b);
    }
    void remember()
    {
        if (history.size() == 64) history.erase (history.begin());
        history.push_back (draft);
        undo.setEnabled (true);
    }
    void sync()
    {
        smooth.setSelectedId (draft.smooth ? 1 : 2, juce::dontSendNotification);
        duration.setSelectedItemIndex (draft.length, juce::dontSendNotification);
        int rateIndex = 0;
        for (int i = 0; i < kNumRates; ++i)
            if (std::abs (kRateHz[(size_t) i] - draft.rateHz) < 1.0e-6)
            {
                rateIndex = i;
                break;
            }
        rate.setSelectedItemIndex (rateIndex, juce::dontSendNotification);
        play.setSelectedId (draft.playback == 2 || (draft.playback == 0 && draft.direction == 5) ? 2 : 1, juce::dontSendNotification);
        name.setText (draft.name, false);
        undo.setEnabled (! history.empty());
        canvas.repaint();
    }
    void publish (bool reset = false)
    {
        draft.dirty = true;
        processor.applyUserMotion (draft, reset);
        message = "Live changes. Save As to keep a named copy.";
        canvas.repaint(); repaint();
    }
    PluginProcessor& processor;
    EditorLook look;
    Theme theme;
    juce::File root;
    trench::UserMotion draft;
    std::vector<trench::UserMotion> history;
    MotionCanvas canvas;
    juce::ComboBox smooth, duration, rate, play;
    juce::TextButton done, undo, fresh, restart, save;
    juce::TextEditor name;
    juce::String message;
    static constexpr int kNumRates = 10;
    static constexpr double kRateHz[kNumRates] = { 0.0, 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0 };
};
}

