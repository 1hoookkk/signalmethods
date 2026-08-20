"""
Builds the unified in-memory corpus representation used by every later
phase, from RAW BYTES ONLY, using this tool's own independent decode_lib
(not the existing repository JSON files). phase0_reproduce.py is what
establishes that this independent decode agrees with the existing
repository's decoded JSON; once that gate passes, everything downstream
uses this module as its data source so the whole pipeline does not depend
on trusting any pre-existing decoded artifact.

Read-only against the repository: only reads
  ref/presets/P2k_*.bin
  ref/morpheus/records_stream.bin
"""
import glob
import os
import re

import decode_lib as dl

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
P2K_PRESETS_DIR = os.path.join(REPO_ROOT, "ref", "presets")
MORPHEUS_STREAM = os.path.join(REPO_ROOT, "ref", "morpheus", "records_stream.bin")


def _pair_to_dict(p):
    if isinstance(p, dl.Conjugate):
        return {"kind": "conjugate", "hz": p.hz, "r": p.r}
    if isinstance(p, dl.RealPair):
        return {"kind": "real", "a": p.root_a, "b": p.root_b}
    return {"kind": "degenerate"}


def load_p2k_objects(sample_rate_hz=39_062.5):
    """Returns list of objects: {corpus:'p2k', index, name, path, corners:[
        {label, stages:[{pole, zero, scale} x6]} x4]}"""
    objs = []
    paths = sorted(glob.glob(os.path.join(P2K_PRESETS_DIR, "P2k_*.bin")))
    for path in paths:
        base = os.path.basename(path)
        m = re.match(r"P2k_(\d+)_(.+)\.bin$", base)
        idx, name = m.group(1), m.group(2)
        with open(path, "rb") as f:
            data = f.read()
        corners_geo = dl.decode_p2k_body(data, sample_rate_hz)
        corners = []
        for ci, label in enumerate(dl.P2K_CORNER_LABELS):
            stages = []
            for si, g in enumerate(corners_geo[ci]):
                stages.append({
                    "stage_index": si,
                    "pole": _pair_to_dict(g.pole),
                    "zero": _pair_to_dict(g.zero),
                    "scale": g.scale,
                })
            corners.append({"label": label, "corner_index": ci, "stages": stages})
        objs.append({
            "corpus": "p2k", "index": int(idx), "name": name, "path": path,
            "sample_rate_hz": sample_rate_hz, "corners": corners,
        })
    return objs


R_MIN = 1e-6


def load_morpheus_objects(sample_rate_hz=39_062.5):
    """Returns list of objects: {corpus:'morpheus', index, name, corners:[
        {corner_index, gain, stages:[{pole, zero, scale=None} x7]} x8]}
    scale is left None per-section (Morpheus is corner/cascade-gain native,
    not per-section-gain native -- see amplitude-handling note in the
    findings report). Null stages (raw pole words == raw zero words, or
    r<=R_MIN for both pole and zero) are marked is_null=True."""
    records = dl.decode_all_morpheus_records_v2(MORPHEUS_STREAM)
    objs = []
    for idx, rec in enumerate(records):
        corners = []
        for ci, corner in enumerate(rec["corners"]):
            stages = []
            for si, sec in enumerate(corner["sections"]):
                pr = sec["pole"]["r"]
                zr = sec["zero"]["r"]
                raw = sec["raw"]
                is_null = (raw[0] == raw[2] and raw[1] == raw[3]) or (pr <= R_MIN and zr <= R_MIN)
                pole = {"kind": "conjugate", "hz": sec["pole"]["hz"], "r": pr} if pr > R_MIN else {"kind": "degenerate"}
                zero = {"kind": "conjugate", "hz": sec["zero"]["hz"], "r": zr} if zr > R_MIN else {"kind": "degenerate"}
                stages.append({
                    "stage_index": si, "pole": pole, "zero": zero,
                    "scale": None, "is_null": is_null, "raw": raw,
                })
            corners.append({
                "corner_index": ci, "gain": corner["gain"], "stages": stages,
            })
        objs.append({
            "corpus": "morpheus", "index": idx, "name": rec["name"],
            "sample_rate_hz": sample_rate_hz, "corners": corners,
        })
    return objs


if __name__ == "__main__":
    p2k = load_p2k_objects()
    morph = load_morpheus_objects()
    print(f"P2K objects: {len(p2k)} (expect 33)")
    print(f"Morpheus objects: {len(morph)} (expect 289)")
    print("P2K first object:", p2k[0]["name"], p2k[0]["corners"][0]["stages"][0])
    print("Morpheus first object:", morph[0]["name"], morph[0]["corners"][0]["gain"])
