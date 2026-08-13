# Architecture

## Data flow

```text
ref/ + operator input
        |
        v
Station / authoring tools
        |
        +--> packed body artifact
        |
        +--> cascade inspection and comparison artifacts
```

The whole cascade is the object being judged. Inspectors may display individual
sections to explain the stored cascade, but they do not assign section types,
roles, or bands.

## Repository layout

```text
station/       native authoring application
inspectors/    read-only cascade measurement and rendering
tools/         authoring commands and import/export helpers
ref/           source evidence, captures, manuals, and provenance manifests
recipes/       authored inputs and reproducible body specifications
tests/         binary, numerical, and workflow verification
```

Generated plots, builds, caches, and audition output are local artifacts and
must not become architectural inputs.

## Runtime dependency

The canonical packed format, decoder, interpolation, response calculation, and
runtime constants are owned by `trench-plugin/trench-core`.

Station must consume `trench-core` from a pinned Git commit. It must not use a
live path dependency into another agent's working tree. Updating the pin is a
deliberate compatibility change with its own verification and commit.

Until the plugin repository has a committed `trench-core`, do not invent a
temporary substitute. Add the dependency only after there is a real revision to
pin.

## Inspector boundary

An inspector:

1. reads a body through the pinned canonical decoder;
2. measures or renders the complete serial cascade;
3. reports the inputs, sample rate, core revision, and output path;
4. never edits the body it is inspecting;
5. never converts a measurement into a universal authoring law.

The Station and command-line inspectors should share the same canonical
response calculation. Duplicated filter mathematics is a defect.

## Cross-repository changes

If authoring needs a new packed/runtime capability:

1. record the exact missing capability without proposing an implementation;
2. implement and verify it in `trench-plugin/trench-core`;
3. commit it there;
4. update the pinned revision here;
5. rerun authoring and inspector verification.

No source file is edited in both repositories.

