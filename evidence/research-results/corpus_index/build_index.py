import glob
import json
import math
import os
import struct

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
P2K = os.path.join(ROOT, "plugin", "presets", "p2k")
MORPHEUS = os.path.join(ROOT, "evidence", "factory-data", "morpheus", "decoded")
OUT = os.path.dirname(os.path.abspath(__file__))
P2K_DATUM = 44100.0
BINS = 128
LOW, HIGH = 40.0, 5000.0
GRID = LOW * (HIGH / LOW) ** (np.arange(BINS) / (BINS - 1))
SECTIONS = 7
FLOOR_DB = 30.0
SCALE = [2.0 ** -(15 - e) for e in range(16)]


def decode_word(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    exponent = (u >> 12) & 0xF
    mantissa = float(u & 0xFFF)
    x = mantissa / 4096.0 if exponent == 0 else (mantissa + 4096.0) / 8192.0
    return x * SCALE[exponent]


def pole_only_descriptor(words, datum):
    z = np.exp(-2j * np.pi * GRID / datum)
    db = np.zeros(BINS)
    for row in words:
        d = [decode_word(w) for w in row]
        a1 = 4.0 * d[2] + d[3] - 2.0
        a2 = 1.0 - d[3]
        den = 1.0 + a1 * z + a2 * z * z
        db += -20.0 * np.log10(np.maximum(np.abs(den), 1e-9))
    db = np.maximum(db, db.max() - FLOOR_DB)
    return (db - db.mean()).astype(np.float32)


def p2k_nodes():
    corners = ["M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1"]
    nodes = []
    for path in sorted(glob.glob(os.path.join(P2K, "*.body240"))):
        raw = open(path, "rb").read()
        assert len(raw) == 240
        w = struct.unpack("<120H", raw)
        stem = os.path.basename(path)[:-8]
        title = " ".join(t[:1].upper() + t[1:] for t in stem.split("_"))
        for c in range(4):
            words = [list(w[(c * 6 + s) * 5:(c * 6 + s) * 5 + 5]) for s in range(6)]
            words.append([0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF])
            nodes.append({"name": f"{title} {corners[c]}", "source": "p2k", "family": title, "datum": P2K_DATUM, "words": words, "file": os.path.relpath(path, ROOT), "corner": c})
    return nodes


def morpheus_nodes():
    index = json.load(open(os.path.join(MORPHEUS, "index.json")))
    nodes = []
    for f in index["filters"]:
        cube = json.load(open(os.path.join(MORPHEUS, f["path"])))
        datum = float(cube["datum_hz"])
        for corner in cube["corner_data"]:
            rows = [list(int(v) for v in section["words"]) for section in corner["sections"]]
            assert len(rows) == SECTIONS and all(len(r) == 5 for r in rows), (f["path"], len(rows))
            nodes.append({"name": f"{f['manual_name']} c{corner['corner']}", "source": "morpheus", "family": f["family"], "datum": datum, "words": rows, "file": os.path.relpath(os.path.join(MORPHEUS, f["path"]), ROOT), "corner": corner["corner"]})
    return nodes


def main():
    nodes = p2k_nodes() + morpheus_nodes()
    with open(os.path.join(OUT, "corpus_index.bin"), "wb") as out:
        out.write(b"TRCI")
        out.write(struct.pack("<I", len(nodes)))
        for n in nodes:
            name = n["name"].encode("utf-8")
            out.write(struct.pack("<H", len(name)))
            out.write(name)
            out.write(struct.pack("<B", 0 if n["source"] == "p2k" else 1))
            out.write(struct.pack("<d", n["datum"]))
            for row in n["words"]:
                out.write(struct.pack("<5H", *row))
            desc = pole_only_descriptor(n["words"], n["datum"])
            out.write(desc.tobytes())
    manifest = {"schema": "trench-corpus-index-v1", "bins": BINS, "low_hz": LOW, "high_hz": HIGH, "sections": SECTIONS, "count": len(nodes),
                "nodes": [{"id": i, "name": n["name"], "source": n["source"], "family": n["family"], "datum": n["datum"], "file": n["file"], "corner": n["corner"], "words_hex": [" ".join(f"{w:04x}" for w in row) for row in n["words"]]} for i, n in enumerate(nodes)]}
    json.dump(manifest, open(os.path.join(OUT, "corpus_index.json"), "w"), indent=1)
    p2k = sum(1 for n in nodes if n["source"] == "p2k")
    print(f"{len(nodes)} nodes: {p2k} p2k, {len(nodes) - p2k} morpheus; descriptor {BINS} log bins {LOW}-{HIGH} Hz, pole-only, floored {FLOOR_DB} dB under the peak, mean removed; words kept verbatim")


if __name__ == "__main__":
    main()
