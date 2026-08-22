"""Every constant recomputed from the data it claims to come from.

A comment asserting a number cannot fail. This can. Run it and any constant
whose stated origin no longer produces it is a hard error, not a stale line
of prose somebody inherits.
"""
from __future__ import annotations
import json, re, statistics, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAILED: list[str] = []


def check(name, got, want, tol=0.0):
    ok = abs(got - want) <= tol if tol else got == want
    print(f"  {'PASS' if ok else 'FAIL'}  {name}: recomputed {got}, source says {want}")
    if not ok:
        FAILED.append(name)


def anchor_ref_hz():
    """trench-core/src/anchor.rs ANCHOR_REF_HZ = pooled median F3 over
    Hillenbrand 1995 + Peterson-Barney 1952 + Mokhtari & Tanaka 2000."""
    f3, total = [], 0
    for n in ("hillenbrand_1995", "peterson_barney_1952", "etl_mokhtari_tanaka_2000"):
        d = json.loads((ROOT / f"recipes/tables/academia/{n}.json").read_text(encoding="utf-8"))
        total += len(d["objects"])
        for o in d["objects"]:
            rows = o.get("formants") or o.get("rows") or []
            if len(rows) >= 3:
                v = rows[2].get("frequency_hz") or rows[2].get("freq_hz")
                if v:
                    f3.append(float(v))
    src = (ROOT / "trench-core/src/anchor.rs").read_text(encoding="utf-8")
    stated = float(re.search(r"ANCHOR_REF_HZ:\s*f64\s*=\s*([\d.]+)", src).group(1))
    stated_n = int(re.search(r"all (\d+) measured utterances", src).group(1))
    check("utterance count", total, stated_n)
    check("ANCHOR_REF_HZ", round(statistics.median(f3), 1), stated, 0.05)



def bins_reproduce_from_the_rip():
    """Every shipped .bin reproduces from the raw stage-major dump."""
    import hashlib, struct
    raw = (ROOT / "evidence/emulatorx_binary_filter_rip_20260729/p2k_raw_stage_major.bin").read_bytes()
    man = json.loads((ROOT / "ref/presets/P2K_MANIFEST.json").read_text(encoding="utf-8"))
    ok = n = 0
    for e in man["entries"]:
        p = ROOT / "ref/presets" / Path(e["file"]).name
        if not p.is_file():
            continue
        n += 1
        w = struct.unpack("<120H", raw[e["main_dat_index"] * 240:(e["main_dat_index"] + 1) * 240])
        out = []
        for c in range(4):
            for s in range(6):
                out.extend(w[(s * 4 + c) * 5:(s * 4 + c + 1) * 5])
        t = struct.pack("<120H", *out)
        if t == p.read_bytes() and hashlib.sha256(t).hexdigest() == e["sha256"]:
            ok += 1
    check("bins reproduced from the raw rip", ok, n)


for fn in (anchor_ref_hz, bins_reproduce_from_the_rip):
    print(fn.__doc__.split("\n")[0])
    fn()
print()
raise SystemExit(f"{len(FAILED)} FAILED: {', '.join(FAILED)}" if FAILED else "all constants recompute")
