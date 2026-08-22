TRENCH — build the editor. Martens' PALETTE, in Rust, on a cube.

Repo: C:\Users\hooki\trench-x3-clean. Read this file, then NEXT_TASK.md and
AUTHORING_SPEC.md. Tyson is a producer, not a coder: direction arrives as
verdicts and reference images, never as code or exact values. Lead with the
result. Terse.

THE ONLY GOAL: Tyson picks an axis, hears eight filters spread along it, rates
them, the next eight land between the two he liked, and he ships the winner into
the BODY menu with his words recorded beside the geometry. If a task does not
put a control under his hand or a sound in his ears, it is out of scope.

Complexity is not the constraint. Correctness is. Build toward the right shape
even when the cheap shape is closer.

## THE DECISION: Rust + mlua + egui. One binary. No Python.

Three facts force this, in order.

1. A POLE LANDS WHERE YOU PUT IT. Measured on all 710 conjugate poles in the 33
   character bodies: of the 76% that produce a peak, the median offset between
   the placed pole and the audible peak is **0.02 semitones** (p90 2.70). The
   other 24% produce no peak at all.

   Placement does not need an optimiser. `scipy.optimize.least_squares` was the
   only thing keeping this project in Python, and it was solving a problem that
   direct placement does not have. `tools/feature_solve.py` lands 2 of 4 on a
   known-answer test. Placement lands 4 of 4 at 0.02 st. Delete the solver.

   The 24% that vanish is the real problem and it is a RULE, not a search: a
   zero sitting on a later section's pole deletes it downstream. Write the rule,
   check it, show the operator which section ate which peak.

2. A RECIPE IS A FUNCTION, NOT A TABLE. `cluster(1900, 3, fifth)` computes three
   frequencies from a base and an interval. "Notch any gap wider than 0.8
   octaves" is a conditional. PALETTE's whole method is sweeping a parameter of
   that function — eight bodies along an axis is the same recipe called eight
   times with one argument varied. Data cannot express an axis without
   pre-expanding it, and then the axis is gone.

   So recipes are Lua. `mlua`, sandboxed. Lua and not Rhai or a homemade DSL
   because it is thirty years mature in exactly this embedded role and it is
   what Reaper scripts in — an idiom that already exists in music tooling.

   Tyson never types Lua. The editor WRITES the recipe: he drags a point, the
   recipe updates, the response redraws. The script is the file format, not the
   interface. Getting that backwards makes it a config file with a VM attached.

3. THE FFI IS A LIABILITY. Bumping NUM_STAGES 6 -> 7 gave 27 Python tools a
   silent buffer overrun — 30 doubles allocated for what the DLL writes 35 into.
   It corrupted memory quietly until `inspect_body.py` segfaulted. It is patched
   (the DLL now reports its own dimensions) but the class of bug only exists
   because of the boundary. One binary removes the boundary.

This also settles the toolkit re-litigation for good. NEXT_TASK.md chose Dear
PyGui and an earlier session proposed Panel + Bokeh + PyO3 + CMA-ES + BoTorch.
Both are Python, both were chosen when the solver forced Python, and the solver
no longer does. Pick once and stop.

