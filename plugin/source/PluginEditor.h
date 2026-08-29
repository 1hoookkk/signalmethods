#pragma once
#include "PluginProcessor.h"
#include "UiLayout.h"
#include "ui/Theme.h"
#include "ui/FaceplateView.h"
#include "ui/GraphDisplay.h"
#include "ui/KeySnapBox.h"
#include "ui/MovementChip.h"
#include "ui/Onboarding.h"
#include "ui/WheelControl.h"
#include "ui/ValueReadout.h"
#include "ui/TypeSelectorView.h"
#include "ui/BodyBrowser.h"
#include "ui/SectionRail.h"
#include "ui/BayKnob.h"
#include "ui/LabelsLayer.h"
#include "ui/DecalsLayer.h"
#include "ui/DevBypassPanel.h"
#include "ui/MovementSketch.h"
#if TRENCH_GOD_MODE
 #include "ui/GodMode.h"
#endif
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>
class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;
    void resized() override;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1d1915));
    }

    trench::ui::BodyBrowser* bodyListForShot() { return bodyBrowser.get(); }

    void showOnboardingStep (int step);
private:
    void timerCallback() override;
    void reloadLayoutFromDisk();
    juce::Time layoutMtime;
    void layoutComponents();
    void onFrame();
    PluginProcessor& processor;
    float lastProbedMorph = -1.0f, lastProbedQ = -1.0f;
    int lastProbedBodyVersion = -1;
    double lastProbedRate = 0.0;
    trench::UiLayout currentLayout { trench::UiLayout::defaults() };
    trench::ui::Theme theme { currentLayout };
    std::unique_ptr<juce::VBlankAttachment> vblank;
    juce::TooltipWindow tooltipWindow { this, 650 };
    std::unique_ptr<trench::ui::FaceplateView>    faceplate;
    std::unique_ptr<trench::ui::GraphDisplay>     graph;
    std::unique_ptr<trench::ui::KeySnapBox>       keySnapBox;

    trench::ui::SectionRail::LightMenuLnF         movementMenuLnF;
    std::unique_ptr<trench::ui::MovementChip>     movementChip;
    std::unique_ptr<trench::ui::TypeSelectorView> typeSelector;
    std::unique_ptr<trench::ui::BodyBrowser>      bodyBrowser;
    std::unique_ptr<trench::ui::SectionRail>      sectionRail;
    std::unique_ptr<trench::ui::BayKnob>          preampKnob;
    std::unique_ptr<trench::ui::BayKnob>          chewKnob;
    std::unique_ptr<trench::ui::BayKnob>          slamKnob;
    std::unique_ptr<trench::ui::BayKnob>          lowKnob;
    int openSection = 0;
    void applySectionVisibility();
    void updateEditorSize();
    std::unique_ptr<trench::ui::WheelControl>     morphWheel;
    std::unique_ptr<trench::ui::WheelControl>     secondaryWheel;
    std::unique_ptr<trench::ui::ValueReadout>     morphReadout;
    std::unique_ptr<trench::ui::BayKnob>          followKnob;
    std::unique_ptr<juce::Component>              movementBay;
    std::unique_ptr<juce::Component>              autoTrim;
    std::unique_ptr<trench::ui::ValueReadout>     secondaryReadout;
    std::unique_ptr<trench::ui::Onboarding>       onboarding;
    trench::ui::Onboarding::ReplayHotspot         onboardingReplayHotspot;
    float onboardingDemoStart = -1.0f;
    int bodyBeforeTour = -1;
    std::unique_ptr<trench::ui::LabelsLayer>      labels;
    std::unique_ptr<trench::ui::DecalsLayer>      decalsLayer;

    std::unique_ptr<trench::ui::DevBypassPanel>   devPanel;

    std::unique_ptr<trench::ui::MovementSketch>   movementSketch;
#if TRENCH_GOD_MODE

    std::unique_ptr<trench::ui::GodModeOverlay>   godMode;
#endif
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
