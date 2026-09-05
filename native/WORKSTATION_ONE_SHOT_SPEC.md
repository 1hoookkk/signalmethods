# TRENCH Workstation, one-shot build specification

Written 2026-09-05 for an outside model working at maximum reasoning with this repository
attached. Read the whole document before writing a file. Build everything in it. Where it
decides, do not re-decide. Where it is silent, decide the way a careful engineer would, write
the decision in `native/workstation/DECISIONS.md`, and continue. Do not stop to ask.

Do not open `native/workstation/Source/ui`, `native/workstation/Source/render`,
`native/workstation/Source/Main.cpp` or `native/app` for design. Every behaviour you need from
them is transcribed here. They are deleted in step one and their layouts are not to be
inferred or reproduced.

## 0. What you are building

TRENCH is a VST3 plugin, "a musical filter by Signal Methods". It plays a Z-plane filter: a
serial cascade of six second-order sections, four corners of thirty packed sixteen-bit words,
and a MORPH x Q wheel that lerps the words the way the E-mu hardware did. Section gains
multiply; responses add in dB. There is no parallel bank and no per-band normalisation.

TRENCH Workstation is the authoring tool beside the plugin. A musician reads a sound, sees
it, takes a six-voice frame from it, gathers frames into a bank of up to 256 that plays as one
morphing filter, puts four of them on the corners, hears the morph between them, tunes a
corner in notes and dB, and writes the 240-byte body the plugin plays. It is not
shipped. The plugin, its face and its bytes are not touched by this work.

The C++ model that does the mathematics (chords, words, voice leading, morph, export) and its
test suite are kept verbatim. The user-interface shell is rewritten from this document as a
MATLAB toolbox. The model and a real-time audio engine are reached from MATLAB through two MEX
files. Section 2 gives the decision and the reasons.

## 1. The target: Peevers' Spectrogram

The look and the working method are those of Alan Peevers' Spectrogram, the SGI program he
built at Berkeley in 1993-94 and carried to E-mu Systems (a 1995 build and its README are
preserved at `evidence/research-results/emu-sgi-1993/spectrogram/extracted/`; the report is
`evidence/research-results/emu-sgi-1993/REPORT.md`). Two period captures are the visual
target:

- `evidence/research-results/emu-sgi-1993/spectrogram/captures/sgi-archive-spectrogram-full.jpg`
  (1128 x 444). Left: a grey FORMS control panel. Three flat menu buttons across the top
  (File, Colors, Filter). Under them a row of push buttons (Pause, Clear, Mesh) with the
  current state printed beneath, a Window field showing the window type (Hann), and a 3 x 3
  block of small toggle switches with indicator lamps (Impulse, Modify, Persp; ZB, DB, LogF;
  Polar, 2D, Axes). Below: three boxed number fields (FFT Size 256, Win Size 256, Stride 128)
  and a Length field (500); a vertical group of lamp toggles (Monitor, Scope, Meter, Env,
  SynWin); a Translation group of three short sliders (time, amp, freq); a Cursor slider with
  its range boxes at both ends; seven labelled sliders (zoom, gain, floor, twist, tscale,
  ascale, fscale); two RGB slider groups for the colour map. Right: a separate display window,
  black, with the 3D spectrogram surface standing on a white wireframe baseline, frequency
  across, amplitude up, time into the screen, the surface coloured from cyan at the floor to
  red at the peaks.
- `evidence/research-results/emu-sgi-1993/spectrogram/captures/thesis-figure-5-trumpet-surface.gif`
  (507 x 373). A trumpet note as a spectral surface on a boxed baseline: harmonics as parallel
  ridges running along time, coloured by height in a parula-like map, the noise floor dark.

The README states the working method. A sound comes in from a file or from live input.
Its spectrogram is drawn; the surface can be tilted, scaled, cursor-navigated and paused. Env
replaces the FFT surface with the spectral envelope from a 12th-order LPC analysis. A
sequence of envelopes is saved as a filter and re-applied (Modify) to the input or to an
impulse train, and Monitor plays the result. Analysis parameters are typed into number boxes.

Build a similar thing. The mapping to TRENCH:

| Spectrogram | TRENCH Workstation |
|---|---|
| File in, live in | OPEN SOUND FILE, LIVE IN |
| The 3D surface, 2D, LogF, Axes, gain, floor, cursor | The SOUND room's display |
| Env (LPC envelope) | The reader: SPEECH (LPC order 12) or BELLS |
| Save filter | READ FRAME AT CURSOR, then USE AS a corner |
| Modify | FILTER ON |
| Impulse | The SAW and NOISE sources |
| Monitor, Meter | PLAY, the peak meter |
| The FORMS panel | The control panel: grouped switches, number boxes, sliders |

The structure comes from Spectrogram. The palette comes from the law in section 4 (MATLAB
grey ground and white axes, not SGI grey and a black display).

## 2. Framework: MATLAB shell, C++ engine

Decision: the shell is a MATLAB toolbox running in MATLAB R2025b on Windows. The kept C++
model and the C++ core are reached through one MEX file, `trench_bridge`. Real-time audio
runs in C++ on its own thread inside a second MEX file, `trench_audio`, built on miniaudio and
the core's cascade runner. MATLAB never touches the audio thread; it sets state and polls.

