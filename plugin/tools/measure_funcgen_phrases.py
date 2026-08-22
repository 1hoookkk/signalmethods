#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ctypes
import json
import math
import re
import statistics
from pathlib import Path

from pyruntime.arma_measure_lib import RT_DOUBLES  # noqa: E402
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parent.parent
SRC = Path(r"C:\Users\hooki\OneDrive\Documents\Creative Professional"
           r"\Emulator X Family\Templates\Function Generator")

SHIP = {
    "16StepShaker": "BREATHE",
    "EightEightRhythm": "RHYTHM",
    "UpToPitchFUN": "RISE",
    "OctaveBrownFUN": "WANDER",
    "Shark Tooth": "ANSWER",
    "Simple Boogaloo": "STABS",
}

N_FREQS = 200
MIN_HZ, MAX_HZ = 20.0, 16000.0

def authoring_sr() -> float:
    txt = (ROOT / "trench-core" / "src" / "compiler.rs").read_text(encoding="utf-8")
    m = re.search(r"AUTHORING_SR:\s*f64\s*=\s*([0-9_.]+)", txt)
    if not m:
        raise RuntimeError("could not read AUTHORING_SR from trench-core/src/compiler.rs")
    return float(m.group(1).replace("_", ""))

def find_dll() -> Path:
    for rel in ("target/release/trench_core.dll",
                "build/cargo-target/release/trench_core.dll",
                "build/plugin-vs2/cargo-target/release/trench_core.dll"):
        p = ROOT / rel
        if p.is_file():
            return p
    raise RuntimeError("trench_core.dll not found — cargo build -p trench-core --release")

def parse(path: Path) -> dict:
    root = ElementTree.parse(path).getroot()
    values = [0.0] * 64
    trigs = [0] * 64
    for v in root.findall("value"):
        values[int(v.get("index"))] = float(v.text.strip())
    for t in root.findall("trigger"):
        trigs[int(t.get("index"))] = int(t.text.strip())
    num = lambda tag, cast=float: cast(root.find(tag).text.strip())
    return {
        "name": root.get("name").strip(),
        "rate": num("rate"),
        "smooth": num("smooth", int),
        "direction": num("direction", int),
        "steps": num("length", int) + 1,
        "values": values,
        "trigs": trigs,
    }

def load_phrases() -> list[dict]:
    out = []
    for f in sorted(SRC.glob("*.xml")):
        p = parse(f)
        vals = p["values"][: p["steps"]]
        peak = max((abs(v) for v in vals), default=0.0)
        if peak < 1e-6:
            continue
        p["values"] = [v / peak for v in vals]
        p["trigs"] = p["trigs"][: p["steps"]]
        out.append(p)
    return out

class Walk:

    def __init__(self, p: dict, seed: int = 0x9E3779B9):
        self.p = p
        self.rng = seed & 0xFFFFFFFF

    def white_bip(self) -> float:
        self.rng = (self.rng * 1664525 + 1013904223) & 0xFFFFFFFF
        return (self.rng >> 8) * (1.0 / 8388608.0) - 1.0

    def position_for(self, rel: int) -> int:
        n = self.p["steps"]
        if n <= 1:
            return 0
        m = max(rel, 0)
        d = self.p["direction"]
        if d == 1:
            return n - 1 - (m % n)
        if d == 2:
            period = 2 * n - 2
            q = m % period
            return q if q < n else period - q
        if d == 5:
            return m if m < n else n - 1
        return m % n

    def next_position(self, pos: int, rel_next: int) -> int:
        n = self.p["steps"]
        if n <= 1:
            return 0
        d = self.p["direction"]
        if d == 3:
            return int((self.white_bip() * 0.5 + 0.5) * n) % n
        if d == 4:
            up = self.white_bip() >= 0.0
            if pos <= 0:
                return 1
            if pos >= n - 1:
                return n - 2
            return pos + 1 if up else pos - 1
        return self.position_for(rel_next)

