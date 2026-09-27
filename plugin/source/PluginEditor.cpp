#include "PluginEditor.h"
#include "BodyAxes.h"
#include "parameters/CurveMap.h"
#include "ui/Onboarding.h"
#include <cstdlib>
#include "BinaryData.h"
#include "TrenchBodyRoster.h"
static juce::PropertiesFile::Options trenchSettingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "TRENCH";
    o.filenameSuffix = "settings";
    o.folderName = "Signal Methods";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}
using namespace trench::ui;
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    processor.setEditorOpen (true);
    uiFontFamily() = layout.string ("fontFamily", kUiFontName);
    uiEmphasisFontFamily() = layout.string ("fontFamilyEmphasis", kUiEmphasisFontName);
    uiBoldEnabled() = layout.param ("fontBold", 0.0) > 0.5;
    auto panel = juce::ImageCache::getFromMemory (BinaryData::trench_plate_sage_png,
                                                  BinaryData::trench_plate_sage_pngSize);
    auto strip = juce::ImageCache::getFromMemory (BinaryData::trench_ss3_strip_png,
                                                  BinaryData::trench_ss3_strip_pngSize);
#if TRENCH_DEV_PANEL
    if (const char* override = std::getenv ("TRENCH_WHEEL_STRIP"))
        strip = juce::ImageFileFormat::loadFrom (juce::File (juce::String::fromUTF8 (override)));
#endif
    if (const char* override = std::getenv ("TRENCH_WHEEL_STRIP"))
        strip = juce::ImageFileFormat::loadFrom (juce::File (juce::String::fromUTF8 (override)));
    strip = WheelControl::tintLamp (strip, theme.rollerIllumination());
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
    typeSelector->onOpenBrowser = [this] (int current) { bodyBrowser->open (current, face.getLocalBounds()); };
    morphWheel = std::make_unique<WheelControl> (processor.apvts, ParamID::morph, strip, theme);
    secondaryWheel = std::make_unique<WheelControl> (processor.apvts, ParamID::q, strip, theme);
    morphReadout = std::make_unique<ValueReadout> ("morphReadout", theme);
    secondaryReadout = std::make_unique<ValueReadout> ("qReadout", theme);
    morphReadout->bindParameter (processor.apvts.getParameter (ParamID::morph));
    secondaryReadout->bindParameter (processor.apvts.getParameter (ParamID::q));
    morphWheel->onGestureStart = [this] { processor.holdMorph (true); };
    morphWheel->onGestureEnd = [this] { processor.holdMorph (false); };
    morphReadout->onHold = [this] (bool hold) { processor.holdMorph (hold); };
    modulationChip = std::make_unique<ModulationChip> (processor.apvts, theme);
    modulationChip->onRestart = [this] { processor.restartMovement(); };
    modulationChip->customName = [this] { return processor.userMotion.get().name; };
    modulationChip->savedNames = []
    {
        juce::StringArray names;
        for (const auto& motion : trench::MotionLibrary::load())
            names.add (motion.name);
        return names;
    };
    modulationChip->onSaved = [this] (int index)
    {
        const auto library = trench::MotionLibrary::load();
        if (index >= 0 && index < (int) library.size())
            processor.applyUserMotion (library[(size_t) index], true);
    };
    motionBrowser = std::make_unique<BodyBrowser> (theme);
    motionBrowser->setTitle ("Movement");
    motionBrowser->rowSource = [this] { return modulationChip->browserRows(); };
    motionBrowser->onCommit = [this] (int id) { modulationChip->commitBrowserRow (id); };
    modulationChip->onEdit = [this] { motionBrowser->open (-1, face.getLocalBounds()); };
    keySnapBox = std::make_unique<KeySnapBox> (processor.apvts, theme);
    keySnapBox->setSuggestionProviders ([this] { return processor.getDetectedKeyForUi(); });
    keySnapBox->setListeningProvider ([this]
    {
        return juce::jmax (processor.getInputMeterLeftForUi().load (std::memory_order_relaxed),
                           processor.getInputMeterRightForUi().load (std::memory_order_relaxed))
               > 0.0015f;
    });
    inputKnob = std::make_unique<DeskKnob> (processor.apvts, theme, ParamID::preamp, "INPUT");
    outputKnob = std::make_unique<DeskKnob> (processor.apvts, theme, ParamID::output, "OUTPUT");
    inputKnob->setLegendVisible (false);
    slamButton = std::make_unique<SlamButton> (processor.apvts, theme);
    outputKnob->setLegendVisible (false);
    inputReadout = std::make_unique<ValueReadout> ("inputReadout", theme);
    outputReadout = std::make_unique<ValueReadout> ("outputReadout", theme);
    inputReadout->bindParameter (processor.apvts.getParameter (ParamID::preamp));
    outputReadout->bindParameter (processor.apvts.getParameter (ParamID::output));
    labels = std::make_unique<LabelsLayer> (theme);
    labels->setRailLabels ("MORPH", "Q");
    face.setComponentID ("face");
    face.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (face);
    face.addAndMakeVisible (*faceplate);
    face.addAndMakeVisible (*morphWheel);
    face.addAndMakeVisible (*secondaryWheel);
    face.addAndMakeVisible (*graph);
    face.addAndMakeVisible (*typeSelector);
    face.addAndMakeVisible (*morphReadout);
    face.addAndMakeVisible (*secondaryReadout);
    face.addAndMakeVisible (*labels);
    face.addAndMakeVisible (*modulationChip);
    face.addAndMakeVisible (*keySnapBox);
    face.addAndMakeVisible (*inputKnob);
    face.addAndMakeVisible (*outputKnob);
    face.addAndMakeVisible (*slamButton);
    face.addAndMakeVisible (*inputReadout);
    face.addAndMakeVisible (*outputReadout);
    face.addChildComponent (*bodyBrowser);
    face.addChildComponent (*motionBrowser);
    onboarding = std::make_unique<Onboarding> (theme);
    onboarding->onComplete = [this]
    {
        markOnboardingSeen();
        onboarding->setVisible (false);
    };
    face.addChildComponent (*onboarding);
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
    {
        juce::PropertiesFile file (trenchSettingsOptions());
        uiScale = juce::jlimit (1.0f, 2.0f, (float) file.getDoubleValue ("ui.scale", 1.0));
    }
    setSize (juce::roundToInt (kEditorWidth * uiScale), juce::roundToInt (kEditorHeight * uiScale));
