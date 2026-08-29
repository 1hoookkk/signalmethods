#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include "TrenchBodyRoster.h"
#include "dsp/PreampLaw.h"
#include "ui/GlassWords.h"
#include "ui/BayKnob.h"
#include "ui/KeySnapBox.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstdio>

namespace
{
int failures = 0;
void check (bool ok, const char* what, double a = 0.0, double b = 0.0)
{
    std::printf ("%s  %s  (%.6g / %.6g)\n", ok ? "PASS" : "FAIL", what, a, b);
    if (! ok) ++failures;
}
double db (double lin) { return 20.0 * std::log10 (lin); }

void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

void setParam (PluginProcessor& p, const char* id, float denorm)
{
    if (auto* prm = p.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (denorm));
}

struct Run { float peak = 0.0f; bool finite = true; };
Run runSine (PluginProcessor& p, float amplitude, int blocks = 40)
{
    constexpr int n = 512;
    juce::AudioBuffer<float> buf (2, n);
    juce::MidiBuffer midi;
    Run r;
    double phase = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < n; ++i)
        {
            const float s = amplitude * (float) std::sin (phase);
            phase += 2.0 * juce::MathConstants<double>::pi * 220.0 / 48000.0;
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
        }
        p.processBlock (buf, midi);
        if (b >= blocks / 2)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) r.finite = false;
                    r.peak = juce::jmax (r.peak, std::abs (v));
                }
    }
    return r;
}

template <typename T>
T* findChild (juce::Component& root, const juce::String& title = {})
{
    for (auto* c : root.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (c); t != nullptr && (title.isEmpty() || t->getTitle() == title))
            return t;
        if (auto* t = findChild<T> (*c, title))
            return t;
    }
    return nullptr;
}

