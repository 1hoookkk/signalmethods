#!/usr/bin/env python3
"""Cascade detector — the part of the law that can DENY, not just remind.

The sibling hook (df2_series_law.py) injects the series-cascade law before an
edit to cascade code. It cannot tell whether the law changed anything: it fires
on the file, not on the mistake. This one reads the text the agent is about to
write and blocks the specific parallel-filter-bank moves the law forbids.

Every block is a real, countable CATCH, written to cascade_catches.log — true
whether or not the agent mentions it.

The signatures are deliberately narrow. A rule that cries wolf gets switched
off, and a switched-off guard catches nothing.
"""
import datetime
import json
import pathlib
import re
import sys

# files the law governs (kept in step with df2_series_law.py)
GOVERNED = [
    r"trench-core[\\/]src[\\/]",
    r"tools[\\/](joint_fit|extractor|spectral_score|make_body)\.py",
    r"tools[\\/](batch_compiler|batch_ingest|q_attitudes)\.py",
    r"tools[\\/]make_\w*_table\.py",
    r"pyruntime[\\/]",
    r"tools[\\/](body_from_endpoints|make_filter|ir_endpoints|tf_ingest)\.py",
    r"tools[\\/]wordsheet[\\/]",
    r"bench[\\/]facts\.py",
]

# The AUTHORING INTERFACE — the surfaces where a person decides what a body is.
# A subset of GOVERNED, and the only place a rule about how you AUTHOR may fire.
#
# The distinction is not cosmetic. "Level is per section" is TRUE OF THE FORMAT:
# the word row's fifth slot is that section's SCALE, 20.5% of factory character
# frames carry more than one across their six sections (facts.py
# SCALE_UNIFORM_PCT), and both Rossum sources give every section its own
# separated gain a0. Code that PARSES, MEASURES or RUNS that — trench-core/src,
# pyruntime, bench/facts.py — has to be able to name a per-section level, or it
# cannot describe the file it is reading. Denying those edits denied the
# measurement to protect a preference about authoring. So the authoring rule
# fires here and nowhere else.
AUTHORING_INTERFACE = [
    r"tools[\\/](joint_fit|extractor|spectral_score|make_body)\.py",
    r"tools[\\/](batch_compiler|batch_ingest|q_attitudes)\.py",
    r"tools[\\/]make_\w*_table\.py",
    r"tools[\\/](body_from_endpoints|make_filter|ir_endpoints|tf_ingest)\.py",
    r"tools[\\/]wordsheet[\\/]",
]

# (name, regex, scope, why) — each must describe a move that is ALWAYS wrong in
# the files its scope names. scope None means every GOVERNED file.
#
# One rule was removed on 2026-08-13. per-stage-gain was removed the same day
# and RESTORED: its stated justification (the "SCALE law") is false about the
# format, but the rule is right about the interface. You author geometry; the
# level follows. See its reason text.
#
#   per-stage-gain    enforced "level is ONE value per corner (the SCALE law)".
#                     Measured at 79.5% of character frames, not 100% — a
#                     tendency, not a law (facts.py SCALE_UNIFORM_PCT). Both
#                     Rossum sources give every section its own separated gain
#                     a0, and E-MU labels the six parametric sections
#                     "Fc Bw Gain" each. Per-section level is the format's
#                     native shape; this rule forbade writing it.
#
#   post-fit-stage-sort  enforced "lane roles are locked a priori". No source
#                     names any section. The signature also could not tell the
#                     no-op apart from the real thing: permuting slots
#                     identically at both ends moves the response 1.78e-14 dB.
RULES = [
    ("hungarian-outside-extractor",
     r"linear_sum_assignment",
     None,
     "Hungarian matching is legal in exactly one place: tools/extractor.py, on "
     "RAW MEASURED PEAKS during data prep. Anywhere else it re-pairs which root "
     "at one frame becomes which root at the next — measured at up to 95.47 dB "
     "in the middle of the morph while both endpoints stay bit-identical "
     "(AUTHORING_SPEC.md). That is authoring, done silently. Do it on purpose "
     "or not at all."),

    ("per-stage-gain",
     r"(per_(section|stage|formant|lane)_(gain|db|level)|"
     r"(gain|level)_per_(section|stage|formant|lane)|"
     r"(boost|gain)_db\s*\[\s*(s|i|lane|stage|section)\s*\])",
     AUTHORING_INTERFACE,
     "You do not author gain. You drag the poles and zeros, and the level "
     "follows. Bandwidth sets height at exactly 6 dB per halving; where the "
     "zero sits relative to its own pole is worth 34 dB, from +7.9 dB with the "
     "zero on the pole to +41.6 dB two octaves above, and it sets the tilt too. "
     "Talking Hedz carries ONE scale word, 0.56189, in all six sections and "
     "gets S6's +60 dB from a tight pole at 225 Hz with a unit-circle zero 60 "
     "semitones up. Reaching for a per-section dB means you have picked the "
     "wrong control. (The FORMAT does hold a level per section - 20.5% of "
     "factory frames use more than one, facts.py SCALE_UNIFORM_PCT. That is "
     "not a licence to author with it.)"),

    ("linear-response-sum",
     r"(?<!db)(?<!dB)\bmag(nitude)?s?\s*\+=|\bH_total\s*\+=|\bresponse\s*\+=\s*np\.abs",
     None,
     "Responses MULTIPLY in a series cascade (dB add, linear magnitudes do "
     "not). Summing linear magnitudes is parallel-filter-bank math."),
]

LOG = pathlib.Path(__file__).with_name("cascade_catches.log")


def edited_text(tool_input: dict) -> str:
    """Only the text being INTRODUCED — never the file's existing content."""
    parts = []
    for key in ("new_string", "content"):
        v = tool_input.get(key)
        if isinstance(v, str):
            parts.append(v)
    for e in tool_input.get("edits") or []:
        if isinstance(e, dict) and isinstance(e.get("new_string"), str):
            parts.append(e["new_string"])
    return "\n".join(parts)


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except Exception:
        return 0
    tool_input = payload.get("tool_input") or {}
    path = str(tool_input.get("file_path") or tool_input.get("notebook_path") or "")
    if not path or not any(re.search(p, path) for p in GOVERNED):
        return 0

    text = edited_text(tool_input)
    if not text.strip():
        return 0
    # the one sanctioned Hungarian site stays sanctioned
    in_extractor = re.search(r"tools[\\/]extractor\.py$", path) is not None

    for name, pattern, scope, why in RULES:
        if name == "hungarian-outside-extractor" and in_extractor:
            continue
        if scope is not None and not any(re.search(p, path) for p in scope):
            continue
        m = re.search(pattern, text, re.IGNORECASE)
        if not m:
            continue
        line = text[:m.start()].count("\n") + 1
        try:
            with LOG.open("a", encoding="utf-8") as f:
                f.write(f"{datetime.datetime.now().isoformat(timespec='seconds')}\t"
                        f"{name}\t{path}\t{m.group(0)[:60]!r}\n")
        except Exception:
            pass
        print(json.dumps({
            "hookSpecificOutput": {
                "hookEventName": "PreToolUse",
                "permissionDecision": "deny",
                "permissionDecisionReason":
                    f"CASCADE CATCH [{name}] at line {line} of the proposed edit: "
                    f"{m.group(0)[:80]!r}\n\n{why}\n\n"
                    "Rewrite so the series topology holds, then edit again.",
            },
        }))
        return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
