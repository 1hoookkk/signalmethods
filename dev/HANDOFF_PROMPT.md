# Handoff prompt

Paste the block below to start the next session.

---

You're picking up TRENCH in `C:\Users\hooki\trench-native`, branch
`codex/native-arx-authoring`. **Read `dev/SESSION_2026-08-22.md` first** — it's
the index for everything the last session established, in reading order.

Context in one line: TRENCH builds a filter plug-in that interoperates with
E-mu's Z-plane filter format, from hardware and software Tyson owns. Establishing
the format from a device you own so your product reads and writes it correctly is
ordinary compatibility engineering, and the docs cite device addresses because
those are the evidence for the format rules.

**What changed last session.** It began as slice 3 of the workstation phase
(probes — shipped, commit `6931d1d`) and turned into a container survey when a
topology verdict, "fit with one lowpass and six parametric EQ sections", turned
out to be the machine's own documented architecture. The survey is done and it
overturned two things the project had been assuming:

- **The machine is seven poles and six zeros.** Section 7 can never hold a zero
  — 100% of 289 decoded bodies, and the device's audio loop runs its zero-pair
  body six times then falls through to a seventh, pole-only block.
- **The native lineage does not use the P2K 272-rung lattice.** 0.18–0.28% of
  native magnitude words land on it. Do not snap native geometry to that grid.
- Datum is 39,062.5 Hz, now decoded from the device clock tree rather than
  assumed.

**Work in this order.**

1. **Decide the morph parity target — this needs Tyson, and it blocks code.**
   The plug-in doesn't match EmulatorX3 on the same factory preset. The
   interpolation *domain* (packed-then-decode vs decode-then-interpolate) is
   worth 12–25 dB in the interior while leaving corners bit-identical, which is
   exactly the symptom. Our plug-in follows the hardware law and that's proven;
   whether EmulatorX3 made the same choice is untested. See `dev/MORPH.md` §1 for
   the decisive numerical test — it is a capture-and-score, not a listening test.
2. **Reconcile the two encodings.** The device runtime decodes an 11-bit
   minifloat (4 exponent bits, 7 mantissa, low four filled with `0xF`); our
   560-byte body words are 16-bit with a 4/12 split. Not obviously the same law.
   Settle it before any export path claims device compatibility.
   `NATIVE_CONTAINER_SURVEY.md` "Still open", first bullet.
3. **Tyson's stated focus: the manuals, and `.4` versus cubes analysed
   separately.** The family statistics in `DVTD_VOWEL_FIT.md` §7 currently pool
   square and cube bodies; re-derive them split before writing any recipe's
   placement maths from them.
4. **Re-label the decoded corpus.** The eurorack manual covers C000–C280 and its
   names match the body filenames exactly on every spot check. That takes
   `ref/morpheus_decoded/` `geometry: unknown` from 136 down to 8. Cheap, high
   value. Note `.4` is **not** derivable from the bytes — the manual name is the
   only authority (survey §9a).
5. **Row-permutation experiment** (`dev/MORPH.md` §2). Permuting a corner's rows
   doesn't change that corner's response, so improving the interior this way is
   free. Measure the interior first. Constraint: row 7 must hold no zero.
6. Seven-stage fitter path honouring the row-7 law; the core is still six-stage.
   `dev/SEEDING_RECIPES_DESIGN.md` has the proposed structure, design only, no
   implementation — that was the brief.

**Don't re-do these.**
- Matching pursuit seeding. Implemented, measured, 3× worse than the peak
  picker, better on 1 of 44. The "poles go on formants" prior is load-bearing.
- Searching Audity OS or updater images for filter bodies. Verified absent.
- Two scripts in trench-authoring are not citable on runtime behaviour:
  `exact_dsp_stage_emulator.py` indexes the firmware with a negative Python
  offset; `verify_exact_biquad_law.py` never opens it.

**Practical.** The research module is built `cp313` — run analysis scripts with
`py -3.13`, not the default `python` (3.10). Build with `native/build.ps1`, test
with `native/test.ps1`; 61 tests were green at slice 3. The Qt app runs via
`native/run.ps1` (Qt DLLs aren't beside the exe, so don't launch it directly).

**How the last session worked, and what to keep.** Every numeric claim went
through the real C++ core via the research bridge rather than a Python
reimplementation. Two conclusions were overturned mid-session precisely because
the evidence trail was checkable — a 48 kHz inference, and the matching-pursuit
proposal. Negative results are written down on purpose. Keep doing that.