#endif
    setWantsKeyboardFocus (false);
    startTimerHz (60);
    onFrame();
}
PluginEditor::~PluginEditor()
{
    stopTimer();
    processor.setEditorOpen (false);
}
void PluginEditor::resized()
{
    const juce::Rectangle<int> base { 0, 0, kEditorWidth, kEditorHeight };
    face.setBounds (base);
    face.setTransform (juce::AffineTransform::scale (uiScale));
    const auto rectOf = [this] (const char* id) { return theme.rect (id).getSmallestIntegerContainer(); };
    faceplate->setBounds (base);
    labels->setBounds (base);
    graph->setBounds (rectOf ("spectrumGrid"));
    typeSelector->setBounds (rectOf ("typeSelector"));
    morphWheel->setBounds (WheelControl::drumForHole (theme.rect ("morphWell")).getSmallestIntegerContainer());
    secondaryWheel->setBounds (WheelControl::drumForHole (theme.rect ("qWell")).getSmallestIntegerContainer());
    {
        const auto hole = theme.rect ("qWell");
        const auto value = theme.rect ("qReadout");
        const auto chip = juce::Rectangle<float> (hole.getX(), hole.getBottom() + 20.0f,
                                                  value.getRight() - hole.getX(), value.getHeight());
        modulationChip->setBounds (chip.getSmallestIntegerContainer());
        const float left = hole.getX() - 10.0f;
        const float right = theme.rect ("outputReadout").getRight() + 30.0f;
        const float top = chip.getBottom() + 12.0f;
        const float bottom = theme.rect ("outputReadout").getBottom() + 10.0f;
        faceplate->setRoomCaption ("GAIN");
        faceplate->setRoomFrame ({ left, top, right - left, bottom - top }, left + 10.0f, left + 46.0f);
    }
    {
        const auto key = rectOf ("keyBox");
        keySnapBox->setBounds (key.getX(), key.getCentreY() - 11, key.getWidth(), 22);
    }
    inputKnob->setBounds (rectOf ("inputKnob"));
    outputKnob->setBounds (rectOf ("outputKnob"));
    slamButton->setBounds (rectOf ("slamButton"));
    inputReadout->setBounds (rectOf ("inputReadout"));
    outputReadout->setBounds (rectOf ("outputReadout"));
    morphReadout->setBounds (rectOf ("morphReadout"));
    secondaryReadout->setBounds (rectOf ("qReadout"));
#if TRENCH_DEV_PANEL
    devPanel->setBounds (kEditorWidth, 0, kDevPanelWidth, kDevPanelHeight);
#endif
    onboarding->setBounds (base);
    onboarding->setTargets ({
        { modulationChip->getBounds(), "MOVE", "choose a movement, its rate and playback" },
    });



    onboarding->toFront (false);
}
void PluginEditor::setUiScale (float scale)
{
#if ! TRENCH_DEV_PANEL
    uiScale = juce::jlimit (1.0f, 2.0f, scale);
    juce::PropertiesFile file (trenchSettingsOptions());
    file.setValue ("ui.scale", (double) uiScale);
    file.saveIfNeeded();
    setSize (juce::roundToInt (kEditorWidth * uiScale), juce::roundToInt (kEditorHeight * uiScale));
    resized();
#else
    juce::ignoreUnused (scale);
#endif
}
void PluginEditor::mouseDown (const juce::MouseEvent& e)
{
#if ! TRENCH_DEV_PANEL
    if (! e.mods.isPopupMenu())
        return;
    juce::SharedResourcePointer<SelectorLookAndFeel> look;
    juce::PopupMenu menu;
    menu.setLookAndFeel (&*look);
    menu.addSectionHeader ("Size");
    for (const float s : { 1.0f, 1.5f, 2.0f })
        menu.addItem (juce::String (juce::roundToInt (s * 100.0f)) + "%", true, std::abs (uiScale - s) < 0.01f,
                      [safe = juce::Component::SafePointer<PluginEditor> (this), s] { if (safe != nullptr) safe->setUiScale (s); });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ e.getScreenX(), e.getScreenY(), 1, 1 }),
                        [look] (int) {});
