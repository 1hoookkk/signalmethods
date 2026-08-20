## The mental model

There are four concepts, and the operator needs no others.

```
BODY
│
├── CORNERS
│    ├── target
│    └── seven-section realization
│
├── CURRENT CORNER
│    └── optional CURRENT SECTION
│
└── CURRENT INTERPOLATION POSITION
```

The interpolation position has no product nickname. It is the current M/Q/Z position and nothing else.

Three facts carry the whole interface:

* **The corner says where edits go.**
* **The section says what has focus.** It does not change the meaning of FIT.
* **M/Q/Z says what you are hearing.** It never moves the authoring selection.

## One canonical body state

There is exactly one mutable authored representation. It holds:

```
8 corners
7 ordered stages per corner
pole geometry
zero geometry
scale
hold/free state
corner targets
provenance where applicable
```

It must express **conjugate roots and real-axis pairs**. A conjugate-only representation corrupts factory bodies; geometry carrying either form round-trips the corpus bit-exactly.

**Packed words are derived from that state.** Never again editable lanes plus separately editable words. When a view needs words, it derives them; it does not own them.

## Required capabilities

These are the executable form of the model. Each is a testable requirement, not a preference.

**1. Unified global selection — (corner, section).**
Selecting a corner updates the entire interface to show that frame's data and only that frame's data. There is no view left displaying another corner's state.

**2. Corner-bound target comparison — the ghost backdrop.**
The synthesized spectrum of the current corner is superimposed over that corner's target spectrum. The visible difference between the two curves is the error the user is minimizing, and it is readable without a numeric panel. The target is the corner's, never a global overlay.

**3. Direct pole/zero specification — no archetype shorthand.**
The user specifies pole and zero locations directly, and the response is computed strictly from those locations plus scale. The interface never asks for, infers, or displays a filter type, class name or archetype in order to make the solver work.

**4. Parameter-space position graphic.**
A graphic of the parameter space — square for M/Q, cube when Z is exposed — carrying a crosshair at the exact real-time interpolation point. It shows position; it does not author.

**5. Real-time frequency response.**
Moving the interpolation point animates the current frequency response live, so the interpolation between authored corners is seen and heard at the same time. Live during the gesture, not on release.

## Corner selection reconnects the workstation

One click on C2 immediately:

```
selects C2
shows C2 target, response, sections, roots
moves the interpolation position to C2
updates audio without stopping it
```

There is no separate bind, load-endpoint, audition-this-corner or make-active step. Clicking the corner is the whole operation.

Selecting a corner is navigation, not an edit.

## Section focus

Clicking S4 highlights S4 in every view. It may then be dragged, cleared, held, inspected or replaced.

Focus changes nothing about what FIT solves.

## Interpolation position

Moving M/Q/Z sends the actual encoded interpolation position to audio and displays the interpolated response.

It must never modify a corner because the cursor passed over it, and never change the current corner. Its job is verification of the real machine between authored states. The interior is produced by corners and correspondence; it is not authored separately.

Clicking a corner sets both the current corner and the interpolation position. Afterwards the position may be moved away while the corner remains where edits land.

## Targets belong to corners

```
C0 ─ target A
C1 ─ target B
C2 ─ target C
C3 ─ target D
```

A target may come from an analysed WAV, a time slice, a generated curve, a PCA reconstruction, or an imported reference response. Once assigned it belongs to the corner.

There is no global floating target. Analysis creates a target; it does not become a second persistent editing context.

## Recordings are sources, not modes

Recordings appear as assets. A recording carries only what is needed to reproduce its target:

```
label
analysis mode
time or range where applicable
frequency band
optional reference source
corner assignment
```

The workflow is `ADD → inspect → assign to a corner → FIT`. There is no special workflow because a target came from audio.

## FIT

FIT is one atomic corner command.

`select C2`, `FIT` means: fit C2's complete seven-stage serial response to C2's target using the currently free variables.

Candidate progress may be displayed while solving. On success there is **one result, one audio update, one undo entry**.

