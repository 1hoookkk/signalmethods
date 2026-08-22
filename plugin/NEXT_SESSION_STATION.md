# The station — Rust, egui, custom painter only

**This is the task.** `NEXT_TASK.md` is the manual behind it.

`station/` builds and runs. Continue it. Do not start a new app.

    cargo run -p station --offline
    cargo run -p station --offline -- recipes/cubes/hedz_trim.body
    cargo run -p station --offline -- BODY.bin station/laws.json

## Topology

A serial cascade of second-order sections. Responses multiply, dB add, nothing
is local. A section is a pole and a zero — no type, no role, no band.

That is the whole of it. Everything below is storage.

## Storage

**SECTIONS and FRAMES. Never a byte count.**

    NUM_STAGES    sections in the cascade
    NUM_CORNERS   frames the body stores
    NUM_COEFFS    words per section
    BODY_BYTES    what falls out of those three

Ask `trench_core` for all four. A byte literal anywhere in the station is a bug:
the day a section or an axis is added, every literal is wrong and the constants
are already right.

Word row is `[zero-mag, zero-r^2, pole-mag, pole-r^2, SCALE]`. SCALE is that
section's own level, one per section.

Frame index is `m | q<<1 | z<<2` — Morph varying fastest, frame 1 the
all-axes-zero frame, the third axis the rear plane. Three axes is a **cube**,
two a **square**. The axes are **Morph**, **Freq. Tracking**, **Transform 2**.

A square loads as its sections plus an identity remainder (`IDENTITY_STAGE`,
multiplies by exactly 1.0) with its frames duplicated onto the far plane.
In-place edits to a legacy corner must mirror via `set_legacy_word`.

Between frames the runtime linearly interpolates the **encoded words**. Nothing
is refitted in between; that is the whole morph.

## What it is

Eight corner cards (Z0 floor / Z1 ceiling) drawn from the open body, and the
laws you wrote measured against the selected frame. Click a card to select it;
arrows move MORPH, Q and T toggle their axes, R reloads `station/laws.json`.

Every mark is placed by `src/paint.rs`. **No egui widget is used anywhere** —
no Button, Slider, ComboBox, ScrollArea. Keep it that way.

    src/theme.rs      colours, mono font
    src/paint.rs      hairline, frame, label, curve, gauge, decades
    src/body.rs       load any body, responses, roots, scale
    src/law.rs        laws.json -> measured quantities -> pass/fail
    src/cube.rs       the eight cards
    src/laws_panel.rs the law list
    src/app.rs        layout, keys, hit-testing

## The one rule about laws

`station/laws.json` is his. The station measures and reports; it never proposes
a bound and never fixes anything. Adding a quantity means one arm in
`law::measure`; it is then usable from the file immediately.

## Next, in order

1. **Drag roots on a card.** Selecting a frame works; editing it does not.
   Needs hit-testing against pole/zero positions and a write back through
   `stage_law::words_from_roots_at` into `packed.words[frame][s]`.
   `set_legacy_word` mirrors onto the far plane — use it or `to_rom_bytes`
   panics.
2. **Save.** Nothing writes yet. `to_native_bytes` always; `to_rom_bytes`
   only when `is_legacy_representable`.
3. **Section strip for the selected frame**, on one fixed dB scale with
   `pole Hz r / zero Hz r` under each, as `dev/reference/hedz_endpoint_sections_AUTHORED.png`.

## Hard limits, all measured

- SCALE <= 4.0 linear (+12.04 dB). Above it the encoder refuses with `Scale`.
- Frequency `fs/2048 .. 0.49*fs`.
- Pole radius is **not monotone** through the encoder — a hole below
  `1 - r^2 = 2^-15`. Test with `radius_survives_encoding`, never a threshold.
- Two roots per section. No third-order section exists.
- The morph is **linear in stored words**. It cannot be curved.
- `interpolate_biquad` returns `NUM_STAGES` rows, not `LEGACY_STAGES`.

## Do not

- Do not write a new minifloat. `minifloat::encode` is the encoder;
  `(k * 2048.0) as u16` is not.
- Do not sort a corner's stages. Correspondence is the authored quantity —
  permuting one end silently moves the middle up to 95.47 dB.
- Do not name a section. It is a pole and a zero. Naming a **corner** is fine.
- Do not add a per-section gain control. Bandwidth sets height at 6 dB per
  halving; the zero's offset is worth 34 dB. `.claude/hooks/cascade_detector.py`
  blocks it.
- Do not enforce 0 dB at DC. Factory bodies tilt 25.5 dB on purpose.

## State

`python trench.py doctor` HEALTHY. Gate `5187ee1fb8f8f27d`, 184 bodies.
`trench facts` no drift. Pre-existing red test, not ours:
`bite_interstage::drive_generates_harmonics`, wants >20 dB H3 lift, gets 19.2.

Everything measured this session is in `bench/facts.py` with a derivation, and
`NEXT_TASK.md` is the working manual — read its section 7, "Proven vs fitted",
before trusting any number in it.

The Python editor `tools/wordsheet` still works and is the reference for the
geometry math while porting.
