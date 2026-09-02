# CHECKPOINT 2026-09-02 (late) — pick up fresh here

Branch `face/ship-candidate-fx`, native tree committed at 948d6b6c (native suite 27/27
headless). Launcher `TRENCH Workstation.bat`; build through the MSVC env only:
`TRENCH Build App.bat`, or a .cmd that calls VsDevCmd.bat then
`cmake --build --preset app --target trench_native trench_native_tests`; tests
`ctest -R ^native\.` in `out/build/app` with QT_QPA_PLATFORM=offscreen. A bare shell
build fails C1083 and leaves stale binaries. Never two vcpkg-configuring builds at
once. Commit native work with explicit pathspecs only (other chats stage `plugin/`).

## Rulings 2026-09-02 late (Tyson's words)

- Thesis: "im shipping TRENCH. not this native app. but this app is what im using to
  make the filters for TRENCH because im not a coder. my standard is basically the cube
  filter responses and the 33 p2k. today we ship as 240 bytes 4 corners."
- "im not trying to work towards what they have. i would have shipped by now. im trying
  to basically infer what sort of tooling they used to make them."
- "Include Harmonic Helpers", "Enforce Slot 6 Invariants", "Make Trajectory Auditing
  Mandatory", "add the real pole shape word to the console" - all built, see below.
- Offline interior audition: dropped (the pad with AUDITION open already is it). Heat
  map on the pad: deferred until the meter's ring lands somewhere unexplained.
- Research phase closed. Next: voice bodies end to end and let the first one expose
  what the tool still needs.

## What the tooling inference settled today

- Rossum US10514883B2: a corner = six parametric EQ bands + a low-pass section; per
  section pole angle, pole radius, zero angle, zero radius, gain; frequency and
  resonance encoded independently in log space.
- Massie JAES 1993 + Bristow-Johnson: the section is an allpass on a 4-multiply
  normalized ladder plus feedforward; k1 = -cos w0 (frequency only), k2 = pole radius
  (Q only), K = boost/cut only. The row IS his section. Piano corner verified: zero on
  the pole angle, peak = 20 log10(zero bw / pole bw) per rung within 0.2 dB.
- Massie saol-users 1999: parametric bands interpolate legally; 8 basis frames = the
  cube; cascade never parallel; corners are the only authored thing.
- Massie SOS Oct 1995: Morpheus tool "too limited" for the voicer David Bristow; the
  post-1995 tool gave "the parametric filter functions accessed directly".
- P2K section 6 = the low-pass: zero radius byte always 0 (unit circle), pitch parked
  at Nyquist 61/127 or pulled down as a ceiling (median 9.5 kHz).
- Corner relations, all 33 bodies: slot identity 68%; sharpening per row by hand
  (within-body variance 5.4x between); Morph partner re-voiced (zero/pole slope 0.15);
  gain words = DC-unity rule only. POSTURE and SHARPEN are seeds, never the finish.
- Presentation (inference, unconfirmed): Mac app in 1993, Windows at Creative by 1997,
  Matlab beside both, a separate ROM packer. Witnesses: Dana Massie, David Bristow,
  Kevin Monahan, the P2K software architect CV, Rossum. DSPx 1994 citation unlocated.
- Corpus: evidence/research-results/corpus/massie_interpolation_corpus.md;
  prediction tests evidence/research-results/p2k_prediction_tests.{md,json,csv,py}.

## Earlier tonight (2026-09-02 evening)
## Tyson's rulings tonight (his words)

- The row is "Frequency (the harmonic note), Q (pole bandwidth / ring time),
  and Peak Gain (zero bandwidth / boost in dB)."
- Zero locked to the pole by default (a locked row is level-neutral; sliding
  the zero off for a shelf or notch is the exception and keeps a control).
- "i need dials for the parameters or mixer faders. and to be able to type in
  each as well."
- "as close as emu's propietary tool as we can infer" ... "things need to be
  erased for it to be what it needs to be."
- Image accepted: harmonic resonators stacked like organ drawbars, one fader
  per rung.

## What the app is

Six rows x four corners (FROM/TO x Q0/Q100). Per row: Frequency, Q, Peak Gain
on a dial or fader each, with a typed box each; a typed value moves the dial to
the nearest hardware word; every readout comes from the packed words. One
zero-unlock per row. CUT per corner, last. The fixed +-30 dB display with the
hard 0 dB line, showing the corner being edited and the row alone. Export to
.body240 / .trenchbody; audition. Nothing on the face changes itself.

## What the app is not

Not a fitter, not an analyzer, not a sound or curve importer, not a z-plane
editor, not a template browser. E-mu's voicer set positions by ear and eye;
nothing in either bank was typed as Hz, notes, Q or dB (see the piano cube
below - musical intent, hand-set values).

