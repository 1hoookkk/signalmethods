# Runtime Special Cases

Source: `EmulatorX.dll`.

This note separates universal runtime behavior from class-specific coefficient
writers. The formulas and addresses are observed. Descriptive names are
shorthand.

## Packed Interpolation

`FUN_1802c3d40` clamps both live controls to `0..1`, then bilinearly
interpolates each packed `u16` word. Each intermediate leg truncates before the
next interpolation leg. The interpolation occurs before minifloat decode.

## Packed Decode

`FUN_1802c3600` has explicit sentinels:

```text
decode(0x0000) = 0.0
decode(0xffff) = 1.0
```

After decoding, it recombines:

```text
c0 = 4 * d0 + d1
c1 = d1
c2 = 4 * d2 + d3
c3 = d3
c4 = runtime_scale * d4
```

`FUN_1802c0150` initializes `runtime_scale` at `object + 0x2a8` to `4.0`.

## AGC Table Construction

`FUN_1802bfa10` copies the 16-float base AGC table into each live instance.

```text
sample_rate <= 65000:  value
65000 < sample_rate <= 130000: sqrt(value)
sample_rate > 130000: sqrt(sqrt(value))
```

## AGC Processing

`FUN_1802c04e0` keeps a running multiplier at `this + 0x08` and reads the live
16-float table at `this + 0x44`. The index wraps with `& 0x0f`; it does not
clamp. Mono uses the sample magnitude. Interleaved stereo uses the larger
channel magnitude and applies one shared gain update to both channels.

If the updated gain is below `1.0`, samples are multiplied by it. Otherwise the
running gain is reset to `1.0`.

The repo's `agc_drive` pre-scale is an authoring and audition control. It is not
part of the observed DLL path.

## Morph Designer Padding

`FUN_1802c6590` skips descriptor types outside `1..3`. If at least four valid
rows compile, it forces runtime row count to six and pads missing rows with:

```text
[0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000]
```

## Adjacent Class Writers

The Type `1..3` Morph Designer grammar is not the only writer targeting the
corner banks.

`FUN_1802c59b0` writes a fixed three-row structure and duplicates both endpoints
across the Q axis. Its callers provide different fixed words or table-selected
profiles:

| Function | Class | Observed special case |
| --- | --- | --- |
| `FUN_1802c5d60` | `CPhantomMorph1` | fixed fallback words passed into the three-row writer |
| `FUN_1802c5e40` | `CPhantomMorphLP` | chooses one of 16 six-word profiles using clamped live control |
| `FUN_1802c5f10` | `CPhantomMorphLPX` | chooses one of 16 three-word profiles and mirrors them |
| `FUN_1802c6020` | `CPhantomMorph2` | independent fixed three-row writer with signed spread controls, boundary folding, and radius clamps |

Class names are the MSVC RTTI names reached from the dispatch table at
`0x1806d5f80`; each 32-byte record pairs a descriptor with the compile function
of the following record. These four plus `CPhantomMorphDesigner`
(`FUN_1802c6590`) are the five user-programmable morph targets.

The 16 six-word profiles `FUN_1802c5e40` selects live at `0x1806d73c0`, twelve
bytes each, little-endian. Five of the six columns are strictly monotone across
the 16 steps — three falling, two rising; the sixth wobbles by one lsb over four
of its fifteen steps.

These are class-specific authoring grammars. They should be treated as separate
metrology targets rather than merged into the Morph Designer Type `1..3`
compiler.

## Shared Frequency and Radius Law

`FUN_1802c59b0` is the three-row writer the four morph classes above feed. Its
arithmetic is the whole of their authoring grammar:

```text
base  = word[0x1806d73a0 + rate*4]         # [4896, 4500, 900, 220]
slope = word[0x1806d73b0 + rate*4]         # [ 442,  440, 405, 350]

v   = slope * descriptor_byte + base       # frequency code, per rate
rad = (v >> 1) + 0x6400                    # radius code
```

Radius is an affine function of frequency. Morph Designer's
`rad = ((freq * 0x7c) >> 8) + 0x76` is the same law with different constants:
expanded to word units it is `440*byte + 4608` against this writer's
`442*byte + 4896`. Two independently written compilers, one law. Neither carries
a per-section volume term; the fifth word is written as the constant `0xdfff`.

## P2K ROM Bank

Talking Hedz is not produced by the Q-collapsed Morph Designer or generated X3
writers above. The provenance-bearing P2K target is the 240-byte packed ROM bank
at `ref/presets/P2k_013_talking_hedz.bin`.

`FUN_1802d3ce0` is `CPhantomFilterP2k`'s bank writer. It reads a static table and
copies five-word rows into the corner bank. There is no arithmetic on the words.

```text
table  = 0x1806d7610                       # +0x1e reaches the first row
rate   = word[object + 0x0c]               # 0..3, the sample-rate family index
skin   = word[object + 0x18]               # 0..49
row    = table + (skin * 4 + rate) * 0xf0
```

The second index is **not a variant**. `word[object + 0x0c]` is the same
sample-rate family index every other `CPhantom` class uses to select
`base[]`/`slope[]` at `0x1806d73a0`/`0x1806d73b0`, and those tables hold four
entries. The table is 50 filters × 4 sample rates.

What each rate slot holds differs by half of the table, and the xStream law
("banks are distinct designs per rate, bank selection not remapping") describes
only one half.

**Skins `000`..`032`, the 33 stored bodies: one design, pre-warped.** Decoding
all four banks of a skin at one fixed rate, every live pole is an exact scaling
of bank 0's. Across all 31 skins with four comparable banks:

```text
bank1 / bank0 = 0.91878  (expect 44100/48000  = 0.91875)   31/31
bank2 / bank0 = 0.45939  (expect 44100/96000  = 0.45937)   31/31
bank3 / bank0 = 0.22969  (expect 44100/192000 = 0.22969)   31/31
```

Standard deviation 1e-4 or better, and internally exact within every skin. The
only base rate making that a real rate family is 44,100, giving 44.1k / 48k /
96k / 192k. **The baseline hardware datum for the skins is 44,100 Hz**, bank 0
is the 44.1k bank, and these four slots are pre-warps so the runtime never has
to move z-domain geometry per rate. 39,062.5 Hz is the Morpheus datum and does
not apply here.

**Primitives `033`..`049`, the 17 fixed classes: distinct designs per rate.**
None of the 7 testable primitives falls in that family on any bank. This is what
the closed-form compilers predict: `base = [4896, 4500, 900, 220]` and
`slope = [442, 440, 405, 350]` are not proportional across rates, so each rate
compiles its own filter rather than a rescaled one. The xStream law holds here
and must not be generalised to the skins.

The current `trench-core/src/hedz_rom.rs` fixture mirrors those packed words and
`Cartridge::hedz_rom()` enters the packed interpolation path. The retired
MorphDesigner-derived float/golden fixture was Q-collapsed and must not be used
as Talking Hedz truth.
