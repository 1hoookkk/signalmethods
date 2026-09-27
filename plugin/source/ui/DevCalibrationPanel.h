#pragma once
#include "WheelLoopPanel.h"
#include "../PluginProcessor.h"
#include "../parameters/DevCalibration.h"
#include "../parameters/CurveMap.h"

namespace trench::ui
{
inline constexpr int kDevPanelWidth = 540;
inline constexpr int kDevPanelHeight = 760;
class DevPanel final : public juce::Component, private juce::Timer
{
public:
    DevPanel (const Theme& theme, PluginProcessor& p, juce::File loopDir)
        : processor (p), loopPanel (theme, p.wheelLoop(), std::move (loopDir))
    {
        for (size_t i = 0; i < calibration::count; ++i)
        {
            const auto& d = calibration::variables[i];
            auto row = std::make_unique<Row>();
            row->label.setText (d.label, juce::dontSendNotification);
            row->label.setColour (juce::Label::textColourId, juce::Colour (0xffd4ded7));
            row->slider.setSliderStyle (juce::Slider::LinearHorizontal);
            row->slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 94, 24);
            row->slider.setTextValueSuffix (juce::String (" ") + d.unit);
            row->slider.setTooltip (d.help);
            row->label.setTooltip (d.help);
            row->slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffd4ded7));
            row->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff111815));
            const bool shown = std::any_of (std::begin (calibration::taste), std::end (calibration::taste), [&d] (const char* id) { return juce::String (id) == d.id; });
            form.addChildComponent (row->label);
            row->label.setVisible (shown);
            if (i != 0 && d.low == 0 && d.high == 1 && d.step == 1)
            {
                row->toggle.setTooltip (d.help);
                row->toggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffd4ded7));
                auto* toggle = &row->toggle;
                const bool driver = juce::String (d.id).startsWith ("cal_follow_");
                toggle->onStateChange = [toggle, driver] { toggle->setButtonText (toggle->getToggleState() ? (driver ? "FOLLOW" : "ON") : (driver ? "MANUAL" : "OFF")); };
                form.addChildComponent (*toggle);
                toggle->setVisible (shown);
                row->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, d.id, *toggle);
                toggle->onStateChange();
            }
            else
            {
                form.addChildComponent (row->slider);
                row->slider.setVisible (shown);
                row->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, d.id, row->slider);
                if (d.logarithmic && d.low > 0)
                    row->slider.setSkewFactorFromMidPoint (std::sqrt ((double) d.low * d.high));
            }
            rows.push_back (std::move (row));
        }
        viewport.setViewedComponent (&form, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
        loopViewport.setViewedComponent (&loopPanel, false);
        loopViewport.setScrollBarsShown (true, false);
        addChildComponent (loopViewport);
        page.addItemList ({ "DSP CALIBRATION", "REPEATABLE MORPH LOOP" }, 1);
        page.setSelectedId (1);
        page.onChange = [this] { resized(); };
        addAndMakeVisible (page);
        for (auto* c : { &status, &meters })
        {
            c->setColour (juce::Label::textColourId, juce::Colour (0xffc4d2c8));
            c->setFont (juce::Font (juce::FontOptions (12.0f)));
            addAndMakeVisible (*c);
        }
        notes.setTextToShowWhenEmpty ("Judgment dimension / source / listening notes", juce::Colours::grey);
        addAndMakeVisible (notes);
        setup (captureA, "STORE A", [this] { capture ("A"); });
        setup (captureB, "STORE B", [this] { capture ("B"); });
        setup (recallA, "HEAR A", [this] { recall ("A"); });
        setup (recallB, "HEAR B", [this] { recall ("B"); });
        setup (exportButton, "EXPORT SESSION", [this] { exportSession(); });
        setup (resetButton, "RESET TO NAKED", [this]
        {
            naked(); log ("reset_dev_defaults");
            status.setText ("NAKED: processing and movement off. Body and Morph/Q retained.", juce::dontSendNotification);
        });
        setup (hardwareButton, "RESET TO DEFAULTS", [this]
        {
            for (const auto& d : calibration::variables) setParameter (d.id, d.initial);
            log ("reset_defaults");
            status.setText ("DEFAULTS: shipping path. DRIVE pushes the filter; SLAM saturates its output. No AGC.", juce::dontSendNotification);
        });
        {
            const juce::ScopedLock lock (processor.calibrationArchiveLock);
            notes.setText (processor.calibrationArchive.getProperty ("notes").toString(), false);
        }
        notes.onTextChange = [this]
        {
            const juce::ScopedLock lock (processor.calibrationArchiveLock);
            processor.calibrationArchive.setProperty ("notes", notes.getText(), nullptr);
        };
        startTimerHz (10);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (24);
        page.setBounds (r.removeFromTop (26)); r.removeFromTop (6);
        meters.setBounds (r.removeFromTop (72));
        viewport.setVisible (page.getSelectedId() == 1);
        loopViewport.setVisible (page.getSelectedId() != 1);
        viewport.setBounds (r.removeFromTop (412));
        loopViewport.setBounds (viewport.getBounds());
        const int fw = viewport.getWidth() - 16;
        loopPanel.setSize (fw, 440);
        int y = 0;
        for (auto& row : rows)
        {
            if (! row->label.isVisible()) continue;
            row->label.setBounds (0, y, 208, 28);
            row->slider.setBounds (208, y, fw - 208, 28);
            row->toggle.setBounds (208, y, fw - 208, 28);
            y += 30;
        }
        form.setSize (fw, y);
        r.removeFromTop (6);
        buttons (r.removeFromTop (27), { &captureA, &recallA, &captureB, &recallB });
        r.removeFromTop (6);
        notes.setBounds (r.removeFromTop (26)); r.removeFromTop (6);
        buttons (r.removeFromTop (27), { &exportButton, &resetButton, &hardwareButton });
        status.setBounds (r);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff181e1b));
        g.setColour (juce::Colour (0xffdfebe2));
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText ("TRENCH / CALIBRATION", 14, 8, getWidth()-28, 24, juce::Justification::centredLeft);
    }
