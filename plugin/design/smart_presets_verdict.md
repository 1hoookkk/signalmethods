# Smart presets — verdict

2026-08-04. Source: Tyson's smart-presets vision dump. Verdict: **two buildable
things, three already built, two out.** Everything condenses to one new feature
and two preset families.

## The through-line

**The LISTENER** — a control-rate stage that watches the input and turns what it
hears into MORPH/Q movement. Sources: pitch (mono vocal/instrument), RMS energy,
transient density. Each source maps through a non-linear curve with amount and
response-speed parameters. Nothing new in the DSP thread: the engine already
steers both axes per block, the plugin already has modulation plumbing, and
pitch detection is already proven in the capture rig. This is the one real build.

**Family A — FOLLOW** (instrument): the filter chases the note. Auto-wah on
vocal/keys/guitar. Formant-dodging excluded (below).

**Family B — BUS** (glue): GLUE (low BITE + slow AGC sag), SUB-AIR (corner-locked
sub band + high-mid notch air), BREATHE (RMS-follow morph drift). All body work
in the existing pipeline, ears judge.

## Per idea

| Idea | Verdict |
|---|---|
| 1. Formant & pitch tracking | **Partial.** Pitch-follow = the LISTENER, build it. Formant-wrapping (dodge the voice's F1/F2, preserve intelligibility) is a different product — a smart vocal EQ — and geometrically foreign: notches live where the body puts them, and a filter that moves its notches around the voice stops being a filter. Park it. |
| 2. Q-auto-compensation | **Already built.** The +24 dB broadband peak cap + AGC leveller. An energy-adaptive version is possible but insurance that sits still beats insurance that dances. Low priority. |
| 3. Runtime linter / manifold deformation | **No.** Transients can't destabilize the cascade — coefficients only move when MORPH/Q move, and those moves are certified and ramped before the audio thread. And live reshaping rewrites the body at runtime; ROM bodies stay verbatim, always. A body that can blow up is killed at authoring, never patched live. |
| 4. Control-rate feature mapping | **Yes — this is the LISTENER.** Snare-triggered micro-sweeps, intensity-widened morph corridor = presets on one source. |
| Distributed interstage saturation | **Just built** (the BITE work, 2026-08-04). Voltage sag = a slow glue preset on top. **BITE is GRIT** (2026-08-04): one knob, both mechanisms — bus presets ride the GRIT knob. |
| Sub-anchor / air split | **Body work.** Sub-anchor = the low band authored identical across all four corners so the wheel can't disturb it (corner-constant features are proven in the corpus). Air = high-mid notch carving, already how the machine does air. No new engine. |
| Spectral breathing | **LISTENER + gentle body.** The masking-prevention claim is marketing; the real thing is slow morph drift with energy. |
| Neural spectral envelope / all-pass phase decoupling | **Out.** Research-scale, no ear evidence, and phase-decoupled magnitude shaping is parallel-bank thinking in a serial machine. |

## The LISTENER — built and proven (2026-08-04)

`trench-core/src/listener.rs` — control-rate pitch follower. Frame-based
autocorrelation (60–500 Hz vocal range, octave guard, voicing gate), glide
smoothing, and a self-centring reference (τ 2 s): ratio = 2^(offset·amount),
clamped ±12 st, multiplied into the engine's pitch_ratio — body verbatim, the
datum moves. Engine setter `set_listener(amount, speed_ms)`, FFI
`trench_engine_set_listener`. Probe: `listener-probe <body.body240>`.

**Evidence (true rate, packed bytes):**
- Tracker: locked within 3% of the known melody f0 in **99.6%** of voiced
  frames; 440 Hz tracked at 440.19 (0.04% error).
- Integration test (resonant body, A3→A4 step): follower holds gain at the
  voice's f0 **+8 dB** over static on the high note.
- P2K_Multi_Q_Vox: follower +6.3 dB at 2×, +4.3 dB at 6× (the harmonics the
  body colors — its peak sits at 600 Hz). TYSON_WAH/TYSON_VOX lose ~5 dB: their
  response RISES through the voice band, so transposing pushes the voice down
  the slope. **Follow's value is body-dependent — the body's features must
  ride the voice band.** A follow-specific body is the next authoring job.

## Oversampled saturation (2026-08-04 follow-up)

Gemini's "polyphase oversampling with zero phase smear" is the only new engine
question, and it is **buildable without breaking the no-SRC law**: the cascade
stays at the authored host rate, words verbatim — only the memoryless interstage
saturator is interpolated (upsample → saturate at 4× → lowpass → decimate),
exactly what `Oversampler4x` already does in the probe binaries. It is real
surgery in `cascade.rs` (block-based interstage instead of sample-based) and a
second build after the LISTENER, not a law violation. Verdict: **build #2,
worth it for the bus family** — the aliasing probe shows measurable foldback at
high drive, and glass-clean treble is the bus pitch.

## Secret sauce bussing and master — the recipe (2026-08-04)

E-mu's own templates ("Multi Band Cut + Boost", "Multi Peak Hi Pass", "Harmonic
Tweak", "HiClassLoPass") confirm the grammar: their multi-band filters were
alternating carve/boost designer sections — the exact ROM grammar our bodies
already speak (zeros carving valleys, peaks riding passbands). The bus family
is body work in that grammar plus the new surface:

- **GLUE** — GRIT at low amounts (0.05–0.15): the resonance-coupled interstage
  saturation is the glue; the slow AGC does the sag. No audible crunch by
  construction — low drive, soft curve (0.25·(1−d)² stays soft).
- **SUB-AIR** — sub band authored identical across all four corners (corner-
  constant law, proven in the corpus) so the wheel cannot disturb it; air =
  high-mid notch carving in the passband, the machine's native air.
- **BREATHE** — LISTENER slow-follow of RMS → gentle morph drift.

Ear gate unchanged: bodies judged against the loved list.

## Key-locked anchors (2026-08-04 follow-up 2)

Gemini's "key-locked spectral anchors" is the one new item: detected key shifts
the whole response so notches land on the mix's harmonic intervals. This is
classic key-follow (synth filters followed the keyboard), applied via the
existing **Hz-anchored recompile** path — the body stays verbatim, the datum
moves. Buildable as LISTENER extension #2. Caveats: the KeyDetector is
editor-gated today (~6s chroma window, runs only while the UI is open) — shipping
it means running the model always; and key decisions are slow, so this drives
slow anchoring, never pitch-follow (that's the capture rig's f0 autocorrelator).

Gemini's other two points are already the plan: multi-band blueprints = the bus
body recipe above (the E-mu templates are now the reference shapes); light
PREAMP + soft knee = GLUE. No new work.
