#include "PluginEditor.h"
#include "BinaryData.h"
#include "TrenchBodyRoster.h"
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
constexpr float kBayRight = 194.0f;   // compact: sized to its content, not the wheels
constexpr float kBayPad   = 8.0f;     // even inner margin on all four sides
constexpr float kBayRowGap = 6.0f;    // extra air under the selector, before row 0
constexpr int   kBayRowH  = 42;       // one row = knob height + its air
// The taller rows have to come from somewhere. The plate between the Q wheel
// and the selector was 40px of dead beige, the widest empty gap on the face, so
// the whole bay moves up 12 and the room grows down 6 - still clear of the
// plate's inner carve at y 490.
const juce::Rectangle<int>   kBaySelector { (int) (kBayLeft + kBayPad), 328, 90, 17 };
const juce::Rectangle<float> kBayRooms[2] = {
    { kBayLeft, 336.0f, kBayRight - kBayLeft, 2.0f * kBayPad + kBayRowGap + 3.0f * (float) kBayRowH },
    // MOVEMENT holds two occupants since TRACK retired (2026-08-15): the
    // PRESET box and FOLLOW. Both rooms carve the SAME rectangle — GAIN's
    // three rows set the size, so MOVEMENT carries a row of air at the
    // bottom rather than resizing the drawer per page.
    { kBayLeft, 336.0f, kBayRight - kBayLeft, 2.0f * kBayPad + kBayRowGap + 3.0f * (float) kBayRowH },
};
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
    typeSelector->onAnnounce   = [this] (const juce::String& s) { graph->announce (s); };
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
    amountWheel = std::make_unique<ThinWheel> (processor.apvts, ParamID::amount);
    amountWheel->onValueGesture = [this] (float v) { graph->showAmountCue (v); };
    // THE BAY: two rooms under Q, one open at a time. GAIN is the chain before
    // and after the cascade; MOVEMENT is how the wheel travels on its own.
    // SOURCE (resample) stays retired.
    sectionRail = std::make_unique<SectionRail> (theme);
    preampKnob = std::make_unique<MixKnob> (processor.apvts, theme, ParamID::preamp, "Input");
    chewKnob   = std::make_unique<MixKnob> (processor.apvts, theme, ParamID::chew,   "Bite");
    slamKnob   = std::make_unique<MixKnob> (processor.apvts, theme, ParamID::slamDrive, "Output");
    followKnob = std::make_unique<MixKnob> (processor.apvts, theme, ParamID::envAmount, "Follow");
    // TRACK retired from the face (Tyson 2026-08-15): the pitch listener was a
    // detector-driven retuner the X3 never had, and E-mu's authored answer to
    // pitch-following is the cube's own third axis. The parameter stays for
    // old sessions; the engine wiring stays; the knob is gone. The THIRD-AXIS
    // control ("Transform 2") appears only when a 560-byte cube body loads —
    // there is no cube load path yet, so it is not built yet.
    // MOVEMENT is the ONLY home for modulation now - the glass chip is gone.
    modSourceBox = std::make_unique<ModSourceBox> (processor.apvts, theme);
    modSourceBox->livePhraseProvider = [this] { return processor.hasLivePhraseForUi(); };
    sectionRail->onToggleSection = [this] (int s)
    {
        // Picking the open door's word again shuts the drawer (2026-08-15).
        openSection = (s == openSection) ? -1 : s;
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
    addAndMakeVisible (*amountWheel);
    addAndMakeVisible (*keySnapBox);   // KEY stays on the top bar
    addAndMakeVisible (*sectionRail);
    // Room contents: only the open room's lanes are on the plate.
    addChildComponent (*preampKnob);
    addChildComponent (*chewKnob);
    addChildComponent (*slamKnob);
    addChildComponent (*modSourceBox);
    addChildComponent (*followKnob);
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
    devPanel->onOpenChanged = [this] (bool open)
    {
        setSize (trench::ui::kEditorWidth
                     + (open ? trench::ui::DevBypassPanel::kWidth : 0),
                 trench::ui::kEditorHeight);
    };
    onboardingReplayHotspot.onSecretClick = [this]
    {
        devPanel->open (processor.dspBridge.getBypass());
    };
    // Click the TRENCH badge to replay the tour. No new faceplate furniture.
    addAndMakeVisible (onboardingReplayHotspot);
    setResizable (false, false);
    setSize (kEditorWidth, kEditorHeight);
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
    amountWheel->setBounds (rectOf ("amountWheel"));
    {
        // THE BAY, restored to its authored joinery (mock_sel comp): a room
        // CARVED into the plate, low and to the left, with the selector seated
        // in a break at the frame's top-left. The lanes live inside the carve -
        // they never float on bare plate.
        // The two door-words, seated where the room-frame break lands. One
        // fixed width in every state (closed, GAIN, MOVEMENT).
        sectionRail->setBounds (kBaySelector.withWidth (sectionRail->preferredWidth()));
        // ONE LANE PER ROW, chain order top-down - the stack the plate was
        // carved for. Nothing sits beside anything, nothing leaves the carve.
        // ONE ROW GRAMMAR, shared by both rooms: a full-height row band, the
        // knob in a fixed left column, the caption-over-value block centred in
        // the column beside it. Rows are 42 tall against a 32 knob, so the air
        // BETWEEN rows is far wider than the 2px inside a caption/value pair -
        // each control groups as one unit.
        const int x0 = juce::roundToInt (kBayLeft + kBayPad);
        const int x1 = juce::roundToInt (kBayRight - kBayPad);
        const int w  = x1 - x0;
        const int rowY = juce::roundToInt (kBayRooms[0].getY() + kBayPad + kBayRowGap);
        const auto rowAt = [&] (int i) { return juce::Rectangle<int> (x0, rowY + i * kBayRowH, w, kBayRowH); };
        preampKnob->setBounds (rowAt (0));
        chewKnob->setBounds   (rowAt (1));
        slamKnob->setBounds   (rowAt (2));
        // DEPTH retired 2026-08-10: travel is part of each preset's record.
        // TRACK retired 2026-08-15 (see the knob's construction site above).
        // FOLLOW is the room's one knob under the header — the input's
        // dynamics driving the wheel.
        followKnob->setBounds (rowAt (1));
        // SOURCE has no knob, so it occupies the value column alone - the same
        // box, on the same axis, as every knob row's readout (MixKnob's block
        // starts 36px in and its box is kBayValueWidth centred in what's left).
        {
            // PRESET has no knob. Parking it in the value column left a
            // knob-width hole of bare plate beside it — a lane visibly missing
            // its control — and centring it in the row instead put it on a
            // third axis, 20px off the value boxes stacked under it.
            //
            // It is not a lane. It is the room's header: the one choice both
            // knobs below answer to. So it spans the carve's full inner width,
            // which reads as a header rather than a misaligned lane AND is the
            // only way the longest preset name ("1 Oct Random - Chromatic", 24
            // characters) fits between its two steppers.
            //
            // It carries a caption stacked over a select, so unlike a numeric
            // lane it needs BOTH pieces' height.
            constexpr int capH = 13;
            const auto row = rowAt (0);
            modSourceBox->setBounds (x0, row.getCentreY() - kBaySourceHeight / 2 - capH,
                                     w, kBaySourceHeight + capH);
        }
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
    const bool gain   = openSection == SectionRail::kDrive;
    const bool motion = openSection == SectionRail::kMotion;
    preampKnob->setVisible (gain);
    chewKnob->setVisible (gain);
    slamKnob->setVisible (gain);
    modSourceBox->setVisible (motion);
    followKnob->setVisible (motion);
    // -1 reaches the rail as the CLOSED state: both words engraved quiet.
    sectionRail->setOpenSection (openSection);
    // The visible cap hugs the selected word. Seat it here as well as in
    // layoutComponents because the room-frame break is measured from it and
    // the first call runs before the editor is sized.
    const auto seat = kBaySelector.withWidth (sectionRail->preferredWidth());
    sectionRail->setBounds (seat);
    // The plate carves the open page's room, broken over the WHOLE word strip
    // (a break around only the open word ran the frame line through the shut
    // word). The lit word in the break is the tab of the drawer it opened.
    // Closed (openSection -1): no carve at all, two quiet words on bare plate.
    faceplate->setRoomFrame (openSection < 0 ? juce::Rectangle<float>()
                                             : kBayRooms[juce::jlimit (0, 1, openSection)],
                             (float) seat.getX() - 4.0f,
                             (float) seat.getRight() + 4.0f);
}
void PluginEditor::onFrame()
{
    // NO FADE, EVER (Tyson 2026-08-15 "No fucking fade"). The old law dimmed
    // MORPH, Q, KEY, BITE and the MOVEMENT side to 28% on the No-filter body
    // because they act on nothing there — honest, but with No filter as the
    // shipping default it made the FIRST face read as unplugged. The hardware
    // model wins instead: every control stays full-strength and fully
    // interactive, exactly like the knobs on an unpatched synth — they turn,
    // the identity body just gives them nothing to change. SLAM, MIX and the
    // preamp were always live on the wet path.
    keySnapBox->refreshSuggestion();
    if (devPanel != nullptr && devPanel->isVisible())
        devPanel->setAgcReductionDb (processor.getAgcReductionDbForUi());
    // The citron pilot dot retired with SectionRail::setHot (2026-08-14): an
    // armed MOVEMENT moves the MORPH wheel and the trace on every page, so
    // the motion is the indicator. PRESET's OFF stays the one truthful
    // Movement bypass (2026-08-09 bay refactor).
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
    labels->setMixValue (read (ParamID::amount));
    const bool morphActive = morphWheel->isMouseOverOrDragging (true) || morphReadout->isMouseOverOrDragging (true);
    const bool secondaryActive = secondaryWheel->isMouseOverOrDragging (true) || secondaryReadout->isMouseOverOrDragging (true);
    morphReadout->setActive (morphActive);
    secondaryReadout->setActive (secondaryActive);
}
