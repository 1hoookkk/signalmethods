#pragma once
#include "PluginProcessor.h"
#include "UiLayout.h"
#include "ui/Theme.h"
#include "ui/FaceplateView.h"
#include "ui/GraphDisplay.h"
#include "ui/WheelControl.h"
#include "ui/DeskKnob.h"
#include "ui/KeySnapBox.h"
#include "ui/SlamButton.h"
#include "ui/ValueReadout.h"
#include "ui/TypeSelectorView.h"
#include "ui/BodyBrowser.h"
#include "ui/GlassWords.h"
#include "ui/ModulationChip.h"
#include "ui/ModulationBay.h"
#include "ui/LabelsLayer.h"
#if TRENCH_DEV_PANEL
#include "ui/DevCalibrationPanel.h"
#endif
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
struct MorphFollower
{
    static constexpr double kSmoothMs = 25.0;
    double shown = 0.0, lastMs = 0.0;
    bool primed = false;
    double advance (double target, double nowMs)
    {
        if (! primed) { shown = target; lastMs = nowMs; primed = true; return shown; }
        const double dt = juce::jlimit (0.0, 100.0, nowMs - lastMs);
        lastMs = nowMs;
        shown += (target - shown) * (1.0 - std::exp (-dt / kSmoothMs));
        if (std::abs (target - shown) < 1.0e-4) shown = target;
        return shown;
    }
};
class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff1d1915)); }
    void mouseDown (const juce::MouseEvent& e) override;
    void setUiScale (float scale);
    float getUiScale() const noexcept { return uiScale; }
private:
    float uiScale = 1.0f;
    void onFrame();
    void timerCallback() override { onFrame(); }
    PluginProcessor& processor;
    float lastProbedMorph = -1.0f, lastProbedQ = -1.0f;
    int lastProbedBodyVersion = -1;
    double lastProbedRate = 0.0;
    double lastProbedKeyRatio = -1.0;
    MorphFollower morphFollower;
    const trench::UiLayout layout { trench::UiLayout::defaults() };
    trench::ui::Theme theme { layout };
    juce::TooltipWindow tooltipWindow { this, 650 };
    juce::Component face;
    std::unique_ptr<trench::ui::FaceplateView>    faceplate;
    std::unique_ptr<trench::ui::GraphDisplay>     graph;
    std::unique_ptr<trench::ui::TypeSelectorView> typeSelector;
    std::unique_ptr<trench::ui::BodyBrowser>      bodyBrowser;
    std::unique_ptr<trench::ui::BodyBrowser>      motionBrowser;
    std::unique_ptr<trench::ui::ModulationChip>   modulationChip;
    std::unique_ptr<trench::ui::KeySnapBox>       keySnapBox;
    std::unique_ptr<trench::ui::DeskKnob>         inputKnob;
    std::unique_ptr<trench::ui::SlamButton> slamButton;
    std::unique_ptr<trench::ui::DeskKnob>         outputKnob;
    std::unique_ptr<trench::ui::ValueReadout>     inputReadout;
    std::unique_ptr<trench::ui::ValueReadout>     outputReadout;
    std::unique_ptr<trench::ui::WheelControl>     morphWheel;
    std::unique_ptr<trench::ui::WheelControl>     secondaryWheel;
    std::unique_ptr<trench::ui::ValueReadout>     morphReadout;
    std::unique_ptr<trench::ui::ValueReadout>     secondaryReadout;
    std::unique_ptr<trench::ui::LabelsLayer>      labels;
#if TRENCH_DEV_PANEL
    std::unique_ptr<trench::ui::DevPanel>         devPanel;
#endif
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