void savePng (const juce::Image& img, const juce::String& name)
{
    auto f = juce::File::getCurrentWorkingDirectory().getChildFile (name);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("SHOT  %s  %dx%d\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

bool anyVisibleOfTitle (juce::Component& root, const juce::String& title)
{
    for (auto* c : root.getChildren())
    {
        if (c->getTitle() == title && c->isShowing()) return true;
        if (anyVisibleOfTitle (*c, title)) return true;
    }
    return false;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::printf ("== PREAMP law ==\n");
    check (trench::preampGain (0.0f) == 1.0f, "preampGain(0) is exactly unity", trench::preampGain (0.0f), 1.0);
    check (std::abs (db (trench::preampGain (0.5f)) - 20.0) < 0.01, "preampGain(0.5) is +20 dB", db (trench::preampGain (0.5f)), 20.0);
    check (std::abs (db (trench::preampGain (1.0f)) - 40.0) < 0.01, "preampGain(1.0) is +40 dB", db (trench::preampGain (1.0f)), 40.0);

    std::printf ("== bridge boundary ==\n");
    {
        TrenchDspBridge bridge;
        bridge.prepare (48000.0, 512);
        const bool loaded = bridge.loadCartridgeBytes (BinaryData::identity_body240, (size_t) BinaryData::identity_body240Size);
        check (loaded, "identity body loads into the bridge");
        bridge.setInputPreamp (trench::preampGain (0.0f));
        juce::AudioBuffer<float> buf (2, 64);
        buf.clear();
        buf.setSample (0, 0, 0.25f);
        buf.setSample (1, 0, 0.25f);
        TrenchParams params;
        bridge.process (buf, params);
        float sum = 0.0f;
        for (int i = 0; i < 64; ++i) sum += buf.getSample (0, i);
        check (std::abs (buf.getSample (0, 0) - 0.25f) < 1.0e-5f && std::abs (sum - 0.25f) < 1.0e-4f,
               "PREAMP 0 through identity body returns the impulse unchanged", buf.getSample (0, 0), 0.25);
    }

    std::printf ("== processor at defaults ==\n");
    const double hostRate = std::getenv ("TRENCH_RATE") != nullptr ? std::atof (std::getenv ("TRENCH_RATE")) : 48000.0;
    PluginProcessor processor;
    processor.setPlayConfigDetails (2, 2, hostRate, 512);
    processor.prepareToPlay (hostRate, 512);
    pump (300);
    const float in = 0.001f;
    const auto base = runSine (processor, in);
    check (base.finite, "default output finite");
    check (base.peak > in * 0.5f, "nonzero input at default settings is not silenced", base.peak, in);
    check (std::abs (db (base.peak / in) - db (1.6107)) < 0.2, "PREAMP 0 = unity into the fixed voice gain", db (base.peak / in), db (1.6107));


    setParam (processor, ParamID::preamp, 1.0f);
    setParam (processor, ParamID::slamDrive, 1.0f);
    const auto hidden = runSine (processor, in);
    check (std::abs (db (hidden.peak / base.peak)) < 0.2, "hidden INPUT/OUTPUT at saved extremes leave the sound at unity", db (hidden.peak / base.peak), 0.0);
    setParam (processor, ParamID::slamDrive, 0.0f);
    setParam (processor, ParamID::preamp, 0.0f);
    const auto loud = runSine (processor, 0.9f);
    check (loud.finite && loud.peak > 0.1f, "full-scale input at defaults stays finite and audible", loud.peak, 0.9);

    if (std::getenv ("TRENCH_MEASURE") != nullptr)
    {
        trench::rescanBodyRoster();
        int n = 0; trench::bodyRoster (n);
        int bodyIndex = -1;
        for (int i = 0; i < n; ++i)
            if (trench::bodyDisplayName (i).containsIgnoreCase (std::getenv ("TRENCH_MEASURE"))) { bodyIndex = i; break; }
        check (bodyIndex >= 0, "measure body found", bodyIndex, n);
        if (bodyIndex >= 0)
        {
            setParam (processor, ParamID::body, (float) bodyIndex);
            pump (400);
            std::printf ("MEASURE body %s rate %.0f loadOk %d\n", trench::bodyDisplayName (bodyIndex).toRawUTF8(), processor.getSampleRate(), (int) processor.getLastLoadOk());
            for (float morph : { 0.0f, 0.5f, 1.0f })
            {
                setParam (processor, ParamID::morph, morph);
                setParam (processor, ParamID::q, 0.0f);
                runSine (processor, 0.0f, 8);
                constexpr int N = 8192;
                juce::AudioBuffer<float> buf (2, N);
                buf.clear();
                buf.setSample (0, 0, 0.5f); buf.setSample (1, 0, 0.5f);
                juce::MidiBuffer midi;
                for (int start = 0; start < N; start += 512)
                {
                    float* ch[2] = { buf.getWritePointer (0) + start, buf.getWritePointer (1) + start };
                    juce::AudioBuffer<float> slice (ch, 2, 512);
                    processor.processBlock (slice, midi);
                }
                float coeffs[trench::kUiCoeffCount] = {}; float boost = 1.0f;
                processor.probeCurrentBodyForUi (morph, 0.0f, coeffs, boost);
                const double sr = processor.getSampleRate();
                std::printf ("MEASURE morph %.2f  freq: measured dB / coefficient dB\n", morph);
                double worst = 0.0;
                for (double f : { 60.0, 120.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 12000.0 })
                {
                    const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
                    std::complex<double> acc (0.0, 0.0);
                    for (int i = 0; i < N; ++i) acc += (double) buf.getSample (0, i) * std::polar (1.0, -w * i);
                    const double measured = 20.0 * std::log10 (std::abs (acc) / 0.5 / 1.6107);
                    std::complex<double> h (1.0, 0.0);
                    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
                    for (int sct = 0; sct < trench::kUiStageCount; ++sct)
                    {
                        const float* c = coeffs + sct * trench::kUiCoeffsPerStage;
                        h *= ((double) c[0] + (double) c[1] * z1 + (double) c[2] * z2) / (1.0 + (double) c[3] * z1 + (double) c[4] * z2);
                    }
                    const double expected = 20.0 * std::log10 (std::abs (h));
                    worst = juce::jmax (worst, std::abs (measured - expected));
                    std::printf ("  %7.0f Hz  %7.2f  /  %7.2f\n", f, measured, expected);
                }
                check (worst < 1.0, "audio response matches the coefficient response (worst dB)", worst, 1.0);
            }
        }
    }
    std::printf ("== key ==\n");
    check (TrenchDspBridge::keySnapRatio (0) == 1.0, "KEY OFF is ratio 1", TrenchDspBridge::keySnapRatio (0), 1.0);
    check (std::abs (TrenchDspBridge::keySnapRatio (7) - std::pow (2.0, 6.0 / 12.0)) < 1e-9, "KEY F# min is +6 semitones", TrenchDspBridge::keySnapRatio (7), std::pow (2.0, 0.5));
    check (std::abs (TrenchDspBridge::keySnapRatio (20) - std::pow (2.0, -5.0 / 12.0)) < 1e-9, "KEY G maj is -5 semitones", TrenchDspBridge::keySnapRatio (20), std::pow (2.0, -5.0 / 12.0));
    {
        trench::rescanBodyRoster();
        int n = 0; trench::bodyRoster (n);
        int bodyIndex = -1;
        for (int i = 0; i < n; ++i)
            if (trench::bodyDisplayName (i).containsIgnoreCase ("Crisp")) { bodyIndex = i; break; }
        if (bodyIndex >= 0)
        {
            setParam (processor, ParamID::body, (float) bodyIndex);
            pump (400);
            setParam (processor, ParamID::morph, 0.0f);
            setParam (processor, ParamID::keySnap, 0.0f);
            const auto off = runSine (processor, 0.01f);
            setParam (processor, ParamID::keySnap, 7.0f);
            const auto snapped = runSine (processor, 0.01f);
            check (snapped.finite && std::abs (db (snapped.peak / off.peak)) > 1.0, "KEY F# audibly shifts the Crisp body at 220 Hz (dB)", db (snapped.peak / off.peak), 1.0);
            setParam (processor, ParamID::keySnap, 0.0f);
            setParam (processor, ParamID::body, (float) trench::kDefaultBodyIndex);
            pump (400);
        }
    }
    std::printf ("== face ==\n");
    auto* editor = processor.createEditorIfNeeded();
    juce::Component holder;
    holder.setSize (editor->getWidth(), editor->getHeight());
    holder.addAndMakeVisible (editor);
    holder.addToDesktop (juce::ComponentPeer::windowIsTemporary);
    holder.setVisible (true);
    pump (600);
    check (editor->getWidth() == trench::ui::kEditorWidth && editor->getHeight() == trench::ui::kEditorHeight,
           "editor is at DAW size", editor->getWidth(), editor->getHeight());
    auto* words = findChild<trench::ui::GlassWords> (*editor);
    auto* c1 = findChild<trench::ui::BayKnob> (*editor, "Color 1");
    auto* c3 = findChild<trench::ui::BayKnob> (*editor, "Color 3");
    auto* keyBox = findChild<trench::ui::KeySnapBox> (*editor);
    check (words != nullptr && c1 != nullptr && c3 != nullptr && keyBox != nullptr && keyBox->isShowing(), "Modulation chip, KEY box, three slots exist");
    if (words == nullptr || c1 == nullptr || c3 == nullptr)
        return 1;
    check (words->isShowing() && c1->isShowing() && c3->isShowing(), "Modulation, KEY, and the three slots are always on the face");
    for (const char* gone : { "Input", "Bite", "Follow", "Movement", "Output", "Low", "Division", "Section", "Generator" })
        check (! anyVisibleOfTitle (*editor, gone), (juce::String ("absent from the face: ") + gone).toRawUTF8());
    check (c1->getHeight() >= 24 && words->getHeight() >= 18, "rows are legible", c1->getHeight(), words->getHeight());
    pump (150);
    savePng (holder.createComponentSnapshot (holder.getLocalBounds()), "trench_face_main.png");
    setParam (processor, ParamID::movePreset, 1.0f);
    pump (150);
    savePng (holder.createComponentSnapshot (holder.getLocalBounds()), "trench_face_gen_on.png");
    processor.editorBeingDeleted (editor);
    holder.removeChildComponent (editor);
    delete editor;
    std::printf ("== %d failure(s) ==\n", failures);
    return failures == 0 ? 0 : 1;
}