* No ACCEPT. Ctrl+Z is rejection.
* Escape while fitting cancels and leaves authored bytes unchanged.
* If the corner changes or anything is edited before an asynchronous result returns, that result is stale and must not land.

The complete serial cascade is always the comparator.

## Authoring order

Anchor the formant resonators first, in **S1..S5**, then apply tilt and gain trim in
**S6 and S7**. Automatic placement follows the same order.

This is corroborated by the corpus rather than assumed: P2K bodies carry exact
power-of-two headroom cuts at S3 and S6 with never a compensating boost elsewhere, so the
late stages are where level is handled.

## Holds are the only solver constraint the user needs

The user says only: this geometry may move, or it may not. Hold state is exposed directly on the section or root being manipulated.

Locking is binary. Do not display a third lock state, and do not display "empty" as though it were one — an empty lane is already visible by having no root on its plot.

The solver does everything else internally. This does not justify panels of optimizer settings.

## Direct manipulation and FIT are the same mutation

Dragging a pole changes the body. FIT changes the body. Afterwards they are indistinguishable.

There is no manual-result versus fitted-result state. Both immediately update section curves, summed response, roots, derived words, audio, and audit-stale state.

## Playback is power to the instrument

`Space` starts playback and it stays on across corner selection, M/Q/Z movement, root dragging, section changes, FIT, undo and corner copy.

Pointer-up on the interpolation control must not stop playback. The user should never re-audition; if playback is on, changes are heard.

## Layout follows the signal path

The body is a serial cascade. The interface is laid out as that cascade, left to right, in signal order.

```
IN → S1 → S2 → S3 → S4 → S5 → S6 → S7 → OUT
```

The section strip is a **selector, not an editor**. Each cell shows that section alone —
it answers "what is this section", which is what a picker must answer. The running signal
belongs to the response views, and to the per-section decomposition below.

Consequences that must hold:

* Cells are equal width and never move. Position identifies a section, so a layout that
  resizes or reorders cells destroys the identity it is meant to carry.
* All seven stay visible at once. A section that is unused or bypassed dims; it never
  shrinks and never leaves the chain.
* Section order is signal order. Never reorder cells for visual tidiness.
* Cells carry no text but their identity, and no per-cell verbs. One verb row acts on the
  current section.

Selecting a section must expose its causality — what arrives, what it does, what happens
downstream, and what leaves.

## Selection drives one inspector

Structure on the left, signal in the middle, properties of the current selection on the right.

```
STRUCTURE          SIGNAL PATH                    INSPECTOR
body, corners,     S1 … S7 signal-so-far          properties of whatever
sections, sources  RESPONSE, ROOTS, M/Q/Z         is currently selected
```

There is exactly one place to select a corner and exactly one panel that describes it. Selecting a corner fills the inspector with that corner: its target, its residual, FIT. Selecting a section fills it with that section: pole, zero, scale, hold.

A panel must not become a second place to make the same selection. Direct manipulation still happens on the plots themselves — the inspector reports and edits the same state, it does not become the only way to change it.

These are not separate tools. A modification in one view becomes visible in all of them on the next render.

## Undo

History contains authored mutations only: root drag, section clear, corner copy, target assignment, FIT.

It never contains selection, section focus, M/Q/Z movement, playback, hover, opening a recording, or plot zoom.

One pointer gesture is one entry. One FIT is one entry.

## Copy

Copy operates on domain objects: copy corner, replace corner; copy section, replace section.

Do not add nouns for temporary implementation states.

## Body operations

AUDIT evaluates the completed encoded surface. WRITE emits the body. Both are whole-object operations and belong to neither the section nor the target.

Audits are safety gates, not design advice.

## Command vocabulary

```
BODY       NEW  AUDIT  WRITE
CORNERS    C0 C1 C2 C3 …   COPY  CLEAR
TARGET     ADD  ASSIGN  AVG / SLICE where applicable
FIT        FIT
SECTIONS   S1 … S7   HOLD/FREE  CLEAR  P↔Z while it remains useful
PLAYBACK   PLAY  M  Q  Z when applicable
```

