# THE EDIT ROOM

Spec, 2026-09-05. The corner editor as E-mu's designer had it: one low-pass and five
peak bands, set by ear in musical terms. The tool does the pole-zero maths. Ruling:
"It's entirely perceptual and meaningfully musical." Nothing here changes the ship face.

## What a row is

A row is one pole and one zero. It has no type. The designer sees it as a band with a
place, a width and an amount, and the zero is placed by the amount:

- boost: zero on the pole, wider than it, by the amount in dB
- cut: zero on the pole, tighter than it
- tilt: zero far below the pole, one key
- ceiling: zero on the unit circle at the top, one key, row 6's default

After the drop the row is a pole and a zero again. The app remembers no mode.

## What the designer reads

Every row shows four things, in this order, and nothing else by default:

| PITCH | WIDTH | AMOUNT | ZERO |
|---|---|---|---|
| note and cents: `A3 +8` | semitones: `0.4 st` | dB: `+12 dB` | `on`, `tilt`, `ceiling`, or the zero's own note when unlocked |

- PITCH is the pole's frequency as a note name with cents. Below it, in grey, the
  interval above the lowest pole in the frame: `root`, `8ve`, `12th`, `2 8ve`, `17th`,
  or `+n st` when it is not a clean interval. Overtones are keyboard intervals.
- WIDTH is the pole's bandwidth in semitones, from the radius: bandwidth in Hz is
  `-ln(r) * fs / pi`, then to semitones about the pitch.
- AMOUNT is the band's peak in dB, measured from the row's own response, so a tilt and
  a ceiling read truthfully.
- ZERO says where the zero is in one word. Unlocked zeros read as a note and a width
  like the pole, on a second line.

Row 6 is labelled `ceiling` and opens with CEILING on and its pole as the cutoff. That
is the template, not an option. Rows 1 to 5 open with the zero locked on the pole.

Hz and radius exist behind one key, MACHINE, off by default. On, every row gains a
second grey line: `227 Hz  r 0.992  |  910 Hz  r 0.962`. Off again and it is gone.

## Typed and dragged

- Click a number to type it. PITCH accepts a note (`A3`, `A3+8`, `Bb2`), a Hz value
  with `hz`, or an interval from the root (`12th`, `+7st`). WIDTH accepts semitones, or
  Hz with `hz`. AMOUNT accepts dB. Enter commits, Escape cancels, Tab moves right, Shift
  Tab left, Up and Down move rows. Wheel over a number nudges: a cent, a tenth of a
  semitone, half a dB.
- Drag on the row's plot moves the pole: across is PITCH, up is AMOUNT with the zero
  following if it is locked. Drag the zero handle when unlocked.
- Keys per row: LOCK (zero on the pole), TILT (zero two octaves below the pole, wide),
  CEILING (zero at the top on the circle). One of the three at a time. Row number is the
  row switch, as now.
- OPEN stays, as the Klatt F1 control on the lowest pole.

## Level

One LEVEL number for the whole body, in dB, default 0 which is the DC unity rule: six
equal gain words whose product is unity at DC. Typing `-6` cuts the whole cascade by
6 dB, spread six ways, the hand cut E-mu made when a corner got loud. UNITY becomes
LEVEL `0`. The gain words are never shown per row.

## The far corner

The four corner keys stay top right with the crude responses. Selecting a corner shows
its rows. Beside each PITCH, in grey, the same row's pitch in the partner corner across
MORPH, so F1 reads `A3 +8   >  B6 -2` and the sweep is legible before it is heard.
SHARPEN stays: Q corners are the M corners with every width divided by four.

## The plots

- The cascade at the top, one white curve, as now.
- Six row plots, each the row's own response, the pole as a white square, the zero as a
  cyan dot, as now. The row plot's x axis carries note names at the octaves (`C2` to
  `C10`) instead of `100 / 1k / 10k`, with `1k` and `10k` only under MACHINE.
- The ARMAdillo on the right, pitches on its rim as notes at the octaves.

## Text rules

No sentences. No units where the column header carries them. Grey for derived values,
white for the ones you set, cyan for the chosen corner and its zero handles, yellow only
for the live playing position. Consolas 11, 8 px grid, 14 px keys, as everywhere.

## Acceptance

- Typing `A3` into PITCH gives a pole at 220.00 Hz within one cent after the word
  round trip. Typing `227hz` reads back as `A3 +54`.
- WIDTH `1 st` at A3 gives a radius equal to `exp(-pi * bw / fs)` for bw of one
  semitone about 220 Hz, within the word quantisation.
- AMOUNT `+12` with LOCK gives a row response whose peak is 12.0 dB within 0.3 dB.
- CEILING on row 6 gives a zero at r 1.000 at 20 kHz, matching the P2K byte rule.
- LEVEL `0` reproduces the six equal words of the bank on export, word for word.
- MACHINE off: no Hz and no radius anywhere in the room.

## Build order

1. Note, interval and semitone formatting and parsing in the model, with tests.
2. Row text in the new columns, MACHINE key.
3. Typed entry with the key handling.
4. TILT key and LEVEL.
5. Partner pitch beside each row, octave notes on the axes.
