from __future__ import annotations

import glob
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

DECISIONS = {
    "topology": ("7 sections x 8 corners, trilinear, 560-byte native body",
                 "2026-08-13", "NEXT_TASK.md"),
    "editor": ("one editor: Rust + mlua recipes + egui, one binary, no Python UI",
               "2026-08-13", "NEXT_SESSION_EDITOR.md"),
    "placement": ("place poles directly; no optimiser. A pole lands within 0.02 st",
                  "2026-08-13", "bench/facts.py POLE_PLACEMENT_ST"),
    "gate": ("every shipped body byte-identical through the compat path",
             "2026-08-13", "trench-core/src/bin/topology_gate.rs"),
}

CANONICAL_EDITOR = "tools/wordsheet"
RETIRED_EDITORS = ["tools/workstation", "tools/quartet_composer.py"]
GATE_HASH = "5187ee1fb8f8f27d"
AUTHORING_PATH = [
    "tools/body_from_endpoints.py", "tools/ir_endpoints.py",
    "tools/tf_ingest.py", "tools/make_filter.py",
]


def check(label, ok, detail=""):
    print(f"  [{'ok ' if ok else 'FAIL'}] {label}" + (f"  {detail}" if detail else ""))
    return ok


def one_canonical_editor():
    claims = []
    for p in glob.glob(str(ROOT / "tools/**/*.md"), recursive=True) + [str(ROOT / "CLAUDE.md")]:
        try:
            t = Path(p).read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for m in re.finditer(r"^.*CANONICAL(?: ROUTE)?.*$", t, re.M | re.I):
            line = m.group(0).strip()
            if "canonical" in line.lower() and len(line) < 200:
                claims.append((Path(p).relative_to(ROOT).as_posix(), line[:90]))
    return claims


def constants_are_registered():
    from bench.facts import FACTS
    known = {f.name for f in FACTS}
    stray = []
    for rel in AUTHORING_PATH:
        p = ROOT / rel
        if not p.exists():
            continue
        t = p.read_text(encoding="utf-8", errors="ignore")
        for m in re.finditer(r"^([A-Z][A-Z0-9_]{3,})\s*=\s*(-?\d+\.?\d*)\s*$", t, re.M):
            name = m.group(1)
            if name in known or name.startswith(("RT_", "NUM_", "SR_", "DEFAULT_")):
                continue
            stray.append((rel, name, m.group(2)))
    return stray


def unsourced_constants():
    from bench.facts import FACTS
    return [f.name for f in FACTS if f.kind == "UNSOURCED"]


def main() -> int:
    print("DECISIONS IN FORCE — re-open one only by measurement, never by preference\n")
    for k, (what, when, where) in DECISIONS.items():
        print(f"  {k:11s} {what}")
        print(f"  {'':11s} decided {when}, {where}")
    print()

    ok = True
    print("REPO")
    ok &= check(f"canonical editor present: {CANONICAL_EDITOR}",
                (ROOT / CANONICAL_EDITOR).exists())
    retired = [d for d in RETIRED_EDITORS if (ROOT / d).exists()]
    ok &= check("retired editors removed", not retired,
                f"still present: {', '.join(retired)}" if retired else "")

    claims = one_canonical_editor()
    ok &= check("exactly one file claims canonical authority", len(claims) <= 1,
                f"{len(claims)} claims" if len(claims) > 1 else "")
    for f, line in claims[:6]:
        print(f"         {f}: {line}")

    print("\nCONSTANTS")
    stray = constants_are_registered()
    ok &= check(f"every constant on the authoring path is registered "
                f"({len(AUTHORING_PATH)} files)", not stray,
                f"{len(stray)} unregistered" if stray else "")
    for f, n, v in stray[:8]:
        print(f"         {f}: {n} = {v}")

    uns = unsourced_constants()
    ok &= check("no unsourced constants", not uns,
                f"{len(uns)}: {', '.join(uns)}" if uns else "")

    print("\nGOLDEN GATE")
    exe = ROOT / "target" / "release" / "topology-gate.exe"
    if not exe.exists():
        check("topology-gate built", False, "cargo build -p trench-core --release")
        ok = False
    else:
        out = subprocess.run([str(exe), str(ROOT / "target" / "gate.bin")],
                             capture_output=True, text=True, cwd=str(ROOT))
        got = ""
        for line in out.stdout.splitlines():
            if "fnv1a64" in line:
                got = line.split()[-1]
        ok &= check(f"every shipped body unchanged ({GATE_HASH})", got == GATE_HASH,
                    f"got {got}" if got != GATE_HASH else "")

    print()
    print("HEALTHY" if ok else "NOT HEALTHY — fix the FAILs above before authoring")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
