#!/usr/bin/env python3
import json
import math
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from shapes import _group  # noqa: E402  (the one zero-bucket definition)
from batch_compiler import travel_class, register, q_sign  # noqa: E402

LOCK = 0.75
MIN_REFS = 3

def slot_readings(refs: list[dict], slot: int) -> dict:
    voiced, zbucket, trav, reg, qs = [], [], [], [], []
    for d in refs:
        c = d["corners"]
        for cn in ("M0_Q0", "M100_Q0"):
            row = c[cn][slot]
            ph, pr = row["pole"]["hz"], row["pole"]["radius"]
            zh, zr = row["zero"]["hz"], row["zero"]["radius"]
            live = ph and ph > 20 and pr >= 0.5
            voiced.append("voiced" if live else "frame")
            if live and zh and zh > 20:
                zbucket.append(_group(12 * math.log2(zh / ph), zr))
            elif zh and zh > 20 and zr >= 0.9999:
                zbucket.append("silence line")
            if cn == "M0_Q0" and live:
                reg.append(register(ph))
        a = c["M0_Q0"][slot]["pole"]["hz"]
        b = c["M100_Q0"][slot]["pole"]["hz"]
        trav.append(travel_class(a or 0.0, b or 0.0))
        dq = (c["M0_Q100"][slot]["pole"]["radius"]
              - c["M0_Q0"][slot]["pole"]["radius"])
        qs.append(q_sign(dq))
    return dict(voiced=voiced, zero=zbucket, travel=trav,
                register=reg, q=qs)

def verdict(values: list[str]) -> tuple[str, float]:
    if not values:
        return "-", 0.0
    (top, n), = Counter(values).most_common(1)
    return top, n / len(values)

def main() -> None:
    families: dict[str, list[dict]] = {}
    for f in sorted((ROOT / "dossiers" / "characters").glob("P2k_*.json")):
        d = json.loads(f.read_text())
        families.setdefault(d.get("x3_type", "?"), []).append(d)

    out = {"source": "dossiers/characters/P2k_*.json",
           "lock_threshold": LOCK, "min_refs": MIN_REFS, "families": {}}
    for fam, refs in sorted(families.items()):
        entry = {"references": [d["name"] for d in refs], "slots": {}}
        print(f"\n{fam}  ({len(refs)} refs: "
              f"{', '.join(d['name'] for d in refs)})")
        if len(refs) < MIN_REFS:
            entry["verdict"] = f"only {len(refs)} reference(s) - " \
                               "nothing to generalise"
            print(f"  {entry['verdict']}")
            out["families"][fam] = entry
            continue
        for slot in range(6):
            r = slot_readings(refs, slot)
            locked, free = {}, {}
            for feat, vals in r.items():
                top, share = verdict(vals)
                (locked if share >= LOCK else free)[feat] = \
                    dict(value=top, agreement=round(share, 2))
            entry["slots"][f"S{slot + 1}"] = dict(locked=locked, free=free)
            lk = "  ".join(f"{k}={v['value']} {v['agreement']:.0%}"
                           for k, v in locked.items())
            print(f"  S{slot + 1}  LOCKED: {lk if lk else '(nothing)'}")
        out["families"][fam] = entry

    dst = ROOT / "design" / "archetype_sos.json"
    dst.write_text(json.dumps(out, indent=1))
    print(f"\n-> {dst.relative_to(ROOT)}")

if __name__ == "__main__":
    main()
