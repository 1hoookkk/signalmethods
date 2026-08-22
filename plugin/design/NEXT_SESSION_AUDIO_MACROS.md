# Next session: the audio-first modulators

2026-08-04. Tyson's feature brief, mapped to engineering. The through-line:
**modulation is motion is distortion** — one shared control-rate pipeline, three
macro sources, two wheels, zero separate routing menus.

## Already built — no work, reference only

| Claim | Reality |
|---|---|
| Resonance-BITE loop | **DONE** — `cascade.rs`: `local_drive = g + 6·r·drive`, Q motion already drives the interstage saturation automatically. "Modulating motion is modulating distortion" is true today. |
| Pitch follower | **DONE** — `listener.rs` (ACF tracker), engine `set_listener`, FFI `trench_engine_set_listener`. |
| Key snap (semitone) | **DONE** — engine `key_snap`, FFI setter. Missing: the *detected-key* feed (below). |
| Voltage sag backbone | **DONE** — the AGC leveller (slow, musical, already shipping). |
| Hz-anchored transposition | **DONE** — `pitch_ratio` (body verbatim, datum moves). |

## The pipeline (one shape, three sources)

```
input tap (pre-cascade, same tap the listener uses)
  → source (ENV | KEY | PULSE)
  → control-rate offset (per control block, BEFORE set_parameters)
  → existing CORDS smoothing (the wheel's lag law)
  → set_parameters(morph + dmorph, q + dq)      ← nothing else changes
```

The LISTENER is the template for all three. Bodies stay verbatim — macros move
offsets and the frequency datum, never rewrite the body.

### 1. ENV macro — envelope follower (the drum-pop / pad-breathe one)

- **Source**: peak/RMS of the input tap, one-pole attack ~1 ms / release ~200 ms.
- **Law**: `offset = (env − ref) · amount`, clamped to ±1 wheel unit. ref is the
  listener-style slow EMA so it centres on the arrangement's average level.
- **Destinations**: MORPH offset, Q offset, and/or PREAMP drive (Bloom).
- **Proof**: drum hit pops a formant open, quiet pad decays back into warmth.
- Check `motion.rs` / `keyframe.rs` first — LFO/envelope machinery may already
  exist in core to reuse instead of building fresh.

### 2. KEY macro — harmonic snap (the puck gesture)

- **Source**: the RTNeural KeyDetector (already in the plugin, editor-gated
  today — shipping KEY means running the model always, or a manual key
  override on the face; perf check first).
- **Law**: detected tonic root → semitone offset; the puck (or wheel) frequency
  offset snaps to the **nearest harmonic fundamental of the detected key**
  (root, fifth, third — the intervals the body's notches already speak).
- **Gesture**: double-click or Ctrl/Cmd-drag on the puck/wheel = snap.
- **Core**: a `key_offset` semitone value fed like `key_snap`; the snap law in
  engine (nearest of the key's harmonic set, not free semitones).

### 3. PULSE macro — tempo LFO (the kinetic halo's driver)

- **Source**: DAW tempo (host BPM → FFI `set_tempo`), phase accumulated at
  control rate.
- **Law**: sine or triangle on MORPH or Q offset; depth = halo width; rate
  1/4, 1/8, 1/16 (and whole for slow pads).
- **Check first**: motion.rs / keyframe.rs may already have a clock/LFO.

### 4. PREAMP Bloom (envelope → saturation)

- Static mode = current fixed gain. Bloom mode = the ENV source rides
  `preamp_drive` (the desk input gain already feeds `trench_saturate`), so
  chord attacks push the curve toward the hard clip, decays relax it.
- Core: one toggle + reuse ENV. UI: knob-center click.

## Surface (plugin face, UI laws apply: restraint, no reference, original naming)

- **Dual-wheel + linked XY canvas**: drag on the screen moves both wheels
  (X → MORPH, Y → Q); hover+scroll micro-adjusts one axis; double-click a
  wheel snaps to its preset default.
- **Kinetic Halo**: an arc around each wheel showing the active modulation
  range (depth + current offset). Right-click/scroll on the wheel sets the
  halo width — assignment never leaves the main view.
- **The 3-macro bar** under the visualizer: ENV / KEY / PULSE. Assign in one
  second: click the macro, touch a wheel, drag the halo width. Done.
- **No "Z-plane", no reference terminology, anywhere.** Original product
  naming only (house style).

## Build order (one change per verdict, ears gate everything)

1. **ENV** — core + proof (drum-pop test), then Bloom (one-line core + knob
   click). Proves the whole macro shape before the rest.
2. **KEY** — KeyDetector always-on check, key_offset law, puck snap gesture.
3. **PULSE** — tempo clock, halo driver.
4. **Halo + XY canvas** — surface polish, only after 1–3 give the halo real
   data to show.
5. Ship gate: every macro against the loved list, ears only.

## Laws (no exceptions)

- Bodies verbatim — macros move offsets/datum only.
- One change per verdict; a direction failing twice changes the method.
- UI restraint law; no reference-naming on any visible surface.
- Every feature proven with runtime evidence (packed bytes, true rate) before
  it's claimed.
