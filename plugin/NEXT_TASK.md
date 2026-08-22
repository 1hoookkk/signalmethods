# Authoring a cube — the manual, 2026-08-13

**This is reference, not the task. The task is `NEXT_SESSION_STATION.md`.**
Read that first; come here for what a section is, what the levers are, and
section 7, which separates what is measured from what was fitted.


Read `CLAUDE.md` first for the contract. This is the working manual: what the
machine does, what is measured, what is fitted, and the commands.

Every number below has a name in `bench/facts.py` and re-derives with
`python trench.py facts`. If a number here has no name there, it is a bug.

---

## 1. What a section is

**A pole and a zero. Nothing else.** It has no type, no role, no band.

E-MU's p.99 block diagram labels "1 Low Pass Section + 6 Parametric Equalizer
Sections", but the sentence above it calls that *"one of the possible ways that
the Morpheus filter can be configured"*. Measured over 792 factory sections:
**zero are a lowpass or a plain shelf.** 61.1% make a peak and a notch, 23.0% a
peak, 13.4% a notch, 2.5% nothing.  → `LOWPASS_SECTIONS`

There are **two kinds**, and the corpus is not a continuum — it is a 4.6× spike
against uniform:

| | zero vs its own pole | what it does | share |
|---|---|---|---|
| **contained** | within ±6 st | a local feature, band left alone | 37% |
| **spanning** | more than 18 st away | a feature **and** a slope | 42% |

→ `CONTAINED_SECTION_PCT`, `SPANNING_SECTION_PCT`

---

## 2. The three levers, measured through the real encoder

**Bandwidth sets height. Exactly 6 dB per halving.**
400→200→100→50→25→12→6 Hz gives 9.4, 15.4, 21.3, 27.3, 33.3, 39.6, 45.6 dB.
That is Rossum's radial law surviving the 16-bit encoder intact.

**The zero sets level and tilt. 34 dB of range from one number.**

```
  zero at 0.25x its pole   +18.0 dB   section RISES
  zero at 0.5x             +16.3
  zero ON the pole          +7.9      minimum output
  zero at 2x               +27.9      section FALLS
  zero at 4x               +41.6
```

A zero **below** its pole makes the section rise; **above**, fall. A zero
sitting on its pole is the quietest thing you can build.

**Pole spacing decides one feature or two.** Two identical poles closing from
2 octaves to a semitone: +20.9 → +37.6 dB, smooth, no threshold. Under 3 st they
fuse and arrive ~+14.6 dB hotter than either asked for; past 12 st they
decouple and 78.7% deliver within 6 dB.  → `STACK_EXCESS_DB`,
`INDEPENDENT_DELIVERY_PCT`

The combined peak sits **between** the two poles and walks as they move — that
is the whole of the "2:1 gearing". A pole does not own its peak.

---

## 3. dB ADD. Every section's tilt sums.

This is the trap. Tilt is LF minus HF, per section, and the total is the sum.

```
  Talking Hedz   ends  -50.1 +69.8 = +19.7      four voices +17.3   total +37.0
  first attempt  ends  -50.1 +69.8 = +19.7      five voices +33.0   total +52.7
```

Identical ends, wildly different totals. **Every contained voice with its zero
above its pole tilts positive**, and five of them out-tilted both ends combined.
Turning two into risers (zero *below* the pole) trims the sum back.

Level multiplies the same way: seven sections at −5 dB is −35 dB of broadband.

---

## 4. The three axes — E-MU's own words, 184 filters

`ref/MORPHEUS_AXIS_GRAMMAR.md`, data in `ref/morpheus_axis_grammar.json`.

| theme | Morph | Freq. Tracking | Transform 2 |
|---|---:|---:|---:|
| frequency move | **58%** | 36% | 19% |
| notch / peak shape | **39%** | 13% | 11% |
| key tracking | 1% | **43%** | 5% |
| brightness / tone | 7% | 22% | **34%** |
| resonance / depth | 9% | 14% | **31%** |
| **amplitude / volume** | 2% | 1% | **42%** |

**Morph picks the shape. Freq. Tracking places it on the keyboard. Transform 2
sets how much.** A Transform 2 that moves poles in frequency is not what E-MU
built.  → `TRANSFORM2_LIVE_CUBES`

---

## 5. Talking Hedz, decoded

Its whole design, and the only fully-read factory filter:

- **S1 spanning-rise** — pole 10523 Hz held still (−1.9 st across Morph), zero
  391 Hz climbing **+27.6 st** to 1931. Offset −57 → −27. Its zero is *damped*
  (r 0.935): a soft shoulder, no notch.
- **S6 spanning-fall** — pole 225 Hz climbing **+38 st** to 2020, zero 7221 Hz
  at **r = 1.0000 exactly** climbing +17.2 st. Offset +60 → +39.
