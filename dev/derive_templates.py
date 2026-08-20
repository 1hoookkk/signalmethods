"""Generic stage templates: the menu the 8,593-entry TF bank cannot be.

The bank is the resolution index -- it must be exact and total, so it has one
entry per distinct transfer function and is never shown to a person. A menu
needs the opposite: a handful of families, each with frequency left free.

A template is what remains when absolute frequency is factored out:

    does it have a pole?  a zero?
    where does the zero sit relative to its pole   (semitones)
    how resonant is the pole                       (R' = 20 log10(1/(1-R)))
    how resonant is the zero

Seating a template asks only for a frequency; everything else comes with it.
"""
import os
import json
import math
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BANK = os.path.join(ROOT, "ref", "stage_vocabulary.json")
OUT = os.path.join(ROOT, "ref", "stage_templates.json")
FS = 39062.5


def rprime(r):
    if r >= 1.0:
        return 60.0
    if r <= 0.0:
        return 0.0
    return min(60.0, 20.0 * math.log10(1.0 / (1.0 - r)))


def q_band(rp):
    if rp < 2:
        return "flat"
    if rp < 12:
        return "broad"
    if rp < 26:
        return "tight"
    if rp < 45:
        return "sharp"
    return "extreme"


def st_band(d):
    if d is None:
        return None
    a = abs(d)
    if a < 1:
        return "on the pole"
    lab = "above" if d > 0 else "below"
    if a < 4:
        return f"just {lab}"
    if a < 9:
        return f"{lab} by a fifth"
    if a < 15:
        return f"{lab} by an octave"
    if a < 27:
        return f"{lab} by two octaves"
    return f"far {lab}"


def main():
    V = json.load(open(BANK, encoding="utf-8"))
    seen, rows = set(), []
    for pos in V["positions"].values():
        for e in pos["shapes"]:
            if e["name"] in seen:
                continue
            seen.add(e["name"])
            rows.append(e)

    tmpl = defaultdict(lambda: dict(occ=0, entries=0, filters=set(),
                                    ops=Counter(), pos=Counter(),
                                    hz=[], examples=[]))
    for e in rows:
        s = e["seat"]
        fp, rp, fz, rz = (s["pole_hz"], s["pole_r"], s["zero_hz"], s["zero_r"])
        has_p, has_z = rp > 0.02, rz > 0.02
        b0, b1, b2, a1, a2 = e["tf"]
        if not has_p and not has_z:
            if abs(b0 - 1) < 1e-9 and abs(b1) < 1e-9 and abs(a1) < 1e-9:
                key = ("bypass", None, None, None)
            else:
                key = ("gain only", None, None, None)
        elif has_p and not has_z:
            key = ("pole only", None, q_band(rprime(rp)), None)
        elif has_z and not has_p:
            key = ("zero only", None, None, q_band(rprime(rz)))
        else:
            d = 12 * math.log2(max(fz, 1e-6) / max(fp, 1e-6))
            key = ("pole + zero", st_band(d), q_band(rprime(rp)),
                   q_band(rprime(rz)))
        t = tmpl[key]
        t["occ"] += e["evidence"]["occurrences"]
        t["entries"] += 1
        t["filters"].update(e["evidence"]["exemplar_filters"])
        for op, n in e["operations"].items():
            t["ops"][op] += n
        if has_p:
            t["hz"].append(fp)
        if len(t["examples"]) < 3:
            t["examples"].append(e["name"])

    total = sum(t["occ"] for t in tmpl.values())
    out = []
    for key, t in sorted(tmpl.items(), key=lambda kv: -kv[1]["occ"]):
        kind, rel, pq, zq = key
        if kind == "pole + zero":
            label = f"{pq} pole, {zq} zero {rel}"
        elif kind == "pole only":
            label = f"{pq} pole, no zero"
        elif kind == "zero only":
            label = f"{zq} zero, no pole"
        else:
            label = kind
        hz = sorted(t["hz"])
        out.append({
            "template": label,
            "kind": kind,
            "zero_vs_pole": rel,
            "pole_q": pq,
            "zero_q": zq,
            "free_parameter": "frequency" if kind != "bypass" else None,
            "occurrences": t["occ"],
            "share": round(t["occ"] / total, 4),
            "distinct_tfs": t["entries"],
            "frequency_range_hz": ([round(hz[0], 1),
                                    round(hz[len(hz) // 2], 1),
                                    round(hz[-1], 1)] if hz else None),
            "operations": dict(t["ops"].most_common(4)),
            "example_entries": t["examples"],
        })
    json.dump({"contract": "generic stage templates: absolute frequency is the "
                           "free parameter, everything else is the family. "
                           "Resolution still goes through ref/"
                           "stage_vocabulary.json by_tf; this is the menu.",
               "generated_by": "dev/derive_templates.py",
               "datum_sr_hz": FS,
               "templates": out},
              open(OUT, "w", encoding="utf-8"), indent=1)

    print(f"{len(rows)} distinct transfer functions  ->  {len(out)} templates")
    print(f"wrote {OUT}")
    print()
    print(f"{'template':<42}{'share':>7}{'TFs':>7}{'freq lo/med/hi':>26}")
    cum = 0.0
    for r in out:
        cum += r["share"]
        fr = (f"{r['frequency_range_hz'][0]:.0f} / "
              f"{r['frequency_range_hz'][1]:.0f} / "
              f"{r['frequency_range_hz'][2]:.0f}"
              if r["frequency_range_hz"] else "-")
        print(f"{r['template']:<42}{100*r['share']:6.1f}%{r['distinct_tfs']:7d}"
              f"{fr:>26}")
        if cum > 0.97:
            break
    print()
    print(f"top 10 templates cover {100*sum(r['share'] for r in out[:10]):.1f}% "
          f"of all stage instances")
    print(f"top 20 cover {100*sum(r['share'] for r in out[:20]):.1f}%")


if __name__ == "__main__":
    main()