def morph_series(p: dict, cycles: int, blocks_per_cycle: int,
                 center: float = 0.5, depth: float = 0.5,
                 block_sec: float = 512 / 44100.0) -> list[float]:
    w = Walk(p)
    pos = w.position_for(0)
    nxt = w.next_position(pos, 1)
    vals = p["values"]
    smooth = bool(p["smooth"])
    out: list[float] = []
    sm = None
    a = 1.0 - math.exp(-block_sec / 0.015)
    for c in range(cycles):
        if c > 0:
            pos = nxt
            nxt = w.next_position(pos, c + 1)
        for b in range(blocks_per_cycle):
            phase = b / blocks_per_cycle
            cur = vals[pos]
            bip = cur + (vals[nxt] - cur) * phase if smooth else cur
            raw = center + bip * depth
            raw = min(1.0, max(0.0, raw))
            sm = raw if sm is None else sm + (raw - sm) * a
            out.append(sm)
    return out

class Probe:
    def __init__(self, body: bytes, sr: float):
        self.lib = ctypes.CDLL(str(find_dll()))
        self.lib.trench_packed_probe.restype = ctypes.c_int32
        self.buf = (ctypes.c_uint8 * 240).from_buffer_copy(body)
        self.sr = sr
        self.freqs = [MIN_HZ * (MAX_HZ / MIN_HZ) ** (i / (N_FREQS - 1))
                      for i in range(N_FREQS)]
        self._cache: dict[int, list[float]] = {}

    def curve(self, morph: float, q: float = 0.0) -> list[float]:
        key = int(round(morph * 2000))
        hit = self._cache.get(key)
        if hit is not None:
            return hit
        out = (ctypes.c_double * RT_DOUBLES)()
        mr, um, nm = ctypes.c_double(), ctypes.c_uint32(), ctypes.c_uint32()
        rc = self.lib.trench_packed_probe(self.buf, 240, ctypes.c_double(key / 2000.0),
                                          ctypes.c_double(q), out,
                                          ctypes.byref(mr), ctypes.byref(um),
                                          ctypes.byref(nm))
        if rc != 0:
            raise RuntimeError(f"trench_packed_probe rc {rc}")
        rows = [list(out[i * 5:(i + 1) * 5]) for i in range(6)]
        dbs = []
        for f in self.freqs:
            w = 2 * math.pi * f / self.sr
            z = complex(math.cos(-w), math.sin(-w))
            h = complex(1.0, 0.0)
            for b0, b1, b2, a1, a2 in rows:
                den = 1.0 + a1 * z + a2 * z * z
                h *= (b0 + b1 * z + b2 * z * z) / (den if abs(den) > 1e-12 else 1e-12)
            dbs.append(20 * math.log10(max(abs(h), 1e-9)))
        self._cache[key] = dbs
        return dbs

def spectral_dist(a: list[float], b: list[float]) -> float:
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)) / len(a))