- **S2..S5 contained voices** at 1006 / 1772 / 2651 / 5201 Hz, three of them
  with the zero within half a semitone of **+4 st** above their own pole.
- **The ends do 67% of all the movement** (spans 79 and 121 dB vs 12-41).
- **One SCALE, 0.56189, in all six sections.** The ends get their level from
  geometry — a tight low pole with a unit-circle zero 60 st above — never gain.
- **Morph mutes as much as it moves.** S4 goes 41 → 13 dB of span because its
  zero walks onto its own pole. S3 the same.
- **S2 and S6 trade places** and cross at Morph 40, both poles at 556 Hz,
  0.02 st apart — a bloom that exists at neither end of the wheel.
- **Q touches bandwidth only.** Every letter and offset is unchanged Q0 → Q100;
  bandwidths collapse toward ~12 Hz. (Hedz-specific: corpus-wide only 25.5% of
  Q100 rows land near 12 Hz, so use `Q_ARM` for a general rule.)

---

## 6. Build a cube

```
  cargo run -p trench-core --release --bin cube -- recipes/cubes/TABLE.json
  python tools/inspect_body.py recipes/cubes/TABLE.body 44100 OUT.png
```

`cube` prints five falsifiable checks — order, the 560-byte round-trip, that the
body **refuses** to fit in 240 bytes, what section 7 carries against the
identity, and how far Transform 2 moves it. Then the plate decides. **There is
no listening step.**

Table format: 7 sections × 8 frames, each frame
`{pole_hz, pole_bw_hz, zero_hz, zero_bw_hz, gain_db}`. Frame index is
`m | q<<1 | z<<2`. A section may be `"identity": true` — though no factory
filter ever idles one.

Worked examples in `recipes/cubes/`: `first` (all contained — the mistake),
`hedz_trim` (lands on the corpus at M0), `hedz_move` (ends moving as Hedz does).

**Hard limits the encoder enforces**
- SCALE ceiling **4.0 linear = +12.04 dB**. Above it, `cube` refuses with `Scale`.
- Frequency domain `fs/2048 .. fs/2`; authoring ceiling `0.49 × fs`.
- Pole radius is **not monotone** through the encoder — there is a hole just
  below `1 - r² = 2^-15`. Test the value, never a threshold.
- Editing a legacy corner in place must mirror onto `ci+4` or `to_rom_bytes`
  panics. `interpolate_biquad` returns 7 rows.

**Architecture to aim at** (33 character bodies): crown **17.8** dB above
median, deepest dip **69.8** below, low-to-high tilt **25.5**, ~4 peaks per
frame, ~0.5 octave between them.

---

## 7. Proven vs fitted — read this before trusting section 6

**Measured**: everything in sections 1-5. All of it re-derives from the corpus
or from the real encoder.

**Fitted**: the four numbers that put `hedz_trim` on the corpus — zero bandwidth
×6, riser depth −6 st, two risers, voice bandwidth ×0.40 — came from a grid
search against the architecture targets, not from a derivation. The *mechanism*
is measured; those four are a fit.

**Untested**: everything about a seventh section. No 7-section body has ever
been decoded. `LOWPASS_SECTIONS`, the tilt figures, the two letters — all come
from **P2K, which is 12th order and 4 frames**. A biquad is a biquad, so the
per-section physics carries; "S1 rises, S6 falls" is a statement about the two
ends of a *six*-section body.

---

## 8. Open

- **The swap.** Moving an end 38 st across Morph only works if a voice comes the
  other way. Hedz sends S2 down to 227 Hz as S6 climbs to 2020. Without it the
  climbing end lands on the voice stack — measured at 1.8 st, a collision.
  This is why `hedz_move` blows up to 46.5 dB crown at M100.
- **720 re-pairings per body** leave both endpoints bit-identical and move the
  middle of the wheel up to 95.47 dB. 29 presets × 720 = 20,880 filters, no new
  geometry, no fitting. Never attempted.
- **The cube ROM is not decoded.** `ref/rossum_morpheus/`. Do not start without
  `ref/morpheus_axis_grammar.json` as the oracle — it is 89 falsifiable
  predictions about what each filter's axes must do.
- **Truth pass unfinished.** Still to delete: `design/SLOT_GRAMMAR.md`,
  `design/STAGE_LAW.md`, `design/SPECTRAL_SCORE.md`,
  `docs/BODY_PIPELINE_INTENT.md`. Still to strip: `voice_roles` from ten
  `*.trenchprofile.json`, `tools/trench_profile.py`, `tools/author_body.py`,
  `ROLE_HZ_BOUNDS` in `tools/guide_projection.py`, `ROLE_BANDS` in
  `tools/mutation.py`. Those are invented section role-names still executing.
- **Pre-existing red test**, untouched: `bite_interstage::drive_generates_harmonics`
  wants drive 0.8 to lift H3 >20 dB and gets 19.2.
