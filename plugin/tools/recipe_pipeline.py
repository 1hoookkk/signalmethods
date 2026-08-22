"""Recipe pipeline: measurement source → instrument body.

Chains the full authoring pipeline: measure → fit frozen poles →
assign lane correspondence → apply recipe family norms →
author zero trajectories → author Q attitude → DC anchor →
taste lint → certify → audition.

Usage:
  python tools/recipe_pipeline.py NAME M0_SOURCE M100_SOURCE \
      --family vocal --q-attitude spare-the-air:5:0.04

  python tools/recipe_pipeline.py NAME M0 M100 M0Q100 M100Q100 \
      --family sweep --q-attitude asymmetric-relay:1,6:2:0.22:0.66

Sources: wav, wav:1.5-3.0, sofa:az=90,el=0, curve.txt, raw:wav, sum:...
(see make_body.py measure() for full spec grammar)
"""
from __future__ import annotations
import argparse
import ctypes as C
import json
import os
import sys
import time
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import (DATUM, ROOT as ARMA_ROOT, fit_arma,
    harmonic_envelope, load_wav, log_grid_target, pack_and_certify,
    roots_response_db, trim_gain_budget, dc_anchor_body)
try:
    from joint_fit import (assign, template_seed, apply_template_order,
        LANE_TEMPLATES, joint_refine)
    _HAS_JOINT_FIT = True
except ImportError:
    _HAS_JOINT_FIT = False
    LANE_TEMPLATES = {}

sys.path.insert(0, str(ROOT / "tools"))
try:
    from taste_linter import (lint_body, Violation)
    _HAS_LINTER = True
except ImportError:
    _HAS_LINTER = False
    Violation = None
from recipe_families import (load_index, family_norms, suggest_template,
    classify_target, list_families)
from q_attitudes import parse_q_spec

PAD = (0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000)
RATE = 44100.0

CATEGORY_Q_DEFAULTS = {
    "WORKHORSE":    "uniform-gentle:0.04",
    "SWEEP":        "asymmetric-relay:1,6:2:0.22:0.40",
    "VOCAL":        "spare-the-air:5:0.04",
    "PHASER+COMB":  "flat",
    "BASS+ACID":    "single-bloomer:6:0.25",
    "TONE+EQ":      "flat",
    "CHARACTER":    "asymmetric-relay:1,6:2:0.22:0.40",
}