def measure(p: dict, probe: Probe, cycles: int, bpc: int, cycle_sec: float) -> dict:
    morphs = morph_series(p, cycles, bpc)
    curves = [probe.curve(m) for m in morphs]
    block_sec = cycle_sec / bpc

    span = max(morphs) - min(morphs)

    bins = [0] * 20
    for m in morphs:
        bins[min(19, int(m * 20))] += 1
    tot = len(morphs)
    ent = -sum((c / tot) * math.log(c / tot) for c in bins if c) / math.log(20)

    deltas = [spectral_dist(curves[i], curves[i - 1]) for i in range(1, len(curves))]
    travel = statistics.mean(deltas) / block_sec if deltas else 0.0

    peak = max(deltas) if deltas else 0.0
    sit = sum(1 for d in deltas if d < 0.02 * max(peak, 1e-9)) / len(deltas) if deltas else 0.0
    mov = sum(1 for d in deltas if d > 0.25 * max(peak, 1e-9)) / len(deltas) if deltas else 0.0
    asm = min(1.0, 4.0 * sit * mov)

    reps: list[tuple[list[float], int]] = []
    for c in curves:
        for i, (r, n) in enumerate(reps):
            if spectral_dist(c, r) < 3.0:
                reps[i] = (r, n + 1)
                break
        else:
            reps.append((c, 1))
    poses = sum(1 for _, n in reps if n / len(curves) > 0.02)

    return {
        "name": p["name"], "steps": p["steps"], "direction": p["direction"],
        "smooth": bool(p["smooth"]),
        "wheel_span": round(span, 4), "dwell_entropy": round(ent, 4),
        "travel": round(travel, 3), "poses": poses,
        "sit_frac": round(sit, 4), "move_frac": round(mov, 4), "asm": round(asm, 4),
    }

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--body", default=str(ROOT / "bodies" / "candidates" / "TRENCH_HEDZ.body240"))
    ap.add_argument("--cycles", type=int, default=0, help="0 = two full laps of the phrase")
    ap.add_argument("--blocks", type=int, default=21, help="blocks per step cycle")
    ap.add_argument("--bpm", type=float, default=120.0)
    ap.add_argument("--note-beats", type=float, default=0.5, help="1/8 at 4/4")
    ap.add_argument("--out", default=str(ROOT / "out" / "funcgen_measure.json"))
    args = ap.parse_args()

    sr = authoring_sr()
    body = Path(args.body).read_bytes()
    if len(body) != 240:
        raise SystemExit(f"{args.body} is {len(body)} bytes, expected 240")
    probe = Probe(body, sr)
    cycle_sec = args.note_beats / (args.bpm / 60.0)

    phrases = load_phrases()
    rows = []
    for p in phrases:
        cycles = args.cycles or max(16, p["steps"] * 2)
        r = measure(p, probe, cycles, args.blocks, cycle_sec)
        r["kept"] = p["name"] in SHIP
        r["ship_name"] = SHIP.get(p["name"], "")
        rows.append(r)

    rows.sort(key=lambda r: -r["asm"])
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({
        "body": Path(args.body).name, "sample_rate": sr, "bpm": args.bpm,
        "cycle_sec": cycle_sec, "phrases": rows}, indent=2), encoding="utf-8")

    kept = [r for r in rows if r["kept"]]
    kill = [r for r in rows if not r["kept"]]
    print(f"body {Path(args.body).name}   sr {sr}   {len(rows)} phrases "
          f"({len(kept)} kept / {len(kill)} killed)\n")
    hdr = f"{'':2} {'phrase':22} {'st':>3} {'dir':>3} {'sm':>3} {'span':>6} {'ent':>5} {'trav':>7} {'pose':>4} {'sit':>5} {'mov':>5} {'ASM':>5}"
    print(hdr)
    print("-" * len(hdr))
    for r in rows:
        mark = "**" if r["kept"] else "  "
        print(f"{mark} {r['name'][:22]:22} {r['steps']:3} {r['direction']:3} "
              f"{'Y' if r['smooth'] else 'n':>3} {r['wheel_span']:6.3f} "
              f"{r['dwell_entropy']:5.2f} {r['travel']:7.2f} {r['poses']:4} "
              f"{r['sit_frac']:5.2f} {r['move_frac']:5.2f} {r['asm']:5.2f}")

    print("\nseparation (median kept vs median killed, and kept's rank out of "
          f"{len(rows)}):")
    for key in ("wheel_span", "dwell_entropy", "travel", "poses", "sit_frac",
                "move_frac", "asm"):
        mk = statistics.median([r[key] for r in kept])
        mn = statistics.median([r[key] for r in kill])
        order = sorted(rows, key=lambda r: -r[key])
        ranks = sorted(order.index(r) + 1 for r in kept)
        print(f"  {key:14} kept {mk:8.3f}   killed {mn:8.3f}   "
              f"kept ranks {ranks}")
    print(f"\nwrote {out}")

if __name__ == "__main__":
    main()
