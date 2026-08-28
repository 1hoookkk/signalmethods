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
    // THE VOID BEHIND THE PUNCHED PLATE. The wheel wells are real holes in the
    // plate art, and the drum ink covers only 127.5 of the 137.4px slot — the
    // slivers show whatever is behind. Un-painted, that was JUCE's default
    // black, which read as a black frame around the wheel (Tyson 2026-08-14
    // "redundant black frame... the padding"). A code-drawn cavity rectangle
    // over the art regressed twice; recolouring the void itself has no
    // geometry to get wrong — it is only ever visible through the holes.
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1d1915));
    }
    /// FaceShot hook: the body list lives in its own window now.
    trench::ui::BodyBrowser* bodyListForShot() { return bodyBrowser.get(); }
    /// FaceShot hook: force the tour visible at a given step for judging.
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
    // The MOVEMENT list's voice. Declared BEFORE the chip so the chip - and any
    // menu still targeting it - is gone before the LookAndFeel it borrows.
    trench::ui::SectionRail::LightMenuLnF         movementMenuLnF;
    std::unique_ptr<trench::ui::MovementChip>     movementChip;
    std::unique_ptr<trench::ui::TypeSelectorView> typeSelector;
    std::unique_ptr<trench::ui::BodyBrowser>      bodyBrowser;
    std::unique_ptr<trench::ui::SectionRail>      sectionRail;
    std::unique_ptr<trench::ui::BayKnob>          preampKnob;
    std::unique_ptr<trench::ui::BayKnob>          chewKnob;
    std::unique_ptr<trench::ui::BayKnob>          slamKnob;
    std::unique_ptr<trench::ui::BayKnob>          lowKnob;
    int openSection = trench::ui::SectionRail::kDrive;
    void applySectionVisibility();
    void updateEditorSize();
    std::unique_ptr<trench::ui::WheelControl>     morphWheel;
    std::unique_ptr<trench::ui::WheelControl>     secondaryWheel;
    std::unique_ptr<trench::ui::ValueReadout>     morphReadout;
    std::unique_ptr<trench::ui::ValueReadout>     secondaryReadout;
    std::unique_ptr<trench::ui::Onboarding>       onboarding;
    trench::ui::Onboarding::ReplayHotspot         onboardingReplayHotspot;
    float onboardingDemoStart = -1.0f;
    int bodyBeforeTour = -1;   // the tour borrows a body; this brings the user's back
    std::unique_ptr<trench::ui::LabelsLayer>      labels;
    std::unique_ptr<trench::ui::DecalsLayer>      decalsLayer;
    // The dev bypass desk. Compiled into the SHIPPING build on purpose: the
    // stages it takes out are only judgeable in a host, on real music. Opened
    // with Ctrl+Shift+click on the TRENCH badge, invisible otherwise.
    std::unique_ptr<trench::ui::DevBypassPanel>   devPanel;
    // The phrase sketch, opened from the desk. Drawn strokes go straight out to
    // the LIVE slot; SAVE writes a template the bake picks up.
    std::unique_ptr<trench::ui::MovementSketch>   movementSketch;
#if TRENCH_GOD_MODE
    // The layout desk. Armed with Ctrl+Shift+G; off, it is invisible and
    // mouse-transparent, so no parameter gesture can reach it by accident.
    std::unique_ptr<trench::ui::GodModeOverlay>   godMode;
#endif
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
