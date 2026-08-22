# Native workstation redesign — 2026-08-22

Scope: the Qt app under `native/app` and the parts of `native/trench-core` it drives.
Everything below is derived from reading the code and the measured P2K corpus
(`dev/audit_interior.py`, the 33-body role census in the session log); nothing is
a manual rule.

## 1. What the code is today, and where it fights the job

### Ownership as built

| Thing | Owner today | Problem |
|---|---|---|
| The body (4×6×5 words) | `BodyDocument` (`app/body_document.cpp`) | Fine. One source of truth, undoable. |
| Which corner is being edited | `BodyDocument::corner_` | Fine (added today). |
| The *authoring vocabulary* | none — the UI exposes raw `(mag, r²)` words as draggable pole/zero tokens | The designer never authors in words. Every factory row has a **role** (tilt / peak / notch / parked) that is derivable from geometry but is nowhere in the model. |
| The perceptual space the fit is judged in | `p2k::grid()` — a fixed 512-point log grid 20 Hz…0.499·Sr with fixed ERB weights, baked into a function-local static (`objective.cpp`) | Not configurable. `FitOptions::loss` exists as an escape hatch but replaces the whole objective, it does not parameterise it. Band limits, emphasis, smoothing, peak-vs-floor weighting all live nowhere. |
| The target | `BodyDocument::target_` as 512 dB values, produced by `measure::target_on_grid` or `corner_response_db` | The target has no provenance (source model, f0, which corner it came from) once it is a vector. |
| The fit | `FitController` thread → `fit_corner_watched` | Correct machinery (one section per step, pins honoured live, stop & keep). But it optimises **only** the weighted residual; it has no notion of role, and so it happily writes pole and zero at the same Hz with the zero on the circle (the TB 303 cliff). |
| Seeds | `seed.cpp` — 5 hand-written topologies + 2 target-derived + peel + continuous LM | The seeds encode topology as *frequency spreads*; none encodes the factory's role envelope. |
| Audition | none | The app cannot ride morph, cannot play audio, cannot render the interior. `dev/audit_interior.py` does it from Python. |
| Plot | `ResponsePlotWidget` (720 lines) — owns response cache, trace image cache, tokens, drag state, residual band, running-peak lane, refusal marker | One widget owns five unrelated things; corner response is computed here, not in the document. |
| Chassis bar | `ChassisBar` — verbs + readout + typed-root entry + corner grid | A status line that became the only control surface. |

### The three structural faults

1. **No authoring vocabulary between words and pixels.** The factory designed in
   roles-with-two-numbers (the X3 designer rows, Fant's F/Q, Bell's typed list); the
   app offers lattice words dressed as tokens. Everything the user finds arbitrary
   (the hex readout, the `Hz Q` box, the corner grid) is this gap.
2. **The objective is a constant.** `grid()` is the single global perceptual space.
   "Fit in the user-configured perceptual space" is impossible without threading a
   `Space` object through `Grid`, `Scratch`, the sweeps and the seeds.
3. **No interior.** The app edits four corners blind to the lerp between them, which
   is the thing the product actually plays.

## 2. Target structure

```
trench-core (Qt-free)
  space.hpp      PerceptualSpace: grid (lo, hi, N), weight law, smoothing, emphasis bands
  role.hpp       Role classification from geometry + the factory envelope per role
  p2k.hpp        unchanged laws; Grid becomes an instance built from a PerceptualSpace
  fit            fit_corner_watched(target, seeds, opts{space, roles, freedom}, …)
  morph.hpp      interior probe: interpolate_body → response at (m, q) in a Space
  measure.hpp    unchanged

app (Qt)
  Document        body + corner + target (with provenance) + Space + RoleIntent per row
  SectionModel    QAbstractTableModel over six rows: role, pole Hz/r, zero Hz/r, scale
  Plot            renders Document state for the current (corner | morph, q) — no caches of its own beyond the trace image
  MorphStrip      morph / q sliders that drive an interior probe
  SpaceDock       the perceptual-space controls (band, emphasis, weight law)
  FitController   unchanged shape; takes Space + roles
  Audition        (later) JUCE device playback through the real packed lerp
```

### 2.1 `PerceptualSpace` (core)

```
struct PerceptualSpace {
  double lo_hz = 20, hi_hz = 0.499 * kSr; size_t points = 512;
  enum class Weight { kErb, kFlat, kLogFlat } weight = kErb;
  std::vector<Band> emphasis;      // {lo, hi, gain}: ×w inside a band
  double smooth_octaves = 0;       // running mean width applied to target AND model
  double floor_db = -60;           // residual below this much under the target's median is clipped
};
Grid make_grid(const PerceptualSpace&);
```

`Grid` keeps exactly its current fields and methods; `grid()` becomes
`default_grid()` and stays the parity reference (`P2kParity.TheGridAndItsCriticalBandWeightsAreTheReferenceOnes`
must keep passing against `make_grid(PerceptualSpace{})`). `Scratch` and every
`sweep_*` / `polish*` / `corner_cost` take `const Grid&` instead of calling `grid()`.
The `Cost` enum stays.

Why these four knobs and not more: band limits and emphasis are Bell's `w_i`;
smoothing is his filter-bank simulation step (compare like with like); the floor
clip is what stops a −110 dB notch from owning the objective. Each is one number.