Installed and available (verified 2026-09-05 with `ver`): MATLAB 25.2 (R2025b), Audio Toolbox,
DSP System Toolbox, Signal Processing Toolbox, Control System Toolbox. Selected MEX compiler:
Microsoft Visual C++ 2022. CMake 4.0.0. MATLAB root `C:/Program Files/MATLAB/R2025b`.
Verified the same day: with `Matlab_ROOT_DIR` set, CMake's `find_package(Matlab COMPONENTS
MX_LIBRARY MAIN_PROGRAM)` reports version 25.2 and extension `mexw64`, `matlab_add_mex` builds
a MEX through the vcvars64 wrapper with Ninja, and `matlab -batch` loads and calls it. Under
`matlab -batch` an invisible figure exports a PNG with `exportgraphics`, and `pwelch`,
`buildtool`, `matlab.unittest` and `audioread` are all present. `matlab.exe` is on PATH.

Reasons, recorded so they are not re-argued:

1. The palette law names MATLAB. Every plot in this tool is an axes on a grey ground with
   white panels, blue data, orange chosen, gold live. In MATLAB that is the default, not an
   imitation.
2. Peevers' program was a research workstation tool; MATLAB is its living equal, and Massie
   and Peevers had Matlab beside E-mu's own tools. Tyson already writes MATLAB
   (`tools/matlab/`).
3. The Signal Processing, DSP and Audio toolboxes replace hand-written spectrograms, windows,
   LPC and file readers with proven calls. Less code, fewer places to be wrong.
4. A MEX bridge keeps the model, the core and both C++ test suites verbatim. Words, the
   plugin's lerp, voice leading and export stay exact where exactness matters.
5. The source structure Tyson asked for, "like MathWorks' open-source GitHub", is literal:
   a toolbox folder with packages, one function per file, class-based tests, a buildfile.

Use the classic figure system: `figure`, `uipanel`, `uicontrol`, `axes`, `line`, `surf`,
`image`. Not App Designer, not `uifigure`. Classic figures are faster under drag-driven
audio, run invisible under `matlab -batch`, and are the FORMS panel's nearest relative.
Set `figure('Renderer','opengl')`.

## 3. Repository: keep, delete, create

### Keep verbatim (do not edit)

- `native/core/` in full. The engine. Its public API is in section 6.
- `native/workstation/Source/model/` in full: `Frame`, `Morph`, `Body`, `Library`,
  `Delaunay`, `Stitch`, `Sound`, `Timeline`. You may add new files to `model/`; you may not
  change existing signatures or behaviour. The bridge compiles the files it needs.
- `native/workstation/Tests/MorphTests.cpp` and its target `TRENCH_WorkstationTests`
  (defined in `plugin/CMakeLists.txt`). Seventeen checks; they stay green.
- `native/python/` (the research bench) and its data: `native/python/workstation/stitch.json`,
  `native/python/workstation/chords/*.json`. Add and commit
  `native/python/workstation/frames_3d.json`: it is the bank (304 records) and is untracked
  today. Do not regenerate it.
- `plugin/` in full, except the target edits to `plugin/CMakeLists.txt` named below.
- `native/WORKSTATION_EDIT.md`, `native/WORKSTATION_CARICATURE.md`,
  `native/RESEARCH_PROMPT_2026-09.md`, and this file.

### Delete (git history keeps them)

- `native/workstation/Source/ui/`, `native/workstation/Source/render/`,
  `native/workstation/Source/Main.cpp`, `native/workstation/drafts/`.
- `native/app/` (the parked Qt tool), `native/tests/` (its tests), `native/audio/` (its
  audio boundary), `native/Testing/`.
- `native/MARIMO_PROMPT.md`, `native/REWRITE_PLAN.md`, `native/WORKSTATION_PLAN.md`,
  `native/WORKSTATION_STITCH.md`, `native/NEXT_SESSION.md`, `native/PROMPT_HERO_SPACE.md`,
  `native/NEXT_SESSION_PROMPT.md`.
- In the root `CMakeLists.txt`: `add_subdirectory(native/audio)` and the whole
  `TRENCH_BUILD_APP` block. In `vcpkg.json`: the `gui` feature and the `qtbase` override. In
  `CMakePresets.json`: the `app` configure and build presets.
- In `plugin/CMakeLists.txt`: the `TRENCH_Workstation` target and the
  `trench_workstation_gestures` test. Keep `TRENCH_WorkstationTests` and its test
  `trench_workstation`.

One implementation at a time, no duplicates. After this step the only authoring tool in the
repository is the MATLAB toolbox.

### Create

```
native/workstation/
  README.md                       what it is, how to build, how to run, how to test
  DECISIONS.md                    every decision you took where this document was silent
  buildfile.m                     tasks: mex (invokes the CMake build), test, check, shot
  toolbox/
    +trench/
      setup.m                     adds toolbox/ and toolbox/mex to the path, checks the MEX files
      launch.m                    opens the tool
      check.m                     the headless gesture check (section 10.C)
      shot.m                      writes one PNG per room, invisible figure
      +bridge/                    one thin MATLAB function per MEX command (section 7)
      +audio/                     one thin MATLAB function per audio command (section 8)
      +model/                     chord arithmetic done in MATLAB: bellChord, the line
                                  (anchor and hop arithmetic), naming of copies
      +io/                        scanSounds, openSound, exportPath, openBank, saveBank,
                                  tableToChords, makeFactoryBanks (sections 5.6, 5.7)
      +ui/
        @Workstation/             the app: state, the one Live value, the timer, the panel
        @SoundRoom/  @FramesRoom/  @MorphRoom/  @CornerRoom/
        @ResponsePanel/  @BodyGroup/  @ControlPanel/
        +draw/                    one function per drawn thing (responseCurve, fixedGrid,
                                  spectrogramSurface, frameMarks, padPosition, stageAxes ...)
    mex/                          build output: trench_bridge.mexw64, trench_audio.mexw64 (ignored by git)
  banks/                          the factory banks, written by makeFactoryBanks and committed;
                                  the musician's banks are saved here too
  bridge/
    trench_bridge.cpp             the model MEX (section 7)
    trench_audio.cpp              the audio MEX (section 8)
    CMakeLists.txt                included from plugin/CMakeLists.txt
  tests/
    tBridge.m                     matlab.unittest, section 10.B
    tRooms.m                      matlab.unittest, section 10.C
    tShot.m                       matlab.unittest, section 10.D
  Source/model/                   kept
  Tests/MorphTests.cpp            kept
```

Source structure law, from MathWorks' toolbox guidance (github.com/mathworks/toolboxdesign):
a `toolbox/` folder holding packages, one public function or one class per file, class
folders (`@Name`) for classes with several method files, `tests/` with class-based
`matlab.unittest` tests, a `buildfile.m` with named tasks, a `README.md`. Function names are
verb-noun camelCase (`readFrameAtCursor`, `drawResponseCurve`). No file over 300 lines. No
function over 60 lines except a drawing function that is one straight list of calls.

No code comments in any language, MATLAB and C++ alike; not a help block, not an H1 line.
Documentation lives in `README.md` files, one per folder that needs one.

## 4. Laws

These stand. Do not re-litigate them.

1. A frame's truth is its chord: six stages, each a pole voice and a zero voice (on, note as
   a MIDI number, width in semitones) and a gain in dB. Words are compiled from the chord at
   a datum. The round trip on the 33 bodies is byte-exact where the words can hold the chord
   and within a quarter dB in response everywhere. Every edit goes through the chord; nothing
   writes words except `compile`.
2. Voice leading: a partner's stages are reordered to the pinned frame's nearest notes;
   partnerless voices dissolve in place as a cancelling pole and zero. Section order commutes,
   so the exported body carries the led corners and the plugin's word lerp plays the voice
   leading. MORPH 0..100 is six short glides.
3. Reads are bells: a razor pole (0.25 semitones wide) with a zero on the same note sixteen
   times wider (4.0 semitones). Read frames are peaks, never low-passes.
4. MORPH and Q are the raw cube axes, played straight. Nothing on the surface changes itself.
   The plugin plays bytes; nothing here changes the ship face.
5. Row 6 is the ceiling: a zero on the unit circle at the top, its pitch the cutoff. It is
   never pushed past the ends and never re-voiced by leading.
6. Level: the DC unity rule, the product of the six sections normalised to 0 dB at DC and
   written as six equal gain words, on by default, applied when playing and when writing.
7. Out of bounds, the caricature: a morph past its ends pushes each row in note space
   (linear in semitones) and width space (geometric), refits every voice so the words hold
   the pitch, and clamps every pole radius at 0.9995. In bounds nothing may change; the plugin's
   truncating word lerp is the law there.
8. The surface is perceptual and musical: notes with cents, intervals above the root in
   semitones, widths in semitones, gains in dB. Hz and radius live behind one key and are off
   by default.
9. Words allowed on the surface: filter, morph, Q, frame, note, dB, root, voicing, resonance,
   body, corner, sound, pole, zero, width, ceiling, and the literal names in section 9.
   No sentences on the surface. One status line.
10. Naming law (Tyson, 2026-09-05): every control is named by the literal thing it does, in
    those words, or carries no text at all. No abstract or vague names: not "capture", not
    "take", not "export", not "commit", not "analyse". Section 9 gives the names. A shorter
    literal name is allowed if it loses no meaning; an abstract one never.
11. Palette (MATLAB crossed with a high-end Unix workstation): ground `#cccccc`; axes and
    panels white; frames and text black; data blue `#0072bd`; chosen orange `#d95319`; live
    gold `#c48f00`; structure grey `#b0b0b0`, fine lines `#d6d6d6`; a key is a grey box
    (`#e2e2e2`, edge `#6e6e6e`) that inverts to `#2b2b2b` with white text when on. Flat fills,
    crisp strokes, exact geometry. No glow, no blur, no gradient, no shadow, no rounded
    corners, no icons, no decoration. Never black-and-neon.
12. Typography: Arial for labels, keys, ticks and names; Lucida Console only where columns
    must align. Never a whole surface in a monospace face.
13. Plots: a magnitude axes keeps a figure aspect, 3:2 or 4:3, laid out as a grid of small
    axes; never a letterboxed strip. The spectrogram and anything along time are exempt. Every
    magnitude axes is the fixed frame: 20 Hz to 20 kHz on a log axis with lines at 100, 1k and
    10k, +30 to -30 dB with a hard line at 0 dB. Never auto-scale.
14. Refused, for good: a 3D browser of the bank, a bank visible while the morph is playing,
    automatic rearrangement of anything, another parameter space, a pole plot in the morph
    room, a shelf, source floors, a response forest, faders as the primary control, default
    ImGui or web-widget looks.
15. Everything must earn its keep on sound and control. Never keep a control because it was
    there before.
16. All tests run headless. Never open a window on the user's screen from a test. Tests are
    acceptance tests: fix the code, never loosen a threshold.
17. No code comments, in any language.
18. A bank is up to 256 anchors in numbered slots and is one filter: one line of anchors in
    the bank's sort order. At an anchor the sound is that frame exactly. Between two
    neighbouring anchors the sound is the voice-led pair morph at MORPH 0..100. Every hop
    spans the same 0..100 and, under a sweep, the same time, whatever the distance between
    the two anchors on the map: moving anchor to anchor is always one musical morph of the
    interpolation, never a blend of many. The line, the donor browse and the corners act on
    the open bank alone. The library (every source, the 1,567 Morpheus corners included) is
    where anchors are taken from and is never played as a filter. A bank is fed from three kinds of source and no other: the factory
    corners (E-mu's hand-voiced standard), reads from sound (the Spectrogram method), and
    textbook literature tables (formant tables such as Hillenbrand 1995 and Klatt 1980, each
    converted by the one rule in 5.7). A bank is a family that morphs well: one factory, one
    table, or the musician's own reads of one voice, instrument or room; the library's
    NEAREST TO sort (voice-leading cost from the chosen frame) is the guide. Factory banks
    are P2K first and open at first launch, then one per table, then one per read source
    except MORPHEUS. 128 was rejected: the standard's 132 corners, 130 of them distinct word
    for word, do not fit.

## 5. Representation and formats

### 5.1 Words

A section is five sixteen-bit words: word 1 zero magnitude, word 2 zero r-squared, word 3
pole magnitude, word 4 pole r-squared, word 5 gain. A frame is six sections: a 6 x 5 `uint16`
matrix in MATLAB (rows are stages 1..6 top to bottom, columns are words 1..5). The identity
section is `[0xDFFF 0xFFFF 0xDFFF 0xFFFF 0xDFFF]`. Words are minifloats; decode and encode
only through the bridge.

The plugin's lerp is per word, `a + trunc((b - a) * t)` against an int32 reference. The wheel
is bilinear on the words: along MORPH within each Q edge, then along Q. Corner order is
`M0 Q0, M1 Q0, M0 Q1, M1 Q1`.

### 5.2 Chord

A chord in MATLAB is a 6 x 7 double matrix, one row per stage:

```
[poleOn poleNote poleWidth zeroOn zeroNote zeroWidth gainDb]
```

`on` is 0 or 1. `note` is MIDI (69 = 440 Hz). `width` is semitones, bounded 0..120. The
datum for the plugin's bodies is 44,100 Hz; Morpheus cubes are read at 39,062.5 Hz and
compiled at 44,100 Hz so they play at true pitch. Width from radius at a datum is
`12 * log2(1 + bw / hz)` with `bw = -ln(r) * datum / pi`; radius from width inverts it. Use
the bridge for both; never reimplement.

### 5.3 Frame

A MATLAB struct: `name` (string), `group` (one of `P2K, MORPHEUS, X3, VOWELS, HEADS, XL-1,
INSTRUMENTS`), `chord` (6 x 7), `words` (6 x 5 uint16 at 44,100 Hz), `capture` (logical,
true for anything made at runtime), `source` (string: the file or the read origin), `root`,
`voicing`, `resonance` (section 9.3). A body's corner is a frame index plus, when it is a
copy, the name of the corner it copies.

### 5.4 The body file

`plugin/presets/user/ws_<YYYYMMDD_HHMMSS>.body240`: exactly 240 bytes, four corners in the
order above, six sections each, five words each, little-endian `uint16`. Byte offset of a word
is `corner * 60 + section * 10 + word * 2`. A switched-off row is written as the identity
section. The 33 factory bodies at `plugin/presets/p2k/*.body240` have the same layout.

### 5.5 The bank on disk

- `native/python/workstation/frames_3d.json`: a flat array of `{ "name", "root", "f1", "f2",
  "f3", "words" }` where `words` is 6 x 5 integers. 304 records: 132 P2K corners
  (`"<Body> · M0 Q0"` and so on, the middle dot U+00B7), 5 MORPHEUS, 9 X3, 2 VOWELS, 35
  HEADS, 31 XL-1, 90 INSTRUMENTS. Only `name` and `words` are read.
- `plugin/presets/p2k/*.body240`: the fallback if the JSON is missing; four frames per file.
- `native/python/workstation/chords/*.json`, schema `trench-chords-v1`:
  `{ "schema", "floor", "source", "chords": [ { "name", "source", "stages": [ { "pole":
  {"note","width"} | null, "zero": {"note","width"} | null, "gain_db" } x 6 } ] }`.
- `native/python/workstation/stitch.json`: the factory graph. Only its `nodes` with `"floor":
  "MORPHEUS"` are used here: `{ "floor", "datum", "words", "members", "faces", "x", "y", "z" }`;
  1,567 nodes; the first string in `members` is the name. Each is decompiled at its `datum` and compiled at
  44,100 Hz by the kept model.
- Sounds: every `*.wav` under `recipes/recordings`, `evidence/captures/inputs` and
  `evidence/measured-bodies/ir_library`, recursively, plus anything opened by file dialog.
- The Morpheus decoded cubes at `evidence/factory-data/morpheus/decoded/**/*.json` are not
  read by the tool; they are the source of the stitch nodes.

### 5.6 The bank file

`native/workstation/banks/<name>.bank.json`, schema `trench-bank-v1`:

```
{ "schema": "trench-bank-v1", "name": "P2K", "slots": 256,
  "frames": [ { "slot": 1, "name": "Ace Of Bass · M0 Q0", "group": "P2K",
                "source": "plugin/presets/p2k/ace_of_bass.body240 corner 0",
                "stages": [ { "pole": {"note": 78.96, "width": 19.35} | null,
                              "zero": {"note": 83.2, "width": 5.1} | null,
                              "gain_db": 0.0 } x 6 ] } ... ] }