#else
    juce::ignoreUnused (e);
#endif
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
    modulationChip->setActive (processor.isMorphModulatedForUi());
    {
        const auto names = trench::axisNamesForBody (processor.getLoadedBodyIndex());
        labels->setRailLabels (names.morph, names.q);
    }
    const auto read = [this] (const char* paramID)
    {
        if (auto* v = processor.apvts.getRawParameterValue (paramID))
            return juce::jlimit (0.0f, 1.0f, v->load());
        return 0.0f;
    };
    float coeffs[trench::kUiCoeffCount] = {};
    float boost = 1.0f;
    const bool morphMoving = processor.isMorphModulatedForUi();
    const double morphShown = morphFollower.advance (processor.getEffectiveMorphForUi(), juce::Time::getMillisecondCounterHiRes());
    const float baseMorph = morphMoving ? (float) morphShown
                                        : trench::curves::curveMap (trench::curves::Axis::morph, read (ParamID::morph));
    const float baseQ = trench::curves::curveMap (trench::curves::Axis::q, read (ParamID::q));
    const int bodyVersion = processor.bodyVersionForUi.load (std::memory_order_relaxed);
    const double probeRate = processor.getSampleRate();
    const double probeKeyRatio = (double) juce::jlimit (0, 24, (int) processor.apvts.getRawParameterValue (ParamID::keySnap)->load());
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
    keySnapBox->refreshSuggestion();
    inputReadout->setNormalised (processor.apvts.getParameter (ParamID::preamp)->getValue());
    outputReadout->setNormalised (processor.apvts.getParameter (ParamID::output)->getValue());
    inputReadout->setActive (inputKnob->isMouseOverOrDragging (true) || inputReadout->isMouseOverOrDragging (true));
    outputReadout->setActive (outputKnob->isMouseOverOrDragging (true) || outputReadout->isMouseOverOrDragging (true));
}
