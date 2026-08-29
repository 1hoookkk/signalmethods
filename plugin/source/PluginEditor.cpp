#include "PluginEditor.h"
#include "BinaryData.h"
#include "TrenchBodyRoster.h"
using namespace trench::ui;
namespace
{
int movePresetIndex (juce::AudioProcessorValueTreeState& apvts)
{
    if (auto* v = apvts.getRawParameterValue (ParamID::movePreset))
        return juce::roundToInt (v->load());
    return 0;
}
juce::String bankPatternName (int presetIndex)
{
    if (presetIndex >= 1 && presetIndex <= trench::kNumFuncGenPatterns)
        return trench::kFuncGenPatterns[presetIndex - 1].name;
    return {};
}
}
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
    auto strip = juce::ImageCache::getFromMemory (BinaryData::trench_roller_strip_png,
                                                  BinaryData::trench_roller_strip_pngSize);
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
    bodyBrowser->onCommit  = [this] (int index) { typeSelector->setSelectedBody (index); };
    bodyBrowser->onRestore = [this] (int index) { processor.restoreBodyForUi (index); };
    typeSelector->onOpenBrowser = [this] (int current) { bodyBrowser->open (current, getLocalBounds()); };
    morphWheel = std::make_unique<WheelControl> (processor.apvts, ParamID::morph, strip, theme);
    secondaryWheel = std::make_unique<WheelControl> (processor.apvts, ParamID::q, strip, theme);
    morphReadout = std::make_unique<ValueReadout> ("morphReadout", theme);
    secondaryReadout = std::make_unique<ValueReadout> ("qReadout", theme);
    morphReadout->bindParameter (processor.apvts.getParameter (ParamID::morph));
    secondaryReadout->bindParameter (processor.apvts.getParameter (ParamID::q));
    colorKnobs[0] = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::chew, "Color 1");
    colorKnobs[1] = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::envAmount, "Color 2");
    colorKnobs[2] = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::track, "Color 3");
    glassWords = std::make_unique<GlassWords> (theme);
    keyBox = std::make_unique<KeyBox> (processor.apvts, theme, movementMenuLnF);
    glassWords->onStep = [this] (int dir)
    {
        auto* prm = processor.apvts.getParameter (ParamID::movePreset);
        if (prm == nullptr)
            return;
        const int n = trench::kNumFuncGenPatterns;
        const int now = movePresetIndex (processor.apvts);
        const int next = (now >= 1 && now <= n) ? ((now - 1 + dir) % n + n) % n + 1
                                                : (dir > 0 ? 1 : n);
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) next));
        prm->endChangeGesture();
    };
    glassWords->onOpenMenu = [this]
    {
        auto* prm = processor.apvts.getParameter (ParamID::movePreset);
        if (prm == nullptr)
            return;
        const auto names = prm->getAllValueStrings();
        const int cur = movePresetIndex (processor.apvts);
        const bool liveReady = processor.hasLivePhraseForUi();
        juce::PopupMenu m;
        m.setLookAndFeel (&movementMenuLnF);
        m.addItem (1, "OFF", true, cur == 0);
        m.addSeparator();
        for (int i = 1; i < names.size(); ++i)
            m.addItem (i + 2, names[i], names[i] == "LIVE" ? liveReady : true, cur == i);
        juce::Component::SafePointer<PluginEditor> self (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (glassWords.get()),
                         [self, prm] (int id)
                         {
                             if (self == nullptr || id <= 0)
                                 return;
                             prm->beginChangeGesture();
                             prm->setValueNotifyingHost (prm->convertTo0to1 (id == 1 ? 0.0f : (float) (id - 2)));
                             prm->endChangeGesture();
                         });
    };
    labels = std::make_unique<LabelsLayer> (theme);
    labels->setRailLabels ("MORPH (%)", "Q (%)");
    addAndMakeVisible (*faceplate);
    addAndMakeVisible (*morphWheel);
    addAndMakeVisible (*secondaryWheel);
    addAndMakeVisible (*graph);
    addAndMakeVisible (*typeSelector);
    addAndMakeVisible (*morphReadout);
    addAndMakeVisible (*secondaryReadout);
    addAndMakeVisible (*glassWords);
    addAndMakeVisible (*keyBox);
    for (auto& k : colorKnobs) addAndMakeVisible (*k);
    addAndMakeVisible (*labels);
    addChildComponent (*bodyBrowser);
    setResizable (false, false);
    setSize (kEditorWidth, kEditorHeight);
    setWantsKeyboardFocus (false);
    vblank = std::make_unique<juce::VBlankAttachment> (this, [this] { onFrame(); });
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
    morphReadout->setBounds (rectOf ("morphReadout"));
    secondaryReadout->setBounds (rectOf ("qReadout"));
    {
        const auto glass = rectOf ("spectrumGrid");
        glassWords->setBounds (glass.getX() + 12, glass.getBottom() - 26, 120, 18);
    }
    keyBox->setBounds (rectOf ("keyBox"));
    {
        const auto row = rectOf ("colorRow");
        for (int i = 0; i < 3; ++i)
            colorKnobs[(size_t) i]->setBounds (row.getX() + row.getWidth() * i / 3, row.getY(), row.getWidth() / 3, row.getHeight());
    }
}
void PluginEditor::onFrame()
{
    {
        const int preset = movePresetIndex (processor.apvts);
        const auto bank = bankPatternName (preset);
        const juce::String moveName =
            preset <= 0            ? juce::String ("OFF")
          : bank.isNotEmpty()      ? bank
          : preset == trench::Movement::kGrowlIndex ? juce::String ("GROWL")
                                                    : juce::String ("LIVE");
        glassWords->setState (moveName, processor.isMorphModulatedForUi());
        const bool listening = juce::jmax (processor.getInputMeterLeftForUi().load (std::memory_order_relaxed),
                                           processor.getInputMeterRightForUi().load (std::memory_order_relaxed)) > 0.0015f;
        keyBox->setState (processor.getDetectedKeyForUi(), listening);
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
    const float baseMorph = morphMoving ? processor.getEffectiveMorphForUi() : read (ParamID::morph);
    const float baseQ = read (ParamID::q);
    const int bodyVersion = processor.bodyVersionForUi.load (std::memory_order_relaxed);
    const double probeRate = processor.getSampleRate();
    if (baseMorph != lastProbedMorph || baseQ != lastProbedQ
        || bodyVersion != lastProbedBodyVersion || probeRate != lastProbedRate)
    {
        if (processor.probeCurrentBodyForUi (baseMorph, baseQ, coeffs, boost))
        {
            lastProbedMorph = baseMorph;
            lastProbedQ = baseQ;
            lastProbedBodyVersion = bodyVersion;
            lastProbedRate = probeRate;
            graph->updateFromCoeffs (coeffs, boost, probeRate > 0.0 ? probeRate : 44'100.0);
        }
    }
    const bool morphHandDown = morphWheel->isMouseButtonDown (true) || morphReadout->isMouseButtonDown (true);
    const bool moving = morphMoving && ! morphHandDown;
    const float morphValue = moving ? processor.getEffectiveMorphForUi() : read (ParamID::morph);
    morphWheel->setDisplayOverride (moving, morphValue);
    morphReadout->setNormalised (morphValue);
    const float qValue = read (ParamID::q);
    secondaryWheel->setDisplayOverride (false, qValue);
    secondaryReadout->setNormalised (qValue);
    morphReadout->setActive (morphWheel->isMouseOverOrDragging (true) || morphReadout->isMouseOverOrDragging (true));
    secondaryReadout->setActive (secondaryWheel->isMouseOverOrDragging (true) || secondaryReadout->isMouseOverOrDragging (true));
}
