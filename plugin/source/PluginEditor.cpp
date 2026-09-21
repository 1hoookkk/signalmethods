#include "PluginEditor.h"
#include "parameters/CurveMap.h"
#include "ui/Onboarding.h"
#include <cstdlib>
#include "BinaryData.h"
#include "TrenchBodyRoster.h"
using namespace trench::ui;
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    processor.setEditorOpen (true);
    uiFontFamily() = layout.string ("fontFamily", kUiFontName);
    uiEmphasisFontFamily() = layout.string ("fontFamilyEmphasis", kUiEmphasisFontName);
    uiBoldEnabled() = layout.param ("fontBold", 0.0) > 0.5;
    auto panel = juce::ImageCache::getFromMemory (BinaryData::df2_panel_beige_png,
                                                  BinaryData::df2_panel_beige_pngSize);
    auto strip = juce::ImageCache::getFromMemory (BinaryData::trench_ss3_strip_png,
                                                  BinaryData::trench_ss3_strip_pngSize);
#if TRENCH_DEV_PANEL
    if (const char* override = std::getenv ("TRENCH_WHEEL_STRIP"))
        strip = juce::ImageFileFormat::loadFrom (juce::File (juce::String::fromUTF8 (override)));
#endif
    if (const char* override = std::getenv ("TRENCH_WHEEL_STRIP"))
        strip = juce::ImageFileFormat::loadFrom (juce::File (juce::String::fromUTF8 (override)));
    faceplate = std::make_unique<FaceplateView> (panel, theme);
    faceplate->setBufferedToImage (true);
    graph = std::make_unique<GraphDisplay> (theme, processor.apvts, juce::String());
    typeSelector = std::make_unique<TypeSelectorView> (processor.apvts, theme);
    typeSelector->onSeed = [this]
    {
        if (processor.seedCurrentBody())
        {
            graph->playSeedPulse();
            graph->announce ("SIBLING SEEDED");
        }
    };
    typeSelector->onExportBody = [this] { processor.exportCurrentBody(); };
    typeSelector->onAnnounce   = [this] (const juce::String& s) { graph->announce (s); };
    bodyBrowser = std::make_unique<BodyBrowser> (theme);
    bodyBrowser->onPreview = [this] (int index) { processor.previewBodyForUi (index); };
    bodyBrowser->onCommit  = [this] (int index) { typeSelector->setSelectedBody (index); processor.restoreBodyForUi (index); };
    bodyBrowser->onRestore = [this] (int index) { processor.restoreBodyForUi (index); };
    typeSelector->onOpenBrowser = [this] (int current) { bodyBrowser->open (current, getLocalBounds()); };
    morphWheel = std::make_unique<WheelControl> (processor.apvts, ParamID::morph, strip, theme);
    secondaryWheel = std::make_unique<WheelControl> (processor.apvts, ParamID::q, strip, theme);
    morphReadout = std::make_unique<ValueReadout> ("morphReadout", theme);
    secondaryReadout = std::make_unique<ValueReadout> ("qReadout", theme);
    morphReadout->bindParameter (processor.apvts.getParameter (ParamID::morph));
    secondaryReadout->bindParameter (processor.apvts.getParameter (ParamID::q));
    modulationChip = std::make_unique<ModulationChip> (processor.apvts, theme);
    modulationChip->onRestart = [this] { processor.restartMovement(); };
    modulationBay = std::make_unique<ModulationBay> (processor, *modulationChip, theme);
    modulationChip->onEdit = [this] { modulationBay->edit(); };
    labels = std::make_unique<LabelsLayer> (theme);
    labels->setRailLabels ("MORPH", "Q");
    addAndMakeVisible (*faceplate);
    addAndMakeVisible (*morphWheel);
    addAndMakeVisible (*secondaryWheel);
    addAndMakeVisible (*graph);
    addAndMakeVisible (*typeSelector);
    addAndMakeVisible (*morphReadout);
    addAndMakeVisible (*secondaryReadout);
    addAndMakeVisible (*modulationBay);
    addAndMakeVisible (*labels);
    addChildComponent (*bodyBrowser);
    onboarding = std::make_unique<Onboarding> (theme);
    onboarding->onComplete = [this]
    {
        markOnboardingSeen();
        onboarding->setVisible (false);
    };
    addChildComponent (*onboarding);
    {
        const bool headless = std::getenv ("TRENCH_HEADLESS") != nullptr;
        const bool forced = std::getenv ("TRENCH_SHOW_ONBOARDING") != nullptr;
        onboarding->setVisible (forced || (! headless && ! onboardingSeen()));
        if (onboarding->isVisible())
            onboarding->watch (*this);
        if (const char* hover = std::getenv ("TRENCH_ONBOARDING_HOVER"))
            onboarding->forceHover (juce::String (hover).getIntValue());
    }
#if TRENCH_DEV_PANEL
    devPanel = std::make_unique<DevPanel> (theme, processor,
        juce::File (TRENCH_TABLE_STITCH_ROOT).getChildFile ("plugin/patterns/loops"));
    addAndMakeVisible (*devPanel);
    setResizable (false, false);
    setSize (kEditorWidth + kDevPanelWidth, kDevPanelHeight);
#else
    setResizable (false, false);
    setSize (kEditorWidth, kEditorHeight);