### 2.2 `Role` (core)

Derived, never stored:

```
enum class Role { kParked, kTilt, kPeak, kNotch, kPeakNotch, kRealAxis };
Role role_of(const StageWords&);                    // geometry → role (the census rule)
struct RoleEnvelope { double pole_lo_hz, pole_hi_hz, pole_r_lo, pole_r_hi, zero_offset_oct_lo, zero_offset_oct_hi, zero_r_lo, zero_r_hi; };
const RoleEnvelope& envelope(Role);                 // measured from the 33-body bank; a test re-derives it
```

Measured envelope (this session, 132 corners): tilt pole median 655 Hz
(p10 194, p90 2946) r ≈ .986, zero ≥ 2 oct away; mirrored tilt pole ≈ 10.5 kHz;
peak rows zero within ~1 oct, above the pole in 66 %, zero r < .97; parked =
both roots above 15 kHz. The numbers live in one table with the derivation
script beside it (`bench/facts` style), not in prose.

### 2.3 Fit = the same stepper, inside the envelope

`FitOptions` gains `const Grid* grid` and `std::array<std::optional<Role>, 6> intent`.
`stage_moves_masked` rejects a candidate word when the row has an intent and the
candidate leaves its envelope. That is the whole change to the search: a
coordinate sweep with an admissibility test it already has
(`magnitude_admissible`, `is_legal`) extended by one more predicate. One section
per step, pins, stop-and-keep, stale-result guards all stay.

Seeds: add one **role seed** — the factory prototype per role laid on the target's
peaks (tilt at the ends, peaks on the measured maxima with zeros a fifth above) —
and keep the existing ones. Seeds from another corner (rows held by index) already
exist via `CornerWords`.

### 2.4 Interior (core)

`morph_response(body, m, q, const Grid&)` = `interpolate_body` → `corner_response_db`
on the given grid. Plus `interior_audit(body, grid)` returning max/mean step on a
21×5 lattice (what `dev/audit_interior.py` does) so the app can show the worst lerp
step as one number.

### 2.5 App

**Document** owns: `PackedBody`, `corner`, `Target {curve, source path, Source, f0}`,
`PerceptualSpace`, `RoleIntent[6]`, and `View {morph, q}` (transient, not undoable).
Editing primitives are unchanged (`applySection/commitGesture/applyCorner/commitFit`).
New undoable edits: set a row's role intent; set the Space.

**SectionModel** — the six rows, one line each: role glyph, pole Hz, pole r (shown as
Q), zero Hz, zero r, scale dB. Editing a cell goes through `words_from_root` exactly
like the typed entry does today, so it is the same lattice law and one undo step.
This replaces the hex readout and the `Hz Q` box.

**Plot** — draws: target, the current corner's response, the interior response at
`View{morph,q}` when the morph strip is not at a corner, the residual band, tokens.
Tokens stay (dragging a root on the curve is the Bell/Fant interaction), but they are
labelled by role colour, and a token outside its role envelope is drawn hollow.
The plot stops owning the response computation; it asks the Document.

**MorphStrip** — two sliders (Morph, Q). At 0/1 they *are* the corner selector
(corner = m | q<<1), so the 2×2 grid goes away. In between, the plot shows the lerp
and the strip shows the worst interior step.

**SpaceDock** — band lo/hi, emphasis band(s), weight law, smoothing. Changing it
re-scores immediately (the residual band and the number update), and FIT uses it.

**Chassis bar** keeps the verbs (TARGET / FIT / STOP & KEEP / DISCARD / DC / SAW).

### 2.6 What goes

- Hex word readout, `Hz Q` line edit, the 2×2 corner grid.
- `ResponsePlotWidget` computing its own cascade/contributions.
- `FitOptions::loss` as the only way to change the objective (kept for tests, not for the UI).

## 3. Verification

Core (gtest):
- `make_grid(PerceptualSpace{})` equals the parity grid bit-for-bit.
- `role_of` reproduces the census counts on the 33 bodies (numbers checked in).
- A fit with a tilt intent on S6 never writes a pole above the envelope; a fit with
  no intent is byte-identical to today's (`P2kFitterSemantics` stays green).
- `morph_response` at the four corners equals `corner_response_db`.
- Smoothing / band / emphasis each change the score in the direction the definition says.

App (Qt slots):
- Editing a SectionModel cell = one undo entry and the same words as the typed path.
- Morph strip at 1,0 selects corner 1; at 0.5 the plot shows the lerp.
- Space change re-scores without touching the body.
- All existing 27 slots stay.

Product proof, same shape as today: identity → rows with roles → fit each corner in
a chosen Space → save → `roster add` → RenderNull null vs engine. The TB 303 targets
are the test case; the pass mark is "no cliff" = no row outside its envelope and
interior max step under the factory range (Hedz: 3.9 dB).

## 4. Order

1. Core: `PerceptualSpace` + `make_grid`, thread `const Grid&` through p2k. Tests.
2. Core: `role_of` + envelope table + census test. Fit intent predicate + role seed.
3. Core: `morph_response` / `interior_audit`.
4. App: Document grows Target/Space/Intent/View; SectionModel; MorphStrip; SpaceDock;
   plot reads from Document. Remove the three surfaces.
5. Proof on the 303 targets; then author a first real body from rows.
