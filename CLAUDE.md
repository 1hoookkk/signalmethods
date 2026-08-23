# Project Instructions

## Operating principle

This repository is evidence-driven reverse engineering and product development.

Preserve established behavior and verified data, but do not preserve accidental implementation structure merely because it already exists.

Use repository evidence, executable tests, measured behavior, and primary-source documentation to make decisions. Do not invent architectural rules from isolated observations, comments, naming, or historical assumptions.

When evidence changes, update the model of the project rather than defending stale prose.

## How to work

When a task is clear, execute it.

Do not stop at analysis, produce a plan instead of an implementation, or ask for permission for ordinary reversible changes.

Before editing:

* inspect the relevant code and its callers
* identify the actual ownership and data flow
* find relevant tests, round-trip checks, fixtures, and invariants
* distinguish intended behavior from accidental implementation details

Then make the smallest **coherent** change that solves the actual problem.

“Smallest change” does not mean smallest diff. If a bug is caused by state or responsibility being split incorrectly across several modules, fix the boundary across those modules rather than adding another local patch.

Ask the user only when a genuinely consequential decision cannot be established from the repository or task: destructive operations, incompatible product semantics, missing external requirements, or multiple materially different valid outcomes.

For minor ambiguity, use the strongest available repository evidence and proceed.

Do not make the user locate files, call sites, tests, or dependencies that can be found by searching the repository.

## Evidence hierarchy

Prefer evidence in roughly this order:

1. executable behavior and reproducible measurements
2. null, round-trip, parity, and regression tests
3. decoded or recovered source data
4. primary vendor specifications, binaries, manuals, patents, or formats
5. current repository documentation
6. comments and naming
7. inference

A lower source must not silently override a higher one.

When documentation and executable evidence disagree, investigate the discrepancy. If the documentation is stale, correct it as part of the task when appropriate.

Do not turn a hypothesis into a project rule.

Preserve strange behavior when it is verified. Do not “clean it up” merely because a conventional implementation would look different.

## Hard project invariants

Ruled 2026-08-23: the E-mu Z-plane lineage is the reference taste and the proof the
idea works, not the format we preserve. The 33 P2K bodies and the 289 Morpheus cubes are
imported content and evidence. Byte-level parity with P2K hardware is no longer a goal.

A native body is a serial cascade of six second-order pole/zero sections over four
authored corners. Log magnitudes add; the complete cascade is the comparator.

A section is (Fc, bandwidth in octaves, gain, zero offset in octaves, depth) in float,
designed in Hz at the host sample rate. No word lattice, no byte caps, no fixed datum.
Pole radius is bounded by a stability margin, not a chip ceiling.

The interior is a blend of section coefficients, never of roots. The stable and the
conjugate regions of a second-order section are convex in coefficient space, so any blend
of stable corners is stable and any blend of conjugate pairs stays conjugate. Author
corners; ride and audit the resulting interior; do not invent a separate morph model.

DC gain is unity by a closed form on the geometry. Never peak-normalise. No per-stage
gain cuts.

Section identity is ordered and persists across corners. Lanes are coefficient ancestry,
not resonance identity: sections may cross in frequency; never sort or re-pair
established lanes. New corners inherit lanes from an existing corner. Permutation search
is a repair tool for imported corners or outlier bodies, never the default.

Zeros are free. No placement prior, follow rule or interval constraint in the fitter.
Offset and depth are authored controls.

Target and candidate pass through the same auditory representation before scoring. The
one number the user tunes to stays one number.

Direct manipulation and FIT are two ways of changing the same authored state.

A body is accepted by listening. The morph interior is a mathematical consequence of
the corners and the blend law; it is never measured to judge a body. The tables in
`dev/interior_envelope.txt` and `dev/modulation_envelope.txt` are regression fixtures for
the engine: rerun them when the engine changes and prove the factory bodies did not move.

Until the float engine lands, the existing packed-word engine and its parity tests remain
the engine of record. Do not break them while building the replacement.

## Acceptance evidence

Treat existing null, bit-exact round-trip, parity, packing, and canonical regression tests as load-bearing.

Before changing a representation, import/export path, sample-rate interpretation or interpolation path, identify the relevant acceptance test. Retire a packed-word parity test only when its float replacement exists and the 33 imported bodies null against their decoded responses.

A change that produces cleaner code but breaks verified round-trip or factory behavior is a regression unless the task explicitly establishes that the old behavior was wrong.

Add a regression test when fixing a bug with a stable reproducible failure.

