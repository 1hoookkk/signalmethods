# HEADSPACE vertical-slice specification

HEADSPACE authors a **240-byte, four-corner filter body**. Each corner is one complete 60-byte state containing six serial biquad sections × five 16-bit words. The application has only two working surfaces: **AUTHOR** and **BODY**.

## 1. AUTHOR

AUTHOR creates and edits the four corner states.

```text
C ───────── D     Q = 100
A ───────── B     Q = 0

    Morph 0 → 100
```

The two horizontal rows are independent 1D Morph edges: `A→B` and `C→D`. Their Morph sliders are **audition-only** and use the canonical packed-word interpolation. Moving a slider never changes a corner.

At startup the user selects either one or two compatible six-section templates. One template initializes `A=B=C=D`. Two templates initialize `A=first`, `B=second`, then copy `C=A` and `D=B` byte-exact. No Q, bandwidth, gain, or other transformation is applied automatically.

The user selects A, B, C, or D and then edits that endpoint. A perceptual/acoustic space, initially the vowel/formant space, writes directly into the **selected endpoint**. Changing the selected acoustic position updates that corner immediately and therefore updates its 1D Morph edge automatically.

### Corner construction

New all-pole anchors use six conjugate pole pairs. For every section, zeros and base gain begin parked:

```text
d0 = 0.25
d1 = 1.0
d4 = 0.25
```

giving:

```text
b0 = 1
b1 = 0
b2 = 0
```

Pole frequency `F` and bandwidth `BW` determine:

```text
R  = exp(-π BW / fs)
a1 = -2R cos(2πF/fs)
a2 = R²

d3 = 1 - a2
d2 = (a1 + 2 - d3) / 4
```

Words 2 and 3 must always be recomputed together when pole frequency or bandwidth changes.

### Ingest

AUTHOR accepts three source types:

**Table/template:** six `{frequency, bandwidth}` pole pairs. Compile the six pole pairs and park all zeros.

**Audio:** run the defined order-12 LPC extraction and accept only results resolving to exactly six valid complex-conjugate pole pairs. Sort them by frequency, compile them as the six sections, and park the zeros.

**Raw packed state:** byte-copy an already compatible six-section, 30-word corner. Existing zeros and gain are preserved. States using another section count, storage datum, or packed format are not directly accepted into this path.

### Zero editing

Zeros are a second editing stage after a pole scaffold exists.

A section zero is initially parked. Enabling it exposes zero frequency and bandwidth. For zero radius `Rz`:

```text
b1 = -2Rz cos(2πFz/fs)
b2 = Rz²

d1 = 1 - b2
d0 = (b1 + 2 - d1) / 4
```

Words 0 and 1 must be recomputed together whenever the zero moves or changes bandwidth.

After zero edits, apply the defined gain-normalization routine through Word 4. The UI must not independently invent another gain law.

### Copy hotkeys

The selected **endpoint**, never the auditioned interpolation state, is the copy source:

```text
1 → copy selected endpoint to A
2 → copy selected endpoint to B
3 → copy selected endpoint to C
4 → copy selected endpoint to D
```

The copy is byte-exact over all 30 words.

There is no operation that captures an in-between 1D Morph position into a corner.

### AUTHOR flow

```text
select one/two templates
        ↓
automatic A/B/C/D initialization
        ↓
select endpoint
        ↓
edit using acoustic space / poles / zeros
        ↓
sweep its 1D Morph edge to listen
        ↓
refine endpoint
        ↓
optionally copy with 1/2/3/4
        ↓
repeat for remaining corners
```

Audio and the response display always derive from the same currently auditioned 30-word state.

---

## 2. BODY

BODY does not author filter geometry.

It receives only:

```text
A B C D
```

and exposes the real runtime Morph × Q surface:

```text
C ───────── D
│           │
│     •     │
│           │
A ───────── B
```

Dragging the pad performs the canonical bilinear packed-word interpolation across the four corners and sends that exact resolved state to both audio and the response display.

BODY is strictly read-only with respect to A/B/C/D. It contains no template controls, perceptual-space editing, pole editing, zero editing, automatic Q generation, or corner capture.

Export serializes the existing four packed states in canonical order:

```text
A | B | C | D
```

for exactly **240 bytes**.

## Hard invariants

**Only explicit endpoint editing or the `1–4` copy commands may change a corner.** Both 1D Morph audition and the 2D BODY pad are read-only.

The six sections remain in strict serial cascade. Canonical packed encoding, decoding, interpolation, section order, sample-rate datum, response evaluation, and export logic must be reused from the existing core rather than reimplemented inside ImGui.
