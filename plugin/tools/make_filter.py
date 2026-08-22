from __future__ import annotations
import argparse, json, math, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tools.body_from_endpoints import build, EP
from tools.ir_endpoints import bandwidth_hz

MEASURED = ROOT / "filters" / "measured"
SHIP_BODIES = ROOT / "plugin" / "presets" / "bodies"
ROSTER = ROOT / "plugin" / "presets" / "PresetRoster.inc"
VERDICTS = ROOT / "recipes" / "verdicts"


def features(name):
    d = json.loads((EP / f"{name}.endpoint.json").read_text())
    return [(r["pole_hz"], r["pole_bw_hz"], r.get("prominence_db", 0.0)) for r in d["rows"]]


def roster_has(slug):
    return slug in ROSTER.read_text()


def roster_add(label, slug):
    s = ROSTER.read_text().rstrip("\n")
    if "MEASURED" not in s:
        s += ("\n// MEASURED  - solved from instrument-body impulse responses; every named\n"
              "//             resonance is a measured mode with a measured bandwidth.")
    s += f'\nTRENCH_PRESET("{label}", "{slug}", "MEASURED")\n'
    ROSTER.write_text(s)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("endpoint_a")
    ap.add_argument("endpoint_b")
    ap.add_argument("slug")
    ap.add_argument("--label", default=None)
    ap.add_argument("--permutation", default=None,
                    help="comma-separated slot order for the M100 end, e.g. 0,2,1,3,4,5")
    ap.add_argument("--install", action="store_true")
    args = ap.parse_args()

    perm = [int(x) for x in args.permutation.split(",")] if args.permutation else None
    label = args.label or args.slug.replace("_", " ").title()
    body = MEASURED / f"MEAS_{args.slug}.body240"

    rc, msg = build(args.endpoint_a, args.endpoint_b, body, perm)
    if rc != 0:
        print(f"FAILED: {msg}")
        return 1
    print(f"wrote {body}")

    for side, name in (("M0", args.endpoint_a), ("M100", args.endpoint_b)):
        print(f"\n  {side}  {name}")
        print(f"    {'Hz':>8} {'bandwidth Hz':>13} {'Q':>7} {'measured prominence dB':>23}")
        for hz, bw, pr in features(name):
            print(f"    {hz:8.0f} {bw:13.1f} {hz / max(bw, 1e-9):7.1f} {pr:23.1f}")

    VERDICTS.mkdir(parents=True, exist_ok=True)
    rec = {
        "slug": args.slug, "label": label,
        "endpoint_m0": args.endpoint_a, "endpoint_m100": args.endpoint_b,
        "permutation": perm, "body_bytes": body.stat().st_size,
        "body_sha_prefix": __import__("hashlib").sha256(body.read_bytes()).hexdigest()[:16],
        "features_m0": [{"hz": h, "bw_hz": b, "prominence_db": p} for h, b, p in features(args.endpoint_a)],
        "features_m100": [{"hz": h, "bw_hz": b, "prominence_db": p} for h, b, p in features(args.endpoint_b)],
        "verdict": None, "notes": None,
    }
    vpath = VERDICTS / f"{args.slug}.verdict.json"
    if vpath.exists():
        old = json.loads(vpath.read_text())
        rec["verdict"], rec["notes"] = old.get("verdict"), old.get("notes")
    vpath.write_text(json.dumps(rec, indent=1))
    print(f"\nverdict slot: {vpath}  (write your word into \"verdict\")")

    if args.install:
        shutil.copy2(body, SHIP_BODIES / body.name)
        if not roster_has(f"MEAS_{args.slug}"):
            roster_add(label, f"MEAS_{args.slug}")
            print(f"roster += {label}")
        print("run tools\\build_install_vst3.ps1 to put it in the menu")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