def measure(spec: str):
    import re

    if spec.lower().startswith("tf:") or spec.lower().endswith(".tf.json"):
        path = Path(spec[3:] if spec.lower().startswith("tf:") else spec)
        with open(path) as fh:
            d = json.load(fh)
        grid = np.asarray(d["freqs_hz"], dtype=float)
        dbs = np.asarray(d["mag_db"], dtype=float)
        dbs = dbs - dbs.mean()
        kind = d.get("kind", "tf")
        f0 = d.get("f0_hz")
        label = f"{Path(d.get('source', path)).stem} [{kind}]"
        if f0:
            label += f" f0={f0:.0f}Hz"
        return label, grid, dbs

    if spec.lower().startswith("cat:"):
        from catalogue import load as _cat_load, find as _cat_find, entry_to_spec
        entry = _cat_find(_cat_load(), spec[4:])
        return measure(entry_to_spec(entry))

    if spec.lower().startswith("diff:"):
        a_spec, b_spec = spec[5:].split("|", 1)
        la, ga, da = measure(a_spec)
        lb, gb, db_ = measure(b_spec)
        lo, hi = max(ga[0], gb[0]), min(ga[-1], gb[-1])
        if hi <= lo * 1.5:
            raise SystemExit(f"diff: measurements barely overlap")
        grid = np.geomspace(lo, hi, 256)
        dbs = (np.interp(np.log(grid), np.log(ga), da)
               - np.interp(np.log(grid), np.log(gb), db_))
        dbs -= dbs.mean()
        return f"[{la}] minus [{lb}]", grid, dbs

    m = re.match(r"^(.*?\.wav)(?::([\d.]+)-([\d.]+))?$", spec, re.I)
    if m:
        path = Path(m.group(1))
        x, sr = load_wav(path)
        t0 = float(m.group(2)) if m.group(2) else 0.0
        t1 = float(m.group(3)) if m.group(3) else len(x) / sr
        seg = x[int(t0 * sr):int(t1 * sr)]
        f0, hf, hdb = harmonic_envelope(seg, sr)
        if len(hf) >= 8:
            grid, dbs, _ = log_grid_target(hf, hdb)
            label = f"{path.stem} {t0:.2f}-{t1:.2f}s f0={f0:.0f}Hz"
            return label, grid, dbs
        from scipy.signal import welch
        nseg = min(len(seg), 4096)
        fr, pxx = welch(seg, fs=sr, nperseg=nseg, noverlap=nseg // 2)
        lo, hi = 60.0, min(16000.0, sr * 0.45)
        grid = np.geomspace(lo, hi, 256)
        ratio = (hi / lo) ** (1.0 / (len(grid) - 1))
        dbs = np.empty_like(grid)
        for i, gc in enumerate(grid):
            sel = (fr >= gc / ratio) & (fr <= gc * ratio)
            p = pxx[sel].mean() if sel.any() else np.interp(gc, fr, pxx)
            dbs[i] = 10.0 * np.log10(max(p, 1e-18))
        dbs -= dbs.mean()
        label = f"{path.stem} {t0:.2f}-{t1:.2f}s LTAS"
        return label, grid, dbs

    raise SystemExit(f"unrecognised source spec: {spec}")

def author_zeros(corner_roots: list, family_id: str, idx: dict):
    try:
        norms = family_norms(idx, family_id)
    except KeyError:
        return corner_roots

    for ci, roots in enumerate(corner_roots):
        for si, stage in enumerate(roots):
            if si >= len(norms["lanes"]):
                continue
            ln = norms["lanes"][si]
            pose = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"][ci]
            z2p = ln.get("zero_to_pole_octaves", {}).get(pose)
            if z2p is None:
                continue

            zhz, zr = stage[2], stage[3]
            phz = stage[0]
            if zr < 0.01 or phz < 1:
                continue

            current_octaves = np.log2(zhz / phz) if zhz > 0 and phz > 0 else 999
            lo = z2p.get("min", -10)
            hi = z2p.get("max", 10)

            if current_octaves < lo:
                stage[2] = phz * (2 ** lo)
            elif current_octaves > hi:
                stage[2] = phz * (2 ** hi)

    return corner_roots

def apply_q_attitude(corner_roots_q0, corner_roots_q100, q_deltas: dict):
    for qi in [2, 3]:
        for si in range(6):
            lane = si + 1
            delta = q_deltas.get(lane, 0.0)
            if delta != 0.0:
                corner_roots_q100[qi][si][1] = min(
                    0.999, corner_roots_q100[qi][si][1] + delta)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("name", nargs="?", help="body name")
    ap.add_argument("sources", nargs="*", help="2 or 4 source specs")
    ap.add_argument("--family", default=None,
                    help="Recipe family ID (R001-R033) or 'auto'")
    ap.add_argument("--q-attitude", default="flat",
                    help="Q attitude: flat, single-bloomer:N:Δr, asymmetric-relay:A,B:C:D:Δr, spare-the-air:N:Δr, uniform-gentle:Δr, all-negative:Δr")
    ap.add_argument("--template", default=None,
                    help="Lane template name (vowel, sweep, phaser, wah, ...)")
    ap.add_argument("--no-joint", action="store_true",
                    help="Skip joint refinement (faster, less accurate)")
    ap.add_argument("--list-families", action="store_true",
                    help="List available recipe families and exit")
    ap.add_argument("--list-templates", action="store_true",
                    help="List available lane templates and exit")
    ap.add_argument("--from-catalogue", nargs="+", metavar="NAME",
                    help="Use recipes/catalogue.json entries as the sources: "
                         "2 names (M0 M100) or 4 (adds the Q100 corners). "
                         "A single name with a morph_pair expands to its two ends.")
    ap.add_argument("--list-catalogue", action="store_true",
                    help="List catalogue entries and exit")
    args = ap.parse_args()

    if args.list_catalogue:
        from catalogue import load as _cat_load, list_entries
        for e in list_entries(_cat_load()):
            print(f"  {e.get('family',''):16s}  {e.get('name','')}")
        return

    if args.list_families:
        idx = load_index()
        for rid, cat, tags in list_families(idx):
            print(f"  {rid}  {cat:15s}  {', '.join(tags)}")
        return

    if args.list_templates:
        for name in sorted(LANE_TEMPLATES):
            print(f"  {name}")
        return

    if not args.name:
        ap.error("name is required (or use --list-families / --list-templates / --list-catalogue)")

    sources = args.sources
    if args.from_catalogue:
        from catalogue import load as _cat_load, find as _cat_find, entry_to_spec
        cat = _cat_load()
        names = list(args.from_catalogue)

        if len(names) == 1:
            entry = _cat_find(cat, names[0])
            pair = entry.get("morph_pair")
            if not pair:
                ap.error(f"{names[0]!r} is a single endpoint — give two names, "
                         "or pick an entry that declares a morph_pair")
            names = list(pair)

        if len(names) not in (2, 4):
            ap.error("--from-catalogue needs 2 or 4 entry names")

        sources = []
        for n in names:
            entry = _cat_find(cat, n)
            spec = entry_to_spec(entry)
            print(f"  catalogue: {entry['name']}  ->  {spec}")
            sources.append(spec)

    if len(sources) not in (2, 4):
        ap.error("need 2 or 4 source specs (or --from-catalogue)")

    evidence_dir = ROOT / "evidence" / f"body_{args.name}_e2e"
    evidence_dir.mkdir(parents=True, exist_ok=True)

    t0 = time.monotonic()
    report = []

    print("=" * 60)
    print(f"RECIPE PIPELINE: {args.name}")
    print("=" * 60)
    print()

    targets = []
    for spec in sources:
        label, grid, dbs = measure(spec)
        targets.append((label, grid, dbs))
        print(f"  measured: {label}  ({len(grid)} pts, {dbs.min():.0f}..{dbs.max():.0f} dB)")

    print()
    print("--- Stage 2: Frozen pole fit ---")
    corner_roots = []
    corner_words = []
    for label, grid, dbs in targets:
        roots, words, metrics = fit_arma(grid, dbs, RATE)
        corner_roots.append(roots)
        corner_words.append(words)
        print(f"  {label}: fit rms={metrics[0]:.2f} dB, sections={int(metrics[2])}")
        for si, (phz, pr, zhz, zr, sc) in enumerate(roots):
            if pr > 0 or zr > 0:
                print(f"    S{si+1}: pole={phz:.0f}Hz r={pr:.3f}  "
                      f"zero={zhz:.0f}Hz r={zr:.3f}  scale={sc:.3f}")

    print()
    print("--- Stage 3: Lane correspondence ---")
    if len(corner_roots) == 2:
        corner_roots = [corner_roots[0], corner_roots[1],
                        corner_roots[0].copy(), corner_roots[1].copy()]
        corner_words = [corner_words[0], corner_words[1],
                        corner_words[0], corner_words[1]]
        report.append("Q axis collapsed (2 sources): Q100 = byte copy of Q0")

    if _HAS_JOINT_FIT:
        perm = assign(corner_roots)
        print(f"  lane assignment: {perm}")
    else:
        print("  lane assignment: skipped (scipy not available — install for correspondence)")

    print()
    print("--- Stage 4: Recipe family ---")
    idx = load_index() if args.family else None

    if args.family and args.family != "auto":
        family_id = args.family
        print(f"  family: {family_id} (user-specified)")
    elif args.family == "auto" and idx:
        mid_grid = targets[0][1]
        mid_dbs = targets[0][2]
        family_id = classify_target(mid_grid, mid_dbs, idx)
        print(f"  family: {family_id} (auto-classified)")
    else:
        family_id = None
        print("  family: none specified")

    smart_q = args.q_attitude
    if family_id and idx:
        fams = {rid: (cat, tags) for rid, cat, tags in list_families(idx)}
        cat, tags = fams.get(family_id, ("?", ["?"]))
        print(f"  category: {cat}  tags: {tags}")
        if _HAS_JOINT_FIT:
            roles = suggest_template(idx, family_id)
            print(f"  suggested roles: {roles}")
        if args.q_attitude == "flat" and cat in CATEGORY_Q_DEFAULTS:
            smart_q = CATEGORY_Q_DEFAULTS[cat]
            print(f"  smart Q default: {smart_q}")

    template_name = args.template
    if template_name and _HAS_JOINT_FIT:
        print(f"  template: {template_name}")
        corner_roots = [template_seed(corner_roots, template_name)]
        x_flat = []
        sc_flat = []
        for roots in corner_roots:
            for stage in roots:
                x_flat.extend(stage[:4])
                sc_flat.append(stage[4])

    print()
    print("--- Stage 5: Zero authoring ---")
    if family_id and idx:
        corner_roots = author_zeros(corner_roots, family_id, idx)
        print(f"  zeros nudged toward {family_id} norms")
    else:
        print("  skipped (no recipe family)")

    print()
    print("--- Stage 6: Q attitude ---")
    q_deltas = parse_q_spec(smart_q)
    print(f"  profile: {smart_q}")
    for lane, delta in sorted(q_deltas.items()):
        if delta != 0:
            print(f"    lane {lane}: Δr={delta:+.3f}")
    apply_q_attitude(corner_roots, corner_roots, q_deltas)

    print()
    print("--- Stage 7: Trim + pack ---")
    all_words = []
    for roots in corner_roots:
        words, trimmed, peak_db = trim_gain_budget(roots, RATE)
        all_words.extend(words)
    body, max_r = pack_and_certify(all_words)
    print(f"  packed: max_r={max_r:.6f}")

    print()
    print("--- Stage 8: DC anchor ---")
    body = dc_anchor_body(body, RATE)
    body2, max_r2 = pack_and_certify(
        list(np.frombuffer(body, dtype='<u2').astype(int)))
    print(f"  DC-anchored: max_r={max_r2:.6f}")

    print()
    print("--- Stage 9: Taste linter ---")
    violations = []
    if _HAS_LINTER:
        try:
            lib = C.CDLL(str(ROOT / "target" / "release" / "trench_core.dll"))
            violations = lint_body(
                evidence_dir.parent / f"{args.name}.body240", lib)
        except Exception:
            tmp = evidence_dir / "_lint_tmp.body240"
            tmp.write_bytes(body2)
            lib = C.CDLL(str(ROOT / "target" / "release" / "trench_core.dll"))
            violations = lint_body(tmp, lib)
    else:
        print("  skipped (taste_linter not available)")

    if violations:
        print(f"  {len(violations)} VIOLATION(S):")
        for v in violations:
            print(f"    {v}")
    elif _HAS_LINTER:
        print("  CLEAN")

    out = ROOT / "bodies" / "candidates" / f"{args.name}.body240"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(body2)
    print(f"\n  wrote {out}")

    elapsed = time.monotonic() - t0
    print(f"\n  pipeline complete in {elapsed:.1f}s")

    report.append(f"family: {family_id or 'none'}")
    report.append(f"q_attitude: {args.q_attitude}")
    report.append(f"template: {template_name or 'none'}")
    report.append(f"lint: {len(violations) if violations else 0} violations")
    report.append(f"certify: max_r={max_r2:.6f}")
    report.append(f"elapsed: {elapsed:.1f}s")
    report.append(f"wrote: {out}")
    (evidence_dir / "report.txt").write_text("\n".join(report))

    print(f"\n  evidence → {evidence_dir}")

if __name__ == "__main__":
    main()
