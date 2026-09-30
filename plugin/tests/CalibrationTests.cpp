#include "PluginProcessor.h"
#include "MorphSmoothingTests.h"
#include "DriveSlamTests.h"
#include "PluginEditor.h"
#include <cstdio>
#include <stdexcept>
#include "CalibrationAudition.h"
#include "dsp/Movement.h"

namespace
{
void require (bool ok, const char* message)
{
    if (! ok) throw std::runtime_error (message);
}
void set (PluginProcessor& p, const char* id, float value)
{
    auto* v = p.apvts.getParameter (id);
    require (v != nullptr, "missing calibration parameter");
    v->setValueNotifyingHost (v->convertTo0to1 (value));
}
juce::MemoryBlock body()
{
    juce::MemoryBlock out;
    require (juce::File (TRENCH_TABLE_STITCH_ROOT).getChildFile ("plugin/presets/p2k/talking_hedz.body240").loadFileAsData (out), "missing body fixture");
    return out;
}
double difference (const std::vector<float>& a, const std::vector<float>& b)
{
    double sum = 0;
    for (size_t i = 0; i < a.size(); ++i) sum += std::abs ((double) a[i] - b[i]);
    return sum / (double) a.size();
}
std::vector<float> render (trench::calibration::Values values, float drive = 0.7f, bool timing = false)
{
    TrenchDspBridge bridge;
    bridge.prepare (48000, 128);
    const auto bytes = body();
    require (bridge.loadCartridgeBytes (bytes), "body load");
    bridge.applyCalibration (values);
    bridge.setOutputLevel (juce::Decibels::decibelsToGain (drive));
    TrenchParams p; p.q = 0.7f; p.poleDistortion = values[5];
    std::vector<float> result;
    float trajectory[128];
    juce::AudioBuffer<float> audio (2, 128);
    double elapsed = 0;
    for (int block = 0; block < 180; ++block)
    {
        for (int i = 0; i < 128; ++i)
        {
            const int n = block * 128 + i;
            trajectory[i] = 0.1f + 0.8f * (float) n / (180.0f * 128.0f);
            const float x = block < 120 ? 5.0f * (float) (std::sin (n * 0.029) + 0.3 * std::sin (n * 0.113)) : 0;
            audio.setSample (0, i, x); audio.setSample (1, i, x * 0.7f);
        }
        const double start = juce::Time::getMillisecondCounterHiRes();
        bridge.processTrajectory (audio, trajectory, p);
        elapsed += juce::Time::getMillisecondCounterHiRes() - start;
        for (int i = 0; i < 128; ++i)
        {
            const float x = audio.getSample (0, i);
            require (std::isfinite (x), "nonfinite calibration audio");
            result.push_back (x);
        }
    }
    if (timing) std::printf ("BENCH 48k stereo 480ms audio: mode %.0f hop %.0f consumed %.3fms (%.2f%% of audio duration)\n", values[0], values[1], elapsed, elapsed / 4.8);
    return result;
}
void exactInterpolation()
{
    const auto bytes = body();
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        TrenchDspBridge bridge;
        bridge.prepare (rate, 128);
        require (bridge.loadCartridgeBytes (bytes), "load packed reference");
        auto v = trench::calibration::defaults(); v[0] = 0; v[2] = 0; v[5] = 0;
        bridge.applyCalibration (v);
        juce::AudioBuffer<float> audio (2, 128);
        for (float morph : { 0.0f, 0.137f, 0.503f, 0.929f, 1.0f })
        {
            audio.clear();
            TrenchParams p; p.morph = morph; p.q = 0.371f;
            for (int settle = 0; settle < 60; ++settle) bridge.process (audio, p);
            float actual[trench::kUiCoeffCount], expected[trench::kUiCoeffCount], boost;
            bridge.readUiSnapshot (actual, boost);
            require (TrenchDspBridge::probePackedBody (bytes.getData(), bytes.getSize(), morph, p.q, rate, expected, boost), "packed probe");
            for (int i = 0; i < trench::kUiCoeffCount; ++i)
                require (std::abs (actual[i] - expected[i]) < 1.0e-6f, "exact mode differs from packed-word reference");
        }
    }
}
void stateRoundtrip()
{
    PluginProcessor p;
    p.setRateAndBufferSizeDetails (48000, 128);
    p.prepareToPlay (48000, 128);
    for (const auto& d : trench::calibration::variables) set (p, d.id, d.high);
    juce::ValueTree trial ("Context"); trial.setProperty ("action", "keep_lower_half", nullptr);
    p.calibrationArchive.addChild (trial, -1, nullptr);
    for (const char* id : trench::calibration::retired)
    {
        juce::ValueTree stale ("PARAM");
        stale.setProperty ("id", id, nullptr);
        stale.setProperty ("value", 2.0f, nullptr);
        p.apvts.state.addChild (stale, -1, nullptr);
    }
    juce::MemoryBlock state; p.getStateInformation (state);
    PluginProcessor q;
    q.setRateAndBufferSizeDetails (48000, 128);
    q.prepareToPlay (48000, 128);
    q.setStateInformation (state.getData(), (int) state.getSize());
    for (const auto& d : trench::calibration::variables)
        require (std::abs (q.apvts.getRawParameterValue (d.id)->load() - d.high) < 0.001f, "lost calibration parameter");
    require (q.calibrationArchive.getNumChildren() == 1, "lost trial archive");
    require (! q.apvts.state.getChildWithName ("DevCalibrationSession").isValid(), "recursive archive in APVTS");
    for (const char* id : trench::calibration::retired)
        require (! q.apvts.state.getChildWithProperty ("id", id).isValid(), "retired AGC value survived restore");
}
std::vector<float> processorRender (const char* id, float value)
{
    PluginProcessor p;
    p.setRateAndBufferSizeDetails (48000, 128);
    p.prepareToPlay (48000, 128);
    const auto bytes = body();
    require (p.installBodyBytes (bytes.getData(), bytes.getSize()), "processor body fixture");
    set (p, "cal_guard", 1); set (p, "cal_feedback", 1);
    set (p, "q", 0.7f); set (p, id, value);
    std::vector<float> out;
    juce::AudioBuffer<float> audio (2, 128); juce::MidiBuffer midi;
    for (int block = 0; block < 60; ++block)
    {
        set (p, "morph", block < 30 ? 0.1f : 0.9f);
        for (int i = 0; i < 128; ++i)
        {
            const float x = (float) std::sin ((block * 128 + i) * 0.057) * 3;
            audio.setSample (0, i, x); audio.setSample (1, i, x);
        }
        p.processBlock (audio, midi);
        for (int i = 0; i < 128; ++i)
        {
            const float x = audio.getSample (0, i);
            require (std::isfinite (x) && std::abs (x) <= trench::kFinalSafetyCeiling, "processor safety failed");
            out.push_back (x);
        }
    }
    return out;
}
juce::TextButton* button (juce::Component& c, const juce::String& text)
{
    if (auto* b = dynamic_cast<juce::TextButton*> (&c); b != nullptr && b->getButtonText() == text) return b;
    for (auto* child : c.getChildren()) if (auto* found = button (*child, text)) return found;
    return nullptr;
}
juce::Slider* calibrationSlider (juce::Component& c, const juce::String& tooltip)
{
    if (auto* s = dynamic_cast<juce::Slider*> (&c); s != nullptr && s->getTooltip().endsWith (tooltip)) return s;
    for (auto* child : c.getChildren()) if (auto* found = calibrationSlider (*child, tooltip)) return found;
    return nullptr;
}
juce::ToggleButton* calibrationToggle (juce::Component& c, const juce::String& tooltip)
{
    if (auto* b = dynamic_cast<juce::ToggleButton*> (&c); b != nullptr && b->getTooltip().endsWith (tooltip)) return b;
    for (auto* child : c.getChildren()) if (auto* found = calibrationToggle (*child, tooltip)) return found;
    return nullptr;
}
void liveSliderAudio()
{
    PluginProcessor p;
    p.setRateAndBufferSizeDetails (48000, 128);
    p.prepareToPlay (48000, 128);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    const auto blockRms = [&]
    {
        juce::AudioBuffer<float> audio (2, 128); juce::MidiBuffer midi;
        for (int block = 0; block < 8; ++block)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i) audio.setSample (c, i, 0.1f);
            p.processBlock (audio, midi);
        }
        return audio.getRMSLevel (0, 0, 128);
    };
    auto* slider = calibrationSlider (*editor, trench::calibration::variables[13].help);
    require (slider != nullptr, "missing monitor slider");
    auto* ringAttack = calibrationSlider (*editor, trench::calibration::variables[9].help);
    require (ringAttack != nullptr && ! ringAttack->isEnabled(), "inactive ring control appears active");
    auto* ringSwitch = calibrationToggle (*editor, trench::calibration::variables[7].help);
    require (ringSwitch != nullptr, "missing ring switch");
    ringSwitch->setToggleState (true, juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    require (ringAttack->isEnabled(), "ring control did not activate");
    ringSwitch->setToggleState (false, juce::sendNotificationSync);
    const float before = blockRms();
    slider->setValue (-24, juce::sendNotificationSync);
    require (std::abs (p.apvts.getRawParameterValue ("cal_output_db")->load() + 24) < 0.001f, "slider did not update APVTS");
    const float after = blockRms();
    require (p.calibrationBlocks.load() >= 16, "audio block acknowledgement missing");
    require (std::abs (p.calibrationReceived[13].load() + 24) < 0.001f, "audio thread did not acknowledge slider");
    const float measuredDb = juce::Decibels::gainToDecibels (after / before);
    std::printf ("LIVE monitor slider: %.3f dB audio change\n", measuredDb);
    require (std::abs (measuredDb + 24) < 0.02f, "live slider did not reach audio");
    slider->setValue (0, juce::sendNotificationSync);
    require (std::abs (blockRms() - before) < 0.00001f, "live slider restore failed");
    for (const auto& d : trench::calibration::variables)
    {
        if (auto* b = calibrationToggle (*editor, d.help))
        {
            b->setToggleState (true, juce::sendNotificationSync);
            require (p.apvts.getRawParameterValue (d.id)->load() == 1, "switch did not enable its audio parameter");
            b->setToggleState (false, juce::sendNotificationSync);
            require (p.apvts.getRawParameterValue (d.id)->load() == 0, "switch did not disable its audio parameter");
            continue;
        }
        auto* s = calibrationSlider (*editor, d.help);
        require (s != nullptr, "missing slider");
        s->setValue (d.high, juce::sendNotificationSync);
        require (std::abs (p.apvts.getRawParameterValue (d.id)->load() - d.high) < 0.001f, "slider binding failed");
        s->setValue (d.initial, juce::sendNotificationSync);
    }
}
void defaultsUnity()
{
    PluginProcessor p;
    p.setRateAndBufferSizeDetails (48000, 128);
    p.prepareToPlay (48000, 128);
    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    for (float level : { 0.1f, 0.5f, 0.79f, 0.9f, 1.0f, 1.25f })
    {
        float worst = 0;
        for (int block = 0; block < 8; ++block)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i) audio.setSample (c, i, i % 2 ? -level : level);
            p.processBlock (audio, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i)
                worst = std::max (worst, std::abs (std::abs (audio.getSample (c, i)) - level));
        }
        require (worst == 0.0f, "default No filter changes the signal; the dev defaults must match release");
    }
    std::puts ("DEFAULTS: No filter passes every tested level unchanged, as in release");
}
void nakedDefaults()
{
    PluginProcessor p;
    p.setRateAndBufferSizeDetails (48000, 128);
    p.prepareToPlay (48000, 128);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    auto* reset = button (*editor, "RESET TO NAKED");
    require (reset != nullptr, "missing naked reset");
    reset->onClick();
    for (const auto* id : { "cal_ramp", "cal_morph_ms", "cal_feedback", "cal_ring",
                           "cal_desk", "cal_guard", "preamp", "movePreset" })
        require (p.apvts.getRawParameterValue (id)->load() == 0, "naked reset left processing active");
    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    const auto fill = [&] { for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i) audio.setSample (c, i, i % 2 ? -1.25f : 1.25f); };
    fill(); p.processBlock (audio, midi);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i)
        require (std::abs (audio.getSample (c, i) - (i % 2 ? -1.25f : 1.25f)) < 1.0e-6f, "naked reset leaves the finite filter output unbounded");
    set (p, "cal_guard", 1);
    fill(); p.processBlock (audio, midi);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i)
        require (std::abs (audio.getSample (c, i) - trench::softGuard (i % 2 ? -1.25f : 1.25f)) < 1.0e-6f, "output guard contains No filter");
    const auto bytes = body();
    require (p.installBodyBytes (bytes.getData(), bytes.getSize()), "naked reset body fixture");
    const int selectedBody = p.getLoadedBodyIndex();
    set (p, "morph", 0.731f); set (p, "q", 0.713f);
    for (const auto& d : trench::calibration::variables) set (p, d.id, d.high);
    set (p, "preamp", 0.7f); set (p, "movePreset", 1);
    p.wheelLoop().blank (1); p.wheelLoop().play();
    reset->onClick();
    for (const auto& d : trench::calibration::variables)
    {
        const bool processing = std::any_of (std::begin (trench::calibration::processing), std::end (trench::calibration::processing),
            [&d] (const char* id) { return juce::String (id) == d.id; });
        require (std::abs (p.apvts.getRawParameterValue (d.id)->load() - (processing ? 0.0f : d.initial)) < 0.001f, "naked reset left processing active");
    }
    for (const auto* id : { "preamp", "movePreset", "keySnap" })
        require (p.apvts.getRawParameterValue (id)->load() == 0, "naked reset left a performance effect active");
    require (p.wheelLoop().currentMode() == trench::WheelLoop::Mode::Idle, "naked reset left loop playing");
    require (p.getLoadedBodyIndex() == selectedBody, "naked reset changed body");
    require (std::abs (p.apvts.getRawParameterValue ("morph")->load() - 0.731f) < 0.00001f
          && std::abs (p.apvts.getRawParameterValue ("q")->load() - 0.713f) < 0.00001f, "naked reset changed authored position");
    std::puts ("NAKED: fixed stage saturation remains on; reset disables optional processing and movement, retains body/Morph/Q");
}
void devMorphDisplay()
{
    struct PlayHead : juce::AudioPlayHead
    {
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (120.0);
            p.setPpqPosition (0.0);
            p.setIsPlaying (true);
            return p;
        }
    } playHead;
    PluginProcessor p;
    p.setPlayHead (&playHead);
    p.setRateAndBufferSizeDetails (48000, 128);
    p.prepareToPlay (48000, 128);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    set (p, "morph", 0.5f);
    set (p, "movePreset", 0);
    p.wheelLoop().blank (1);
    p.wheelLoop().setSteps ({ 0.75f, 0.75f, 0.75f, 0.75f }, 1, true);
    p.wheelLoop().play();
    juce::AudioBuffer<float> audio (2, 128);
    juce::MidiBuffer midi;
    audio.clear();
    p.processBlock (audio, midi);
    require (p.isMorphModulatedForUi(), "Dev loop does not drive the Morph display with Movement off");
    require (std::abs (p.getEffectiveMorphForUi() - 0.75f) < 0.00001f, "display does not receive loop trajectory");
    p.wheelLoop().stop();
    for (int i = 0; i < 8; ++i) p.processBlock (audio, midi);
    require (! p.isMorphModulatedForUi(), "display did not return to manual Morph");
    set (p, "cal_morph_ms", 100);
    set (p, "morph", 0.9f);
    p.processBlock (audio, midi);
    require (p.isMorphModulatedForUi(), "Morph lag is hidden from the display");
    require (p.getEffectiveMorphForUi() > 0.5f && p.getEffectiveMorphForUi() < 0.6f, "display skipped Morph lag");
    std::puts ("Dev display: loop with Movement off and Morph lag reach the displayed trajectory");
}
void screenshot (const juce::File& destination)
{
    PluginProcessor p;
    p.setRateAndBufferSizeDetails (48000, 128);
    p.prepareToPlay (48000, 128);
    const auto bytes = body();
    require (p.installBodyBytes (bytes.getData(), bytes.getSize()), "install UI fixture");
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    require (editor->getWidth() == 790 && editor->getHeight() == 760, "dev dimensions");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    for (int i = 1; i < trench::bodyCount(); ++i)
        if (trench::bodyDisplayName (i).equalsIgnoreCase ("Talking Hedz")) { set (p, "body", (float) i); break; }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    const auto click = [&] (const char* label)
    {
        auto* b = button (*editor, label); require (b != nullptr, "missing calibration button"); b->onClick();
    };
    set (p, "cal_feedback_db", -6); click ("STORE A");
    set (p, "cal_feedback_db", 12); click ("STORE B");
    click ("HEAR A");
    require (std::abs (p.apvts.getRawParameterValue ("cal_feedback_db")->load() + 6) < 0.001f, "A recall failed");
    click ("HEAR B");
    require (std::abs (p.apvts.getRawParameterValue ("cal_feedback_db")->load() - 12) < 0.001f, "B recall failed");
    click ("RESET TO NAKED");
    click ("RESET TO DEFAULTS");
    for (const auto& d : trench::calibration::variables)
        require (std::abs (p.apvts.getRawParameterValue (d.id)->load() - d.initial) < 0.001f, "defaults reset failed");
    juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
    juce::FileOutputStream stream (destination);
    require (stream.openedOk() && stream.setPosition (0) && stream.truncate().wasOk(), "screenshot open");
    require (stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream), "screenshot write");
}
}
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        if (argc == 3 && juce::String (argv[1]) == "--morph-stress")
        {
            const float smoothMs = juce::String (argv[2]).getFloatValue();
            int failed = 0;
            for (const double rate : { 44100.0, 48000.0, 96000.0 })
                for (const int baseFlip : { 31, 44, 63, 88, 100, 127, 151, 176, 223, 271, 352, 511, 701, 1024, 2033, 4410, 8192 })
                {
                    TrenchDspBridge bridge;
                    bridge.prepare (rate, 512);
                    auto values = trench::calibration::defaults();
                    values[3] = smoothMs;
                    bridge.applyCalibration (values);
                    const auto bytes = body();
                    require (bridge.loadCartridgeBytes (bytes), "morph stress body loads");
                    TrenchParams params; params.q = 1.0f;
                    std::array<float, 512> trajectory {};
                    juce::AudioBuffer<float> audio (2, 512);
                    juce::Random noise (7);
                    const int flip = (int) std::lround (baseFlip * rate / 44100.0);
                    const int blocks = (int) std::ceil (rate * 5.0 / 512.0);
                    float peak = 0.0f;
                    bool finite = true;
                    for (int block = 0; block < blocks && finite; ++block)
                    {
                        for (int i = 0; i < 512; ++i)
                        {
                            trajectory[(size_t) i] = ((block * 512 + i) / flip) % 2 == 0 ? 0.0f : 1.0f;
                            const float x = 0.5f * (noise.nextFloat() * 2.0f - 1.0f);
                            audio.setSample (0, i, x); audio.setSample (1, i, x);
                        }
                        bridge.processTrajectory (audio, trajectory.data(), params);
                        for (int i = 0; i < 512; ++i)
                        {
                            const float y = audio.getSample (0, i);
                            finite = finite && std::isfinite (y) && std::abs (y) < 1.0e6f;
                            peak = std::max (peak, std::abs (y));
                        }
                    }
                    std::printf ("%s smooth %.3f ms rate %.0f flip %d peak %.2f dB\n", finite ? "PASS" : "FAIL", smoothMs, rate, flip, juce::Decibels::gainToDecibels (peak));
                    if (! finite) ++failed;
                }
            return failed == 0 ? 0 : 1;
        }
        if (argc == 2 && juce::String (argv[1]) == "--morph-smoothing")
        {
            require (morphSmoothingTests() == 0, "subtle Morph/Q smoothing contract");
            devMorphDisplay();
            return 0;
        }
        if (argc == 3 && juce::String (argv[1]) == "--audition")
        {
            calibrationAudition (juce::File (juce::String::fromUTF8 (argv[2])));
            return 0;
        }
        if (argc == 7 && juce::String (argv[1]) == "--probefile")
        {
            juce::MemoryBlock bytes;
            require (juce::File (juce::String::fromUTF8 (argv[2])).loadFileAsData (bytes), "probe file");
            float coeffs[trench::kUiCoeffCount], boost;
            require (TrenchDspBridge::probePackedBody (bytes.getData(), bytes.getSize(), juce::String (argv[3]).getFloatValue(),
                juce::String (argv[4]).getFloatValue(), juce::String (argv[5]).getDoubleValue(), coeffs, boost,
                juce::String (argv[6]).getDoubleValue()), "probe");
            for (int s = 0; s < trench::kUiStageCount; ++s)
                std::printf ("%d %.8g %.8g %.8g %.8g %.8g\n", s, coeffs[s * 5], coeffs[s * 5 + 1], coeffs[s * 5 + 2], coeffs[s * 5 + 3], coeffs[s * 5 + 4]);
            return 0;
        }
        if (argc == 5 && juce::String (argv[1]) == "--probe")
        {
            const auto bytes = body();
            float coeffs[trench::kUiCoeffCount], boost;
            require (TrenchDspBridge::probePackedBody (bytes.getData(), bytes.getSize(), juce::String (argv[2]).getFloatValue(),
                juce::String (argv[3]).getFloatValue(), juce::String (argv[4]).getDoubleValue(), coeffs, boost), "probe");
            for (int s = 0; s < trench::kUiStageCount; ++s)
                std::printf ("%d %.8g %.8g %.8g %.8g %.8g\n", s, coeffs[s * 5], coeffs[s * 5 + 1], coeffs[s * 5 + 2], coeffs[s * 5 + 3], coeffs[s * 5 + 4]);
            return 0;
        }
        if (argc == 3 && juce::String (argv[1]) == "--sweep-emu")
        {
            sweepAudition (juce::File (juce::String::fromUTF8 (argv[2])), true);
            return 0;
        }
        if (argc == 3 && juce::String (argv[1]) == "--sweep")
        {
            sweepAudition (juce::File (juce::String::fromUTF8 (argv[2])));
            return 0;
        }
        exactInterpolation();
        require (morphSmoothingTests() == 0, "subtle Morph/Q smoothing contract");
        require (driveSlamTests() == 0, "DRIVE / soft clip contract");
        for (float x = -8; x < 8; x += 0.013f)
        {
            const float y = trench::calibration::guard (x, trench::kGuardLinearZone, trench::kFinalSafetyCeiling);
            require (std::abs (y - trench::softGuard (x)) < 1.0e-6f, "guard baseline differs");
            require (std::abs (y) <= trench::kFinalSafetyCeiling, "guard ceiling exceeded");
        }
        stateRoundtrip();
        liveSliderAudio();
        defaultsUnity();
        nakedDefaults();
        devMorphDisplay();
        const auto processorReference = processorRender ("cal_input_db", 0);
        for (auto [id, value] : { std::pair<const char*,float>{"cal_input_db",-12.0f}, {"cal_morph_ms",50.0f}, {"cal_output_db",-12.0f}, {"cal_guard_knee",0.2f}, {"cal_guard_ceiling",-6.0f} })
        {
            const double delta = difference (processorReference, processorRender (id, value));
            std::printf ("%s processor delta %.9g\n", id, delta);
            require (delta > 1.0e-7, "processor control has no effect");
        }
        {
            PluginProcessor follower;
            follower.setRateAndBufferSizeDetails (48000, 512);
            follower.prepareToPlay (48000, 512);
            follower.setEditorOpen (true);
            const auto fixture = body();
            require (follower.installBodyBytes (fixture.getData(), fixture.getSize()), "follow body loads");
            set (follower, ParamID::morph, 0.2f);
            set (follower, "cal_follow_1", 1.0f);
            juce::MidiBuffer none;
            juce::AudioBuffer<float> audio (2, 512);
            int clock = 0;
            const auto run = [&] (float amplitude, int blocks)
            {
                for (int k = 0; k < blocks; ++k, ++clock)
                {
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < 512; ++i)
                            audio.setSample (c, i, amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (clock * 512 + i) / 48000.0));
                    follower.processBlock (audio, none);
                }
                return follower.getEffectiveMorphForUi();
            };
            const float rest = run (0.0f, 20);
            const float loud = run (0.5f, 20);
            const float back = run (0.0f, 200);
            std::printf ("follow: wheel 1 at rest %.4f, under a -6 dBFS tone %.4f, after silence %.4f\n", rest, loud, back);
            require (loud > rest + 0.3f && std::abs (back - rest) < 0.01f, "FOLLOW moves wheel 1 with the level and returns it to where it is set");
            set (follower, "cal_follow_1", 0.0f);
            set (follower, "cal_follow_2", 1.0f);
            set (follower, ParamID::q, 0.1f);
            run (0.0f, 200);
            const float qRest = follower.getEffectiveQForUi();
            run (0.5f, 20);
            const float qLoud = follower.getEffectiveQForUi();
            run (0.0f, 200);
            const float qBack = follower.getEffectiveQForUi();
            std::printf ("follow: wheel 2 at rest %.4f, under the tone %.4f, after silence %.4f\n", qRest, qLoud, qBack);
            require (qLoud > qRest + 0.3f && std::abs (qBack - qRest) < 0.01f, "FOLLOW moves wheel 2 with the level and returns it to where it is set");
            set (follower, "cal_follow_1", 1.0f);
            set (follower, "cal_follow_depth", 0.5f);
            set (follower, "cal_follow_depth_2", -0.5f);
            set (follower, ParamID::morph, 0.3f);
            set (follower, ParamID::q, 0.8f);
            run (0.0f, 200);
            const float mRest = follower.getEffectiveMorphForUi(), qRest2 = follower.getEffectiveQForUi();
            run (0.5f, 20);
            const float mLoud = follower.getEffectiveMorphForUi(), qLoud2 = follower.getEffectiveQForUi();
            std::printf ("opposite: wheel 1 %.3f -> %.3f, wheel 2 %.3f -> %.3f\n", mRest, mLoud, qRest2, qLoud2);
            require (mLoud > mRest + 0.2f && qLoud2 < qRest2 - 0.2f, "opposite depths move the two wheels in opposite directions");
            set (follower, "cal_follow_2", 0.0f);
            set (follower, "cal_follow_1", 0.0f);
            int stepped = -1;
            for (int k = 0; k < trench::kNumFuncGenPatterns && stepped < 0; ++k)
            {
                const auto& pattern = trench::kFuncGenPatterns[k];
                if (pattern.steps >= 3 && pattern.direction == 0 && pattern.rateHz <= 0.0
                    && pattern.values[0] != pattern.values[1] && pattern.values[1] != pattern.values[2]) stepped = k;
            }
            require (stepped >= 0, "a stepped movement exists for the swing check");
            const auto changes = [&] (double amount)
            {
                trench::Movement motion;
                motion.prepare (48000.0);
                motion.setSwing (amount);
                trench::MovementTransport clockNow; clockNow.bpm = 120.0; clockNow.ppq = 0.0; clockNow.playing = true;
                std::vector<float> path (48000 * 8);
                motion.render (path.data(), (int) path.size(), clockNow, stepped + 1, trench::Movement::StepTransition,
                               trench::Movement::authoredLengthChoice (trench::kFuncGenPatterns[stepped]));
                std::vector<int> at;
                for (size_t i = 1; i < path.size() && at.size() < 2; ++i) if (path[i] != path[i - 1]) at.push_back ((int) i);
                return at;
            };
            const auto straight = changes (0.0), swung = changes (1.0);
            require (straight.size() == 2 && swung.size() == 2, "the stepped movement changes twice");
            std::printf ("swing: off step starts at sample %d straight, %d swung; next on-step %d straight, %d swung\n", straight[0], swung[0], straight[1], swung[1]);
            require (std::abs ((double) swung[0] / straight[0] - 1.5) < 0.01 && std::abs (swung[1] - straight[1]) <= 1,
                     "SWING pushes the off step late by half a step at 100 % and leaves the next on-step in place");
            const auto noted = [&] (bool tracking, int note)
            {
                PluginProcessor p;
                p.setRateAndBufferSizeDetails (48000, 512);
                p.prepareToPlay (48000, 512);
                require (p.installBodyBytes (fixture.getData(), fixture.getSize()), "note body loads");
                set (p, "cal_note_track", tracking ? 1.0f : 0.0f);
                set (p, ParamID::q, 0.6f);
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
                std::vector<float> out;
                for (int k = 0; k < 40; ++k)
                {
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) audio.setSample (c, i, 0.3f * (float) std::sin (0.031 * (k * 512 + i)) + 0.2f * (float) std::sin (0.17 * (k * 512 + i)));
                    p.processBlock (audio, k == 0 ? midi : none);
                    if (k >= 20) out.insert (out.end(), audio.getReadPointer (0), audio.getReadPointer (0) + 512);
                }
                return out;
            };
            const double tracked = difference (noted (true, 60), noted (true, 72));
            const double untracked = difference (noted (false, 60), noted (false, 72));
            std::printf ("note tracking: an octave up changes the output by %.6f when on, %.6f when off\n", tracked, untracked);
            require (tracked > 1.0e-3 && untracked == 0.0, "MIDI note tracking transposes the filter only when switched on");
        }
        {
            const auto worst = [] (int flipEvery, float still)
            {
                TrenchDspBridge bridge;
                bridge.prepare (44100, 512);
                const auto bytes = body();
                require (bridge.loadCartridgeBytes (bytes), "pumping body loads");
                TrenchParams params; params.q = 1.0f;
                std::vector<float> trajectory (512);
                juce::AudioBuffer<float> audio (2, 512);
                juce::Random noise (7);
                float peak = 0.0f;
                bool finite = true;
                for (int block = 0; block < 400; ++block)
                {
                    for (int i = 0; i < 512; ++i)
                    {
                        const int n = block * 512 + i;
                        trajectory[(size_t) i] = flipEvery <= 0 ? still : ((n / flipEvery) % 2 == 0 ? 0.0f : 1.0f);
                        const float x = 0.5f * (noise.nextFloat() * 2.0f - 1.0f);
                        audio.setSample (0, i, x); audio.setSample (1, i, x);
                    }
                    bridge.processTrajectory (audio, trajectory.data(), params);
                    for (int i = 0; i < 512; ++i)
                    {
                        const float y = audio.getSample (0, i);
                        finite = finite && std::isfinite (y);
                        if (block >= 20) peak = std::max (peak, std::abs (y));
                    }
                }
                return finite ? peak : -1.0f;
            };
            float still = 0.0f, stillAt = 0.0f;
            for (int k = 0; k <= 20; ++k)
            {
                const float at = worst (0, (float) k / 20.0f);
                if (at > still) { still = at; stillAt = (float) k / 20.0f; }
            }
            float fastest = 0.0f;
            for (const int flip : { 44, 63, 88, 100, 176, 352, 1024, 4410 })
            {
                const float moving = worst (flip, 0.0f);
                std::printf ("pumping: Morph flips 0<->1 every %d samples at Q 100, peak %.2f dB vs %.2f dB at the loudest still Morph (%.2f)\n", flip,
                             juce::Decibels::gainToDecibels (moving), juce::Decibels::gainToDecibels (still), stillAt);
                require (moving > 0.0f, "non-finite output under fastest Morph motion");
                fastest = std::max (fastest, moving);
            }
        }
        auto base = trench::calibration::defaults();
        base[2] = 1; base[5] = 1;
        base[12] = 0;
        const auto reference = render (base);
        for (auto [index, value] : { std::pair<size_t,float>{0,1}, {1,128}, {2,0}, {5,0}, {6,12}, {7,1}, {12,1} })
        {
            auto v = base; v[index] = value;
            const double delta = difference (reference, render (v));
            std::printf ("%s audio delta %.9g\n", trench::calibration::variables[index].id, delta);
            require (delta > 1.0e-7, "control does not affect stressed render");
        }
        for (const auto group : { 7 })
        {
            auto v = base; v[(size_t) group] = 1;
            const auto original = render (v);
            const auto check = [&] (size_t index, float value)
            {
                auto changed = v; changed[index] = value;
                const double delta = difference (original, render (changed));
                std::printf ("%s conditional delta %.9g\n", trench::calibration::variables[index].id, delta);
                require (delta > 1.0e-8, "conditional control does not affect render");
            };
            if (group == 7) { check (8, 0); check (9, 50); check (10, 700); check (11, -30); }
        }
        auto bench = trench::calibration::defaults();
        render (bench, 0.7f, true); bench[0] = 1; render (bench, 0.7f, true);
        bench[0] = 0; bench[1] = 1; render (bench, 0.7f, true);
        if (argc > 1) screenshot (juce::File (juce::String::fromUTF8 (argv[1])));
        std::puts ("PASS calibration: packed reference at three rates, persistence, audio reachability");
        return 0;
    }
    catch (const std::exception& e) { std::fprintf (stderr, "FAIL: %s\n", e.what()); return 1; }
}