private:
    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        juce::ToggleButton toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };
    void setup (juce::TextButton& b, const char* text, std::function<void()> fn)
    {
        b.setButtonText (text); b.onClick = std::move (fn); addAndMakeVisible (b);
    }
    static void buttons (juce::Rectangle<int> r, std::initializer_list<juce::TextButton*> list)
    {
        const int w = r.getWidth() / (int) list.size();
        for (auto* b : list) b->setBounds (r.removeFromLeft (w).reduced (2, 0));
    }
    void setParameter (const char* id, float v)
    {
        if (auto* p = processor.apvts.getParameter (id))
        {
            p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture();
        }
    }
    void naked()
    {
        for (const auto& d : calibration::variables) setParameter (d.id, d.initial);
        for (const auto* id : calibration::processing) setParameter (id, 0);
        setParameter (ParamID::preamp, 0);
        setParameter (ParamID::slamDrive, 0);
        setParameter (ParamID::movePreset, 0);
        setParameter (ParamID::keySnap, 0);
        processor.wheelLoop().stop();
        processor.clearCalibrationNoteLatch();
    }
    juce::String bodyBytes()
    {
        return processor.bodyBytesForCalibration().toBase64Encoding();
    }
    juce::ValueTree context()
    {
        juce::ValueTree v ("Context");
        v.setProperty ("sampleRate", processor.getSampleRate(), nullptr);
        v.setProperty ("blockSize", processor.getBlockSize(), nullptr);
        v.setProperty ("declaredDatum", processor.dspBridge.declaredDatumForCalibration(), nullptr);
        v.setProperty ("bodyIndex", processor.getLoadedBodyIndex(), nullptr);
        v.setProperty ("bodyName", trench::bodyDisplayName (processor.getLoadedBodyIndex()), nullptr);
        v.setProperty ("bodyBytesBase64", bodyBytes(), nullptr);
        v.setProperty ("build", juce::String (__DATE__) + " " + __TIME__, nullptr);
        v.setProperty ("notes", notes.getText(), nullptr);
        v.addChild (processor.apvts.copyState(), -1, nullptr);
        juce::ValueTree motion ("WheelLoop");
        motion.setProperty ("beats", processor.wheelLoop().beatsRecorded(), nullptr);
        juce::String ticks;
        for (float x : processor.wheelLoop().snapshot()) ticks += juce::String (x, 9) + " ";
        motion.setProperty ("ticks", ticks, nullptr);
        v.addChild (motion, -1, nullptr);
        return v;
    }
    void capture (const char* name)
    {
        if (processor.bodyBytesForCalibration().getSize() == 0)
        { status.setText ("Load a body before storing a comparison.", juce::dontSendNotification); return; }
        auto c = context(); c.setProperty ("slot", name, nullptr);
        const juce::ScopedLock lock (processor.calibrationArchiveLock);
        auto old = processor.calibrationArchive.getChildWithProperty ("slot", name);
        if (old.isValid()) processor.calibrationArchive.removeChild (old, nullptr);
        processor.calibrationArchive.addChild (c, -1, nullptr);
        status.setText (juce::String ("Stored ") + name + ". Full parameters and source bytes retained.", juce::dontSendNotification);
    }
    void recall (const char* name)
    {
        juce::ValueTree c;
        {
            const juce::ScopedLock lock (processor.calibrationArchiveLock);
            c = processor.calibrationArchive.getChildWithProperty ("slot", name).createCopy();
        }
        if (! c.isValid()) { status.setText ("Store this slot first.", juce::dontSendNotification); return; }
        if (c.getProperty ("bodyBytesBase64").toString() != bodyBytes()
            || (double) c.getProperty ("sampleRate") != processor.getSampleRate()
            || (int) c.getProperty ("blockSize") != processor.getBlockSize()
            || (double) c.getProperty ("declaredDatum") != processor.dspBridge.declaredDatumForCalibration())
        { status.setText ("Source body or rate changed. Restore that context before comparing.", juce::dontSendNotification); return; }
        const auto currentMotion = context().getChildWithName ("WheelLoop");
        const auto savedMotion = c.getChildWithName ("WheelLoop");
        if (currentMotion.getProperty ("ticks") != savedMotion.getProperty ("ticks")
            || currentMotion.getProperty ("beats") != savedMotion.getProperty ("beats"))
        { status.setText ("Morph loop changed. Restore the recorded loop before comparing.", juce::dontSendNotification); return; }
        const auto state = c.getChildWithName (processor.apvts.state.getType());
        for (auto* p : processor.getParameters())
            if (auto* id = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            {
                const auto node = state.getChildWithProperty ("id", id->paramID);
                if (node.isValid() && id->paramID != ParamID::body)
                    setParameter (id->paramID.toRawUTF8(), (float) node.getProperty ("value"));
            }
        log (juce::String ("recall_") + name);
        status.setText (juce::String ("Hearing ") + name + ". Filters retain state; allow settling and repeat the same loop.", juce::dontSendNotification);
    }
    void log (const juce::String& action)
    {
        auto trial = context();
        trial.setProperty ("action", action, nullptr);
        trial.setProperty ("time", juce::Time::getCurrentTime().toISO8601 (true), nullptr);
        trial.setProperty ("inputRms", processor.calibrationInputRms.load(), nullptr);
        trial.setProperty ("outputRms", processor.calibrationOutputRms.load(), nullptr);
        trial.setProperty ("safetyFraction", processor.calibrationCeilingFraction.load(), nullptr);
        const juce::ScopedLock lock (processor.calibrationArchiveLock);
        processor.calibrationArchive.addChild (trial, -1, nullptr);
        status.setText (action + " recorded. Measurements are live block values, not loudness judgments.", juce::dontSendNotification);
    }
    void exportSession()
    {
        log ("export");
        const auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("TRENCH Calibration");
        if (! dir.createDirectory()) { status.setText ("Cannot create session directory.", juce::dontSendNotification); return; }
        const auto file = dir.getNonexistentChildFile ("session-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S"), ".xml");
        const juce::ScopedLock lock (processor.calibrationArchiveLock);
        auto xml = processor.calibrationArchive.createXml();
        status.setText (xml != nullptr && xml->writeTo (file) ? file.getFullPathName() : "Session export failed.", juce::dontSendNotification);
    }
    void timerCallback() override
    {
        const auto read = [this] (size_t i) { return processor.apvts.getRawParameterValue (calibration::variables[i].id)->load(); };
        const bool desk = false;
        for (size_t i = 0; i < rows.size(); ++i)
        {
            juce::String inactive;
            if (i == 0 && processor.getSampleRate() > 0
                && std::abs (processor.getSampleRate() - processor.dspBridge.declaredDatumForCalibration()) < 0.1)
                inactive = "WORDS and GRID use the same path at the body's declared rate.";
            if (i == 6 && read (5) < 0.5f) inactive = "Enable Feedback clipping.";
            if (i >= 8 && i <= 11 && read (7) < 0.5f) inactive = "Enable Section ring limiter.";
            if (i >= 13 && i <= 15 && ! desk) inactive = "The separate output drive stage is retired.";
            if ((i == 17 || i == 18) && read (19) < 0.5f) inactive = "Enable Final output guard.";
            rows[i]->slider.setEnabled (inactive.isEmpty());
            rows[i]->toggle.setEnabled (inactive.isEmpty());
            rows[i]->label.setAlpha (inactive.isEmpty() ? 1.0f : 0.4f);
            const auto tip = inactive.isEmpty() ? juce::String (calibration::variables[i].help)
                : "INACTIVE: " + inactive + " " + calibration::variables[i].help;
            rows[i]->slider.setTooltip (tip);
            rows[i]->toggle.setTooltip (tip);
            rows[i]->label.setTooltip (tip);
        }
        const auto blocks = processor.calibrationBlocks.load (std::memory_order_acquire);
        bool received = blocks != 0;
        for (size_t i = 0; i < calibration::count; ++i)
            received = received && std::abs (read (i) - processor.calibrationReceived[i].load()) < 0.001f;
        const auto now = juce::Time::getMillisecondCounter();
        if (blocks != lastBlocks) lastAudioTime = now;
        const bool live = blocks != 0 && now - lastAudioTime < 1000;
        lastBlocks = blocks;
        juce::String audioStatus = ! processor.getLastLoadOk() ? "DSP bypassed: body load failed"
            : ! live ? "Waiting for audio processing"
            : received ? "DSP running / settings received" : "DSP running / settings pending";
        if (live && processor.getLastLoadOk() && processor.calibrationInputRms.load() < 0.000001f)
            audioStatus += " / input silent";
        const auto db = [] (float x) { return juce::String (juce::Decibels::gainToDecibels (x, -120.0f), 1); };
        const float outPeak = processor.dspBridge.postDeskPeakForCalibration();
        if (outPeak >= heldPeak || now - heldAt > 3000) { heldPeak = outPeak; heldAt = now; }
        meters.setText ("Host " + juce::String (processor.getSampleRate(), 0) + " Hz / datum "
            + juce::String (processor.dspBridge.declaredDatumForCalibration(), 0) + " Hz (declared)\n"
            + "RMS in/out " + db (processor.calibrationInputRms.load()) + " / " + db (processor.calibrationOutputRms.load())
            + " dBFS\n"
            + "Output peak " + db (outPeak) + "   held 3 s " + db (heldPeak) + " dBFS" + (heldPeak > 1.0f ? "   OVER 0 dBFS" : "") + "\n"
            + "Peak pre/post desk " + db (processor.dspBridge.preDeskPeakForCalibration()) + " / " + db (processor.dspBridge.postDeskPeakForCalibration())
            + (read (19) < 0.5f ? "   Guard OFF\n" : "   Guard " + juce::String (100 * processor.calibrationCeilingFraction.load(), 1) + "%\n")
            + audioStatus, juce::dontSendNotification);
    }
    PluginProcessor& processor;
    WheelLoopPanel loopPanel;
    juce::Component form;
    std::vector<std::unique_ptr<Row>> rows;
    juce::Viewport viewport, loopViewport;
    juce::ComboBox page;
    juce::Label status, meters;
    float heldPeak = 0.0f;
    juce::uint32 heldAt = 0;
    juce::TextEditor notes;
    juce::TextButton captureA, captureB, recallA, recallB, exportButton, resetButton, hardwareButton;
    std::uint64_t lastBlocks = 0;
    juce::uint32 lastAudioTime = 0;
    juce::TooltipWindow tooltips { this, 350 };
};
}
