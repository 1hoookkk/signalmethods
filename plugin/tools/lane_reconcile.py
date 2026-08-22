#!/usr/bin/env python3
from __future__ import annotations

import glob
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NVOICE = 4

def formant_row_count(o):
    rows = o.get("formants") or o.get("modes") or o.get("rows")
    return 0 if rows is None else len(rows)

def main():
    print("A. COMPILER GRAMMAR (batch_compiler.py)")
    print("   S1 conditioner (static, family median)")
    print("   S2-S5 talkers  -> exactly 4 measured fundamentals demanded today")
    print("   S6 conditioner (static, family median, unit zero)\n")

    print("B. TABLES vs the 4-talker demand (rows per object):")
    deltas = {}
    for f in sorted((ROOT / "recipes/tables" / "academia").glob("*.json")):
        d = json.loads(f.read_text())
        if d.get("schema") != "acoustic-source-v1":
            continue
        objs = d.get("objects", [])
        if not objs:
            continue
        counts = [formant_row_count(o) for o in objs]
        n3 = sum(1 for c in counts if c == 3)
        n4 = sum(1 for c in counts if c == 4)
        n5 = sum(1 for c in counts if c >= 5)
        other = sum(1 for c in counts if c not in (3, 4) and c >= 1)
        zero = sum(1 for c in counts if c == 0)
        status = "COMPILES:4-talker" if n4 == len(objs) else (
            "REJECTED today (3 or 5)" if n3 or n5 else "NOT-4-row")
        print(f"   {f.name:44s} n={len(objs):5d}  "
              f"3-row:{n3:4d} 4-row:{n4:4d} 5+:{n5:3d} other:{other:3d} "
              f"0-row:{zero:3d}  -> {status}")
        deltas[f.name] = n3 + n5

    print("\nC. RECIPE PROFILES - declared voice roles:")
    for f in sorted((ROOT / "recipes/profiles").glob("*.trenchprofile.json")):
        d = json.loads(f.read_text())
        vr = d.get("voice_roles", {})
        print(f"   {f.name:44s} required={vr.get('required')}")

    print("\nD. WHO CONSUMES THE PROFILES for lane assignment?")
    hits = []
    for p in (ROOT / "tools").glob("*.py"):
        txt = p.read_text(errors="ignore")
        if "trenchprofile" in txt and "voice_roles" in txt:
            hits.append(p.name)
    print("   profiles wired into:", hits or "NOTHING - the compiler reads "
          "dossiers (load_anchors), not these profiles")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
