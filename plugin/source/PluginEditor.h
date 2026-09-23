#pragma once
#include "PluginProcessor.h"
#include "UiLayout.h"
#include "ui/Theme.h"
#include "ui/FaceplateView.h"
#include "ui/GraphDisplay.h"
#include "ui/WheelControl.h"
#include "ui/DeskKnob.h"
#include "ui/KeySnapBox.h"
#include "ui/ValueReadout.h"
#include "ui/TypeSelectorView.h"
#include "ui/BodyBrowser.h"
#include "ui/GlassWords.h"
#include "ui/ModulationChip.h"
#include "ui/ModulationBay.h"
#include "ui/LabelsLayer.h"
#include "ui/Onboarding.h"
#if TRENCH_DEV_PANEL
#include "ui/DevCalibrationPanel.h"
#endif
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
class PluginEditor final : public juce::AudioProcessorEditor
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
    PluginProcessor& processor;
    float lastProbedMorph = -1.0f, lastProbedQ = -1.0f;
    int lastProbedBodyVersion = -1;
    double lastProbedRate = 0.0;
    double lastProbedKeyRatio = -1.0;
    std::uint32_t lastMorphUpdates = 0;
    double morphFrom = 0.0, morphTo = 0.0, morphShown = 0.0, morphArrivedMs = 0.0, morphIntervalMs = 20.0;
    const trench::UiLayout layout { trench::UiLayout::defaults() };
    trench::ui::Theme theme { layout };
    std::unique_ptr<juce::VBlankAttachment> vblank;
    juce::TooltipWindow tooltipWindow { this, 650 };
    juce::Component face;
    std::unique_ptr<trench::ui::FaceplateView>    faceplate;
    std::unique_ptr<trench::ui::GraphDisplay>     graph;
    std::unique_ptr<trench::ui::TypeSelectorView> typeSelector;
    std::unique_ptr<trench::ui::BodyBrowser>      bodyBrowser;
    std::unique_ptr<trench::ui::ModulationChip>   modulationChip;
    std::unique_ptr<trench::ui::KeySnapBox>       keySnapBox;
    std::unique_ptr<trench::ui::DeskKnob>         inputKnob;
    std::unique_ptr<trench::ui::DeskKnob>         outputKnob;
    std::unique_ptr<trench::ui::ValueReadout>     inputReadout;
    std::unique_ptr<trench::ui::ValueReadout>     outputReadout;
    std::unique_ptr<trench::ui::WheelControl>     morphWheel;
    std::unique_ptr<trench::ui::WheelControl>     secondaryWheel;
    std::unique_ptr<trench::ui::ValueReadout>     morphReadout;
    std::unique_ptr<trench::ui::ValueReadout>     secondaryReadout;
    std::unique_ptr<trench::ui::LabelsLayer>      labels;
    std::unique_ptr<trench::ui::Onboarding>       onboarding;
#if TRENCH_DEV_PANEL
    std::unique_ptr<trench::ui::DevPanel>         devPanel;
#endif
    bool onboardingSeen() const;
    void markOnboardingSeen();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
