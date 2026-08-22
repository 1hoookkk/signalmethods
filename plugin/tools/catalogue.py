"""catalogue — resolve a recipes/catalogue.json entry to a measurable source.

The catalogue is the browsable index: pick a name, get back something the
pipeline can measure. Three entry shapes resolve:

  frequencies_hz + bandwidths_hz   -> synthesized resonator-bank TF
  tf                               -> path to a .tf.json from batch_ingest
  wav                              -> path to a raw WAV

Usage:
  from tools.catalogue import load, find, entry_to_spec, list_entries
  cat = load()
  e = find(cat, "ah (father)")
  spec = entry_to_spec(e)          # -> "tf:...json" or "synth:..." or "path.wav"

CLI:
  python tools/catalogue.py list
  python tools/catalogue.py list --family vocal-formant
  python tools/catalogue.py show "ah (father)"
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
CATALOGUE = ROOT / "recipes" / "catalogue.json"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from batch_ingest import FREQS, FLOOR_DB, normalise  # noqa: E402

def load(path: Path | None = None) -> dict:
    with open(path or CATALOGUE) as fh:
        return json.load(fh)

def list_entries(cat: dict, family: str | None = None) -> list[dict]:
    src = cat.get("sources", [])
    if family:
        src = [e for e in src if e.get("family") == family]
    return src

def find(cat: dict, name: str) -> dict:
    sources = cat.get("sources", [])
    for e in sources:
        if e.get("name") == name or e.get("intent_spec") == name:
            return e
    lowered = name.lower()
    hits = [e for e in sources
            if lowered in e.get("name", "").lower()
            or lowered in (e.get("intent_spec") or "").lower()]
    if len(hits) == 1:
        return hits[0]
    if not hits:
        raise KeyError(f"no catalogue entry matching {name!r}")
    names = ", ".join(h["name"] for h in hits[:6])
    raise KeyError(f"{name!r} is ambiguous — matches: {names}")

def resonators_to_tf(freqs_hz, bandwidths_hz=None, levels_db=None,
                     default_q=8.0, topology="cascade") -> np.ndarray:
    freqs_hz = [float(f) for f in freqs_hz if f and f > 0]
    if not freqs_hz:
        raise ValueError("no usable frequencies")

    if bandwidths_hz:
        bws = [float(b) for b in bandwidths_hz]
        bws += [f / default_q for f in freqs_hz[len(bws):]]
    else:
        bws = [f / default_q for f in freqs_hz]

    if levels_db:
        lvls = [float(v) for v in levels_db]
        lvls += [0.0] * (len(freqs_hz) - len(lvls))
    else:
        lvls = [0.0] * len(freqs_hz)

    def section_mag(f0, bw):
        denom = np.maximum(FREQS * max(bw, 1.0), 1e-9)
        return 1.0 / np.sqrt(1.0 + ((FREQS ** 2 - f0 ** 2) / denom) ** 2)

    if topology == "parallel":
        mag = np.zeros_like(FREQS)
        for f0, bw, lvl in zip(freqs_hz, bws, lvls):
            mag += (10.0 ** (lvl / 20.0)) * section_mag(f0, bw)
        db = 20.0 * np.log10(np.maximum(mag, 1e-9))
    else:
        db = np.zeros_like(FREQS)
        for f0, bw, lvl in zip(freqs_hz, bws, lvls):
            db += 20.0 * np.log10(np.maximum(section_mag(f0, bw), 1e-9)) + lvl

    return normalise(db)

def cumulative_stages(freqs_hz, bandwidths_hz=None, levels_db=None,
                      default_q=8.0) -> list[np.ndarray]:
    freqs_hz = [float(f) for f in freqs_hz if f and f > 0]
    n = len(freqs_hz)
    if not n:
        raise ValueError("no usable frequencies")
    bw = list(bandwidths_hz) if bandwidths_hz else []
    lv = list(levels_db) if levels_db else []
    return [
        resonators_to_tf(freqs_hz[:i + 1], bw[:i + 1] or None,
                         lv[:i + 1] or None, default_q, topology="cascade")
        for i in range(n)
    ]

def entry_to_tf(entry: dict) -> dict:
    name = entry.get("name", "?")

    tf_path = entry.get("tf")
    if tf_path:
        p = Path(tf_path)
        if not p.is_absolute():
            p = ROOT / p
        with open(p) as fh:
            return json.load(fh)

    freqs = entry.get("frequencies_hz")
    if freqs:
        db = resonators_to_tf(freqs, entry.get("bandwidths_hz"),
                              entry.get("levels_db"))
        return {
            "source": f"catalogue:{name}",
            "kind": "synth",
            "freqs_hz": FREQS.tolist(),
            "mag_db": [round(float(v), 3) for v in db],
            "summary": {
                "peak_db": round(float(np.max(db)), 1),
                "peak_hz": round(float(FREQS[int(np.argmax(db))]), 0),
                "range_db": round(float(np.max(db) - np.min(db)), 1),
            },
        }

    raise ValueError(
        f"entry {name!r} has no tf, wav, or frequencies_hz — "
        "add one before using it as a morph endpoint")

def entry_to_spec(entry: dict, tf_dir: Path | None = None) -> str:
    wav = entry.get("wav")
    if wav:
        p = Path(wav)
        return str(p if p.is_absolute() else ROOT / p)

    tf_path = entry.get("tf")
    if tf_path:
        p = Path(tf_path)
        return f"tf:{p if p.is_absolute() else ROOT / p}"

    out_dir = tf_dir or (ROOT / "recipes" / "tfs" / "_catalogue")
    out_dir.mkdir(parents=True, exist_ok=True)
    slug = (entry.get("intent_spec") or entry.get("name", "entry"))
    slug = "".join(c if c.isalnum() or c in "-_" else "_" for c in slug)
    out = out_dir / f"{slug}.tf.json"
    with open(out, "w") as fh:
        json.dump(entry_to_tf(entry), fh)
    return f"tf:{out}"

def main():
    argv = sys.argv[1:]
    if not argv:
        print(__doc__)
        return

    cat = load()
    cmd = argv[0]

    if cmd == "list":
        family = None
        if "--family" in argv:
            i = argv.index("--family")
            if i + 1 < len(argv):
                family = argv[i + 1]
        entries = list_entries(cat, family)
        fam_width = max((len(e.get("family", "")) for e in entries), default=10)
        for e in entries:
            freqs = e.get("frequencies_hz")
            detail = ""
            if freqs:
                detail = " ".join(f"{f:g}" for f in freqs[:4])
                if len(freqs) > 4:
                    detail += " …"
            elif e.get("tf"):
                detail = "tf"
            elif e.get("wav"):
                detail = "wav"
            elif e.get("morph_pair"):
                detail = " → ".join(e["morph_pair"])
            print(f"  {e.get('family',''):{fam_width}s}  {e.get('name',''):38s} {detail}")
        print(f"\n{len(entries)} entries" + (f" in {family}" if family else ""))
        return

    if cmd == "show":
        if len(argv) < 2:
            print("usage: catalogue.py show <name>")
            sys.exit(1)
        e = find(cat, argv[1])
        print(json.dumps(e, indent=2))
        try:
            tf = entry_to_tf(e)
            s = tf.get("summary", {})
            print(f"\nresolves to: {tf['kind']}  "
                  f"peak {s.get('peak_db')} dB @ {s.get('peak_hz')} Hz  "
                  f"range {s.get('range_db')} dB")
        except ValueError as err:
            print(f"\nnot yet measurable: {err}")

        freqs = e.get("frequencies_hz")
        if freqs:
            print("\nsignal so far (serial cascade — dB add):")
            stages = cumulative_stages(freqs, e.get("bandwidths_hz"),
                                       e.get("levels_db"))
            for i, db in enumerate(stages):
                pk = FREQS[int(np.argmax(db))]
                print(f"  S1..S{i+1}  added {freqs[i]:>6.0f} Hz   "
                      f"peak {np.max(db):+6.1f} dB @ {pk:>6.0f} Hz   "
                      f"range {np.max(db) - np.min(db):5.1f} dB")
        return

    print(f"unknown command {cmd!r} (use: list, show)")
    sys.exit(1)

if __name__ == "__main__":
    main()