Do not add a verb until you can state the domain mutation it performs.

## Never require

Treat any of these as a UX regression:

```
LAND before FIT
ACCEPT after FIT
selecting the same corner in more than one panel
choosing which section FIT operates on
assigning a filter type to make the solver work
deriving Q100 from a statistical type average
manually maintaining lane order
stopping playback to inspect an edit
a separate target, fit, or interpolation-audition mode
navigation in Undo
duplicated lanes/words state
mandatory template seeding
mandatory stage roles
```

The division of labour: **the user authors corner responses and, where necessary, constrains their realization. The workstation handles representation, synchronization, fitting, interpolation playback and bookkeeping.**

## What this design may not change

Interaction work never alters the packed-word interpolation law, the corner index law, the encoder, the solver objective, lane ownership, section order, the response grid, sample-rate datums, or parity behaviour.

## Visual and interaction language

The workstation is technical instrument software, not a web dashboard.

Design from the traditions of scientific visualization, EDA, signal-analysis and workstation software: dense, explicit, plot-centric, direct and operational. Do not imitate a historical OS skin.

Use modern browser behavior with disciplined technical presentation.

### Visual rules

- Use neutral light/cool-gray structural chrome around dark measurement wells.
- Do not default the application to a generic dark theme.
- Dark backgrounds belong primarily to plots and data surfaces.
- Use square geometry: 0–2 px radius, 1 px structural borders, compact spacing.
- No cards, glass, gradients, soft shadows, floating panels, decorative blobs or marketing-style whitespace.
- No generic AI-product visual language.
- Do not introduce ornamental animation.
- Panels dock against each other; plots receive available space before chrome does.
- Keep controls compact but usable.
- Do not use browser-default widget appearance. Preserve native semantics where useful, but style every visible control as part of this workstation.
- Do not invent a new visual treatment when an existing workstation primitive can express the same thing.

### Information color

Color represents domain information or state, never decoration.

Stage colors S1–S7 are semantic identities and must remain consistent across every view.

Selection must not replace semantic color; indicate selection through outline, weight, marker size or chrome state.

Reserve warning/error colors for actual warning/error conditions.

### Plots

All plots representing the same domain must use the same coordinate transform, margins, ticks and alignment.

RESPONSE, ROOTS and section views must visually agree about frequency position.

Use thin grids, restrained axes and bright data traces.

Plots are working surfaces, not illustrations.

Do not add legends when spatial position, stable color or direct labeling already identifies the data.

### Typography

Use complete technical words where space permits.

Do not abbreviate ordinary UI words merely to save a few characters.

Domain notation such as S1, M, Q, Hz and dB is appropriate.

Permanent text should normally be a noun, verb, unit, identifier or measurement.

Avoid explanatory sentences in the primary workstation.

Prefer:
`FIT 0.48 dB`
over:
`The fitting process completed with an RMS error of 0.48 dB.`

Use small, highly legible typography. Numeric measurements and technical identifiers should use monospaced numerals.

Hierarchy comes primarily from layout, bands, alignment and borders rather than oversized type.

### Interaction

The workstation should behave like an instrument.

Selecting an object makes it the thing being operated on.

Direct manipulation should alter the represented object immediately.

Playback, selection, fitting and editing should compose without modal workflow overhead.

Prefer reversible immediate actions plus Undo over confirmation dialogs.

Do not introduce modes, dialogs, setup steps or duplicate controls unless the operation genuinely requires them.

Do not expose implementation bookkeeping in the interface.

### Density

Optimize for information density, not emptiness.

Empty space is justified only when it improves interpretation or interaction.

Reclaim unused panel area for plots and working surfaces.

Do not compress controls so far that labels become cryptic or interaction becomes difficult.

### Test for every new element

Before adding permanent UI, establish:

1. the technical object it represents;
2. the operation it enables;
3. why it deserves permanent space;
4. whether that state is already represented elsewhere;
5. which existing visual primitive it uses.

Adding a new visual primitive requires a real functional reason.
