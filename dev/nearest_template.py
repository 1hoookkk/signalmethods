"""Rank every authored start point against a target curve.

Fitting from an empty body makes the solver invent lane ownership. This picks
the nearest already-authored object instead, so the fit begins from a design a
human made, with its lanes already assigned, and only has to close the residual.

The pool is every start point we hold:
  ref/md_templates   E-mu's own Morph Designer templates, compiled by the
                     FUN_1802c6590 arithmetic
  ref/x3             the 17 generic forms, compiled by the CPhantom classes
  ref/presets        the 33 authored character skins

Distance is level-aligned RMS in dB over the repo's 1024-point log grid,
40-16,000 Hz at 44,100 Hz, taken at the corner that fits best.

Usage:
  python dev/nearest_template.py <target.json|mouth-name> [--top 10]
"""
import sys, os, glob, json
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "cell_dictionary"))
import decode_lib as dl

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SR = 44100.0
GRID = np.array(dl.log_grid_hz(40.0, 16000.0, 1024))
POOLS = [("md", "ref/md_templates"), ("x3", "ref/x3"), ("skin", "ref/presets")]


def corner_curves(path):
    body = dl.decode_p2k_body(open(path, "rb").read(), SR)
    out = []
    for ci, corner in enumerate(body):
        live = [g for g in corner if not dl.stage_is_identity(g)]
        if not live:
            continue
        c = np.asarray(dl.corner_response_db(live, list(GRID), SR))
        if np.isfinite(c).all():
            out.append((ci, c, len(live)))
    return out


def pool():
    items = []
    for tag, rel in POOLS:
        for p in sorted(glob.glob(os.path.join(ROOT, rel, "*.bin"))):
            name = os.path.splitext(os.path.basename(p))[0]
            for ci, curve, live in corner_curves(p):
                items.append((tag, name, ci, live, curve))
    return items


def distance(target, curve):
    """Level-aligned RMS: a start point is judged on shape, not on gain."""
    d = target - curve
    return float(np.sqrt(np.mean((d - d.mean()) ** 2)))


def load_target(arg):
    if os.path.exists(arg):
        doc = json.load(open(arg))
        v = doc["target_db"] if isinstance(doc, dict) and "target_db" in doc else doc
        return np.asarray(v, dtype=float)
    hits = glob.glob(os.path.join(ROOT, "recipes", "vocal", "dvtd", "*", "*%s*" % arg, "*.txt"))
    if not hits:
        raise SystemExit("no target matching %r" % arg)
    rows = []
    for line in open(hits[0]):
        parts = line.replace(",", " ").split()
        if len(parts) < 2:
            continue
        try:
            f, mag = float(parts[0]), float(parts[1])
        except ValueError:
            continue
        if f > 0.0 and mag > 0.0:
            rows.append((f, 20.0 * np.log10(mag)))
    if not rows:
        raise SystemExit("no numeric rows in %s" % hits[0])
    hz = np.array([r[0] for r in rows])
    db = np.array([r[1] for r in rows])
    return np.interp(np.log(GRID), np.log(hz), db)


def main(argv):
    if not argv:
        print(__doc__)
        return
    top = 10
    if "--top" in argv:
        top = int(argv[argv.index("--top") + 1])
    target = load_target(argv[0])
    if target.size != GRID.size:
        raise SystemExit("target has %d points, grid has %d" % (target.size, GRID.size))
    items = pool()
    scored = sorted(((distance(target, c), tag, n, ci, live) for tag, n, ci, live, c in items))
    print("target %s -> %d start points from %d bodies\n" % (argv[0], len(items), len(set(i[1] for i in items))))
    print("%-6s %-26s %-6s %-7s %s" % ("pool", "start point", "corner", "stages", "residual dB RMS"))
    for d, tag, n, ci, live in scored[:top]:
        print("%-6s %-26s C%-5d %-7d %6.2f" % (tag, n[:26], ci, live, d))
    best = scored[0]
    print("\nbest: %s / %s / C%d at %.2f dB — seat it, then fit the residual" % (best[1], best[2], best[3], best[0]))


if __name__ == "__main__":
    main(sys.argv[1:])
