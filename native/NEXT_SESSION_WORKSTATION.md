# Workstation phase: the fitter gets an instrument

Governing docs: `native/CLAUDE.md` (tech stack, UI doctrine, verification).
Fitter core: `native/trench-core` `trench::core::p2k`, commits `31a9b8e` +
`7238c86` — parity-pinned to forge.py, 51/51 tests. Do not modify its laws.
The Qt shell today: loads a 240-byte body, draws the 512-point response,
has an undo stack and no editing operations.

## The model (ruled, do not relitigate)

The ordered cascade of 2p2z sections is the ONLY authored object: per
section, pole pair + zero pair + scale, stored as five packed u16 words,
per corner. The app is an oscilloscope/debugger around it. Probes expose
words, interpolated execution, cumulative transfer, stage state, and
audio — a probe never has a write path, and no observation is ever
promoted into a second design geometry. Corners are authored; the
interior is a consequence, judged by ear. Row order is bookkeeping:
protected, never renumbered, never displayed as a control.

Intent belongs to the section lane across all corners: one flag plus an
optional note per lane, plus per-lane provenance (factory body/corner,
fitted, hand). Stored in the session file, never in body240, never
changed by any tool. body240 stays the exact E-mu artifact.

Datum per lineage: P2K-legacy bodies at 44,100 Hz; native 8x7 bodies at
39,062.5 Hz. Never substituted, never pooled. The native container's
lattice/laws are unmeasured — fitting that lineage stays refused until
its survey lands. All work in this phase is P2K-legacy.

Geometry is authored on the machine's real resolution: the 272-rung
exponent-indexed lattice (~one byte per axis). Tokens snap live to rungs
during drag — the steppiness IS the instrument. Scale is NOT quantized
to a byte (the bank's own gain words use fine resolution).

## The fitter's user surface — complete, and no larger

The fitter is proven. The user configures it with exactly this:

- WHAT MAY MOVE: click a token to pin/unpin its root; a section's scale
  pins on its chain chip. Pole, zero, scale independently. Freedom is
  read live every step — pinning mid-fit excludes that section from the
  next step on. No panel duplicates of the click.
- WHAT TO CHASE: the target — a reference curve assigned from another
  body/corner's response (first target source; the analyzer comes
  later). Drawn behind the live response. No target = FIT disabled.
- WHEN TO STOP: STOP & KEEP commits the current accepted state as one
  undoable edit. DISCARD restores the exact pre-fit state. The hand is
  the step budget — no iteration-count knob, no tolerance knob, no
  seed selector, no mode dropdown. Seeds come from the current accepted
  cascade (plus the core's internal strategies); every push starts from
  what the author last kept.

Indicators — complete, and no larger:

- tokens move live during FIT; the touched section is visible each
  accepted step (its lane lights, nothing textual);
- pinned tokens are visibly fixed while others move;
- a refused/illegal candidate is drawn ON the curve where it happened,
  not reported in prose;
- residual strip: target minus current, mean-removed;
- running-level lane: peak of the cascade-so-far after each section —
  the box multiplies voltage while the plot sums dB; pure probe;
- selected token readout: Hz, radius, AND the packed word (hex + rung
  index) — the byte is the truth;
- DC drift readout: manual edits do NOT auto-run the gain pass; drift is
  shown, and one explicit verb re-normalizes unity-DC as its own
  undoable edit (adopted default — reversible if it fails in use);
- the identity seventh section of a legacy body does not render as a
  lane; S6's zero-radius token has no radius handle (locked word).

## Slices, in order, each landing in the real app with proof

1. TOKENS: poles/zeros of the loaded body on the response plot, drag to
   edit (live lattice snap, words written, one gesture = one undo),
   click to pin. Real pairs render; degenerate pairs render inert.
2. FIT: worker thread on `fit_corner_watched` — streamed steps, live
   pins, STOP & KEEP, DISCARD, stale-generation drop. Target = pasted
   corner response. All semantics already proven headless; this slice
   makes them visible.
3. PROBES: running-level lane, residual strip, word readout, DC drift.
4. CORNERS + RIDE: corner selector, M/Q position, audition through the
   app's own cascade (JUCE audio-only). Plugin-engine audition parity is
   an open gate, not an assumption — record it, do not claim it.
5. SESSION: cascade + intent + provenance persisted; body240 export
   byte-exact (round-trip test).

Per slice: a fixture harness or unit test for every law it touches, a
screenshot of the real app as proof, no whole-screen mockups, no
instructional prose in the UI. Bench work that may run in parallel:
the S6-floor experiment, the native-container survey, Bell-comparator
loss (enters the core only with a fixture, per the loss-callback rule).

## Forbidden (from standing rulings)

Section roles, names, rankings, taxonomies. Automatic intent changes.
Zero-placement priors or interval rules. Correspondence editing or
loci-as-controls. A second authored representation (PCA, perceptual
axes, measurements stay temporary views). Optimizing/normalizing
"equivalent" word encodings — byte identity is meaning. Confirmation
dialogs where Undo suffices. Any control or indicator not listed above
without a demonstrated interaction problem behind it.
