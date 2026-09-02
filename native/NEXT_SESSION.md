# CHECKPOINT 2026-09-02 (evening) — pick up fresh here

Branch `face/ship-candidate-fx`, native tree committed (native suite 19/19
headless). Launcher `TRENCH Workstation.bat`; build `cmake --preset app &&
cmake --build --preset app`; tests `--target trench_native_tests`, then
`ctest -R ^native\.` in `out/build/app`. Never two vcpkg-configuring builds at
once. Commit native work with explicit pathspecs only (other chats stage
`plugin/` in the same index).

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
templates; suite 18 cases + registry, all green. `core/src/p2k/zeros_fit.cpp`
(fit_zeros_under) is still in core with no caller - erase when core is next
touched.

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

## Open

Whether Frequency shows note names beside Hz. Whether the Frequency fader
can be told "root x n".
