#pragma once
#include "PluginProcessor.h"
#include "UiLayout.h"
#include "ui/Theme.h"
#include "ui/FaceplateView.h"
#include "ui/GraphDisplay.h"
#include "ui/WheelControl.h"
#include "ui/ValueReadout.h"
#include "ui/TypeSelectorView.h"
#include "ui/BodyBrowser.h"
#include "ui/GlassWords.h"
#include "ui/KeySnapBox.h"
#include "ui/LabelsLayer.h"
#include "ui/BayKnob.h"
#include "ui/Onboarding.h"
#if TRENCH_DEV_PANEL
#include "ui/DevPanel.h"
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
private:
    void onFrame();
    PluginProcessor& processor;
    float lastProbedMorph = -1.0f, lastProbedQ = -1.0f;
    int lastProbedBodyVersion = -1;
    double lastProbedRate = 0.0;
    const trench::UiLayout layout { trench::UiLayout::defaults() };
    trench::ui::Theme theme { layout };
    std::unique_ptr<juce::VBlankAttachment> vblank;
    juce::TooltipWindow tooltipWindow { this, 650 };
    std::unique_ptr<trench::ui::FaceplateView>    faceplate;
    std::unique_ptr<trench::ui::GraphDisplay>     graph;
    std::unique_ptr<trench::ui::TypeSelectorView> typeSelector;
    std::unique_ptr<trench::ui::BodyBrowser>      bodyBrowser;
    std::unique_ptr<trench::ui::GlassWords>       glassWords;
    std::unique_ptr<trench::ui::FollowLamp>       followLamp;
    std::unique_ptr<trench::ui::KeySnapBox>       keySnapBox;
    std::unique_ptr<trench::ui::WheelControl>     morphWheel;
    std::unique_ptr<trench::ui::WheelControl>     secondaryWheel;
    std::unique_ptr<trench::ui::ValueReadout>     morphReadout;
    std::unique_ptr<trench::ui::ValueReadout>     secondaryReadout;
    std::unique_ptr<trench::ui::LabelsLayer>      labels;
    std::unique_ptr<trench::ui::BayKnob>          inputKnob, outputKnob, followKnob;
    std::unique_ptr<trench::ui::GlassValue>       zWord;
    std::unique_ptr<trench::ui::Onboarding>       onboarding;
#if TRENCH_DEV_PANEL
    std::unique_ptr<trench::ui::DevPanel>         devPanel;
#endif
    bool onboardingSeen() const;
    void markOnboardingSeen();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
