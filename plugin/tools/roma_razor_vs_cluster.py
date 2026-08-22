#!/usr/bin/env python3
from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]

CARVE = 0.5
CLUSTER = 2.0

def main():
    all_off, carve_off, cluster_carve_off, sparse_carve_off = [], [], [], []
    n_carve = n_own_close = 0
    for f in sorted((ROOT / "dossiers" / "characters").glob("P2k_*.json")):
        d = json.loads(f.read_text())
        for cn, rows in d["corners"].items():
            poles = [(i + 1, r["pole"]["hz"], r["pole"]["radius"])
                     for i, r in enumerate(rows)]
            zeros = [(i + 1, r["zero"]["hz"], r["zero"]["radius"])
                     for i, r in enumerate(rows)]
            for lane, z, _zr in zeros:
                if z <= 0:
                    continue
                own = next(pp for (ll, pp, _r) in poles if ll == lane)
                if own <= 0:
                    continue
                own_st = 12 * math.log2(z / own)
                all_off.append(own_st)
                fs = [(abs(12 * math.log2(z / pp)), ll)
                      for (ll, pp, _r) in poles if ll != lane and pp > 0]
                fs.sort()
                if not fs or fs[0][0] > CARVE:
                    continue
                n_carve += 1
                carve_off.append(own_st)
                victim = fs[0][1]
                v_own = next(pp for (ll, pp, _r) in poles if ll == victim)
                other = [abs(12 * math.log2(v_own / pp))
                         for (ll, pp, _r) in poles
                         if ll != victim and pp > 0]
                tight = bool(other) and min(other) < CLUSTER
                (cluster_carve_off if tight else sparse_carve_off).append(own_st)
    def med(x): return float(np.median(x)) if x else float("nan")
    print("zero-to-OWN-pole offset (st):")
    print(f"  ALL zeros                 n={len(all_off):5d}  med {med(all_off):+5.2f}")
    print(f"  carve set (<{CARVE}st foreign)   n={n_carve:5d}  med {med(carve_off):+5.2f}")
    print(f"    - carve target in a tight cluster: n={len(cluster_carve_off):4d}"
          f"  med {med(cluster_carve_off):+5.2f}")
    print(f"    - carve target isolated       : n={len(sparse_carve_off):4d}"
          f"  med {med(sparse_carve_off):+5.2f}")
    print()
    if carve_off:
        print("READING: if every carve zero keeps the valley (+4..+7 st) offset while")
        print("landing on a neighbour, the 'collision carve' is NOT a species - the")
        print("zero is trying to carve its own pole at +valley and the cluster is so")
        print("dense the neighbour is in the way. If carve med is much lower or")
        print("negative, it IS deliberate neighbour-carving (razor shifted onto the")
        print("next stage).")

if __name__ == "__main__":
    raise SystemExit(main())