## ARCHITECTURE

    LUA RECIPE                  the authored part; stored, diffed, re-run
      anchor / cluster / notch / frame / axis definitions
      the rules  (no zero within N semitones of a later section's pole)
          |
          v
    RUST PLACEMENT              geometry -> 7 sections x 8 corners
      words_from_geometry, the Q-axis law, corner normalisation
          |
          v
    RUST CASCADE                the shipping engine, already built
          |
          +---------------------+
          v                     v
    egui PLOT               AUDIO OUT
      whole cascade           real engine, live, unlimited replay
      per-section curves
      running cascade S1..Sn
      pole/zero lanes
          |
          v
    RATING 1-9  ->  RESPACE  ->  next eight

Martens' module split, mapped:

    Parser -> descriptors -> Pscaler -> parameters -> Synth
    Parser -> judgments   -> Funk    -> functions   -> Pscaler

    descriptors = Hz, bandwidth, prominence, interval   (the Lua recipe)
    Pscaler     = Rust placement                        (build)
    Synth       = trench-core cascade                   (built)
    Funk        = ratings -> respacing                  (build; exists nowhere)

## MARTENS, VERBATIM — the design, not an inspiration

> "it should be possible to choose synthesis algorithms strictly in terms of
> their perceptual features without even looking at how an algorithm has been
> programmed."

RESPACE, the mechanism that converges in three or four rounds instead of
hundreds:

> "the results of the last curve fitting are used to predict how the synthesis
> parameter values should be spaced in the next so as to separate the timbres by
> roughly equal perceptual distances."

SEPARABLE, the precondition on an axis:

> "the dimensions along which the timbres differ must be analyzable — distinct
> enough that the rating of a timbre on one dimension will not be influenced by
> the value along the other."

Gestures, every screen. Keep them:

    left    audition at pointer, unlimited replay
    right   commit point + drop marker
    middle  finish + write file

Freed & Martens: include a variant you expect to be IRRELEVANT and check it
cannot be told apart. That validates an axis before you trust it.

Papers: `ref/martens/Martens_1985_ICMC_PALETTE_*.pdf`,
`ref/martens/Freed_Martens_1986_*.pdf`,
`ref/martens/Sandell_Martens_1992_*.pdf`. Quote them; never paraphrase.

## WHAT ALREADY WORKS — verified, do not rebuild

- The runtime is a CUBE. `NUM_STAGES = 7`, `NUM_CORNERS = 8`,
  `BODY_BYTES = 560`. 14th order, three axes, corner index `m | q<<1 | z<<2`.
  240-byte bodies load bit-identical through the compatibility path — re-check
  with `cargo run -p trench-core --release --bin topology-gate -- out.bin` and
  byte-compare against `fnv1a64 b2d8fdf2ff953582` over 204 bodies.
- `trench-core` cascade, AGC, SLAM, `words_from_geometry`, `body-from-geometry`.
  `cargo test -p trench-core` passes 167.
- `recipes/endpoints/*.endpoint.json` — 16 measured instrument bodies, each row
  a real mode with `pole_hz`, `pole_bw_hz`, `prominence_db` off actual audio.
  12 usable; the 4 Winter upright piano files measure zero prominent modes.
- `recipes/tables/dvtd_vowels.json` — Dresden Vocal Tract, frequency AND -3 dB
  bandwidth off one transfer function. The only source that sits naturally at
  formant spacing.
- `recipes/verdicts/*.json` — features asked, features achieved, body hash, and
  an empty verdict field. Extend this; do not invent a second record.

## THE ARCHITECTURE NUMBERS, MEASURED FROM THE 33 CHARACTER BODIES

On screen as a target the operator aims at, not a report afterwards:

    peaks per corner      4.0
    gap between peaks     0.50 octaves
    crown above median   17.8 dB
    dip below median     69.8 dB
    low-to-high tilt     25.5 dB

Best measured-source bodies so far: 2.0 / 1.24 oct / 19.3 dB / 43.0 dB /
10.8 dB. Crown is there; nothing else is. The 0.50-octave spacing is formant
geometry — instrument bodies do not have modes a fifth apart, vocal tracts do.

## WHAT TO BUILD, IN ORDER

1. Rust placement + the Lua recipe host. `anchor`, `cluster`, `notch_edges`,
   `frame`, and the rule check. Prove it by authoring one body whose four peaks
   land where the recipe says, and plot it beside Deep Bouche.
2. egui surface: whole cascade, per-section curves, the running cascade
   (S1, S1..S2, S1..S3 ...) which is what shows a zero eating a peak, pole/zero
   lanes, and the five architecture numbers live.
3. Audition on left-click, unlimited replay, through the shipping engine.
4. The axis picker and eight-at-a-time.
5. Rating 1-9 and RESPACE. This is `Funk` and it exists nowhere in the repo.
6. Write the record: axis, round, coordinate, rating, geometry, 30 words.
7. CUBE AUTHORING. Nothing writes a 560-byte body — `to_native_bytes()` has
   zero callers, no plugin parameter drives `FilterEngine::set_third_axis`. The
   editor targets 7 lanes and 8 corners NATIVELY; building against 6x4 and
   porting is the expensive order.

## WHAT TO DELETE — ask first, it all stays in git

- `tools/workstation/` (4,288 lines) and `tools/quartet_composer.py`. The
  workstation `BRIEF.md` declares itself "CANONICAL ROUTE" and freezes UI
  development pending an operator review of `corner_session.py` that never
  happened, dated 2026-08-11 — one day before `wordsheet` shipped past it.
  Three editors claiming canonicity is why none is finished.
- `tools/wordsheet/app.py` (Qt). Keep `core.py` as the reference for the
  geometry math while porting, then delete it too.
- `tools/feature_solve.py`, once placement is proven on the same targets.
- `CLAUDE.md` still names `quartet_composer` canonical. Fix it.

## ACCEPTANCE

Tyson runs one binary, names an axis, hears eight, rates them, sees the next
eight land between his two favourites, and ships one into the BODY menu with his
words beside the geometry.

Non-negotiable secondary: `cargo test -p trench-core` still passes 167, and the
topology gate still byte-matches `b2d8fdf2ff953582`.

## TRAPS

- Deep notches are the MECHANISM, not a defect. Factory bodies dip 69.8 dB below
  their own median. A previous session added a term to suppress them and
  flattened the thing that makes peaks read as peaks.
- 23% of factory conjugate zeros sit at r = 1.0000 exactly. A null on the unit
  circle is E-mu practice, not an error. The error is putting one where a peak
  was asked for.
- The word format cannot hold a zero outside the unit circle: `1 - r^2` goes
  negative and encode clamps to 0x0000, which decodes to r = 1.000 — a perfect
  null, silently.
- Editing `words[ci]` for `ci in 0..4` in place must mirror onto `ci+4`
  (`set_legacy_word`) or `to_rom_bytes` panics.
- `interpolate_biquad(...)` returns 7 rows; paths wanting 6 slice
  `[..LEGACY_STAGES]`.
- `render_is_absolute_not_normalized` in `tf_harness_routed.rs` fails and
  already failed at `6843e6497` with the same numbers. Its linearity premise is
  wrong for this engine. Not yours.
- The pairing is worth up to 65 dB at mid-morph and 0.000000 dB at both ends.
  Two endpoints do not determine a filter; 720 pairings hit the same two sounds.

## HOUSE RULES

- Never fill a data gap with a number. If a claim carries a number it goes where
  it can FAIL.
- No comments or docstrings in `tools/`, `pyruntime/`.
- `design/`, `docs/` and the commentary in `recipes/INTENT.md` are agent-written
  narration. E-MU's real descriptors are mixed into INTENT with invention in
  identical formatting. Quote the descriptors, ignore the rest.
- The measurement beats the plausible mechanism. Test before asserting.
- PowerShell for cargo; Git Bash picks the wrong linker.
- Ears are the only shipping gate.
