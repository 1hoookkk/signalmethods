#!/usr/bin/env python3
"""Enforce_DF2_Series_Topology.

PreToolUse hook. When a write touches filter/DSP/fitting code, inject the
series-cascade law into the model's context before the edit is made.
Silent for every other file.
"""
import datetime
import json
import pathlib
import re
import sys

PATTERNS = [
    r"trench-core[\\/]src[\\/]",
    r"tools[\\/](joint_fit|extractor|spectral_score|make_body)\.py",
    r"tools[\\/](batch_compiler|batch_ingest|q_attitudes)\.py",
    r"tools[\\/]make_\w*_table\.py",
    r"pyruntime[\\/]",
    # the live authoring path (trench make) and the fact table it answers to
    r"tools[\\/](body_from_endpoints|make_filter|ir_endpoints|tf_ingest)\.py",
    r"tools[\\/]wordsheet[\\/]",
    r"bench[\\/]facts\.py",
    r"[\\/](cascade|response|stage_law|arma_endpoint|minifloat|engine|armadillo)\.rs$",
]

LAW = """SERIES CASCADE - what survived the source audit of 2026-08-13.

You are editing filter/DSP/fitting code. Every line below is either a verbatim
quote from a primary source or a measurement that re-runs in bench/facts.py.

1. The chain is a series cascade of second-order sections:
       H_total(z) = product of H_i(z)
   Responses MULTIPLY; dB add. E-MU: "Often times, several parametric sections
   are cascaded (placed one after another) in order to create complex filter
   response curves." - Morpheus manual, ref/morpheus_manual_vocabulary.md.

2. Nothing is local. A zero in an early section shapes the TOTAL and can cancel
   a later section's peak: 24% of placed conjugate poles produce no peak in the
   cascade at all (facts.py POLE_SURVIVAL_PCT = 76.0, over 710 poles). Judge
   every result on the whole probed cascade, never on a section in isolation.

3. There are SEVEN sections, not six (cascade.rs NUM_STAGES = 7, facts.py
   NUM_STAGES). A corner is a FRAME. Three axes is a CUBE of 8 frames, two axes
   a SQUARE of 4. The axes are Morph, Freq. Tracking, Transform 2 (facts.py
   CUBE_CORNERS, CORNER_BIT_ORDER).

4. The design is in the sections. A section is a row that exists at all eight
   frames - pole, zero, scale, eight times. That is the whole authored object.

5. A SECTION HAS NO TYPE. It has two roots. E-MU's p.99 block diagram shows
   "1 Low Pass Section + 6 Parametric Equalizer Sections", but the sentence
   introducing it calls that "one of the possible ways that the Morpheus filter
   can be configured" - an example, not the machine. Measured over 792 factory
   sections: ZERO are a lowpass or a plain shelf. 61.1% make a peak AND a notch,
   23.0% a peak, 13.4% a notch, 2.5% nothing (facts.py LOWPASS_SECTIONS).
   Broadband shape comes from the ENDS opposing each other: S1 tilts -22.5 dB
   and S6 +29.8 dB while both also carry the body's biggest peak/notch features
   (facts.py SECTION_TILT_S1_DB, SECTION_TILT_S6_DB). Do not give a section a
   fixed type, a role, or a band.

6. Order is not a law; CORRESPONDENCE is what gets authored. Permuting the slots
   IDENTICALLY at both endpoints changes the response by 1.78e-14 dB - a no-op.
   Permuting them DIFFERENTLY per endpoint leaves both endpoints bit-identical
   (1.42e-14 dB) and moves the middle of the morph by up to 95.47 dB
   (AUTHORING_SPEC.md). Re-pairing after a fit therefore re-authors the filter
   silently. Do it on purpose or not at all. Hungarian matching is legal in
   exactly one place: tools/extractor.py, on RAW MEASURED PEAKS during data
   prep, before any filter math runs.

7. Level is PER SECTION. 79.5% of character frames happen to carry one SCALE
   word across all six of their sections; 20.5% do not, and both Rossum sources
   give every section its own separated gain a0 (facts.py SCALE_UNIFORM_PCT).
   Do not collapse per-section level to one number per frame.

NOT LAW, and no source states one: a pole placement rule, a pole-zero minimum
separation, a section ordering rule, or a role-name for any section. No E-MU
document calls a section air, throat, chest, mouth or anchor
(ref/morpheus_manual_vocabulary.md, "What is NOT in the manual"). Rossum, ICMC
1992: "the precise relative placement of the poles seems to be not very
critical."

Authorities: bench/facts.py - ref/morpheus_manual_vocabulary.md - AUTHORING_SPEC.md"""


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except Exception:
        return 0
    tool_input = payload.get("tool_input") or {}
    path = str(tool_input.get("file_path") or tool_input.get("notebook_path") or "")
    if not path or not any(re.search(p, path) for p in PATTERNS):
        return 0
    # Scoreboard. A FIRE is mechanical: this file is cascade code, so the law
    # was injected. It is NOT evidence the law corrected anything - the agent
    # reports a CATCH in the reply when the law actually changed the code it
    # was about to write. Fires without catches mean the guard is cheap, not
    # that it is working.
    try:
        log = pathlib.Path(__file__).with_name("cascade_fires.log")
        with log.open("a", encoding="utf-8") as f:
            f.write(f"{datetime.datetime.now().isoformat(timespec='seconds')}\t{path}\n")
    except Exception:
        pass
    print(json.dumps({
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "additionalContext": LAW,
        },
        "suppressOutput": True,
    }))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
