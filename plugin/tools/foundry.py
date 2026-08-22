"""THE FOUNDRY — the body compiler, staged for camera.

Runs the REAL pipeline (same code as make_body: measure -> fit -> joint
correspondence race -> certify), narrating each stage with live terminal
visuals. Every number on screen is the actual build; nothing is staged.

  python tools/foundry.py NAME c00 c10 c01 c11     # any make_body source spec

Rehearsal/performance: the first run does the real compile and caches every
number plus the finished body (evidence/foundry_cache/NAME.json). Every run
after that REPLAYS the same genuine build at camera pace — seconds per
stage, zero grinding, identical data. --fresh forces a new compile.

The body lands in bodies/candidates/NAME.body240 — drag it into TRENCH.
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

import numpy as np

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
sys.path.insert(0, str(ROOT / "tools"))

from arma_measure_lib import (DATUM, fit_arma, formant_peaks,  # noqa: E402
                              pack_and_certify, trim_gain_budget)
from joint_fit import certify as certify_bytes  # noqa: E402
from joint_fit import joint_refine, surface_rms  # noqa: E402
from make_body import measure  # noqa: E402

DIM = "\x1b[2m"; BOLD = "\x1b[1m"; OFF = "\x1b[0m"
TEAL = "\x1b[38;5;43m"; AMBER = "\x1b[38;5;214m"; RED = "\x1b[38;5;203m"
BLOCKS = " ▁▂▃▄▅▆▇█"

def slow(text, dt=0.012):
    for ch in text:
        sys.stdout.write(ch)
        sys.stdout.flush()
        time.sleep(dt)
    print()

def banner(title):
    print()
    slow(f"{BOLD}{TEAL}══ {title} " + "═" * max(4, 58 - len(title)) + OFF,
         0.004)
    time.sleep(0.3)

def spark(dbs, width=64, lo=-60.0, hi=30.0):
    idx = np.linspace(0, len(dbs) - 1, width).astype(int)
    out = []
    for v in dbs[idx]:
        t = int(np.clip((v - lo) / (hi - lo), 0, 1) * (len(BLOCKS) - 1))
        out.append(BLOCKS[t])
    return "".join(out)

def compile_real(name, specs):
    corner_names = ["M0 Q0", "M100 Q0", "M0 Q100", "M100 Q100"]
    show = {"name": name, "corners": [], "fits": []}
    curves = []
    for spec, cn in zip(specs, corner_names):
        label, grid, dbs = measure(spec)
        curves.append((grid, dbs))
        show["corners"].append({"cn": cn, "label": label,
                                "spark": spark(dbs)})
    corner_roots, corner_words = [], []
    for (grid, dbs), cn in zip(curves, corner_names):
        pins = formant_peaks(grid, dbs)
        roots, _, metrics = fit_arma(grid, dbs, pinned_hz=pins)
        words, trimmed, _ = trim_gain_budget(roots)
        corner_roots.append(trimmed)
        corner_words.append(list(words))
        show["fits"].append({"cn": cn, "rms": metrics[0], "pins": list(pins)})
    body_naive = pack_and_certify(sum(corner_words, []))[0]
    t0 = time.time()
    body_j, info = joint_refine(corner_roots, curves)
    show["rms_naive"] = surface_rms(body_naive, info["targets"],
                                    info["freqs"], DATUM)
    show["probe_losses"] = info["probe_losses"]
    show["winner"] = info["winning_seed"]
    show["loss_after"] = info["loss_after"]
    show["race_seconds"] = time.time() - t0
    if body_j is not None:
        ok, mr = certify_bytes(body_j)
    else:
        ok, mr = 0, 1.0
    body = body_j if (body_j is not None and ok) else body_naive
    show["certified"] = bool(body_j is not None and ok)
    show["max_r"] = mr
    show["body_hex"] = body.hex()
    return show

def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    name = sys.argv[1]
    fresh = "--fresh" in sys.argv
    specs = [a for a in sys.argv[2:] if a != "--fresh"]

    import json
    cache = ROOT / "evidence" / "foundry_cache" / f"{name}.json"
    if cache.exists() and not fresh:
        show = json.loads(cache.read_text())
    else:
        if len(specs) < 4:
            raise SystemExit(__doc__)
        print(f"{DIM}rehearsal: compiling {name} for real "
              f"(one-time; the show replays instantly)...{OFF}")
        show = compile_real(name, specs)
        cache.parent.mkdir(parents=True, exist_ok=True)
        cache.write_text(json.dumps(show))
        print(f"{DIM}cached. run again for the camera take.{OFF}")
        return

    perform(show)

def perform(show):
    name = show["name"]

    print()
    logo = r"""
 ████████╗██████╗ ███████╗███╗   ██╗ ██████╗██╗  ██╗
 ╚══██╔══╝██╔══██╗██╔════╝████╗  ██║██╔════╝██║  ██║
    ██║   ██████╔╝█████╗  ██╔██╗ ██║██║     ███████║
    ██║   ██╔══██╗██╔══╝  ██║╚██╗██║██║     ██╔══██║
    ██║   ██║  ██║███████╗██║ ╚████║╚██████╗██║  ██║
    ╚═╝   ╚═╝  ╚═╝╚══════╝╚═╝  ╚═══╝ ╚═════╝╚═╝  ╚═╝
              T H E   F O U N D R Y"""
    for line in logo.splitlines():
        print(f"{BOLD}{TEAL}{line}{OFF}")
        time.sleep(0.08)
    print()
    slow(f"{DIM}   one filter, four corners, six sections, 240 bytes.{OFF}",
         0.02)
    slow(f"{AMBER}   forging: {BOLD}{name}{OFF}", 0.02)

    banner("STAGE 1 · MEASURE")
    for c in show["corners"]:
        print(f"  {AMBER}{c['cn']:10s}{OFF} {c['spark']}")
        print(f"  {DIM}{'':10s} {c['label']}{OFF}")
        time.sleep(0.4)

    banner("STAGE 2 · FIT — poles placed on the measured resonances")
    for f in show["fits"]:
        pinstr = ", ".join(f"{p:.0f} Hz" for p in f["pins"]) or "free"
        print(f"  {AMBER}{f['cn']:10s}{OFF} rms {f['rms']:5.2f} dB   "
              f"poles at {pinstr}")
        time.sleep(0.4)

    banner("STAGE 3 · THE RACE — which section rides which lane")
    slow(f"{DIM}  every seed is a different guess at the filter's anatomy;"
         f" they fight through the real engine.{OFF}", 0.008)
    print()
    for i, (lbl, loss) in enumerate(
            sorted(show["probe_losses"].items(), key=lambda kv: kv[1])):
        mark = f"{TEAL}◀ WINNER{OFF}" if lbl == show["winner"] else ""
        print(f"   {i+1:2d}. {lbl:22s} {loss:6.2f} dB  {mark}")
        time.sleep(0.15)
    print()
    slow(f"  naive build: {RED}{show['rms_naive']:.2f} dB{OFF} interior error"
         f"  →  after the race: {TEAL}{show['loss_after']:.2f} dB{OFF}"
         f"   ({show['race_seconds']:.0f}s of engine time)", 0.01)

    banner("STAGE 4 · CERTIFY — 33×33 wheel positions, every one stable")
    body = bytes.fromhex(show["body_hex"])
    mr = show["max_r"]
    if not show["certified"]:
        slow(f"  {RED}refused — the naive build ships instead{OFF}")
    else:
        stamp = r"""
   ┌─────────────────────────────────┐
   │  ██████ ███████ ██████  ████    │
   │  ██     ██      ██   ██  ██     │
   │  ██     █████   ██████   ██     │
   │  ██     ██      ██   ██  ██     │
   │  ██████ ███████ ██   ██ ████    │
   │        C E R T I F I E D        │
   └─────────────────────────────────┘"""
        for line in stamp.splitlines():
            print(f"{TEAL}{BOLD}{line}{OFF}")
            time.sleep(0.06)
        slow(f"   hottest pole r = {mr:.6f} "
             f"{DIM}(1.0 = blowup; the ROM lives at 0.999){OFF}", 0.015)

    out = ROOT / "bodies" / "candidates" / f"{name}.body240"
    out.write_bytes(body)
    banner("STAGE 5 · SHIP")
    slow(f"  240 bytes.  {BOLD}{out}{OFF}", 0.01)
    slow(f"{DIM}  drag it into TRENCH. the wheel does the rest.{OFF}", 0.02)
    print()

if __name__ == "__main__":
    main()
