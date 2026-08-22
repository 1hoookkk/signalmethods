#!/usr/bin/env python3
"""seat.py — marry frames to a measured architecture: the third ingredient.

    python tools/seat.py --m0 <frame> --m100 <frame> \
        --architecture recipes/architectures/P2k_022_DeepBouche.json \
        --name my_body --out bodies/my_body [--answers REF.body240]

A frame is our measured material: `table.json#object_id` (academia corpora)
or a frame-atom `.body240` (its M0/Q0 corner is probed through the packed
path and the voiced sections read back as material).

The architecture supplies everything that is not the voice, all of it
measured (tools/extract_architectures.py):
  - seats        our voices fill the architecture's voiced seats by
                 frequency rank (a priori, before any filter math);
                 seats left over keep the architecture's own pole/zero
                 verbatim — the organs (DeepBouche's chest, its air)
  - carves       each seated voice's zero sits at the seat's measured
                 carve_st from OUR pole (sign is direction: negative digs
                 the low side), zero radius from the seat
  - radii        the seat's measured pole radius per corner (Q rails
                 included) — the voice brings the position, the seat owns
                 the attitude; q_revoice moves the pole by the seat's
                 measured semitones; organs use their own geometry verbatim
  - seal         unit-zero flags ride with the seat geometry unchanged

Journey is rank-parallel (our M0 k-th voice travels to our M100 k-th);
re-pair afterwards in the word space (tools/word_author.py). The whole
cascade is compiled, DC-normalized and certified by the one pipeline, and
--answers runs the referee. Nothing local, nothing invented.
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from batch_compiler import (compile_body, load_table, referee,  # noqa: E402
                            split_ref, voice_radius)

CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]

def frame_voices(ref: str, sr: float) -> list[tuple[float, float]]:
    p, _ = split_ref(ref)
    if p.suffix.lower() == ".body240":
        from cascade_ladder import stages
        sec = stages(Path(ref).read_bytes(), 0.0, 0.0, sr)
        vs = [(s["pole_hz"], s["pole_r"]) for s in sec
              if s["pole_hz"] > 0 and s["pole_r"] > 0]
    else:
        path, oid = split_ref(ref)
        rows, _tsr = load_table(path, oid)
        vs = [(r["freq_hz"], voice_radius(r["bw_hz"], sr)) for r in rows]
    return sorted(vs, key=lambda v: -v[0])

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--m0", required=True)
    ap.add_argument("--m100", required=True)
    ap.add_argument("--architecture", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--answers", metavar="REF.body240")
    ap.add_argument("--rate", type=float, default=48_000.0)
    args = ap.parse_args()

    arch = json.loads(Path(args.architecture).read_text())
    sr = args.rate
    v_m0 = frame_voices(args.m0, sr)
    v_m100 = frame_voices(args.m100, sr)
    k = min(len(v_m0), len(v_m100))
    v_m0, v_m100 = v_m0[:k], v_m100[:k]

    order = [2, 3, 4, 5, 1, 6]
    seats = sorted((s for s in arch["sections"]
                    if s["slot"] in order[:k]
                    and s["pole_hz"]["M0_Q0"] > 0 and s["pole_r"]["M0_Q0"] > 0),
                   key=lambda s: -s["pole_hz"]["M0_Q0"])
    seated = {s["slot"]: rank for rank, s in enumerate(seats)}

    per_corner = []
    for cn in CORNERS:
        voices = v_m0 if cn.startswith("M0") else v_m100
        q100 = cn.endswith("Q100")
        lanes = []
        for sec in sorted(arch["sections"], key=lambda s: s["slot"]):
            slot = sec["slot"]
            if slot in seated:
                hz = voices[seated[slot]][0]
                r = sec["pole_r"][cn]
                if q100:
                    rv = sec["q_revoice_st_M0"]
                    if rv:
                        hz = hz * 2 ** (rv / 12.0)
                q0_twin = cn.split("_")[0] + "_Q0"
                carve = (sec["carve_st"][cn]
                         if sec["carve_st"][cn] is not None
                         else sec["carve_st"][q0_twin])
                zhz = hz * 2 ** (carve / 12.0) if carve is not None else 0.0
                zr = (sec["zero_r"][cn] if sec["zero_hz"][cn] > 0
                      else sec["zero_r"][q0_twin])
                role = "our voice in its seat"
            else:
                q0_twin = cn.split("_")[0] + "_Q0"
                hz, r = sec["pole_hz"][cn], sec["pole_r"][cn]
                zhz, zr = sec["zero_hz"][cn], sec["zero_r"][cn]
                if zhz <= 0:
                    zhz = sec["zero_hz"][q0_twin]
                    zr = sec["zero_r"][q0_twin]
                role = "architecture organ"
            lanes.append(dict(slot=slot, role=role, pole_hz=hz, pole_r=r,
                              zero_hz=zhz, zero_r=zr, zero_note="",
                              scale=1.0))
        per_corner.append(lanes)

    body, info = compile_body(per_corner, sr)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    body_path = out / f"{args.name}.body240"
    body_path.write_bytes(body)
    print(f"  body       {body_path}   certify {info['certify']} "
          f"max_r {info['max_r']:.6f}")
    for cn, lanes in zip(CORNERS, per_corner):
        print(cn)
        for ln in lanes:
            print(f"  S{ln['slot']}  pole {ln['pole_hz']:8.1f} "
                  f"r{ln['pole_r']:.3f}  zero {ln['zero_hz']:8.1f} "
                  f"r{ln['zero_r']:.3f}  {ln['role']}")
    if args.answers:
        referee(body, args.answers, sr)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