## Erase ruling — DONE 2026-09-02 (Tyson: "yes erase them. keep morph and gesture")

Erased from `native/app/`: fit_room, fitted_shelf, zero_fit, lpc_poles,
corpus_shelf, mask_shelf, template_shelf, import_routing, zplane_view,
make_bodies, waveform_strip, section_strip, plus the sound/curve reference
overlay, CLIP audition source, FIT button and `--reference` flag. OPEN takes
.trenchbody / .body240 only. Kept: row_table, cascade_plot, editor_state,
body_io, main_window, main, skin, morph_pad, gesture_dial. Tests voice a
hand-set ladder (`tests/ladder.hpp`, the PianoSndBrd numbers) instead of
templates; suite 18 cases + registry, all green. The callerless
`core/src/p2k/zeros_fit.cpp` (`fit_zeros_under`) has now been erased.

## Fader console — DONE 2026-09-02 (Tyson: "Faders")

`app/row_table.{hpp,cpp}` is the console (sizeHint ~1001x247, sits under the
plot). Corner picker `corner0..3` (0 FROM/Q0, 1 TO/Q0, 2 FROM/Q100, 3 TO/Q100).
Six strips, each: `on{i}`, columns FREQ / Q / GAIN / ZERO with
`{col}Fader{i}` (QSlider over the 256 dial words), `{col}Entry{i}` (typed,
lands on the nearest word), `{col}Readout{i}` (decoded from the packed words
only), `lock{i}`, `cut{i}`. GAIN fader = zero rsq word (Peak Gain readout =
section response at the pole hz); ZERO fader = zero mag word, live only when
unlocked. Lock is derived, never stored: locked = zero present and
`words[0] == p2k::mag_word_for(decoded pole hz, words[1])` (the zero sits on
the lattice seat for the pole's note at the zero's own bandwidth); moving a
locked pole re-seats the zero; clicking LOCK seats it. A ladder voiced through
`addZeroAt` does not always land on that seat (state encodes from the request,
not the decoded pole; row 1: pole 64.1 / zero 57.9 Hz), so the tests click
LOCK first. Suite 21/21 headless.

## Build order

1. Corner gestures (Morph axis = posture, Q axis = sharpen) on morph_pad /
   gesture_dial, which survived the erase.
2. Seat `addZeroAt` on the decoded pole so a fresh row is locked without a
   click; PianoSndBrd corner 0 rebuilt from six rows + spare row + trim equals
   the stored product (script: `evidence/research-results/piano_row_by_row_plot.py`).

## How to voice a ladder (plain words, for Tyson)

Pick the root note (the pitch the drum should ring at). Rows = root x 1, x2,
x3 ... (x2 octave, x3 octave + fifth, x4 two octaves, x5 + major third, x6 + fifth).
Per row: Q = how long it rings (72 = 2.4 s at 64 Hz, 17 = 0.03 s at 1.4 kHz);
Peak Gain = how loud that harmonic is (+20 dB root, +4 dB top, like drawbars).
A spare row with no zero, high and wide, is a treble tilt. CUT last so the
peak sits on the grid. PianoSndBrd did exactly this: 64 / 129 / 388 / 584 /
777 / 1430 Hz = C, C, G, D, G, ~F#; Q 72 / 36 / 34 / 35 / 34 / 17; Peak Gain
+20 / +12 / +12 / +6 / +10 / +4 dB; spare pole 17 kHz; trim -7.5 dB.

## Evidence (untracked, `evidence/research-results/`)

piano_row_by_row.png (+ .py) - the ladder built one row at a time, final =
stored to 1e-6 dB; piano_poles_only.png - poles alone, pinned; piano_ladder.png;
morpheus_axis_*.png; p2k_axis_order_census.py/.txt (Morph voiced first, Q by
copy-and-edit). Morpheus absolute level is unproven (raw product x trim puts
42% of corners above +20 dB) - plot Morpheus pinned, don't chase; the P2K gain
law (DC unity split six ways) is proven and ships.

## Harmonic helpers, slot 6, path meter — DONE 2026-09-02 (late)

Rulings: "Include Harmonic Helpers: quick-dial integer harmonic multipliers (1x, 2x,
3x...) from a base fundamental pitch." "Enforce Slot 6 Invariants: a dedicated
corrective shape stage (unit-circle notch + free tilt pole)." "Make Trajectory
Auditing Mandatory: a live interior morph slider with a real-time peak-gain warning
meter."

