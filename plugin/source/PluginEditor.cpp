#include "ui/DockBays.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include "TrenchBodyRoster.h"
#include <map>
#include <string>
using namespace trench::ui;
namespace
{

constexpr float kBayLeft  = 35.5f;
constexpr float kBayRight = 249.0f;

constexpr int kRailY = 312, kRailH = 13, kBayY = 330, kBayRowH = 32;
constexpr int kGainX = 35, kGainW = 116, kMoveX = 150, kMoveW = 92;
constexpr int kFxChipH = 17;
#if TRENCH_GOD_MODE || defined (TRENCH_PLAYER_DIAGNOSTICS)
juce::File layoutWatchFile()
{
   #if TRENCH_GOD_MODE
    return trench::ui::godLayoutFile();
   #else
    return trench::uiLayoutFile();
   #endif
}

bool readLayoutFile (const juce::File& f, trench::UiLayout& out)
{
    const auto text = f.loadFileAsString();
    if (juce::JSON::parse (text).getDynamicObject() == nullptr)
        return false;
    out = trench::UiLayout::fromJson (text);
    return true;
}
#endif
struct FuncGenVerdicts
{
    static juce::File file()
    {
        return juce::File (TRENCH_TABLE_STITCH_ROOT)
                   .getChildFile ("filters").getChildFile ("funcgen_verdicts.json");
    }
    void load()
    {
        entries.clear();
        const auto parsed = juce::JSON::parse (file());
        if (auto* obj = parsed.getDynamicObject())
            for (const auto& prop : obj->getProperties())
                entries[prop.name.toString().toStdString()] = prop.value.toString().toStdString();
    }
    void set (const juce::String& name, const juce::String& verdict)
    {
        entries[name.toStdString()] = verdict.toStdString();
        auto* obj = new juce::DynamicObject();
        for (const auto& e : entries)
            obj->setProperty (juce::Identifier (juce::String (e.first)), juce::String (e.second));
        const auto f = file();
        f.getParentDirectory().createDirectory();
        f.replaceWithText (juce::JSON::toString (juce::var (obj)));
    }
    int count (const char* verdict) const
    {
        int n = 0;
        for (int i = 0; i < trench::kNumFuncGenPatterns; ++i)
        {
            const auto it = entries.find (std::string (trench::kFuncGenPatterns[i].name));
            if (it != entries.end() && it->second == verdict)
                ++n;
        }
        return n;
    }
    std::map<std::string, std::string> entries;
};
FuncGenVerdicts& funcGenVerdicts()
{
    static FuncGenVerdicts v;
    return v;
}
int movePresetIndex (juce::AudioProcessorValueTreeState& apvts)
{
    if (auto* v = apvts.getRawParameterValue (ParamID::movePreset))
        return juce::roundToInt (v->load());
    return 0;
}

void writeLivePhrase (const trench::ui::MovementSketch& sketch)
{
    static juce::int64 stampMs = juce::Time::currentTimeMillis();
    juce::String json ("{\"values\":[");
    for (int i = 0; i < sketch.steps(); ++i)
    {
        if (i > 0)
            json << ",";
        json << juce::String (sketch.values()[i], 6);
    }
    json << "],\"direction\":" << sketch.direction()
         << ",\"smooth\":" << (sketch.smooth() ? "true" : "false")
         << ",\"stepBeats\":" << juce::String (1.0 / (double) sketch.stepsPerBeat(), 6) << "}";
    const auto f = juce::File (TRENCH_TABLE_STITCH_ROOT)
                       .getChildFile ("filters").getChildFile ("phrase_live.json");
    f.getParentDirectory().createDirectory();
    f.replaceWithText (json);
    stampMs += 1000;
    f.setLastModificationTime (juce::Time (stampMs));
}
juce::File saveSketchTemplate (const trench::ui::MovementSketch& sketch)
{
    const auto dir = juce::File (TRENCH_TABLE_STITCH_ROOT).getChildFile ("rhythms");
    dir.createDirectory();
    juce::File out;
    for (int i = 1; i <= 99 && out == juce::File(); ++i)
    {
        const auto f = dir.getChildFile ("Sketch " + juce::String (i).paddedLeft ('0', 2) + ".xml");
        if (! f.existsAsFile())
            out = f;
    }
    if (out == juce::File())
        return {};
    juce::XmlElement xml ("template");
    xml.setAttribute ("module", "Function Generator");
    xml.setAttribute ("name", out.getFileNameWithoutExtension());
    const auto field = [&xml] (const char* tag, const char* type, const juce::String& text)
    {
        auto* e = xml.createNewChildElement (tag);
        e->setAttribute ("type", type);
        e->addTextElement (text);
    };
    field ("bpm", "long", "1");
    field ("rate", "float", juce::String (sketch.stepsPerBeat()));
    field ("sync", "long", "0");
    field ("smooth", "long", sketch.smooth() ? "1" : "0");
    field ("direction", "long", juce::String (sketch.direction()));
    field ("length", "long", juce::String (sketch.steps() - 1));
    for (int i = 0; i < trench::ui::MovementSketch::kCells; ++i)
    {
        auto* e = xml.createNewChildElement ("value");
        e->setAttribute ("index", i);
        e->setAttribute ("type", "float");
        e->addTextElement (juce::String (i < sketch.steps() ? sketch.values()[i] : 0.0f, 6));
    }
    for (int i = 0; i < trench::ui::MovementSketch::kCells; ++i)
    {
        auto* e = xml.createNewChildElement ("trigger");
        e->setAttribute ("index", i);
        e->setAttribute ("type", "long");
        e->addTextElement ("0");
    }
    return out.replaceWithText (xml.toString()) ? out : juce::File();
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
    reloadLayoutFromDisk();

    auto panel = juce::ImageCache::getFromMemory (BinaryData::df2_panel_beige_png,
                                                  BinaryData::df2_panel_beige_pngSize);
    auto strip = juce::ImageCache::getFromMemory (BinaryData::trench_roller_strip_png,
                                                  BinaryData::trench_roller_strip_pngSize);
    faceplate    = std::make_unique<FaceplateView> (panel, theme);
    faceplate->setBufferedToImage (true);

    graph        = std::make_unique<GraphDisplay> (theme, processor.apvts, juce::String());
    keySnapBox = std::make_unique<KeySnapBox> (processor.apvts, theme);
    keySnapBox->setSuggestionProviders (
        [this] { return processor.getDetectedKeyForUi(); },
        [this] { return processor.getDetectedAltKeyForUi(); });
    keySnapBox->setListeningProvider ([this]
    {
        return juce::jmax (processor.getInputMeterLeftForUi().load (std::memory_order_relaxed),
                           processor.getInputMeterRightForUi().load (std::memory_order_relaxed))
               > 0.0015f;
    });
    typeSelector = std::make_unique<TypeSelectorView> (processor.apvts, theme);
    const auto runSeed = [this]
    {
        if (processor.seedCurrentBody())
        {
            graph->playSeedPulse();
            graph->announce ("SIBLING SEEDED");
        }
    };
    typeSelector->onSeed       = runSeed;
    typeSelector->onExportBody = [this] { processor.exportCurrentBody(); };
    typeSelector->onAnnounce   = [this] (const juce::String& s) { graph->announce (s); graph->setBodyName (s); };

    bodyBrowser = std::make_unique<BodyBrowser> (theme);
    addChildComponent (*bodyBrowser);
    bodyBrowser->onPreview = [this] (int index) { processor.previewBodyForUi (index); };
    bodyBrowser->onCommit  = [this] (int index) { typeSelector->setSelectedBody (index); };
    bodyBrowser->onRestore = [this] (int index) { processor.restoreBodyForUi (index); };
    typeSelector->onOpenBrowser = [this] (int current)
    {
        bodyBrowser->open (current, getLocalBounds());
    };
    morphWheel   = std::make_unique<WheelControl> (processor.apvts, ParamID::morph, strip, theme);
    secondaryWheel = std::make_unique<WheelControl> (processor.apvts, ParamID::q, strip, theme);
    morphReadout = std::make_unique<ValueReadout> ("morphReadout", theme);
    secondaryReadout = std::make_unique<ValueReadout> ("qReadout", theme);
    morphReadout->bindParameter (processor.apvts.getParameter (ParamID::morph));
    secondaryReadout->bindParameter (processor.apvts.getParameter (ParamID::q));

    sectionRail = std::make_unique<SectionRail> (theme);
    preampKnob = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::preamp, "Input");
    chewKnob   = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::chew,   "Bite");
    slamKnob   = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::slamDrive, "Output");
    lowKnob    = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::lowKeep, "Low");
    followKnob = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::envAmount, "Follow");
    for (auto* k : { preampKnob.get(), chewKnob.get(), followKnob.get() })
        k->setScale (0.75f);
    autoTrim   = std::make_unique<trench::ui::AutoTrimMark> (theme);
    {
        auto bay = std::make_unique<trench::ui::MovementBay> (theme);
        bay->onStep     = [this] (int dir) { if (movementChip->onStep) movementChip->onStep (dir); };
        bay->onOpenMenu = [this] { if (movementChip->onOpenMenu) movementChip->onOpenMenu(); };
        movementBay = std::move (bay);
    }

    movementChip = std::make_unique<MovementChip> (theme);
    movementChip->onStep = [this] (int dir)
    {
        auto* p = processor.apvts.getParameter (ParamID::movePreset);
        if (p == nullptr)
            return;

        const int n = trench::kNumFuncGenPatterns;
        const int now = movePresetIndex (processor.apvts);
        const int next = (now >= 1 && now <= n) ? ((now - 1 + dir) % n + n) % n + 1
                                                : (dir > 0 ? 1 : n);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) next));
        p->endChangeGesture();
    };
    movementChip->onOpenMenu = [this]
    {
        auto* p = processor.apvts.getParameter (ParamID::movePreset);
        if (p == nullptr)
            return;
        const auto names = p->getAllValueStrings();
        const int cur = movePresetIndex (processor.apvts);

        const bool liveReady = processor.hasLivePhraseForUi();
        juce::PopupMenu m;
        m.setLookAndFeel (&movementMenuLnF);
        m.addItem (1, "OFF", true, cur == 0);
        m.addSeparator();
        for (int i = 1; i < names.size(); ++i)
            m.addItem (i + 2, names[i], names[i] == "LIVE" ? liveReady : true, cur == i);
        juce::Component::SafePointer<PluginEditor> self (this);
        movementChip->setMenuOpen (true);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (movementChip.get()),
                         [self, p] (int id)
                         {
                             if (self != nullptr)
                                 self->movementChip->setMenuOpen (false);
                             if (self == nullptr || id <= 0)
                                 return;
                             p->beginChangeGesture();
                             p->setValueNotifyingHost (
                                 p->convertTo0to1 (id == 1 ? 0.0f : (float) (id - 2)));
                             p->endChangeGesture();
                         });
    };
    sectionRail->onToggleSection = [this] (int s)
    {

        openSection ^= (1 << s);
        applySectionVisibility();
    };
    onboarding = std::make_unique<Onboarding> (theme);

    const auto setBodyIndex = [this] (int idx)
    {
        if (auto* b = processor.apvts.getParameter (ParamID::body))
            b->setValueNotifyingHost (b->convertTo0to1 ((float) idx));
    };
    const auto loadTourDemoBody = [this, setBodyIndex]
    {

        bodyBeforeTour = juce::roundToInt (
            processor.apvts.getRawParameterValue (ParamID::body)->load());
        int n = 0;
        trench::bodyRoster (n);

        for (int i = 0; i < n; ++i)
            if (trench::bodyDisplayName (i) == "2-Pole Lowpass")
                return setBodyIndex (i);
        setBodyIndex (juce::jmin (1, n - 1));
    };
    onboarding->onDismiss = [this, setBodyIndex]
    {
        onboarding->setVisible (false);
        setBodyIndex (bodyBeforeTour >= 0 ? bodyBeforeTour : trench::kDefaultBodyIndex);
        bodyBeforeTour = -1;
    };

    onboarding->onDemoMorph = [this] (float phase)
    {
        auto* p = processor.apvts.getParameter (ParamID::morph);
        if (p == nullptr)
            return;
        if (onboardingDemoStart < 0.0f)
        {
            onboardingDemoStart = p->getValue();
            p->beginChangeGesture();
        }
        const float far = onboardingDemoStart < 0.5f ? 1.0f : 0.0f;
        const float v = onboardingDemoStart
                      + (far - onboardingDemoStart)
                            * std::sin (juce::MathConstants<float>::pi * phase);
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    };
    onboarding->onDemoEnd = [this]
    {
        if (auto* p = processor.apvts.getParameter (ParamID::morph);
            p != nullptr && onboardingDemoStart >= 0.0f)
        {
            p->setValueNotifyingHost (onboardingDemoStart);
            p->endChangeGesture();
        }
        onboardingDemoStart = -1.0f;
    };
    onboardingReplayHotspot.onClick = [this, loadTourDemoBody]
    {
        loadTourDemoBody();
        onboarding->replay();
    };
    onboardingReplayHotspot.onHover = [this] (bool lit)
    {
        if (labels != nullptr) labels->setBrandLit (lit);
    };
    labels       = std::make_unique<LabelsLayer> (theme);

    labels->setRailLabels ("MORPH (%)", "Q (%)");
    decalsLayer  = std::make_unique<DecalsLayer> (theme);
    decalsLayer->setBufferedToImage (true);
    addAndMakeVisible (*faceplate);
    addAndMakeVisible (*graph);

    addAndMakeVisible (*movementChip);
    addAndMakeVisible (*followKnob);
    addAndMakeVisible (*autoTrim);
    addAndMakeVisible (*movementBay);
    addAndMakeVisible (*typeSelector);
    addAndMakeVisible (*morphWheel);
    addAndMakeVisible (*secondaryWheel);

    faceplate->toBack();
    morphWheel->toBack();
    secondaryWheel->toBack();
    addAndMakeVisible (*morphReadout);
    addAndMakeVisible (*secondaryReadout);
    addAndMakeVisible (*keySnapBox);
    addAndMakeVisible (*sectionRail);

    addChildComponent (*preampKnob);
    addChildComponent (*chewKnob);
    addChildComponent (*slamKnob);
    addChildComponent (*lowKnob);
    applySectionVisibility();
    addChildComponent (*onboarding);

    if (trench::ui::Onboarding::shouldShow())
    {
        loadTourDemoBody();
        onboarding->setVisible (true);
        onboarding->toFront (false);
    }
    addAndMakeVisible (*labels);
    addAndMakeVisible (*decalsLayer);

    devPanel = std::make_unique<trench::ui::DevBypassPanel> (theme);
    devPanel->onChange = [this] (const trench::ui::DevBypassPanel::Bypass& b)
    {
        processor.dspBridge.setBypass (b);
    };
    addChildComponent (*devPanel);

    devPanel->onOpenChanged = [this] (bool) { updateEditorSize(); };
    devPanel->onNext = [this]
    {
        auto* p = processor.apvts.getParameter (ParamID::movePreset);
        if (p == nullptr)
            return;
        const int now = movePresetIndex (processor.apvts);
        const int next = (now >= 1 && now < trench::kNumFuncGenPatterns) ? now + 1 : 1;
        p->setValueNotifyingHost (p->convertTo0to1 ((float) next));
    };
    const auto recordVerdict = [this] (const char* verdict)
    {
        const auto name = bankPatternName (movePresetIndex (processor.apvts));
        if (name.isEmpty())
            return;
        funcGenVerdicts().set (name, verdict);
        if (devPanel->onNext) devPanel->onNext();
    };
    devPanel->onKeep = [recordVerdict] { recordVerdict ("keep"); };
    devPanel->onKill = [recordVerdict] { recordVerdict ("kill"); };
    movementSketch = std::make_unique<trench::ui::MovementSketch>();
    addChildComponent (*movementSketch);
    movementSketch->onChanged = [this]
    {
        writeLivePhrase (*movementSketch);

        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                          processor.apvts.getParameter (ParamID::movePreset)))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (p->choices.size() - 1)));
    };
    movementSketch->onSave = [this]
    {
        const auto saved = saveSketchTemplate (*movementSketch);
        if (saved != juce::File())
            movementSketch->setStatus ("saved " + saved.getFileNameWithoutExtension());
    };
    movementSketch->onClose = [this] { movementSketch->setVisible (false); };
    devPanel->onSketch = [this]
    {
        movementSketch->setVisible (! movementSketch->isVisible());
        if (movementSketch->isVisible())
            movementSketch->toFront (true);
    };
    onboardingReplayHotspot.onSecretClick = [this]
    {
        funcGenVerdicts().load();
        devPanel->open (processor.dspBridge.getBypass());
    };

    addAndMakeVisible (onboardingReplayHotspot);
    setResizable (false, false);
    updateEditorSize();
    setWantsKeyboardFocus (false);
