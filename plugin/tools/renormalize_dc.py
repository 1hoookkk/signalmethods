from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

import hardware_law as hl
import runtime_probe as rp
from runtime_probe import lib

SCALE_WI = 4
MAX_ROUNDS = 8

def _decode_table() -> tuple[np.ndarray, np.ndarray]:
    vals = np.array([lib.trench_packed_decode(w) for w in range(0x10000)])
    order = np.argsort(vals)
    return vals[order], np.arange(0x10000, dtype=np.uint16)[order]

_VALS, _WORDS = _decode_table()

def encode_nearest(value: float) -> int:
    i = int(np.searchsorted(_VALS, value))
    best, err = 0, float("inf")
    for j in (i - 1, i, i + 1):
        if 0 <= j < len(_VALS) and abs(_VALS[j] - value) < err:
            best, err = int(_WORDS[j]), abs(_VALS[j] - value)
    return best

def renormalize(body: bytes, rate: float) -> tuple[bytes, list]:
    words = rp.corner_words(body)
    touched = []
    for c, (m, q) in enumerate(hl.CORNERS):
        before = hl.dc_gain_db(body, m, q, rate)
        if before is None or before <= hl.DC_UNITY_TOL_DB:
            continue
        for _ in range(MAX_ROUNDS):
            dc = hl.dc_gain_db(bytes(words.tobytes()), m, q, rate)
            if dc is None or dc <= 0.0:
                break
            factor = 10.0 ** (-dc / (20.0 * rp.NUM_STAGES))
            moved = False
            for s in range(rp.NUM_STAGES):
                old = lib.trench_packed_decode(int(words[c, s, SCALE_WI]))
                new = encode_nearest(old * factor)
                if new != words[c, s, SCALE_WI]:
                    words[c, s, SCALE_WI] = new
                    moved = True
            if not moved:
                factor2 = 10.0 ** (-dc / 20.0)
                s = int(np.argmax([abs(lib.trench_packed_decode(
                    int(words[c, s, SCALE_WI]))) for s in range(rp.NUM_STAGES)]))
                words[c, s, SCALE_WI] = encode_nearest(
                    lib.trench_packed_decode(int(words[c, s, SCALE_WI])) * factor2)
        body2 = words.tobytes()
        touched.append((c, before, hl.dc_gain_db(body2, m, q, rate)))
    return words.tobytes(), touched

def main(paths: list[str], rate: float = 48000.0) -> None:
    for p in paths:
        path = Path(p)
        body = rp.load_body(path)
        new_body, touched = renormalize(body, rate)
        if not touched:
            print(f"clean  {path.name}")
            continue
        rp.verify_word_law(new_body)
        path.write_bytes(new_body)
        detail = ", ".join(f"C{c} {b:+.3f}->{a:+.3f}" for c, b, a in touched)
        print(f"trim   {path.name}: {detail}")

if __name__ == "__main__":
    main(sys.argv[1:])
