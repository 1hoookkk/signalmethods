from __future__ import annotations

import glob
import json
import os
import subprocess
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOSSIERS = sorted(glob.glob(os.path.join(ROOT, "dossiers", "characters", "P2k_*.json")))
OUTDIR = os.path.join(ROOT, "plots", "inspector")
os.makedirs(OUTDIR, exist_ok=True)

CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
DATUM = 39062.5

def main():
    lane_lines = ["# Sequential lanes — every P2K dossier, every corner",
                  "",
                  "Direct decode listing, ROM stage order S1..S6, datum 39062.5 Hz.",
                  "Format: `S# pole hz/radius (note) | zero hz/radius (note)`.",
                  ""]
    for path in DOSSIERS:
        d = json.load(open(path))
        name = os.path.splitext(os.path.basename(path))[0]
        words = np.array([[r["words"] for r in d["corners"][ck]] for ck in CORNERS],
                         dtype="<u2")
        body = os.path.join(OUTDIR, name + ".body240")
        open(body, "wb").write(words.tobytes())
        subprocess.run([sys.executable, os.path.join(ROOT, "tools", "inspect_body.py"),
                        body, str(DATUM)], check=True, capture_output=True)
        lane_lines.append(f"## {name} [{d['x3_type']}] — {d['x3_one_liner']}")
        for ck in CORNERS:
            rows = d["corners"][ck]
            lane_lines.append(f"### {ck}  (scale_db S1..S6: "
                              + ", ".join(f"{r['scale_db']:+.2f}" for r in rows) + ")")
            for k, r in enumerate(rows):
                p, z0 = r["pole"], r["zero"]
                lane_lines.append(
                    f"- S{k+1} pole {p['hz']:.0f}/{p['radius']:.4f} ({p['note']})"
                    f" | zero {z0['hz']:.0f}/{z0['radius']:.4f} ({z0['note']})"
                    + (" [real-pair pole]" if p["real_pair"] else "")
                    + (" [real-pair zero]" if z0["real_pair"] else ""))
        lane_lines.append("")
        print(name)
    open(os.path.join(ROOT, "docs", "LANES_SEQUENTIAL.md"), "w",
         encoding="utf-8").write("\n".join(lane_lines))
    print("plates + packed bodies:", OUTDIR)

if __name__ == "__main__":
    main()
