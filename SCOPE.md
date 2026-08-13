# Scope

## Purpose

Turn source evidence and operator decisions into a packed body, then show what
that body does as a complete serial cascade.

## This repository owns

- the Station authoring UI;
- direct pole, zero, scale, frame, and correspondence editing;
- command-line authoring tools;
- cascade inspectors and comparison renders;
- source material and provenance under `ref/`;
- authored recipes and non-shipping working bodies;
- authoring tests and reproducible measurements.

## This repository does not own

- the VST3 or its UI;
- the real-time audio callback;
- runtime engine lifetime or thread safety;
- plugin parameters or migration;
- Movement or Function Generator playback;
- the canonical packed decoder or runtime interpolation implementation;
- shipping installers or release packaging.

## Explicitly absent

There is no `laws.json`, law panel, role taxonomy, or policy engine. Source
evidence belongs in `ref/`. Behaviour is observed with cascade inspectors.
Tests may enforce binary, numerical, and reproducibility requirements that can
be demonstrated; they must not encode an aesthetic claim as a universal rule.