Prefer tests that exercise observable behavior and actual data paths over tests that merely reproduce the implementation.

Do not modify fixtures simply to make a changed implementation pass. First establish whether the fixture or the implementation is wrong.

## Engineering practice

Use current stable platform and language practices appropriate to the codebase. For APIs, browser behavior, libraries, formats, or standards that are version-sensitive, verify the current primary documentation rather than relying on memory.

Prefer clear ownership and one source of truth.

Avoid duplicated state, mirrored flags, and multiple modules independently representing the same fact.

Keep persistent authored state separate from transient UI/session state and realtime engine state.

Views should render state and emit interaction events. They should not quietly become alternate owners of domain state.

Realtime/audio processing must not depend on expensive UI-thread work.

Continuous interactions should use the platform's native interaction model and remain responsive under pointer cancellation, rapid input, resize, and asynchronous work.

Async operations must be race-safe. A stale response must never overwrite newer user work.

Long-running operations should be cancellable when practical and must either commit atomically or leave authored state unchanged.

One user gesture should normally correspond to one undoable authored transaction.

Navigation, hover, playback position, selection, and other transient interaction should not pollute authoring history unless they genuinely modify the artifact.

Prefer immediate feedback over modal workflows. If an operation can safely be applied atomically and reverted with Undo, do not add a second confirmation step merely out of caution.

## Refactoring

Refactor when the requested behavior exposes a real ownership or architectural problem.

Do not avoid an appropriate refactor because several files need coordinated changes.

Equally, do not redesign unrelated parts of the repository while solving a localized problem.

Split modules by responsibility, not line count.

A large cohesive module is acceptable. A module that owns unrelated state, UI, network, domain mutation, transport, and lifecycle behavior is not.

Before extracting code, understand its state ownership and call graph. Do not merely move functions into additional files while preserving the same tangled dependencies.

Delete dead code when its lack of consumers has been established and deletion is within the task boundary. Do not retain obsolete paths “just in case.”

Do not create compatibility layers for internal code unless there is an actual compatibility requirement.

## Product and interaction work

Optimize for the user's operation, not for exposing the implementation.

The workstation should behave as an instrument:

* state changes should be immediate and visible
* playback and direct manipulation should compose naturally
* common operations should not require bookkeeping steps
* selection should mean selection, not hidden mutation
* fitting should solve the selected authored state, not create unnecessary workflow modes
* Undo should make aggressive but reversible operations safe
* permanent UI text should be minimal
* internal project terminology should not leak into the interface unless the user needs it to operate the system

Do not add explanatory UI, modes, dialogs, confirmation steps, settings, or abstractions unless they solve a demonstrated interaction problem.

When an operation can infer the obvious context safely from current selection or current state, do so rather than requiring the user to restate it.

Expose complexity when it gives the user meaningful control; hide bookkeeping complexity.

## Agent behavior

Be decisive without being reckless.

Do not manufacture objections or uncertainty when the evidence is sufficient.

Do not interpret “preserve behavior” as “never improve architecture.”

Do not interpret “do not infer” as “ask the user about everything.” Investigate first. Many uncertainties can be resolved from code, tests, history, data, or primary documentation.

Do not preserve a known bug because changing it affects several modules.

Do not create speculative abstractions for possible future requirements.

Do not add fallback paths, feature flags, compatibility shims, or defensive branches without evidence they are needed.

Do not silently broaden the task into unrelated cleanup.

If an implementation attempt exposes a deeper cause, fix the cause when doing so remains inside the user's stated goal.

When the user identifies a fundamental workflow or architectural problem, treat that diagnosis as permission to inspect and correct the complete relevant path rather than repeatedly patching individual symptoms.

When finished:

* run the relevant tests and checks
* exercise the affected workflow when practical
* inspect the diff for accidental scope
* remove temporary/debug code
* report what changed, important evidence, tests run, and any genuinely unresolved issue

Do not claim success without verification.

## Documentation

Keep this file limited to durable project-wide instructions and hard invariants.

Do not use it as a research notebook, changelog, bug tracker, implementation-status report, or archive of superseded theories.

Put detailed evidence and reverse-engineering findings in dedicated documentation.

Put temporary/open work in an issue, task, or status document.

Use scoped `CLAUDE.md` files for subsystem-specific rules when those rules do not apply to the whole repository.

Delete or rewrite stale instructions promptly. Stale authoritative prose is more dangerous than missing prose.

Historical findings are evidence, not permanent instructions.