```

Slots run 1..256; a slot not listed is empty; `frames` is ordered by slot. Chords are the
truth; words are compiled at 44,100 Hz on load through the bridge. The bank's name is the
file name. `trench.io.saveBank` and `trench.io.openBank` are the only readers and writers.

The factory banks are written by `trench.io.makeFactoryBanks(repoRoot)` (`buildtool banks`)
from the library of section 7.1, stopping at 256 frames each, and are committed:

| Bank | Contents |
|---|---|
| `P2K` | the 132 corners in file order, then `vowel schwa`; open at first launch |
| `Hillenbrand 1995` | the 48 vowel frames of `chords/hillenbrand_1995.json` |
| `Klatt 1980` | the Klatt Table II vowels from the core, through the table rule |
| `X3`, `HEADS`, `XL-1`, `INSTRUMENTS` | every library frame of that source, in library order |

MORPHEUS has no factory bank (1,567 corners); its corners are pulled into banks by hand.

### 5.7 The table rule

A textbook literature table enters the library as a `trench-chords-v1` file under
`native/python/workstation/chords/`, written by `trench.io.tableToChords(F, B, names,
citation)`, where `F` and `B` are n x k matrices of formant frequencies and bandwidths in Hz,
`names` the n row names, and `citation` the string stored in `source`. One rule converts a
row: formant k becomes stage k with a pole at note `noteOf(F_k)` and width
`12 * log2(1 + B_k / F_k)` semitones, the bell rule of law 3 applied (zero on the same note,
sixteen times wider), stages beyond the last formant off, row 6 the ceiling at 20 kHz, gain
0 dB; then `fitVoices`, `unityDc`. Regenerate `chords/hillenbrand_1995.json` through this rule
from its CSV source so the shipped table obeys it (its present chords are bare poles); keep
the 48 names. The bridge exposes `klattVowels()` (section 7.2) for the Klatt table. Any
further table follows the same path and gets its own factory bank.

Tables come only from files under `evidence/` that carry a citation. Never type a table from
memory. In hand today: Hillenbrand 1995 (`evidence/mouths/hillenbrand/hillenbrand-vowel-formatted.csv`,
the paper in `evidence/manuals/research/`), Klatt 1980 Table II (the core, `evidence/mouths/klatt`),
and Kent and Vorperian's review of vowel formant bandwidths
(`evidence/manuals/research/Kent_Vorperian_vowel_formant_bandwidths_review.pdf`), which is the
bandwidth source for any table that gives frequencies alone. The ground-truth range Tyson
will add, each to become a bank when its file lands in `evidence/`: Peterson and Barney 1952
(vowel formants and levels, men, women, children); Fant 1960 (vowel formants with
bandwidths); Hawks and Miller 1995 (the bandwidth law); Rodet's CHANT vowel table as printed
in the Csound manual (five formants with amplitudes and bandwidths for bass, tenor,
countertenor, alto, soprano); Meyer, Acoustics and the Performance of Music (formant regions
of the orchestral instruments); Fletcher and Rossing, The Physics of Musical Instruments
(body modes of strings, membranes, plates); Rossing and Perrin 1987 (bell partial ratios).
A table that is missing is left for Tyson; the tool must not invent it.

## 6. Kept C++ API

Signatures are verbatim. The bridge calls these and nothing reimplements them.

### 6.1 Model (`native/workstation/Source/model`, namespace `ws`)

`Frame.h`:

```cpp
constexpr double kDatumHz = trench::core::kP2kDatumHz;
constexpr int kRows = 6;
constexpr int kWords = 5;
constexpr int kMeasures = 8;
constexpr int kCurvePoints = 160;
constexpr int kGroups = 7;
constexpr double kMaxWidth = 120.0;
using Words = std::array<std::array<std::uint16_t, kWords>, kRows>;
using Curve = std::array<double, kCurvePoints>;
extern const char* const kGroupNames[kGroups];
struct Voice { bool on = false; double note = 60.0; double width = 12.0; };
struct Stage { Voice pole, zero; double gainDb = 0.0; std::array<std::uint16_t, kWords> raw { 0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF }; };
using Chord = std::array<Stage, kRows>;
struct RowGeom { bool pole = false; double pHz = 0.0, pR = 0.0; bool zero = false; double zHz = 0.0, zR = 0.0; };
struct Frame { juce::String name; Chord chord {}; Words words {}; std::array<RowGeom, kRows> rows {}; std::array<double, kMeasures> m {}; bool capture = false; int group = 0; };
double noteOf (double hz);
double hzOf (double note);
double widthOf (double hz, double radius, double datum);
double radiusOf (double hz, double width, double datum);
juce::String noteName (double note);
Chord decompile (const Words& words, double datum);
Words compile (const Chord& chord, double datum);
Chord chordFrom (double rootNote, const std::array<double, kRows>& intervals, double width, double gainDb);
void setChord (Frame& frame, const Chord& chord);
void setWords (Frame& frame, const Words& words, double datum);
void fitVoice (Voice& voice, bool pole, double datum);
int groupOf (const juce::String& name);
double resDb (double radius);
std::array<RowGeom, kRows> geometryOf (const Words& words);
trench::core::Cascade cascadeOf (const Words& words);
double responseDb (const trench::core::Cascade& cascade, double hz);
Curve curveOf (const Words& words);
void measure (Frame& frame);
Words blend (const std::vector<const Words*>& parents, const std::vector<double>& weights);
double sectionDb (const Words& words, int row, double hz);
void unityDc (Words& words);
void sharpen (Chord& chord, double keep);
```

`kGroupNames` is `{ "P2K", "MORPHEUS", "X3", "VOWELS", "HEADS", "XL-1", "INSTRUMENTS" }`.
`curveOf` samples 160 log-spaced points from 20 Hz to 20 kHz, clamped to -60..30 dB.
`compile` touches only the words of voices that are on, and the gain word; rows with neither
voice keep their `raw` words. `fitVoice` narrows a voice (x 0.7, up to 24 times) until compile
then decompile keeps its note within 0.1 semitone. `unityDc` writes the same gain word into all
six rows. `sharpen(chord, keep)` multiplies every on pole's width by `keep`.

`Morph.h`:

```cpp
constexpr double kRadiusGuard = 0.9995;
constexpr double kPushLow = -2.0, kPushHigh = 3.0;
constexpr double kWheelLow = -1.0, kWheelHigh = 2.0;
struct Morph { Words words {}; std::array<bool, kRows> guarded {}; bool outside = false; };
struct Excess { double hz; bool above; };
bool guardRadius (Words& words, std::array<bool, kRows>& guarded);
Morph pairMorph (const Words& a, const Words& b, double t);
Morph wheelMorph (const std::array<Words, 4>& corners, double morph, double q);
std::vector<Excess> excessOf (const Words& words);
Words meanWords (const std::vector<const Words*>& parents);
struct Lead { Chord a, b; std::array<int, kRows> map {}; double cost = 0.0; };
double dissolveWidth (double note);
Lead leadTo (const Chord& a, const Chord& b);
double leadCost (const Chord& a, const Chord& b);
Words leadWords (const Words& a, const Words& b);
juce::String intervalsOf (const Chord& c);
Words schwaWords();
```

`pairMorph` in 0..1 is the plugin's word lerp exactly. Outside it is the caricature (law 7),
row 6 plain-lerped, radius guarded. `wheelMorph` in bounds is the bilinear word lerp with
weights `(1-m)(1-q), m(1-q), (1-m)q, mq`. `leadTo(a, b)` tries every permutation of b's
stages (row 6 weighted a quarter), keeps the cheapest, then dissolves partnerless voices both
ways: a lone pole takes the partner's note at `dissolveWidth` and, if it has no zero, gets a
matching zero on the same note and width; a lone zero takes the partner's zero note at a razor
width. `excessOf` returns one `{hz, above}` per run of the curve outside +-30 dB.
`intervalsOf` prints semitone offsets above the lowest narrow pole (width <= 6 st).

`Body.h`:

```cpp
class Body {
public:
    std::array<int, 4> corner { -1, -1, -1, -1 };
    std::array<bool, kRows> rowOn { true, true, true, true, true, true };
    double morph = 0.5, q = 0.5;
    bool unity = true;
    bool ready() const;
    Words cornerWords (const std::vector<Frame>& frames, int i) const;
    Words wheelWords (const std::vector<Frame>& frames) const;
    Morph wheelMorph (const std::vector<Frame>& frames) const;
    bool outside() const;
    std::array<double, 4> weights() const;
    std::array<std::uint8_t, trench::core::kLegacyBodyBytes> legacyBytes (const std::vector<Frame>& frames) const;
    bool exportTo (const std::vector<Frame>& frames, const juce::File& file) const;
};
```

`cornerWords(i)` returns corner i led against corner 0 (corner 0 is led toward the first
differing partner), switched-off rows as the identity section, `unityDc` applied when
`unity`. `legacyBytes` mirrors corners 0..3 into 4..7 and writes the 240-byte layout.

`Library.h` (the parts used):

```cpp
class Library {
public:
    std::vector<Frame> frames;
    bool loadJson (const juce::File& file);
    bool loadBodies (const juce::File& dir);
    int addNamed (const Words& words, const juce::String& name, int group, bool capture);
    void addSchwa();
    int loadChords (const juce::File& file);
};
```

`Stitch.h` (the parts used): `bool loadJson (const juce::File& file);` and
`std::vector<Node> nodes;` where a `Node` carries `int floor; double datum; Chord chord;
Words words;` (already compiled at `kDatumHz`) and the name is the first member string.
`static Shape shapeOf (const Chord& chord);` with
`struct Shape { double root = 60.0, voicing = 0.0, width = 1.0; bool any = false; };`:
root is the lowest narrow pole's note (width <= 6 st, note in 12..132, any pole as fallback),
voicing is the highest minus the lowest narrow pole in octaves, width is the mean pole width.

`Sound.h` is not compiled into the bridge. Its reader, transcribed exactly, is section 7.4.

### 6.2 Core (`native/core/include/trench/core`, the parts the bridge and engine use)

`packed_body.hpp`:

```cpp
inline constexpr std::size_t kCoefficientCount = 5;
inline constexpr std::size_t kSectionCount = 7;
inline constexpr std::size_t kCornerCount = 8;
inline constexpr std::size_t kLegacySectionCount = 6;
inline constexpr std::size_t kLegacyCornerCount = 4;
inline constexpr std::size_t kNativeBodyBytes = 560;
inline constexpr std::size_t kLegacyBodyBytes = 240;
inline constexpr double kMorpheusDatumHz = 39'062.5;
inline constexpr double kP2kDatumHz = 44'100.0;
using PackedSection = std::array<std::uint16_t, kCoefficientCount>;
using Biquad = std::array<double, kCoefficientCount>;
using CornerWords = std::array<PackedSection, kSectionCount>;
using Cascade = std::array<Biquad, kSectionCount>;
inline constexpr PackedSection kIdentitySection{ 0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF };
struct ConjugatePair { double hz{}; double radius{}; };
struct RealPair { double root_a{}; double root_b{}; };
struct DegeneratePair {};
using RootPair = std::variant<ConjugatePair, RealPair, DegeneratePair>;
struct SectionGeometry { RootPair pole; RootPair zero; double scale{1.0}; };
double decode_word(std::uint16_t word);
std::uint16_t encode_word(double value);
std::uint16_t interpolate_word(std::uint16_t a, std::uint16_t b, float fraction);
Biquad section_words_to_biquad(const PackedSection& words);
SectionGeometry geometry_from_words(const PackedSection& words, double sample_rate_hz = kMorpheusDatumHz);
PackedSection words_from_geometry(const SectionGeometry& geometry, double sample_rate_hz = kMorpheusDatumHz);
class PackedBody {
 public:
  static PackedBody from_legacy_bytes(std::span<const std::uint8_t> bytes);
  [[nodiscard]] bool is_legacy_representable() const;
  [[nodiscard]] std::array<std::uint8_t, kLegacyBodyBytes> legacy_bytes() const;
  [[nodiscard]] CornerWords interpolate_words(float morph, float q, float z) const;
  [[nodiscard]] Cascade interpolate_biquads(float morph, float q, float z) const;
  std::array<CornerWords, kCornerCount> words{};
};
double cascade_response_db(std::span<const Biquad> sections, double frequency_hz, double sample_rate_hz);
std::vector<double> logarithmic_frequency_grid(double low_hz, double high_hz, std::size_t point_count);
```

`native_body.hpp`:

```cpp
namespace trench::core::native {
Cascade rewarp_cascade(const CornerWords& words, double datum_hz, double target_hz);
}
```

The rewarp converts each section's words at the datum to pole and zero as Hz plus bandwidth,
redesigns them at the target rate, and adds a level correction so the response near the
section's own pole matches the datum design. Tests pin it at 0.2 dB worst case over 44.1,
48 and 96 kHz and 0.15 dB at 120 Hz.

`audition.hpp`:

```cpp
inline constexpr std::size_t kApproachSamples = 256;
class CascadeRunner {
 public:
  CascadeRunner();
  void set_target(const EncodedCascade& target);
  void set_immediate(const Cascade& coefficients);
  void set_glide(const Cascade& coefficients, std::size_t samples);
  void reset();
  void process(std::span<float> block);
  void set_pole_distortion(double grit) noexcept;
  void set_sample_rate(double sample_rate_hz) noexcept;
  void set_ring_leveller(bool enabled) noexcept;
};
```

`process` is real-time safe: fixed arrays, no allocation, no locks; a non-finite sample
zeroes the state. `set_glide` ramps the coefficients over `samples`. The ring leveller holds
each section's ring within 24 dB of its input with under -60 dB added distortion and is
transparent at low Q.

`body_from_audio.hpp`:

```cpp
namespace trench::core::audio {
struct Resonance { double hz{}; double bw_hz{}; double gain_db{}; };
std::vector<Resonance> resonances_from_audio(std::span<const float> mono, double sample_rate_hz, std::size_t count = 6);
std::vector<Resonance> speech_poles(std::span<const float> mono, double sample_rate_hz, std::size_t count, double model_rate_hz = 11'025.0, std::size_t order = 12);
std::vector<float> resample(std::span<const float> mono, double from_hz, double to_hz);
}
```

`speech_poles` resamples to 11,025 Hz (127-tap windowed sinc) and fits order 12 by
autocorrelation Levinson: the classic speech envelope, one pole per formant.
`resonances_from_audio` is the higher-order all-pole fit with peak picking, for bells. Tests
pin formants within 4 percent and bells within 3 percent.

The C ABI in `trench_core_c.h` exists for other hosts; the bridge links the C++ library
directly and does not use it.

## 7. The bridge MEX: `trench_bridge`

One MEX, first argument a command string, wrapped one-per-file in `+trench/+bridge/` so
MATLAB code never sees the command strings. Built by CMake against `trench_native_core`,
`juce::juce_core`, `juce::juce_graphics` and the model files `Frame.cpp`, `Morph.cpp`,
`Body.cpp`, `Library.cpp`, `Delaunay.cpp`, `Stitch.cpp`. Define
`JUCE_STANDALONE_APPLICATION=1` and `JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1` on the target;
no JUCE message thread is started; JUCE is used only for strings, files, JSON and colour.
Errors raise `mexErrMsgIdAndTxt` with identifiers `trench:bridge:<command>`.

Representations cross the boundary exactly as section 5 states: words `uint16` 6 x 5, chords
double 6 x 7, four corners as `uint16` 6 x 5 x 4, frames as struct arrays, dB and Hz as
double columns. All numbers are validated for shape before use.

### 7.1 Bank

- `frames = loadFrames(repoRoot)`: `Library::loadJson(frames_3d.json)`, else
  `Library::loadBodies(plugin/presets/p2k)`; `addSchwa()`; `loadChords` on every
  `native/python/workstation/chords/*.json`; then `Stitch::loadJson(stitch.json)` and
  `addNamed(node.words, <first member string>, MORPHEUS, false)` for every MORPHEUS node. Returns the
  struct array of section 5.3 with `root`, `voicing`, `resonance` filled from
  `Stitch::shapeOf(chord)` (resonance is the mean width). Order: as loaded.
- `names = groupNames()`.

### 7.2 Chords and words

- `chord = decompile(words)` and `words = compile(chord)` at 44,100 Hz;
  `decompileAt(words, datumHz)`, `compileAt(chord, datumHz)`.
- `chord = chordFrom(rootNote, intervals, width, gainDb)` (`intervals` six semitone offsets).
- `chord = fitVoices(chord)`: `fitVoice` on every on voice, pole and zero.
- `[a, b, map, cost] = leadTo(chordA, chordB)`; `cost = leadCost(chordA, chordB)`.
- `[words, guarded, outside] = pairMorph(wordsA, wordsB, t)`.
- `[words, guarded, outside] = wheelMorph(corners, morph, q)`.
- `words = unityDc(words)`; `chord = sharpen(chord, keep)`.
- `s = intervalsOf(chord)`; `s = noteName(note)`; `n = noteOf(hz)`; `hz = hzOf(note)`;
  `w = widthOf(hz, r)`; `r = radiusOf(hz, w)`.
- `shape = shapeOf(chord)`: `[root voicing meanWidth any]`.
- `t = klattVowels()`: the core's Klatt 1980 Table II as a struct array with `symbol`,
  `F` (1 x 3 Hz) and `B` (1 x 3 Hz), from `trench::core::p2k::klatt_vowels()`
  (`formants.hpp`: `struct Formant { double hz{}; double bw_hz{}; }; struct VowelFormants {
  std::string_view symbol; std::array<Formant, 3> f; }; std::span<const VowelFormants>
  klatt_vowels();`).

### 7.3 Response

- `hz = curveHz()`: the 160 log points from 20 Hz to 20 kHz.
- `db = responseDb(words, hz)`: cascade response at each frequency in `hz`.
- `db = sectionDb(words, row, hz)`: one stage alone.
- `g = geometry(words)`: 6 x 6 `[poleOn pHz pR zeroOn zHz zR]`, for the Hz view.
- `e = excessOf(words)`: n x 2 `[hz above]`.

### 7.4 Reader

- `res = readResonances(block, fs, mode)`: `mode` is `"speech"` (`speech_poles(block, fs, 6)`)
  or `"bells"` (`resonances_from_audio(block, fs, 6)`); returns n x 3 `[hz bw gain]`.
- `words = readFrame(block, fs, mode)`, transcribed from the kept `Sound::frameAt`: the
  resonances above, sorted by Hz; for each of the six rows, if a resonance exists, pole
  `ConjugatePair{ clamp(hz, 20, 20000), clamp(exp(-pi * max(10, bw) / 44100), 0, 0.9995) }`,
  otherwise pole `{20000, 0}`; zero `{20000, 0}`; scale 1; `words_from_geometry` at 44,100;
  then `unityDc`. Returns empty if the block has under 256 samples or no resonance was found.
  The bell rule (law 3) is applied by MATLAB afterwards, `trench.model.bellChord`: decompile,
  and for every on pole set `poleWidth = 0.25`, `zeroOn = 1`, `zeroNote = poleNote`,
  `zeroWidth = 4.0`; then `fitVoices` and `compile`.

### 7.5 Body

- `corners = bodyCorners(corners, rowOn, unity)`: the four corners exactly as
  `Body::cornerWords` returns them (led, rows off as identity, unity applied).
- `bytes = bodyBytes(corners, rowOn, unity)`: 240 `uint8`, via `Body::legacyBytes`.
- `writeBody(path, corners, rowOn, unity)`: `Body::exportTo`; creates the folder.
- `words = bodyLerp(bytes, morph, q)`: `PackedBody::from_legacy_bytes` then
  `interpolate_words(morph, q, 0)`, first six sections. The plugin's own wheel, for checks.
- `ok = bodyRepresentable(bytes)`.

## 8. The audio MEX: `trench_audio`

The engine runs on miniaudio's device thread (fetch miniaudio by `FetchContent` at a pinned
commit; single header). The MEX is `mexLock`ed on first use and released only by
`trench_audio('unload')`. Nothing on the device thread allocates, locks or blocks. Every
MATLAB call returns immediately.

Commands, wrapped one-per-file in `+trench/+audio/`:

- `info = start()`: opens the default output device, two channels, the device's native rate,
  or a duplex device when live input is requested; returns `struct(device, rateHz)` or an
  empty struct with `error` filled. `stop()`. `unload()`.
- `words(w)`: hands a new frame to the device thread through a two-slot buffer and an atomic
  index, consumed at most once per block. On the device thread: build `CornerWords` from the
  six rows plus `kIdentitySection` in row 7, `rewarp_cascade(cw, 44100, rateHz)`, then
  `runner.set_glide(cascade, 256)`.
- `source(name)`: `"saw"` (110 Hz, amplitude 0.4), `"noise"` (uniform, +-0.4), `"sample"`
  (the clip's loop region, linear interpolation from the clip rate to the device rate,
  wrapping from OUT back to IN), `"input"` (the device's capture channel, mixed to mono).
- `clip(mono, fs)`, `region(inSeconds, outSeconds)`.
- `playing(tf)`, `wet(tf)`: `wet` runs `runner.process` on the block; otherwise the source
  passes dry. Output is the block times 0.5 on every channel.
- `x = snapshot()`: the last 16,384 post-filter samples, oldest first.
- `x = inputRing()` and `n = inputWritten()`: the last ten seconds of live input at the
  device rate, for the scrolling spectrogram.
- `p = peak()`: the peak absolute output of the last block. `t = playhead()`: the clip
  position in seconds while the sample source plays. `s = state()`: everything above as a
  struct, for the status line and tests.

On device start: `runner.set_sample_rate(rateHz); runner.reset()`. Ring leveller on. Pole
distortion 0.

## 9. The tool

The tool is one figure: a control panel and a display, in Peevers' arrangement. Which room the
display shows is chosen by four keys at the top of the panel: SOUND, FRAMES, MORPH, CORNER.
The response panel (9.5) and the body group (9.6) are on the control panel in every room. Each
room owns its own list and its own field; no room's keys appear in another room. The layout
is yours to design from the target and the laws; every pixel earns its place; no empty
regions; no clutter.

### 9.1 The one Live value

The app holds exactly one value that says what is sounding: `live.kind` in `none`, `frame`
(a frame index), `line` (the bank line at an anchor index and MORPH 0..100 toward the next
anchor), `wheel` (the body at MORPH, Q), `cornerAlone` (one corner, while its key is held),
`slice` (the reader's frame at the SOUND cursor), `edit` (the corner being edited). A
`frame` is a bank slot or a library row; a `line` position morphs two neighbouring anchors
of the open bank and nothing else. Every gesture sets it. One function,
`wordsOf(app)`, is a switch on it and returns the words. The audio engine, the response
curve, the meter, the status line and the tests all consume that one function. Nothing
else decides what sounds.

### 9.2 SOUND room, the target

The list: every scanned sound (section 5.5) and the sounds opened by OPEN SOUND FILE; LIVE IN
at the top. Selecting a sound loads it (`audioread`, mixed to mono), sets the loop region to
the whole file, and draws it.

The display: the spectrogram of the sound. Number boxes FFT SIZE (default 2048, powers of
two), WINDOW (Hann default; Blackman, Blackman-Harris, Hamming, rectangular), STRIDE (default
512). A 3D switch: off, the 2D image (`image`, time across, log frequency up, dB as colour);
on, the surface (`surf`, frequency across, dB up, time into the screen, on a boxed baseline,
tiltable by right-drag) in the manner of the trumpet figure. LOG F on by default. GAIN and
FLOOR sliders scale and offset the dB map. AXES on: time in seconds, frequency in kHz on log
ticks, amplitude in dB. LIVE IN scrolls the last ten seconds from `inputRing`.

The cursor: a vertical line at one time, dragged in the display or set by the CURSOR slider
and its typed box. IN and OUT: two handles for the loop region, dragged; PLAY loops the
region through the filter. The reader runs at the cursor on a 60 ms window: SPEECH or BELLS,
a two-way switch. Its six voices are drawn on the display at the cursor as marks at their
notes, and its response is drawn in the response panel over the slice's spectrum (grey). While
the SOUND room is open, `live.kind` is `slice`: the filter under the sound is the frame read
at the cursor, so PLAY with FILTER ON is Peevers' Modify.

ENVELOPE (Peevers' Env): a switch that replaces the FFT surface with the reader's response
at every stride, computed once per sound per setting with a progress line in the status; off
by default.

READ FRAME AT CURSOR: the frame at the cursor, the bell rule applied, named
`<sound> @<seconds to two decimals>`, group INSTRUMENTS, `capture` true, placed as an anchor in
the bank's first empty slot, chosen. The status line names it. Then USE AS in the body group puts it on a corner.

### 9.3 FRAMES room, the line of anchors

A bank is up to 256 anchors in numbered slots and is one filter: a line (law 18). The library
is everything the tool knows and is where anchors are taken from. The room holds the bank
list, the library list, the map and the position control.

Three numbers describe a frame (from `shapeOf`): ROOT, the lowest strong voice, shown as a
note; VOICING, the highest strong voice minus the root, in octaves; RESONANCE, the mean voice
width mapped narrower-is-higher, with ticks at 12, 3, 1 and 0.25 semitones. Keep those names.
They are how the line is sorted and how the map is drawn; they are not controls.

The bank list: one row per filled slot: slot number, name, ROOT, VOICING (one decimal),
RESONANCE (semitones), intervals above the root (`intervalsOf`), source. Sorted by SLOT (the
order the anchors were placed, so a path is composed by hand), by LOW > HIGH ROOT, or by
NEAREST TO M0 Q0 (the voice-leading cost from corner M0 Q0; hidden when no corner is set).
The sort order is the line's order; it never changes by itself. Pressing a row jumps the
position to that anchor and plays it exactly (`live.kind = frame`), and makes it the chosen
anchor (orange). The bank's name and its count (`<n> of 256`) are printed above the list.
Bank keys: NEW BANK, OPEN BANK, SAVE BANK, SAVE BANK AS, REMOVE FROM BANK (unlights the chosen
anchor and empties its slot).

The library list: every frame of every source (sections 5.5 and 7.1), foldable by source
with seven checkboxes, MORPHEUS off by default (1,567 rows), sorted by LOW > HIGH ROOT or by
NEAREST TO the chosen anchor (voice-leading cost), which is the guide for choosing an anchor
that morphs well from the one you hold. Pressing a row plays it exactly. PUT IN BANK places
the chosen library frame as an anchor in the bank's chosen empty slot, or the first empty
slot. The library is never edited and never played as a filter; only the bank is.

Placing anchors: a frame lit from the library, a frame read in the SOUND room and a moment
kept in MORPH each become an anchor in the open bank's first empty slot. When the bank is
full the status line says `bank full` and the frame goes to the library under INSTRUMENTS
instead. The factory banks (5.6) are written at build; P2K is open at first launch.

The line: the position on it is ANCHOR (a slot in the line's order) and MORPH (0..100 toward
the next anchor in that order). One POSITION control runs the whole line, typed or slid,
from the first anchor at 0 to the last anchor at the end of its hop, with FINE DRAG at a
tenth; the boxes beside it show `<A name> > <B name>` and the MORPH amount. At an anchor the
sound is that frame exactly. Between two anchors it is the voice-led pair morph: `leadTo` on
the two chords, then `pairMorph(compile(led.a), compile(led.b), MORPH / 100)`
(`live.kind = line`). Every hop spans the same 0..100, so moving from one anchor to the next
takes the same amount of MORPH and, under SWEEP POSITION (a checkbox beside the control that
glides the position along the whole line as a slow triangle at a fixed number of seconds per
hop, 4 by default, typed), the same time, whatever the distance between the two anchors on the
map. Manual and swept movement use the same position.

The map: the library on ROOT across (C1 to C8 as note ticks) and VOICING up (0 to 6
octaves); every library frame a dim grey mark; every anchor of the bank a blue mark whose
size is its RESONANCE (narrower is larger); the chosen anchor orange; the line drawn through
the anchors in their order as a thin grey polyline; the position gold on its segment with its
projections on both axes. Pressing a dim mark lights it: it becomes an anchor in the first
empty slot. Pressing a lit mark chooses it. Marks at the same place stack: draw one mark with
a count; pressing it lists its members in the status line and chooses or lights the first;
the chosen member stays until the musician changes it. The map is a map, not a controller:
the position is not dragged on it; the position moves only on the POSITION control, by
SWEEP, or by pressing a row.

USE AS M0 Q0, USE AS M1 Q0, USE AS M0 Q1, USE AS M1 Q1: put the chosen anchor on that corner.
With no chosen anchor, the position's morph is first kept as an anchor in the first empty
slot, named `<A name> > <B name> <morph>`, then put on the corner. A corner is always a bank
slot. The fill rule: the first frame put anywhere fills all four corners; a second frame put
on M1 Q0 fills M1 Q1 as well (the opposite edge); later frames replace copies first. A copied
corner is named `copy of M0 Q0` (and so on) wherever its name is shown, until it is replaced.
The body's pad in MORPH is the two-dimensional case of the line: four anchors, every edge one
hop of 0..100.

### 9.4 MORPH room, the pad

Shown only when at least one corner is set. The four corner names at the pad's corners.
One position: MORPH across, Q up, both 0..100 with typed boxes. Drag to play
(`live.kind = wheel`). FINE DRAG scales the drag to a tenth, anchored at the press. SWEEP
MORPH and SWEEP Q glide one axis each as a slow triangle. HEAR M0 Q0, held: the first corner
alone; released: back to the position. PAST THE ENDS, off by default: on, the pad and the
boxes run -100..200 on both axes (the wheel push of law 7), the pad drawing the 0..100 square
inside the larger field; guarded rows are named in the status line. KEEP AS FRAME: the
sounding words become a frame named `<M0 Q0 name> <morph>/<q>`, `capture` true, an anchor in the bank's first
empty slot, chosen. EDIT CORNER: the CORNER room on the chosen corner. Pressing a corner name
returns to FRAMES with that slot selected.

The central operation: hold this character, pull toward that character, keep this moment.
Corner M0 Q0 is the pinned frame; corner M1 Q0 is the donor. B: PREVIOUS FRAME and B: NEXT
FRAME replace corner M1 Q0 (and M1 Q1 while it is a copy of M1 Q0) with its neighbour in the
bank list's current sort order, wrapping at the ends, keeping MORPH and Q where they are, so
each candidate is heard
through the same pairing at the same amount. A small MORPH amount can be a large change, so
FINE DRAG must be precise near the start.

Nothing else is drawn: no bank, no pole plot, no glide lines. The response panel shows the
sound.

### 9.5 The response panel

The fixed frame of law 13, aspect 3:2. One blue curve: the response of `wordsOf(app)`, 160
points. Grey: the slice's spectrum in the SOUND room; the measured output trace when MEASURE
is on. Excess ticks at the top or bottom edge at the frequencies where the curve leaves the
frame. A peak meter (post-filter output, from `peak`), a bar beside the plot, gold. Below:
PLAY / STOP; FILTER ON; the source as four radio keys SAW, NOISE, SAMPLE, LIVE IN; MEASURE
(forces NOISE while on; every timer tick takes `snapshot`, averages 4096-point Hann frames at
50 percent overlap with `pwelch`, and draws the result at the 160 points); the device line
(`<device> <rate> Hz` or `no audio device`). The status line is the last row of the panel.

### 9.6 The body group

The four corners as a 2 x 2 block of keys named by their frames (or `copy of ...`), pressing
one chooses that corner for CORNER; a chosen corner is orange. Six row switches 1..6, on by
default; a switched-off row plays and writes as the identity section. UNITY DC, on by
default. WRITE BODY FILE: `writeBody` to `plugin/presets/user/ws_<YYYYMMDD_HHMMSS>.body240`
through `bodyCorners`; the status line prints the path. Disabled until four corners are set.

### 9.7 CORNER room, the six rows

Opens on the chosen corner. Editing works on a copy named `edit <corner name>` so the bank
stays untouched; the copy replaces the corner when the room is left, and is put in the bank
as a frame. `live.kind = edit`.

Six small magnitude axes (4:3, two rows of three), one per stage: that stage alone in blue,
the whole corner in grey, a square handle on the pole and a dot on the zero. Drag the pole
sideways for pitch and up or down for width; the zero likewise. LOCK ZERO TO POLE per row
(the zero follows the pole's note when the pole is dragged; the parametric posture). Beneath
each axes the row as typed numbers, following `native/WORKSTATION_EDIT.md`: PITCH (note name
and cents, typed as `C3+12` or as a number), WIDTH (semitones), AMOUNT (the band's peak in dB
at the pole, edited by moving the zero's width with the zero on the pole), ZERO (one word:
`on pole`, `wider`, `tighter`, `below`, `ceiling`, `off`). The interval above the root is
printed beside PITCH on rows 2..6. Row 6 is labelled CEILING and its PITCH is the cutoff
note; CEILING (key) sets it to 20 kHz on the unit circle. OPEN: a slider on Klatt's F1, 250
to 900 Hz on a log scale with B1 60 to 120 Hz, moving whichever row holds the lowest strong
pole. SHARPEN: every on pole's width divided by four (the bank's Q axis, for making a Q corner
from an M corner). COPY TO: the four corner names; puts a copy of this corner there. SHOW HZ:
adds Hz and radius under each row, off by default. Every edit: chord, `fitVoices`, `compile`,
never words.

The cascade of the whole corner in one 3:2 axes above the six, gold while it sounds.

### 9.8 Gestures and rules of the surface

- Every mouse handler is a method `onSomething(room, pointInDataUnits, phase)` with `phase`
  in `press`, `drag`, `release`, callable without a figure event. The figure's
  `WindowButtonMotionFcn` and `WindowButtonUpFcn` route to those methods; the check (10.C)
  calls them directly.
- A timer at 30 Hz: feeds `words` to the engine when `wordsOf(app)` changed (compare the
  matrix), updates the meter, the playhead and the measured trace, advances sweeps, and
  redraws only what changed (`set` on existing graphics objects, `drawnow limitrate`; never
  recreate axes per tick).
- Names appear on selection or hover; values and parent names occupy the one status line.
- Right-drag orbits the 3D surface in SOUND; nothing else orbits.
- Typed entry everywhere a number is shown; a typed value lands on the nearest value the
  words can hold, and the box shows what landed.

## 10. Acceptance

All of it runs headless: `matlab -batch` with figures created `'Visible','off'`; no window
ever opens during a test. All of it is registered in `buildfile.m` and in ctest.

### A. The C++ suites, unchanged and green

- `TRENCH_WorkstationTests` (ctest `trench_workstation`): 17 checks, `PASS  0 failure(s)`.
  They cover the caricature doubling, the radius guard, the plugin-lerp equality at t = 0.5,
  export of 240 bytes, legacy representability, the guard at every wheel position, excess
  ticks, the schwa, the quarter-dB chord round trip on 132 P2K corners, the 39,062.5 Hz datum,
  a typed chord landing on its notes, voice leading cost, partners, the 3 dB pinned response,
  and the exported led body's lerp equalling the pair morph.
- `trench_core_tests` and `trench_core_from_audio_tests` as registered by `native/core`.

### B. `tests/tBridge.m` (matlab.unittest)

1. `loadFrames` returns at least 304 + 1567 frames; exactly 132 in P2K; the first P2K frame is
   named `Ace Of Bass · M0 Q0`; every frame has a 6 x 5 `uint16` `words` and a 6 x 7 `chord`.
2. For every P2K frame, `responseDb(compile(decompile(words)))` is within 0.25 dB of
   `responseDb(words)` at all 160 points.
3. For the first ten `(M0 Q0, M1 Q0)` bank pairs: `c = bodyCorners([a b a b], all rows on,
   unity off)`, `bytes = bodyBytes(...)` of the same; `bodyLerp(bytes, 0.5, 0)` equals
   `pairMorph(c(:,:,1), c(:,:,2), 0.5)` word for word (the exported led body's own wheel is
   the workstation's pair morph).
4. `writeBody` writes exactly 240 bytes; `bodyRepresentable` is true; the folder is created.
5. `wheelMorph` at every point of an 11 x 11 grid keeps every pole radius at or under
   0.9995 (from `geometry`).
6. A synthetic vowel (formants 500, 1500, 2500 Hz, bandwidths 60, 90, 120 Hz, one second,
   44,100 Hz) through `readResonances(..., "speech")` gives three poles within 4 percent;
   `bellChord(readFrame(...))` has, on every on row, `zeroNote == poleNote`, `poleWidth ==
   0.25` within 0.01 after `fitVoices`, and `zeroWidth == 4.0` within 0.05.
7. The line: for the first ten hops of the P2K bank in SLOT order, the words at MORPH 0 equal
   anchor A exactly, at 100 equal `compile(leadTo(A, B).b)` exactly, and at 50 equal
   `pairMorph` of the led pair at 0.5 word for word.
8. `chordFrom(48, [0 7 12 16 19 24], 1, 0)` compiled and decompiled lands row 1 within 0.05
   semitone of note 48 and row 4 within 0.05 of note 64.
9. The audio engine: `start` returns a device or an error string; with a device, `words`,
   `source("noise")`, `playing(true)` then, after 200 ms, `snapshot` is non-zero and `peak`
   is under 1; `stop`. Without a device the test passes on the error string.
10. `makeFactoryBanks` writes the seven files of 5.6; `P2K.bank.json` holds 133 frames, slot 1
    is `Ace Of Bass · M0 Q0`, slot 133 is `vowel schwa`, and its words compiled from the
    stored chords equal the library's words for every slot; `Hillenbrand 1995` holds 48;
    `Klatt 1980` holds every Klatt vowel; no factory bank exceeds 256.
11. `saveBank` then `openBank` round-trips a bank of ten library frames: same names, same
    slots, same chords within 1e-9, same words.
12. Putting a frame into a bank holding 256 frames returns the status `bank full` and leaves
    the bank unchanged.
13. `tableToChords([500 1500 2500], [60 90 120], "ah", "test")` gives one chord whose three on
    rows have poles within 0.05 semitone of the formant notes, widths within 0.05 semitone of
    `12 * log2(1 + B / F)`, zeros on the same notes sixteen times wider, rows 4 and 5 off, row
    6 the ceiling.

### C. `tests/tRooms.m`, the gesture check (`trench.check` runs the same)

Construct the app invisible against the real bank. Each check is one assertion on
`app.probe()`, a struct with `live` (kind and parameters), `room`, `corners` (four indices),
`cornerNames`, `chosen`, `morph`, `q`, `root`, `voicing`, `resonance`, `status`,
`playing`, `filterOn`, `source`. The sixteen carried over, then the new ones:

1. In FRAMES, pressing a row plays that frame: `live.kind == "frame"`, index matches.
2. The status line names the frame, its root and its intervals.
3. Pressing a different row changes the playing frame.
4. USE AS M0 Q0 exists and is enabled once a frame is chosen.
5. One frame put anywhere fills all four corners: `corners(1) >= 0`, all four equal;
   `cornerNames{2}` starts with `copy of`.
6. A second frame put on M1 Q0 fills the opposite edge: `corners(2) ~= corners(1)`,
   `corners(4) == corners(2)`, `corners(3) == corners(1)`.
7. Across MORPH 0..100 in 21 steps between the two led corners, every voice glides one way:
   no pole's note reverses direction.
8. No pole moves more than 6 semitones in one twentieth of the morph.
9. MORPH opens with the wheel playing: `room == "morph"`, `live.kind == "wheel"`.
10. A drag across a quarter of the pad moves MORPH by 25 within 2.
11. With FINE DRAG on, a half-pad drag moves MORPH by 5 within 2.
12. Holding HEAR M0 Q0 gives `live.kind == "cornerAlone"`.
13. Releasing it returns `live.kind == "wheel"`.
14. KEEP AS FRAME puts a new anchor in the bank's first empty slot with `capture` true and the status
    starting with its name.
15. PLAY reports the device or `no audio device`.
16. Pressing a corner name returns to FRAMES with that slot selected.
17. SOUND: opening `recipes/recordings/test-vowel-ah.wav` draws a spectrogram (the image or
    surface object exists with the expected column count for the stride) and `live.kind ==
    "slice"`.
18. READ FRAME AT CURSOR puts an anchor in the bank's first empty slot named `test-vowel-ah @...`, with the
    bell rule holding on every on row.
19. FRAMES: POSITION at anchor 3 gives `live.kind == "frame"` for slot 3; POSITION at anchor
    3 plus MORPH 40 gives `live.kind == "line"` with words equal to `pairMorph` of the led
    pair at 0.4; every pole's note lies between its two anchors' notes.
20. SWEEP POSITION on with 4 seconds per hop: after ticks worth 4 seconds the position has
    advanced exactly one anchor, on a hop between two anchors far apart on the map and on a
    hop between two close together alike.
21. MORPH with PAST THE ENDS off refuses MORPH 150 (clamped to 100); on, it accepts 150 and
    `probe().status` names any guarded row.
22. B: NEXT FRAME changes `corners(2)` (and `corners(4)` while it is a copy) and leaves
    `morph` and `q` unchanged.
23. CORNER: typing `C3+0` into row 1's PITCH lands within 0.05 semitone of note 48; with LOCK
    ZERO TO POLE on, dragging the pole moves the zero's note with it.
24. CEILING sets row 6's zero to 20 kHz within 1 percent on the unit circle.
25. WRITE BODY FILE writes 240 bytes to `plugin/presets/user/`, and `bodyLerp` of that file
    at (0, 0) equals `bodyCorners(...)(:,:,1)` word for word.
26. Every key named in section 9 exists in its room and in no other room; every key label is
    in the allowed word list of law 9 or in the literal names of section 9.
27. First launch opens the P2K bank: `probe().bank.name == "P2K"` and `probe().bank.count ==
    133`; the map shows 133 marks.
28. PUT IN BANK from the library adds a row at the first empty slot; REMOVE FROM BANK empties
    it; the count follows.
29. With the bank sorted by NEAREST TO M0 Q0, B: NEXT FRAME steps through that order and
    wraps from the last row to the first.
30. SAVE BANK AS, NEW BANK (count 0), OPEN BANK restores the same count and names.
31. With the bank filled to 256 by script, READ FRAME AT CURSOR reports `bank full` and the
    frame appears in the library under INSTRUMENTS.
32. OPEN BANK on `Hillenbrand 1995` sorted LOW > HIGH ROOT, then POSITION swept along the
    whole line in steps of MORPH 10: the words change at every step and no pole reverses its
    direction inside a hop: the vowel space as one filter.
33. Pressing a dim mark on the map lights it as an anchor in the first empty slot; pressing
    it again chooses it; REMOVE FROM BANK unlights it.

### D. `tests/tShot.m`

`trench.shot(dir)` writes `sound.png`, `frames.png`, `morph.png`, `corner.png` from the
invisible figure at 2x (`exportgraphics` or `print -r192`), each over 50 kB, each with the
ground colour `#cccccc` at its top-left pixel and at least one pixel of `#0072bd`.

### E. Build and registration

- The CMake build through the vcvars wrapper produces `trench_bridge.mexw64` and
  `trench_audio.mexw64` and copies them to `native/workstation/toolbox/mex/`.
- `ctest` in `out/build/vst3` runs `trench_workstation` (C++), `trench_workstation_bridge`,
  `trench_workstation_rooms` and `trench_workstation_shot` (each `matlab -batch` on the
  corresponding test class, `TIMEOUT 600`), all green.
- `buildtool test` and `buildtool check` in `native/workstation` run the same.

## 11. Build

- Every CMake build runs through MSVC vcvars64; a bare shell fails with C1083. Use a `.cmd`
  wrapper that calls
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat`
  and then `cmake --preset vst3` and `cmake --build --preset vst3 --target trench_bridge
  trench_audio TRENCH_WorkstationTests`. Add `"Matlab_ROOT_DIR": "C:/Program Files/MATLAB/R2025b"`
  to the `vst3` preset's cache variables.
- `native/workstation/bridge/CMakeLists.txt`, included from `plugin/CMakeLists.txt` after
  JUCE is available: `find_package(Matlab REQUIRED COMPONENTS MX_LIBRARY MAIN_PROGRAM)`,
  `matlab_add_mex(NAME trench_bridge SRC ... LINK_TO trench_native_core juce::juce_core
  juce::juce_graphics juce::juce_recommended_config_flags)`, likewise `trench_audio` against
  `trench_native_core` and miniaudio, C++20, `/MD`, a post-build copy into `toolbox/mex`.
- MATLAB: `cd native/workstation; buildtool mex` invokes the wrapper; `buildtool test` runs
  the three test classes; `trench.setup` then `trench.launch` opens the tool (never from a
  test).
- Commit with explicit pathspecs. Do not stage anything under `plugin/` except
  `plugin/CMakeLists.txt` and `plugin/NEXT_SESSION.md`.

## 12. Decisions taken here by default

Three questions were open at the close of the last session. They are decided as follows;
Tyson may reverse any of them after the build.

1. The FRAMES room is the line of anchors with equal hops (section 9.3), the map of the
   library behind it, beside the sorted lists; there is no free blend, no three-control
   cursor and no 3D space. Tyson's rule of 2026-09-05 (anchors, equal time per hop)
   supersedes the plane draft and its three controls.
2. The MORPH pad stays inside 0..100 by default; PAST THE ENDS is a switch.
3. One frame fills all four corners and a second fills the opposite edge; copies are named
   `copy of ...` until replaced.

4. A bank is 256 slots, fed from the factory corners, from reads and from textbook tables
   and from nothing else (law 18, sections 5.6, 5.7, 9.3). Tyson asked for the size and the
   feed to be decided; both are.

Two more, taken in this document: the shell is MATLAB (section 2), and the Qt app, its tests
and its audio boundary are deleted with the JUCE shell (section 3), because one
implementation at a time is the rule.

## 13. Records to update

- Append one entry to `plugin/NEXT_SESSION.md` under a `2026-09` heading: what was built,
  the counts of every suite, the paths.
- Rewrite the "What this is" and "Builds and tests" sections of `CLAUDE.md` to describe the
  MATLAB toolbox, the two MEX files and the new test names. Leave every other section.
- `native/workstation/README.md` and `DECISIONS.md` as in section 3.

## 14. Tyson's words

These are the instructions in his own words from 2026-09-05, in order. The last line is the
one this document exists to serve.

- "spectrogram is our target."
- "do not infer the current ui or layouts either."
- "look at the images. and instruct gpt to do a similar thing. use source code structured
  like matlabs open source github or something similar."
- "also we have matlab and multiple toolboxes so im willing to use matlab too."
- "do not use abstract vague naming like capture etc. put the most literal naming you can or
  do not include text."
- "Lets decide on the size of a frame bank as it is one big filter." "And if we are even
  know what to feed it." "Or should they be textbook literature tables."
- "I think it should just be one massive long light space you know with a bunch of corners."
- "think of it like anchors. Right? You place an anchor. You move from one anchor to the
  other, and it takes the same amount of time so that it's morphing musically from the
  interpolation."
- "the issue is in the monolithic files."
- "instead of rebuilding we should just get gpt to one shot it from scratch."
