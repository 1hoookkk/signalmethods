from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import runtime_probe as rp  # noqa: E402

FORMAT = "trench-cascade-trace-v1"

def default_states():
    states = [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)]
    states += [(round(float(m), 3), 0.0) for m in np.linspace(0, 1, 11)]
    states += [(0.5, round(float(q), 3)) for q in np.linspace(0, 1, 11)]
    seen, out = set(), []
    for s in states:
        if s not in seen:
            seen.add(s)
            out.append(s)
    return out

def main() -> None:
    args = [a for a in sys.argv[1:]]
    npz = "--npz" in args
    if npz:
        args.remove("--npz")
    states_arg = None
    if "--states" in args:
        i = args.index("--states")
        states_arg = args[i + 1]
        del args[i:i + 2]
    body_path = Path(args[0]).resolve()
    rate = float(args[1]) if len(args) > 1 else 48_000.0

    body = rp.load_body(body_path)
    rp.verify_word_law(body)
    states = ([tuple(float(x) for x in s.split(",")) for s in states_arg.split(";")]
              if states_arg else default_states())

    doc = {
        "format": FORMAT,
        "body_sha256": rp.body_sha256(body),
        "body_file": body_path.name,
        "sample_rate_hz": rate,
        "lane_law": "lanes are fixed slot numbers 1..6 - never frequency-sorted",
        "interpolation_law": "packed u16 words lerped in encoded space "
                             "(minifloat.rs lerp_u16, morph edges then q), "
                             "then decoded; verified against the DLL kernel",
        "frequency_hz": [float(f) for f in rp.GRID],
        "states": [],
    }
    S = len(states)
    F = len(rp.GRID)
    arr = {
        "states": np.zeros((S, 2)),
        "frequency_hz": np.asarray(rp.GRID, dtype=np.float64),
        "packed_words": np.zeros((S, 6, 5), dtype=np.uint16),
        "biquad": np.zeros((S, 6, 5)),
        "stage_db": np.zeros((S, 6, F)),
        "cumulative_db": np.zeros((S, 6, F)),
        "stage_complex": np.zeros((S, 6, F, 2)),
        "cumulative_complex": np.zeros((S, 6, F, 2)),
    }
    for si, (m, q) in enumerate(states):
        st = rp.probe_state(body, m, q, rate)
        if st is None:
            print(f"  state M{m} Q{q}: unstable/nonfinite - skipped")
            continue
        arr["states"][si] = (m, q)
        jstate = {"morph": m, "q": q, "sections": []}
        for s in st["sections"]:
            k = s["lane"] - 1
            arr["packed_words"][si, k] = s["packed_words"]
            arr["biquad"][si, k] = s["biquad"]
            H, A = s["stage_complex"], s["after_complex"]
            arr["stage_db"][si, k] = rp.to_db(H)
            arr["cumulative_db"][si, k] = rp.to_db(A)
            arr["stage_complex"][si, k, :, 0] = H.real
            arr["stage_complex"][si, k, :, 1] = H.imag
            arr["cumulative_complex"][si, k, :, 0] = A.real
            arr["cumulative_complex"][si, k, :, 1] = A.imag
            jstate["sections"].append({
                "lane": s["lane"],
                "packed_words": s["packed_words"],
                "biquad": s["biquad"],
                "geometry": s["geometry"],
                "stage_db": [round(float(v), 4) for v in rp.to_db(H)],
                "before_db": [round(float(v), 4) for v in rp.to_db(s["before_complex"])],
                "after_db": [round(float(v), 4) for v in rp.to_db(A)],
            })
        doc["states"].append(jstate)

    out_json = body_path.with_name(body_path.stem + "_trace.json")
    out_json.write_text(json.dumps(doc), encoding="utf-8")
    print(f"trace: {out_json}  ({len(doc['states'])} states, {F} freq points)")
    if npz:
        out_npz = body_path.with_name(body_path.stem + "_trace.npz")
        np.savez_compressed(out_npz, **arr)
        print(f"dense: {out_npz}")

if __name__ == "__main__":
    main()
