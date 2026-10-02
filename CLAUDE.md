# CLAUDE.md

## Operating rules

Read the task before reading broadly.

Current instructions from Tyson outrank repository documentation.

For HEADSPACE work, the authority is the code and its acceptance suite:

1. `native/workstation/Tests/SliceTests.cpp` - the executable contract
2. `native/workstation/Source/slice/` - the state model it asserts

The write rule those tests enforce: every control is audition-only, a transient draft edit, or an explicit write. Only the 1-4 stamp, undo and export may be the third.

`native/workstation/archive/` holds superseded proposal documents. They are not authority and several of their claims about the tree are false.

`DECISIONS.md`, `FIELD_PUSH.md`, `HEADSPACE_SPEC.md`, old screenshots, old tests, and old implementations are historical context unless the current task explicitly requires them.

Design documents describe intended behaviour. They are not automatically factual descriptions of the current implementation.

For factual claims about code, DSP, file formats, measurements, or historical evidence, verify them from code, reproducible results, or primary evidence before relying on them.

Repeated claims in markdown are not additional evidence.

If sources disagree, identify the disagreement rather than silently reconciling it.

## How to work

Inspect before editing.

Trace actual reads, writes, ownership, state transitions, and call paths. Do not infer architecture from filenames, variable names, comments, or documentation alone.

Distinguish:

- proven from code or measurement
- intended by current product instructions
- inferred
- unknown

When desired product behaviour is already clear, make the smallest correct implementation decision yourself. Do not return internal implementation choices to Tyson.

Fix root causes, not visible symptoms.

Prefer one complete vertical slice over several partial changes.

Do not widen scope while fixing a slice.

Do not refactor unrelated code.

Do not introduce abstractions unless required to make the requested behaviour correct.

Do not add controls, modes, windows, panels, shortcuts, visualisations, state, or workflows that were not requested.

## State and UI

There must be one authoritative source for any product state presented as one thing.

Do not maintain multiple independent interpretations of the same audible or visible state.

The UI must project application state, not invent DSP words, curves, ownership, or hidden product state.

A displayed response must be derived from the same filter words as the corresponding audible state.

Transient audition must not mutate persistent authored state unless an explicit write action requires it.

Selection, audition, and writing are separate operations unless the current task explicitly defines otherwise.

A gesture must perform only the action represented by its control.

If one variable represents product concepts with different permissions, separate those concepts rather than relying on incidental state.

## Tests

Tests are evidence and acceptance checks, not product authority.

Never loosen a threshold merely to obtain green.

Never change an assertion merely because the implementation fails it.

If a historical test contradicts a current explicit product requirement or a reproduced defect, replace it with the smallest test that proves the current required behaviour.

Keep automated tests headless.

Prefer discriminating tests over broad regression additions.

Report only the relevant test lines unless asked for full output.

## HEADSPACE build

Build:

`cmake --build --preset headspace`

Test:

`ctest --preset headspace`

Use repository build and launch scripts when the task specifies them.

Close any running `HEADSPACE.exe` before relinking or launching.

When visual or listening judgement is required:

1. make the relevant implementation correct
2. make the relevant test green
3. make the build green
4. launch HEADSPACE
5. stop for Tyson's judgement

Do not continue into the next judged slice without his verdict.

## Protected scope

Do not touch `native/core/` unless the task explicitly requires it.

Do not touch `plugin/` unless the task explicitly requires it.

HEADSPACE changes should normally remain inside `native/workstation/`.

`plugin/NEXT_SESSION.md` may be changed only when the task explicitly requires an entry.

Do not commit unless Tyson explicitly says to commit.

Use explicit pathspecs for commits.

Do not stage unrelated changes.

Do not clean, restore, overwrite, or otherwise disturb another session's work.

## Code

No code comments.

Follow existing local style.

Do not add:

- compatibility layers
- fallback paths
- duplicate sources of truth
- speculative future hooks
- temporary architecture intended to be cleaned up later

unless explicitly required.

Do not silently change:

- DSP topology
- interpolation law
- sample-rate datum
- gain convention
- section order
- packed words
- persistence format
- export format

Do not normalise, strip zeros, reorder sections, or round-trip through another representation unless explicitly required by the current task.

## Reporting

Be concise.

For implementation work, report:

1. what was wrong
2. what changed
3. build result
4. relevant acceptance-test result
5. what Tyson should judge

Do not dump implementation narration unless asked.

Do not provide lifestyle or emotional coaching.

Do not ask Tyson to choose between internal implementation details when the desired behaviour is already clear.

If genuinely blocked by an unresolved product decision, state the exact unresolved decision and stop.