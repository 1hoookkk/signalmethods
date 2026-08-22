#!/usr/bin/env python3
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from batch_compiler import load_table, plan_body, compile_body  # noqa: E402

Q_LIFT = 0.0089
R_CEIL = 0.9995
OUT = ROOT / "bodies" / "atoms"

RATE = None

def build(table: Path, oid: str) -> str:
    if (OUT / table.stem / f"{oid}.body240").exists():
        return "ok"
    rows, sr = load_table(table, oid)
    if RATE:
        sr = RATE
    if not rows:
        return "empty"
    plan = plan_body(rows, rows, sr, lane_order="ascending",
                     q_attitude="flat")
    per_corner = plan["per_corner"]
    for ci in (2, 3):
        for ln in per_corner[ci]:
            if ln["pole_r"] > 0.0:
                ln["pole_r"] = min(R_CEIL, ln["pole_r"] + Q_LIFT)
    body, _info = compile_body(per_corner, sr)
    folder = OUT / table.stem
    folder.mkdir(parents=True, exist_ok=True)
    (folder / f"{oid}.body240").write_bytes(body)
    return "ok"

def main() -> int:
    global RATE, OUT
    args = [a for a in sys.argv[1:] if not a.startswith("--rate=")]
    for a in sys.argv[1:]:
        if a.startswith("--rate="):
            RATE = float(a.split("=")[1])
            OUT = OUT.parent / f"atoms_{int(RATE)}"
    name_filter = args[0] if args else ""
    tables = sorted(p for p in (ROOT / "recipes/tables/academia").glob("*.json")
                    if name_filter in p.name)
    total = dict(ok=0, refused=0, empty=0)
    for table in tables:
        d = json.loads(table.read_text())
        objs = d.get("objects")
        ids = ([o.get("object_id") for o in objs if o.get("object_id")]
               if isinstance(objs, list) else [table.stem])
        counts = dict(ok=0, refused=0, empty=0)
        for oid in ids:
            try:
                counts[build(table, oid)] += 1
            except (SystemExit, Exception):
                counts["refused"] += 1
        for k in total:
            total[k] += counts[k]
        print(f"{table.stem:<40s} ok {counts['ok']:>5d}   "
              f"refused {counts['refused']:>4d}   empty {counts['empty']:>3d}")
    print(f"{'TOTAL':<40s} ok {total['ok']:>5d}   "
          f"refused {total['refused']:>4d}   empty {total['empty']:>3d}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
