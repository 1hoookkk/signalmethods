#!/usr/bin/env python3
"""pair_bandwidth_priors.py — give the survey corpora a compilable bandwidth.

Hillenbrand (1995) and Peterson-Barney (1952) are the grand-scale vowel
corpus: measured F1-F3 frequencies and F0 for 3,188 utterances, but NO
measured bandwidth (r = exp(-pi*BW/fs) needs a real BW). We cannot invent
bandwidth and we must not imply these radii came from the survey.

So the frequency stays MEASURED and the bandwidth is AUTHORED PRIOR, taken
from the one bulk measured-bandwidth corpus we own: ETL (Mokhtari & Tanaka
2000 — 2,750 vowel rows, all with F1-F4 measured bandwidth). The prior is
the ETL median bandwidth for a formant, optionally conditioned on the vowel
label; every row is marked with BOTH provenances.

What we build here:
  1. MEASURED-BW PRIOR  -> ETL median B1..B4 (all / by vowel label).
  2. SURVEY AGGREGATE    -> per (dataset, vowel label) representative
     endpoints: median F1/F2/F3 and F0 across speakers. "Average it out" so
     the workstation has a browsable handful per table, not 3,188 rows.
  3. PAIRED OBJECTS      -> out table: measured frequencies, authored-prior
     bandwidth (ETL median), full provenance on every row.

Never Klatt. Never implied measured. Bandwidth is always flagged.

Usage:
  python tools/pair_bandwidth_priors.py \
      --survey recipes/tables/academia/hillenbrand_1995.json \
      --prior-source etl_mokhtari_tanaka_2000 \
      --out recipes/tables/academia/h95_paired.json
  (repeats per survey table; the same prior is shared)
"""
from __future__ import annotations

import argparse
import json
import statistics as st
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ACAD = ROOT / "recipes/tables" / "academia"

def load_table(path: Path):
    return json.loads(path.read_text())

def etl_bandwidth_prior(by_vowel: bool):
    d = load_table(ACAD / "etl_mokhtari_tanaka_2000.json")
    acc = {}
    for o in d["objects"]:
        vowel = (o.get("label") or "").lower()
        rows = o.get("formants") or []
        for r in rows:
            f = r.get("mode_or_formant")
            bw = r.get("bandwidth_hz")
            if not f or not bw:
                continue
            key = (vowel if by_vowel else "all", f.upper())
            acc.setdefault(key, []).append(bw)
    out = {"all": {}, "by_vowel": {}}
    for (vowel, f), bws in acc.items():
        col = out["by_vowel"] if vowel != "all" else out["all"]
        col[f] = float(st.median(bws))
    return out

def aggregate_survey(path: Path):
    d = load_table(path)
    groups = {}
    for o in d["objects"]:
        label = (o.get("label") or "?").lower()
        rows = o.get("formants") or []
        fv = {r.get("mode_or_formant"): r.get("frequency_hz") for r in rows}
        groups.setdefault(label, {"f0": [], "F1": [], "F2": [], "F3": []})
        if o.get("f0_hz"):
            groups[label]["f0"].append(o["f0_hz"])
        for k in ("F1", "F2", "F3"):
            if fv.get(k):
                groups[label][k].append(fv[k])
    out = []
    stem = d.get("dataset", path.stem)
    for label in sorted(groups):
        g = groups[label]
        if not g["F1"] or not g["F2"]:
            continue
        row = [{"mode_or_formant": f"F{i+1}",
                "frequency_hz": st.median(g[f"F{i+1}"])}
               for i in range(3)]
        out.append({"object_id": f"{stem}_{label}",
                    "representative_of": f"vowel {label}",
                    "label": label,
                    "f0_hz": st.median(g["f0"]) if g["f0"] else None,
                    "formants": row})
    return out

def pair(survey_aggregate, prior, name, desc):
    objs = []
    for a in survey_aggregate:
        for r in a["formants"]:
            f = r["mode_or_formant"]
            base = prior["all"].get(f)
            r["bandwidth_hz"] = base
            r["q"] = round(r["frequency_hz"] / base, 1) if base else None
            r["frequency_provenance"] = "measured"
            r["bandwidth_provenance"] = "authored_prior"
            r["prior_source"] = "etl_mokhtari_tanaka_2000 (median measured BW)"
        objs.append(a)
    return {
        "schema": "acoustic-source-v1",
        "dataset": name,
        "category": "vowels",
        "citation": "paired frequency (measured) + ETL bandwidth prior (authored)",
        "source": str(ACAD),
        "measurement_method": desc,
        "note": ("frequencies measured by the survey corpus; bandwidths are "
                 "AUTHORED PRIORS = median ETL measured bandwidth, never "
                 "Klatt, never implied measured. Lane grammar: S2-S4 talkers "
                 "+ S5 sentinel (identity) because the survey carries F1-F3."),
        "objects": objs,
    }

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--survey", required=True, type=Path,
                    help="path to the survey table (hillenbrand / pb)")
    ap.add_argument("--out", required=True, type=Path, help="output table")
    args = ap.parse_args()

    prior = etl_bandwidth_prior(by_vowel=False)
    d = load_table(args.survey)
    agg = aggregate_survey(args.survey)
    name = d.get("dataset", args.survey.stem) + "_paired_etl"
    out = pair(agg, prior, name, d.get("measurement_method", ""))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(out, indent=1))
    print(f"prior (ETL median BW): "
          + "  ".join(f"{k}={v:.1f} Hz" for k, v in
                      sorted(prior["all"].items())))
    print(f"aggregated {len(agg)} representative endpoints from "
          f"{len(d['objects'])} utterances -> {args.out}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