- ROOT card at the left of the console (`rootDial` / `rootEntry` / `rootReadout`,
  caption `rootNote` shows the note name). Per strip a `harm{i}` word: `-`, `1x root`,
  `2x 8ve`, `3x 8ve+5th` ... `16x 4-8ve`. Picking n lands FREQ on the nearest word to
  root x n. The shown harmonic is derived (pole within 50 c of root x n), never stored;
  moving ROOT re-lands every row that sits on a harmonic, in one undo step; a hand move
  of FREQ drops the row back to `-`.
- Row 6: shape pinned to NOTCH and disabled, GAIN off, OFFSET column becomes CEIL
  (`offsetDial5` over the mag words, `offsetEntry5` typed Hz, readout Hz). State
  enforces it: `lockedZero` forces bw 0 on section 5 for every zero write,
  `removeZeroAt` and a real-root zero are refused there. The pole is free; moving it
  does not move the ceiling.
- `app/path_meter.{hpp,cpp}`: `PathMeter` under the morph pad (`pathMeter`,
  `pathReadout`). Every change scans a 9x9 morph x Q interior, reports the worst peak
  of the six-section product on the 20 Hz-20 kHz grid ("PATH +x dB M.. Q.. HERE +x
  dB"), bar amber from +20 dB and red from +30 dB (off the fixed frame); the pad draws
  a ring at the worst point when it is over +20. The pad is the live interior slider.
- POLE word per row (`pole{i}`: RING / REAL), derived from the packed words. REAL
  seeds the census's commonest factory real pair (r 0.02/0.75, DC side) through
  `setRealRootAt`; the row reads TILT and `r a/b`, typed boxes and the harmonic word
  go quiet, the two dials still move the hardware words. Nine factory bodies carry
  real poles; the console can now voice them.
- Suite 27/27 headless. `row_table_reads_back_the_packed_words` now expects Hz on
  row 6. Piano ladder copied to four corners then corner 2 up an octave: path worst
  +29.7 dB at M0.75 Q0.12 against +25.4 / +25.8 at the ends - the interior peak
  Massie warned about, now visible.

## Operator pass fixes — DONE 2026-09-02 (late; critique from another chat, Tyson: "your job is this")

1. A newly enabled row seats as a locked 1x ROOT resonator: pole at ROOT with Q 35 (or
   the pole-radius ceiling, Q 21 at 64 Hz), zero on the pole at 4x width (+8 dB bell),
   through the packed-word seat so it lands on the note. Re-enabling a voiced row keeps
   its values. Row 6 seats only its pole; the cage stays.
2. FREQ and Q are independent on the face: a FREQ move re-lands the radius word to hold
   the displayed Q; a Q move re-lands the angle word to hold the displayed Hz
   (`pushPole(index, Kind moved)`). Typed entries and the harmonic words go through it.
3. Shape words: POLE (bare pole), RESONATOR (the bell, zero on the pole, was PAIR),
   NOTCH, EDGE HP, EDGE LP. A fresh row is RESONATOR with GAIN and OFFSET live.
4. PATH meter is two lines (`pathReadout` PATH + worst point, `hereReadout` HERE) at
   170x68 with a 10 px bar; nothing clips. The blank-document contradiction did not
   reproduce headless; the suite now asserts PATH == HERE == 0 on a blank document
   before and after a ladder.
5. WordDial is a vertical fader: 48x132, track + knob, readout on top, drag relative
   (2 px/word, shift 10), wheel, arrows, PageUp/Down 12; click takes keyboard focus and
   paints an orange focus ring.
Suite 27/27 headless.

## Layout — DONE 2026-09-02 (Tyson: "Do we need to cram everything on one screen" / "Now")

One screen, your split. `workSplitter` (QSplitter, vertical): upper pane = plot with
the side column (corner picker, morph pad, PATH meter, POSTURE / SHARPEN / TRANSPOSE /
COPY ACROSS, status) to its right; lower pane = the console at full width. Faders
(WordDial, vertical Expanding) grow with the pane: 289 px at 1600x1000 by default, 529
px with the console dragged to 700. The split is remembered in QSettings ("Signal
Methods" / "TRENCH Workstation" / workSplitter) on every drag. Default split leaves the
console its sizeHint + 120 px. Suite 28/28 headless.

## Open

- DONE: each row's FREQ readout carries its note (`302.2 Hz D4+37`); the harmonic word
  off the overtone grid reads the interval from ROOT (`+27st 2-8ve+m3`, `-7st 5th
  below`) instead of a dash. Both derived, never stored.
- Console Hz are decoded at the 44,100 Hz datum; the prediction tests decode P2K at
  39,062.5 Hz. Confirm which datum the readouts should speak before trusting Hz
  against hardware.
- DONE: removed callerless `core/src/p2k/zeros_fit.cpp` and its dead public API.
