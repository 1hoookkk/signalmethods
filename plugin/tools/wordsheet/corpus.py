from __future__ import annotations

import numpy as np

from .core import ROOT


def factory_bodies() -> list[tuple[str, bytes]]:
    out = []
    for p in sorted((ROOT / "ref" / "presets").glob("P2k_*.bin")):
        b = p.read_bytes()
        if len(b) == 240:
            out.append((p.stem, b))
    return out


def blend(a: bytes, b: bytes, t: float) -> bytes:
    wa = np.frombuffer(a, dtype="<u2").astype(float)
    wb = np.frombuffer(b, dtype="<u2").astype(float)
    return np.clip(np.round((1.0 - t) * wa + t * wb),
                   0, 65535).astype("<u2").tobytes()


def by_distance(source: bytes,
                bodies: list[tuple[str, bytes]]) -> list[tuple[str, bytes]]:
    w = np.array([np.frombuffer(b, dtype="<u2").astype(float)
                  for _, b in bodies])
    sd = w.std(0) + 1e-9
    s = (np.frombuffer(source, dtype="<u2").astype(float) - w.mean(0)) / sd
    z = (w - w.mean(0)) / sd
    order = np.argsort(np.linalg.norm(z - s, axis=1))
    return [bodies[i] for i in order]

