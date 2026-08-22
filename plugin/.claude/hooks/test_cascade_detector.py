#!/usr/bin/env python3
"""What the cascade detector must catch, and what it must let through.

The second half is the point. A rule that denies legitimate work gets switched
off, and a switched-off guard catches nothing — so the boundary is a test, not
a comment. per-stage-gain is a rule about the AUTHORING INTERFACE; the same
words are how core format, FFI and fact code describe the file it reads.

    python .claude/hooks/test_cascade_detector.py
"""
import json
import pathlib
import subprocess
import sys

HOOK = pathlib.Path(__file__).with_name("cascade_detector.py")


def decision(path, text):
    """None if the edit is allowed, the rule name if it is denied."""
    payload = {"tool_input": {"file_path": path, "new_string": text}}
    out = subprocess.run(
        [sys.executable, str(HOOK)],
        input=json.dumps(payload),
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip()
    if not out:
        return None
    reason = json.loads(out)["hookSpecificOutput"]["permissionDecisionReason"]
    return reason.split("[", 1)[1].split("]", 1)[0]


GAIN = "    per_section_gain = [0.0] * 6\n"
HUNGARIAN = "    rows, cols = linear_sum_assignment(cost)\n"
LINEAR_SUM = "    mags += np.abs(h)\n"

# (path, text, expected rule or None, what this pins down)
CASES = [
    # ---- DENY: the authoring interface is where you drag poles, not gains ----
    (r"tools\wordsheet\core.py", GAIN, "per-stage-gain",
     "the lane board is the authoring interface"),
    (r"tools/make_filter.py", GAIN, "per-stage-gain",
     "the authoring path proper"),
    (r"tools\joint_fit.py", GAIN, "per-stage-gain",
     "the fitter authors geometry"),
    (r"tools/batch_compiler.py", GAIN, "per-stage-gain",
     "the planning path"),

    # ---- ALLOW: format, FFI and fact code must be able to NAME the format ----
    # A section carries its own SCALE word. 20.5% of factory character frames
    # use more than one across their six sections. Code that parses, runs or
    # measures that has to say so.
    (r"trench-core\src\minifloat.rs", "let gain_db[s] = decode(words[s][4]);\n", None,
     "the encoder describes the fifth word it decodes"),
    (r"trench-core/src/cascade.rs", GAIN, None,
     "the runtime cascade holds a level per section"),
    (r"pyruntime\ffi.py", GAIN, None,
     "the bindings mirror the format"),
    (r"bench/facts.py", "SCALE_PER_SECTION = per_section_db(bodies)\n", None,
     "the fact table measures per-section SCALE by definition"),

    # ---- the rules that ARE universal keep their whole scope ----
    (r"trench-core\src\cascade.rs", LINEAR_SUM, "linear-response-sum",
     "responses multiply everywhere, core included"),
    (r"tools/joint_fit.py", LINEAR_SUM, "linear-response-sum",
     "and in the tools"),
    (r"bench/facts.py", HUNGARIAN, "hungarian-outside-extractor",
     "re-pairing roots is authoring, done silently"),
    (r"trench-core/src/designer.rs", HUNGARIAN, "hungarian-outside-extractor",
     "including in core"),
    (r"tools/extractor.py", HUNGARIAN, None,
     "the one sanctioned Hungarian site stays sanctioned"),

    # ---- ungoverned files are not this hook's business ----
    (r"plugin\source\PluginProcessor.cpp", GAIN, None, "the plug-in is not governed"),
    (r"docs/whatever.md", HUNGARIAN, None, "narration is not governed"),
]


def main():
    failures = 0
    for path, text, expected, why in CASES:
        got = decision(path, text)
        ok = got == expected
        verb = "deny " + expected if expected else "allow"
        print(f"{'ok  ' if ok else 'FAIL'}  {verb:<34}  {path:<40}  {why}")
        if not ok:
            print(f"        expected {expected!r}, got {got!r}")
            failures += 1
    print(f"\n{'PASS' if failures == 0 else 'FAIL'} ({failures} failure"
          f"{'' if failures == 1 else 's'})")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
