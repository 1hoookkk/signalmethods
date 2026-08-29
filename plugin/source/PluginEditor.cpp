#include "PluginEditor.h"
#include "BinaryData.h"
#include "TrenchBodyRoster.h"
#include <map>
#include <string>
using namespace trench::ui;
namespace
{
// The bay's fixed joinery (mock_sel comp, editor px): the selector bar seats in
// the break at the room frame's top-left, and each room is a carve on the SAME
// anchor. THE CARVE IS THE WHEEL COLUMN CONTINUED (verdict 2026-08-05 "too
// crammed"): its left and right edges are the MORPH/Q wheel + readout block's
// own edges - qWheel source x 114 and morphReadout source right 772, mapped
// through kPanelSourceWidth - so the room reads as the same column carried
// down the plate, not a narrow box beside it.
constexpr float kBayLeft  = 35.5f;    // == the DISPLAY's left edge (source x 110)
constexpr float kBayRight = 249.0f;   // the readout column's right edge (source 772)
// THE DRAWER IS TWO ROWS (Tyson 2026-08-28 "FX+ is a chip on the display
// glass. It should open 2 rows of knobs and params for gain and movement.
// Keep controls to an absolute minimum"): row 1 the four gain knobs as
// compact cells, row 2 the movement picker. No carve, no door word - the
// chip on the glass is the whole door, and the plate below the rows stays
// bare to the notch.
// THE DOCK (Tyson 2026-08-29): the instrument above never moves. Under Q
// sits one permanent rail - GAIN and MOVE - and beneath it two bays with
// fixed seats: GAIN left, MOVEMENT right. Neither, one, or both may be open;
// nothing else moves and the face never changes size.
constexpr int kRailY = 312, kRailH = 13, kBayY = 330, kBayRowH = 32;
constexpr int kGainX = 35, kGainW = 116, kMoveX = 150, kMoveW = 92;
namespace
{
// A printed state, not a control: the gain stage trims itself.
struct AutoTrimMark final : juce::Component
{
    explicit AutoTrimMark (const trench::ui::Theme& theme) : t (theme) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (t.modulationLamp());
        g.fillEllipse (b.getX() + 1.0f, b.getCentreY() - 2.5f, 5.0f, 5.0f);
        g.setFont (trench::ui::telemetryFont (8.5f, false));
        g.setColour (t.labelInk().withAlpha (0.70f));
        g.drawText ("AUTO TRIM", b.withTrimmedLeft (10.0f).toNearestInt(), juce::Justification::centredLeft, false);
    }
    trench::ui::Theme t;
};
// The MOVEMENT bay is preset-first: the phrase with its steppers, then the
// rate. Deeper parameters stay behind the phrase list.
struct MovementBay final : juce::Component
{
    explicit MovementBay (const trench::ui::Theme& theme) : t (theme) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void (int)> onStep;
    std::function<void()>     onOpenMenu;
    void setState (const juce::String& n, const juce::String& r, bool a)
    {
        if (n == name && r == rate && a == active) return;
        name = n; rate = r; active = a; repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto b = getLocalBounds().toFloat();
        if (e.position.y > 26.0f) return;
        if (e.position.x < 16.0f)                { if (onStep) onStep (-1); }
        else if (e.position.x > b.getRight() - 16.0f) { if (onStep) onStep (1); }
        else if (onOpenMenu) onOpenMenu();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (onStep && ! juce::approximatelyEqual (w.deltaY, 0.0f)) onStep (w.deltaY > 0 ? 1 : -1);
    }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto ink = t.labelInk();
        g.setFont (trench::ui::telemetryFont (8.0f, false));
        g.setColour (ink.withAlpha (0.55f));
        g.drawText ("PRESET", b.withHeight (10.0f).toNearestInt(), juce::Justification::centredLeft, false);
        const auto row = juce::Rectangle<float> (b.getX(), b.getY() + 11.0f, b.getWidth(), 16.0f);
        trench::ui::drawMutedBoneReadout (g, row, 3.0f, false, t);
        g.setColour (juce::Colour (0xff2a2722));
        juce::Path l, r;
        l.addTriangle (row.getX() + 9.0f, row.getCentreY() - 3.5f, row.getX() + 9.0f, row.getCentreY() + 3.5f, row.getX() + 4.5f, row.getCentreY());
        r.addTriangle (row.getRight() - 9.0f, row.getCentreY() - 3.5f, row.getRight() - 9.0f, row.getCentreY() + 3.5f, row.getRight() - 4.5f, row.getCentreY());
        g.fillPath (l); g.fillPath (r);
        g.setFont (trench::ui::displayFont (10.0f, false));
        g.drawText (name, row.reduced (12.0f, 0.0f).toNearestInt(), juce::Justification::centred, false);
        g.setFont (trench::ui::telemetryFont (8.0f, false));
        g.setColour (ink.withAlpha (0.55f));
        g.drawText ("RATE", juce::Rectangle<float> (b.getX(), b.getY() + 33.0f, 30.0f, 10.0f).toNearestInt(), juce::Justification::centredLeft, false);
        g.setColour (ink.withAlpha (0.85f));
        g.drawText (rate, juce::Rectangle<float> (b.getX() + 30.0f, b.getY() + 33.0f, b.getWidth() - 30.0f, 10.0f).toNearestInt(), juce::Justification::centredLeft, false);
    }
    trench::ui::Theme t;
    juce::String name { "OFF" }, rate;
    bool active = false;
};
}
constexpr int kFxChipH = 17;
#if TRENCH_GOD_MODE || defined (TRENCH_PLAYER_DIAGNOSTICS)
juce::File layoutWatchFile()
{
   #if TRENCH_GOD_MODE
    return trench::ui::godLayoutFile();   // the proof harness can redirect this
   #else
    return trench::uiLayoutFile();
   #endif
}
// A half-written save must never blank the face: only text that parses into a
// JSON object is allowed to replace the live layout.
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
// The drawn phrase leaves through the same channel the Workstation uses: one
// JSON file the processor polls by modification time, so the stamp must move
// forward on every write even when two strokes land inside the clock's tick.
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
    // The plate: the original beige. Linen tried 2026-07-30, rejected same
    // night ("too much on the eyes") - texture candidates live in git/evidence.
    auto panel = juce::ImageCache::getFromMemory (BinaryData::df2_panel_beige_png,
                                                  BinaryData::df2_panel_beige_pngSize);
    auto strip = juce::ImageCache::getFromMemory (BinaryData::trench_roller_strip_png,
                                                  BinaryData::trench_roller_strip_pngSize);
    faceplate    = std::make_unique<FaceplateView> (panel, theme);
    faceplate->setBufferedToImage (true);
    // SLAM lives on its own knob now; the glass is display-only (the hidden
    // screen-drag was "still ambiguous" - Tyson 2026-07-30).
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
    // Hovering or flicking through bodies AUDITIONS them; only a click writes
    // the parameter, so browsing never touches automation or undo.
    // Clicking BODY opens the face's own library, not a system menu.
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
    // THE DRAWER: FX+ on the glass opens the gain row and the movement row.
    // SOURCE (resample) stays retired.
    sectionRail = std::make_unique<SectionRail> (theme);
    preampKnob = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::preamp, "Input");
    chewKnob   = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::chew,   "Bite");
    slamKnob   = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::slamDrive, "Output");
    lowKnob    = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::lowKeep, "Low");
    followKnob = std::make_unique<BayKnob> (processor.apvts, theme, ParamID::envAmount, "Follow");
    for (auto* k : { preampKnob.get(), chewKnob.get(), followKnob.get() })
        k->setScale (0.75f);
    autoTrim   = std::make_unique<AutoTrimMark> (theme);
    {
        auto bay = std::make_unique<MovementBay> (theme);
        bay->onStep     = [this] (int dir) { if (movementChip->onStep) movementChip->onStep (dir); };
        bay->onOpenMenu = [this] { if (movementChip->onOpenMenu) movementChip->onOpenMenu(); };
        movementBay = std::move (bay);
    }
    // TRACK retired from the face (Tyson 2026-08-15): the pitch listener was a
    // detector-driven retuner the X3 never had, and E-mu's authored answer to
    // pitch-following is the cube's own third axis. The parameter stays for
    // old sessions; the engine wiring stays; the knob is gone. The THIRD-AXIS
    // control ("Transform 2") appears only when a 560-byte cube body loads —
    // there is no cube load path yet, so it is not built yet.
    // MOVEMENT rides row 2 of the drawer: the chip is still the whole
    // control - wheel to step and audition, click for the list.
    movementChip = std::make_unique<MovementChip> (theme);
    movementChip->onStep = [this] (int dir)
    {
        auto* p = processor.apvts.getParameter (ParamID::movePreset);
        if (p == nullptr)
            return;
        // Stepping walks the BANK only: GROWL and LIVE are picked off the
        // list, never landed on by wheeling past the end of the patterns.
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
        // LIVE is only real while a phrase is being fed in.
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
        // Picking the open door's word again shuts the drawer (2026-08-15).
        openSection ^= (1 << s);
        applySectionVisibility();
    };
    onboarding = std::make_unique<Onboarding> (theme);
    // The tour teaches over a REAL curve: it loads a demo body while it is up,
    // then lands on the shipping default.
    const auto setBodyIndex = [this] (int idx)
    {
        if (auto* b = processor.apvts.getParameter (ParamID::body))
            b->setValueNotifyingHost (b->convertTo0to1 ((float) idx));
    };
    const auto loadTourDemoBody = [this, setBodyIndex]
    {
        // The tour BORROWS a body; the user's choice comes back when it ends.
        bodyBeforeTour = juce::roundToInt (
            processor.apvts.getRawParameterValue (ParamID::body)->load());
        int n = 0;
        trench::bodyRoster (n);
        // "Morph LP X" wears its roster pretty-name "2-Pole Lowpass" now; the
        // old literal matched nothing and every tour ran on the index-1
        // fallback. Same body, found by its real name.
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
    // MORPH demo step: the tour sweeps the wheel to the far pose and back so the
    // travel is SEEN. The editor owns the gesture and restores the pose after.
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
    // Deadpan instrument labelling (2026-08-10): blunt, engineering-first —
    // the parentheses were UI politeness, not hardware.
    labels->setRailLabels ("MORPH (%)", "Q (%)");
    decalsLayer  = std::make_unique<DecalsLayer> (theme);
    decalsLayer->setBufferedToImage (true);
    addAndMakeVisible (*faceplate);
    addAndMakeVisible (*graph);
    // AFTER the graph, so the chip sits on the glass rather than under it. The
    // glass itself still takes no clicks; the chip is the only thing on it that
    // does.
    addAndMakeVisible (*movementChip);
    addAndMakeVisible (*followKnob);
    addAndMakeVisible (*autoTrim);
    addAndMakeVisible (*movementBay);
    addAndMakeVisible (*typeSelector);
    addAndMakeVisible (*morphWheel);
    addAndMakeVisible (*secondaryWheel);
    // The wheels live BEHIND the plate: the wells are cut out of the plate
    // art (alpha holes) and the wheel shows through, lip overlapping it —
    // seated in the cutout, spinning while embedded (Tyson 2026-08-07).
    // It is a flat-laying wheel, a pitchwheel on its side — NOT a drum.
    // BEHIND THE PLATE AGAIN (Tyson 2026-08-11: "make the wheel underneath the
    // plate asset"). punch_wheel_wells.py cuts the two openings out of the
    // plate art, so the drum shows through a real hole with the lip overlapping
    // it. Order matters and reads backwards: toBack() drops a child to index 0,
    // so the plate goes down FIRST and the wheels go under it after. Everything
    // else keeps its place above the plate - only the wheels move.
    // OVER THE PLATE (Tyson 2026-08-13: "render them over the plate"). Only the
    // faceplate dropped to the back; the wheels stayed above it, composited ON
    // the art instead of showing through the punched wells.
    // BEHIND AGAIN (Tyson 2026-08-14: "render it behind see if that fixes it").
    // The flat-pitchwheel overscan draws the tread WIDER than the slot, and
    // over the plate that spilled onto the lip ("slightly too far out").
    // Behind the plate, the punched hole windows the tread: the plate's own
    // edge cuts it and nothing can spill. toBack() reads backwards: plate
    // first, then each wheel drops under it.
    faceplate->toBack();
    morphWheel->toBack();
    secondaryWheel->toBack();
    addAndMakeVisible (*morphReadout);
    addAndMakeVisible (*secondaryReadout);
    addAndMakeVisible (*keySnapBox);   // KEY stays on the top bar
    addAndMakeVisible (*sectionRail);
    // Room contents: only the open room's lanes are on the plate.
    addChildComponent (*preampKnob);
    addChildComponent (*chewKnob);
    addChildComponent (*slamKnob);
    addChildComponent (*lowKnob);
    applySectionVisibility();
    addChildComponent (*onboarding);
    // First-run teach: shown for the first few openings, or until clicked away.
    if (trench::ui::Onboarding::shouldShow())
    {
        loadTourDemoBody();
        onboarding->setVisible (true);
        onboarding->toFront (false);
    }
    addAndMakeVisible (*labels);
    addAndMakeVisible (*decalsLayer);
    // The dev bypass desk. Its state lives on the BRIDGE, not here, so closing
    // and reopening the window does not quietly put a stage back.
    devPanel = std::make_unique<trench::ui::DevBypassPanel> (theme);
    devPanel->onChange = [this] (const trench::ui::DevBypassPanel::Bypass& b)
    {
        processor.dspBridge.setBypass (b);
    };
    addChildComponent (*devPanel);
    // The window grows to make room for the desk instead of covering the face.
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
        // LIVE is the last choice: drawing IS selecting it, so a stroke is heard
        // without a second gesture.
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
    // Click the TRENCH badge to replay the tour. No new faceplate furniture.
    addAndMakeVisible (onboardingReplayHotspot);
    setResizable (false, false);
    updateEditorSize();
    setWantsKeyboardFocus (false);
#if TRENCH_GOD_MODE
    godMode = std::make_unique<trench::ui::GodModeOverlay> (currentLayout);
    godMode->onLayoutChanged = [this] { layoutComponents(); repaint(); };
    addChildComponent (*godMode);
    // The toggle has to arrive even while the overlay is asleep, so the editor
    // listens for keys and hands every one of them to the overlay first.
    setWantsKeyboardFocus (true);
    addKeyListener (godMode.get());
    startTimer (250);   // the layout-file watch, armed whether or not the overlay is
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
    // FIRST, before anything else is torn down: kill the per-frame callback and
    // the timers. VBlankAttachment fires onFrame() on EVERY screen refresh and
    // captures `this`; members destruct in REVERSE declaration order, so the
    // components onFrame() touches are gone before the attachment is. One
    // refresh landing mid-teardown reads freed memory - that is the host
    // hanging on close.
    vblank.reset();
    stopTimer();

    // Closing the window mid-tour must not leave a host gesture open with MORPH
    // parked wherever the demo sweep happened to be.
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
    // HOT RELOAD: poll the layout file's modification time. Save the JSON in an
    // editor and the running face follows - no rebuild, no reopen. A file that
    // is mid-write simply fails to parse and is left for the next tick.
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
        // KEY perches ABOVE the BODY bar, on the BRAND's optical baseline -
        // its centre line is taken from the brand label's rect, not a magic
        // offset off the selector.
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
        // DEPTH retired 2026-08-10: travel is part of each preset's record.
        // TRACK retired 2026-08-15 (see the knob's construction site above).
    }
    onboarding->setBounds (base);   // full face: the tour spotlights each control
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
    // ONE HEIGHT (Tyson 2026-08-28 "dont make the ui cut like that"): the
    // face never shortens - shut just means bare plate under the wheels.
    setSize (kEditorWidth + (devPanel != nullptr && devPanel->isVisible()
                                 ? trench::ui::DevBypassPanel::kWidth : 0),
             kEditorHeight);
}
void PluginEditor::onFrame()
{
    // NO FADE, EVER (Tyson 2026-08-15 "No fucking fade"). The old law dimmed
    // MORPH, Q, KEY, BITE and the MOVEMENT side to 28% on the No-filter body
    // because they act on nothing there — honest, but with No filter as the
    // shipping default it made the FIRST face read as unplugged. The hardware
    // model wins instead: every control stays full-strength and fully
    // interactive, exactly like the knobs on an unpatched synth — they turn,
    // the identity body just gives them nothing to change. Output and the
    // preamp remain live around the filter path.
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
    // The chip on the glass reads the pattern and lights while it is running.
    // OFF has no pattern to name, so it wears the word MOVEMENT instead.
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
        static_cast<MovementBay*> (movementBay.get())->setState (preset <= 0 ? juce::String ("OFF") : moveName, rate, processor.isMorphModulatedForUi());
        sectionRail->setStatus (preset <= 0 ? juce::String ("OFF") : moveName + (rate.isNotEmpty() ? "  " + rate : juce::String()));
    }
    const auto read = [this] (const char* paramID)
    {
        if (auto* v = processor.apvts.getRawParameterValue (paramID))
            return juce::jlimit (0.0f, 1.0f, v->load());
        return 0.0f;
    };
    // The response curve draws from the EFFECTIVE (modulated) morph/Q, so an
    // armed modulation visibly plays the curve along with the wheel.
    float coeffs[trench::kUiCoeffCount] = {};
    float boost = 1.0f;
    const bool morphMoving = processor.isMorphModulatedForUi();
    const float baseMorph = morphMoving ? processor.getEffectiveMorphForUi()
                                        : read (ParamID::morph);
    // Q is the static authored second axis — Movement never modulates it.
    const float baseQ = read (ParamID::q);
    // The probe recompiles the whole packed body; only pay for it when the
    // wheel position or the body itself has actually moved since last frame.
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
    // While a hand is on MORPH the readout shows THAT hand, exactly like the
    // wheel does - a running phrase must not argue with the number you are
    // dragging (WheelControl::displayNormalised holds the same law).
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
