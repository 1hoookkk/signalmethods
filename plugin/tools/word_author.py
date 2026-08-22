#!/usr/bin/env python3
"""word_author.py — author bodies IN THE WORD SPACE (Tyson law 2026-08-07).

The .body240 words are the atoms: 4 corners x 6 slots x 5 LE u16. The runtime
interpolates linearly in this encoded space, so authoring happens here —
no decode, no re-encode, no fitter. Two operations only:

  frame     one corner of an existing body, duplicated across all 4 corners
            -> a single-frame body240. Every recipe becomes a mixable atom.
                word_author.py frame SRC.body240 M0_Q0 --out FRAME.body240

  journey   re-pair the morph: permute which M100 slot-row each M0 slot-row
            travels to. Applied to both Q rails (M100_Q0 and M100_Q100).
            The frames do not change; only the journey does (the OTE/ETA law:
            same shapes, different journey = different filter).
                word_author.py journey SRC.body240 --swap S3=S4 --swap S4=S5 \
                    --swap S5=S3 --out NEW.body240
            --swap Sa=Sb : the row that sat in slot b at M100 now sits in
            slot a (so M0's slot-a voice travels to it). Unlisted slots keep
            their own M100 rows.
"""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path

CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]

FALLBACK_SEAT_MAP = {2: 5, 3: 4, 4: 3, 5: 2}

def derive_seats(doss: dict, table_path: str, atom_stem: str) -> dict[int, int]:
    import math
    tbl = json.loads(Path(table_path).read_text())
    obj = next(o for o in tbl["objects"] if o.get("object_id") == atom_stem)
    forms = sorted(f["frequency_hz"] for f in obj["formants"]
                   if f.get("frequency_hz"))
    voices = [(hz, 2 + i) for i, hz in enumerate(forms[:4])]
    travels = []
    for i in range(6):
        p0 = doss["corners"]["M0_Q0"][i]["pole"]["hz"]
        p1 = doss["corners"]["M100_Q0"][i]["pole"]["hz"]
        if p0 > 0 and p1 > 0:
            travels.append((abs(12 * math.log2(p1 / p0)), p0, i + 1))
    movers = sorted(travels, reverse=True)[:len(voices)]
    seats = sorted((p0, slot) for _t, p0, slot in movers)
    return {slot: aslot
            for (_shz, slot), (_vhz, aslot) in zip(seats, voices)}

def load(path: str) -> list[list[list[int]]]:
    raw = Path(path).read_bytes()
    if len(raw) != 240:
        raise SystemExit(f"{path}: {len(raw)} bytes, want 240")
    w = struct.unpack("<120H", raw)
    return [[list(w[(c * 6 + s) * 5:(c * 6 + s) * 5 + 5])
             for s in range(6)] for c in range(4)]

def save(body: list[list[list[int]]], path: str) -> None:
    flat = [x for c in body for s in c for x in s]
    Path(path).write_bytes(struct.pack("<120H", *flat))
    print(f"wrote {path}")

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("op", choices=["frame", "journey", "seat"])
    ap.add_argument("src")
    ap.add_argument("corner", nargs="?", help="frame: which corner to take; "
                    "seat: the M100 atom body240")
    ap.add_argument("--swap", action="append", default=[],
                    metavar="Sa=Sb", help="journey: M100 row of slot b seats "
                    "in slot a (repeatable)")
    ap.add_argument("--dossier", help="seat: the character dossier whose "
                    "words are the rack and the organs")
    ap.add_argument("--table", help="seat: the atoms' source table — enables "
                    "per-character seat derivation (correct sections per "
                    "execution); omitted = legacy fixed interior map")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    if args.op == "seat":
        if not (args.corner and args.dossier):
            raise SystemExit("seat needs: M0_ATOM M100_ATOM --dossier D")
        atom0, atom1 = load(args.src), load(args.corner)
        doss = json.loads(Path(args.dossier).read_text())
        seat_map = (derive_seats(doss, args.table, Path(args.src).stem)
                    if args.table else FALLBACK_SEAT_MAP)
        if len(seat_map) < 2:
            raise SystemExit(
                f"SEAT REFUSED: only {len(seat_map)} voice(s) land within "
                "the guard — the output would be a clone of the reference, "
                "not a body (a body is frames + pairing)")
        body = []
        for ci, cn in enumerate(CORNERS):
            atom = atom0 if cn.startswith("M0") else atom1
            arow = atom[0 if cn.endswith("Q0") else 2]
            drow = [lane["words"] for lane in doss["corners"][cn]]
            rows = []
            for slot in range(1, 7):
                d = list(drow[slot - 1])
                if slot in seat_map:
                    a = arow[seat_map[slot] - 1]
                    d[2], d[3] = a[2], a[3]
                rows.append(d)
            body.append(rows)
        save(body, args.out)
        return 0

    body = load(args.src)
    if args.op == "frame":
        if args.corner not in CORNERS:
            raise SystemExit(f"corner must be one of {CORNERS}")
        f = body[CORNERS.index(args.corner)]
        save([[list(r) for r in f] for _ in range(4)], args.out)
        return 0

    seat = {i: i for i in range(6)}
    for spec in args.swap:
        a, b = spec.upper().split("=")
        seat[int(a[1:]) - 1] = int(b[1:]) - 1
    for ci in (1, 3):
        old = [list(r) for r in body[ci]]
        for dst, src_ in seat.items():
            body[ci][dst] = old[src_]
    save(body, args.out)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