#if TRENCH_GOD_MODE
    godMode = std::make_unique<trench::ui::GodModeOverlay> (currentLayout);
    godMode->onLayoutChanged = [this] { layoutComponents(); repaint(); };
    addChildComponent (*godMode);

    setWantsKeyboardFocus (true);
    addKeyListener (godMode.get());
    startTimer (250);
#endif
    vblank = std::make_unique<juce::VBlankAttachment> (this, [this] { onFrame(); });
   #ifdef TRENCH_PLAYER_DIAGNOSTICS
    startTimer (350);
    resized();
   #endif
}
void PluginEditor::showOnboardingStep (int step)
{
    onboarding->replay (step);
}
PluginEditor::~PluginEditor()
{

    vblank.reset();
    stopTimer();

    if (onboarding != nullptr && onboarding->onDemoEnd != nullptr)
        onboarding->onDemoEnd();
#if TRENCH_GOD_MODE
    removeKeyListener (godMode.get());
#endif
    processor.setEditorOpen (false);
}
void PluginEditor::reloadLayoutFromDisk()
{
   #if TRENCH_GOD_MODE || defined (TRENCH_PLAYER_DIAGNOSTICS)
    auto f = layoutWatchFile();
    if (f.existsAsFile())
    {
        readLayoutFile (f, currentLayout);
    }
    else
    {
        f.getParentDirectory().createDirectory();
        f.replaceWithText (currentLayout.toJson());
    }
    layoutMtime = f.getLastModificationTime();
   #endif
}
void PluginEditor::timerCallback()
{
   #if TRENCH_GOD_MODE || defined (TRENCH_PLAYER_DIAGNOSTICS)

    auto f = layoutWatchFile();
    if (! f.existsAsFile())
        return;
    const auto t = f.getLastModificationTime();
    if (t == layoutMtime)
        return;
    if (! readLayoutFile (f, currentLayout))
        return;
    layoutMtime = t;
    layoutComponents();
    repaint();
   #endif
}
void PluginEditor::resized()
{
    layoutComponents();
}
void PluginEditor::layoutComponents()
{
    trench::ui::uiFontFamily() = currentLayout.string ("fontFamily", trench::ui::kUiFontName);
    trench::ui::uiEmphasisFontFamily() = currentLayout.string ("fontFamilyEmphasis",
                                                                trench::ui::kUiEmphasisFontName);
    trench::ui::uiBoldEnabled() = currentLayout.param ("fontBold", 0.0) > 0.5;
    const juce::Rectangle<int> base { 0, 0, kEditorWidth, kEditorHeight };
    faceplate->setBounds (base);
    labels->setBounds (base);
    labels->toFront (false);
    decalsLayer->toFront (false);
    const auto rectOf = [this] (const char* id) { return theme.rect (id).getSmallestIntegerContainer(); };
    graph->setBounds (rectOf ("spectrumGrid"));
    {
        const auto glass = rectOf ("spectrumGrid");
        movementChip->setBounds (glass.getX() + 10,
                                 glass.getBottom() - 12 - MovementChip::kHeight,
                                 movementChip->preferredWidth(), MovementChip::kHeight);
        sectionRail->setBounds (kGainX, kRailY, sectionRail->preferredWidth(), kRailH);
        sectionRail->toFront (false);
    }
    {

        const auto sel = rectOf ("typeSelector");
        const auto brand = theme.rect ("brandLabel");
        keySnapBox->setBounds (sel.getRight() - 138,
                               juce::roundToInt (brand.getCentreY() - 11.0f), 128, 22);
    }
    typeSelector->setBounds (rectOf ("typeSelector"));
    morphWheel->setBounds (rectOf ("morphWheel"));
    secondaryWheel->setBounds (rectOf ("qWheel"));
    morphReadout->setBounds (rectOf ("morphReadout"));
    secondaryReadout->setBounds (rectOf ("qReadout"));
    {
        const auto laneAt = [&] (int i) { return juce::Rectangle<int> (kGainX, kBayY + i * kBayRowH, kGainW, kBayRowH); };
        preampKnob->setBounds (laneAt (0));
        chewKnob->setBounds   (laneAt (1));
        slamKnob->setBounds   (laneAt (2));
        lowKnob->setBounds    (laneAt (2));
        autoTrim->setBounds   (kGainX + 6, kBayY + 2 * kBayRowH + 2, kGainW, 12);
        movementBay->setBounds (kMoveX, kBayY + 4, kMoveW, 46);
        followKnob->setBounds  (kMoveX - 6, kBayY + 52, kMoveW + 6, kBayRowH);

    }
    onboarding->setBounds (base);
    onboardingReplayHotspot.setBounds (rectOf ("brandLabel"));
    decalsLayer->setBounds (base);
    if (devPanel != nullptr)
    {
        devPanel->setBounds (kEditorWidth, 0,
                             trench::ui::DevBypassPanel::kWidth, kEditorHeight);
        devPanel->toFront (false);
    }
    if (movementSketch != nullptr)
    {
        movementSketch->setBounds (8, kEditorHeight - trench::ui::MovementSketch::kHeight - 8,
                                   kEditorWidth - 16, trench::ui::MovementSketch::kHeight);
        if (movementSketch->isVisible())
            movementSketch->toFront (false);
    }
    const auto fade = [this] (juce::Component* c, const char* id) { if (c) c->setAlpha (theme.opacity (id)); };
    fade (graph.get(),        "spectrumGrid");
    fade (typeSelector.get(), "typeSelector");
    fade (morphWheel.get(),   "morphWheel");
    fade (secondaryWheel.get(), "qWheel");
    fade (morphReadout.get(), "morphReadout");
    fade (secondaryReadout.get(), "qReadout");
#if TRENCH_GOD_MODE
    if (godMode != nullptr)
    {
        godMode->setBounds (base);
        godMode->toFront (false);
    }
#endif
}
void PluginEditor::applySectionVisibility()
{
    using trench::ui::SectionRail;
    const bool gain = (openSection & (1 << SectionRail::kDrive)) != 0;
    const bool move = (openSection & (1 << SectionRail::kMovement)) != 0;
    preampKnob->setVisible (gain);
    chewKnob->setVisible (gain);
    slamKnob->setVisible (false);
    lowKnob->setVisible (false);
    autoTrim->setVisible (gain);
    movementBay->setVisible (move);
    followKnob->setVisible (move);
    sectionRail->setOpenMask (openSection);
    updateEditorSize();
}
void PluginEditor::updateEditorSize()
{
    using trench::ui::SectionRail;

    setSize (kEditorWidth + (devPanel != nullptr && devPanel->isVisible()
                                 ? trench::ui::DevBypassPanel::kWidth : 0),
             kEditorHeight);
}
void PluginEditor::onFrame()
{

    keySnapBox->refreshSuggestion();
    if (devPanel != nullptr && devPanel->isVisible())
    {
        devPanel->setAgcReductionDb (processor.getAgcReductionDbForUi());
        const int kept   = funcGenVerdicts().count ("keep");
        const int killed = funcGenVerdicts().count ("kill");
        devPanel->setCuration (bankPatternName (movePresetIndex (processor.apvts)),
                               kept, killed,
                               trench::kNumFuncGenPatterns - kept - killed);
    }

    {
        const int preset = movePresetIndex (processor.apvts);
        const auto bank = bankPatternName (preset);
        const juce::String moveName =
            preset <= 0            ? juce::String ("MOVEMENT")
          : bank.isNotEmpty()      ? bank
          : preset == trench::Movement::kGrowlIndex ? juce::String ("GROWL")
                                                    : juce::String ("LIVE");
        movementChip->setState (moveName, processor.isMorphModulatedForUi());
        juce::String rate;
        if (auto* div = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (ParamID::moveDivision)))
            rate = div->getCurrentChoiceName();
        static_cast<trench::ui::MovementBay*> (movementBay.get())->setState (preset <= 0 ? juce::String ("OFF") : moveName, rate, processor.isMorphModulatedForUi());
        sectionRail->setStatus (preset <= 0 ? juce::String ("OFF") : moveName + (rate.isNotEmpty() ? "  " + rate : juce::String()));
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
    const float baseMorph = morphMoving ? processor.getEffectiveMorphForUi()
                                        : read (ParamID::morph);

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
            graph->updateFromCoeffs (coeffs, boost,
                                     processor.getSampleRate() > 0.0
                                         ? processor.getSampleRate()
                                         : 44'100.0);
        }
    }

    const bool morphHandDown = morphWheel->isMouseButtonDown (true)
                            || morphReadout->isMouseButtonDown (true);
    const bool moving = processor.isMorphModulatedForUi() && ! morphHandDown;
    const float morphValue = moving ? processor.getEffectiveMorphForUi() : read (ParamID::morph);
    morphWheel->setDisplayOverride (moving, morphValue);
    morphReadout->setNormalised (morphValue);
    const float qValue = read (ParamID::q);
    secondaryWheel->setDisplayOverride (false, qValue);
    secondaryReadout->setNormalised (qValue);
    const bool morphActive = morphWheel->isMouseOverOrDragging (true) || morphReadout->isMouseOverOrDragging (true);
    const bool secondaryActive = secondaryWheel->isMouseOverOrDragging (true) || secondaryReadout->isMouseOverOrDragging (true);
    morphReadout->setActive (morphActive);
    secondaryReadout->setActive (secondaryActive);
}