#endif
    setWantsKeyboardFocus (false);
    vblank = std::make_unique<juce::VBlankAttachment> (this, [this] { onFrame(); });
    onFrame();
}
PluginEditor::~PluginEditor()
{
    vblank.reset();
    processor.setEditorOpen (false);
}
void PluginEditor::resized()
{
    const juce::Rectangle<int> base { 0, 0, kEditorWidth, kEditorHeight };
    const auto rectOf = [this] (const char* id) { return theme.rect (id).getSmallestIntegerContainer(); };
    faceplate->setBounds (base);
    labels->setBounds (base);
    graph->setBounds (rectOf ("spectrumGrid"));
    typeSelector->setBounds (rectOf ("typeSelector"));
    morphWheel->setBounds (rectOf ("morphWheel"));
    secondaryWheel->setBounds (rectOf ("qWheel"));
    const auto px = [] (float value) { return juce::roundToInt (value * (float) kEditorWidth / 250.0f); };
    const auto wheel = secondaryWheel->getBounds();
    modulationBay->setBounds (wheel.getX() + px (5), wheel.getBottom() + px (48), 240, 99);
    morphReadout->setBounds (rectOf ("morphReadout"));
    secondaryReadout->setBounds (rectOf ("qReadout"));
#if TRENCH_DEV_PANEL
    devPanel->setBounds (kEditorWidth, 0, kDevPanelWidth, kDevPanelHeight);
#endif
    onboarding->setBounds (0, 0, kEditorWidth, kEditorHeight);
    onboarding->setTargets ({
        { modulationBay->getBounds(), "MOVE", "choose movement and edit its properties" },
    });



    onboarding->toFront (false);
}
static juce::PropertiesFile::Options trenchSettingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "TRENCH";
    o.filenameSuffix = "settings";
    o.folderName = "Signal Methods";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}
bool PluginEditor::onboardingSeen() const
{
    juce::PropertiesFile file (trenchSettingsOptions());
    return file.getBoolValue ("onboarding.axes", false);
}
void PluginEditor::markOnboardingSeen()
{
    juce::PropertiesFile file (trenchSettingsOptions());
    file.setValue ("onboarding.axes", true);
    file.saveIfNeeded();
}
void PluginEditor::onFrame()
{
    modulationBay->refreshMotion();
    modulationBay->setRadiusActivity (processor.getGritActivityForUi());
    modulationChip->setActive (processor.isMorphModulatedForUi());
    const auto read = [this] (const char* paramID)
    {
        if (auto* v = processor.apvts.getRawParameterValue (paramID))
            return juce::jlimit (0.0f, 1.0f, v->load());
        return 0.0f;
    };
    float coeffs[trench::kUiCoeffCount] = {};
    float boost = 1.0f;
    const bool morphMoving = processor.isMorphModulatedForUi();
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto morphUpdates = processor.getMorphUpdatesForUi();
    if (morphUpdates != lastMorphUpdates)
    {
        const double gap = nowMs - morphArrivedMs;
        if (gap > 0.0 && gap < 200.0) morphIntervalMs += 0.2 * (gap - morphIntervalMs);
        lastMorphUpdates = morphUpdates;
        morphFrom = morphShown;
        morphTo = processor.getEffectiveMorphForUi();
        morphArrivedMs = nowMs;
    }
    morphShown = morphFrom + (morphTo - morphFrom) * juce::jlimit (0.0, 1.0, (nowMs - morphArrivedMs) / juce::jmax (1.0, morphIntervalMs));
    const float baseMorph = morphMoving ? (float) morphShown
                                        : trench::curves::curveMap (trench::curves::Axis::morph, read (ParamID::morph));
    const float baseQ = trench::curves::curveMap (trench::curves::Axis::q, read (ParamID::q));
    const int bodyVersion = processor.bodyVersionForUi.load (std::memory_order_relaxed);
    const double probeRate = processor.getSampleRate();
    const double probeKeyRatio = TrenchDspBridge::transposeRatio (
        processor.isNoteLatched(), processor.getNoteTrackRatio(),
        juce::jlimit (0, 24, (int) processor.apvts.getRawParameterValue (ParamID::keySnap)->load()));
    if (baseMorph != lastProbedMorph || baseQ != lastProbedQ
        || bodyVersion != lastProbedBodyVersion || probeRate != lastProbedRate
        || probeKeyRatio != lastProbedKeyRatio)
    {
        if (processor.probeCurrentBodyForUi (baseMorph, baseQ, coeffs, boost))
        {
            lastProbedMorph = baseMorph;
            lastProbedQ = baseQ;
            lastProbedBodyVersion = bodyVersion;
            lastProbedRate = probeRate;
            lastProbedKeyRatio = probeKeyRatio;
            graph->updateFromCoeffs (coeffs, boost, probeRate > 0.0 ? probeRate : 44'100.0);
        }
    }
    const bool morphHandDown = morphWheel->isMouseButtonDown (true) || morphReadout->isMouseButtonDown (true);
    const bool moving = morphMoving && ! morphHandDown;
    const float morphValue = moving ? (float) morphShown : read (ParamID::morph);
    morphWheel->setDisplayOverride (moving, morphValue);
    morphReadout->setNormalised (morphValue);
    const float qValue = read (ParamID::q);
    secondaryWheel->setDisplayOverride (false, qValue);
    secondaryReadout->setNormalised (qValue);
    morphReadout->setActive (morphWheel->isMouseOverOrDragging (true) || morphReadout->isMouseOverOrDragging (true));
    secondaryReadout->setActive (secondaryWheel->isMouseOverOrDragging (true) || secondaryReadout->isMouseOverOrDragging (true));
}
