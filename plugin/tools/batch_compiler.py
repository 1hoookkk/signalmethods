#!/usr/bin/env python3
"""batch_compiler.py — two morph endpoints -> P2K lane grammar blueprint -> body.

THE FLOW (X to Y — two objects, low morph and high morph):

  m0_peaks.json  m100_peaks.json      # the two MORPH endpoints, as
                                      # frequency tables (freq + bandwidth)
       │  1 INGEST  load the two endpoint frequency tables.
       │            A WAV may be passed instead: it is reduced to a
       │            frequency table FIRST via find_peaks on the FFT
       │            (extract fundamentals; never ARMA-fit the wav).
       ▼
       │  2 TRACK   pre-fit Hungarian on the raw peaks (the one permitted
       │            use): cost = semitone interval, anchored at corner 0;
       │            Track n -> S(n+1) decided here, never later
       ▼
       │  3 GRAMMAR the P2K lane grammar (measured from the ROM dossiers):
       │            S1      CONDITIONING when seated (air pole, cross-
       │                    register zero); passthrough sentinel when not
       │            S2-S5   TALKERS     — the four extracted fundamentals,
       │                    close pole/zero pairs doing the talking
       │            S6      CONDITIONING + the terminal unit zero (the one
       │                    universal ROM law, 132/132 corners); the zero's
       │                    frequency is authorable, its radius is not
       │            voice zero a few st above its pole, serial clearance
       ▼
       │  4 Q CORNERS  the Q axis is NEVER measured. Q100 corners follow
       │            the documented E-MU Q behavior (tools/q_attitudes.py +
       │            recipes/tables/p2k_filter_q_behavior.json): arm/defuse
       │            radius deltas, or a revoice, per lane. Default Q100 =
       │            Q0 (Morph Designer has no Q axis); pass --q-attitude to
       │            author a real Q corner.
       ▼
       │  5 BLUEPRINT <name>_blueprint.json + <name>_corners.txt
       │            the proposed (f, r) coordinates for all 4 corners.
       │            THIS is the hold point — select/edit the corners in the
       │            workstation before anything packs.
       ▼
       │  6 COMPILE  roots -> trench_stage_words_from_roots_at (native rate)
       │            -> trench_pack_body_from_corner_words
       │            -> SCALE: closed analytical unity-DC normalization
       │            -> trench_certify_body 33x33
       │            -> <name>.body240

Ground truth: the P2K lane grammar comes from the decoded ROM dossiers
(dossiers/characters/P2k_*.json). A body is two whole endpoint shapes plus
a per-slot pairing, and slot roles are per-character; the only universal
slot law is the S6 terminal unit zero (132/132 measured). The compiler authors
nothing it did not measure: every pole comes from the two endpoint frames.
S1/S6 are the CONDITIONING lanes — seatable voices whose pole and zero may
live registers apart (TalkingHedz: S1 air pole + low-end carve, S6 throat
pole + the unit zero mid-band); left unseated they fall back to the
passthrough sentinel and the parked terminator. S2–S5 carry OUR measured
fundamentals, so the body is clean-room by construction.

Table format (two objects in, X to Y):
  {"rate_hz": 44100, "peaks": [{"freq_hz": .., "bw_hz": .., "level_db": ..}, ...]}

Usage:
  python tools/batch_compiler.py m0.json m100.json [--q-attitude SPEC] [--blueprint-only]
  python tools/batch_compiler.py m0.wav m100.wav      # extract fundamentals, then same path
  python tools/batch_compiler.py <folder>             # folder with exactly 2 sources
  python tools/batch_compiler.py tbl.json#obj_a tbl.json#obj_b   # pick two
                                      # objects out of one academia table
Options:
  --lane-order ascending|descending  talker lane roles, fixed a priori
                       (descending = the DeepBouche species signature)
  --reserve S4=T2      reserved seat: pin measured track T2 to slot S4,
                       repeatable; free tracks seat around reservations
  --q-attitude SPEC    Q-corner law: flat|dead_q|uniform-gentle:0.07|
                       single-bloomer:6:0.36|asymmetric-relay:1,6:2:0.22:0.66|
                       all-negative:-0.04|revoice:3:0.33:2400  (default flat)
  --blueprint-only      stop after the blueprint (hold point, no body pack)
  --out DIR            write outputs here instead of beside the sources
  --name NAME          override the body name (default: source stem)
"""
from __future__ import annotations

import argparse
import ctypes
import json
import math
import re
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))

from arma_measure_lib import (dc_anchor_body, lib, load_wav,              # noqa: E402
                              averaged_spectrum_db)
sys.path.insert(0, str(ROOT / "tools"))

from extractor import (LOG_POINTS, MIN_SEP_OCT,                            # noqa: E402
                       ZERO_PARK_HZ, ZERO_SEARCH_SPAN_ST, ZERO_ST_ABOVE_POLE,
                       MIN_ZERO_CLEARANCE_ST, extract, track_formants)
from batch_ingest import (FREQS as TF_FREQS, _detect_f0 as detect_f0,      # noqa: E402
                          tonal_to_tf)

CORNER_ORDER = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
BAND = (60.0, 16_000.0)             # extraction band
# six seats since S1/S6 became conditioning lanes: up to six measured rows
# ride (WAV extraction still yields up to extractor.N_FORMANTS fundamentals;
# tables may carry more)
NVOICE = 6

# ── the carve law ───────────────────────────────────────────────────────────
# The grammar's one derived statistic: how a paying zero carves for its
# pole. Lane content never comes from corpus medians — slot roles are
# per-character (measured, dossiers) — only this carving relation does.

_CARVE: tuple[float, float] | None = None


def carve_law() -> tuple[float, float]:
    """THE VOICE ZERO, corpus-wide: (interval_st, zero_r).

    Median over every dossier lane whose zero actually carves (pole and
    zero above 0 Hz; unit zeros excluded — those are terminators, not
    carvers). A paying zero is BROADER than its pole and sits a few
    semitones above it; in a serial cascade a zero as narrow as its pole
    shaves a hairline and pays for nothing. Derived statistic over all 33
    dossiers x 4 corners, never a copied row.
    """
    global _CARVE
    if _CARVE is None:
        voice_st, voice_zr = [], []
        for f in sorted((ROOT / "dossiers" / "characters").glob("P2k_*.json")):
            d = json.loads(f.read_text())
            for cn in CORNER_ORDER:
                for r in d["corners"][cn]:
                    ph, zh = r["pole"]["hz"], r["zero"]["hz"]
                    if ph > 0 and zh > 0 and r["zero"]["radius"] < 0.9999:
                        voice_st.append(12 * math.log2(zh / ph))
                        voice_zr.append(r["zero"]["radius"])
        _CARVE = (float(np.median(voice_st)), float(np.median(voice_zr)))
    return _CARVE


# ── the family structure (design/archetype_sos.json) ───────────────────────
# Per-family SOS structure measured across the P2K references: which
# per-slot features the corpus actually agrees on (locks at >= 75% over
# >= 3 references; everything else is free and says so). Regenerated by
# tools/measure_archetype_structures.py with THESE classifiers — one
# definition, or the measurement and the bench's judgment drift apart.

ARCHETYPES = ROOT / "design" / "archetype_sos.json"
_ARCH: dict | None = None


def archetype_structure(family: str) -> dict | None:
    """One family's measured slot structure (locked + free per slot, or a
    verdict when the corpus is too thin), None for an unknown family."""
    global _ARCH
    if _ARCH is None:
        _ARCH = json.loads(ARCHETYPES.read_text())["families"]
    return _ARCH.get(family)


def travel_class(a: float, b: float) -> str:
    if not (a > 20 and b > 20):
        return "silent end"
    st = abs(12 * math.log2(b / a))
    return "holds" if st <= 5 else ("slides" if st <= 24 else "leaps")


def register(hz: float) -> str:
    return "low" if hz < 400 else ("mid" if hz <= 3500 else "air")


def q_sign(d: float) -> str:
    return "arms" if d > 0.02 else ("defuses" if d < -0.02 else "holds")


def sos_features(per_corner: list[list[dict]], slot: int) -> dict:
    """One planned body's S<slot>, read through the same classifiers the
    family structure was measured with. The zero reading covers only the
    silence line — the one zero relationship that locks anywhere in the
    corpus (S6, 100%, all four generalising families); the carving buckets
    (workstation/shapes) are per-character voicing and never lock."""
    def lane(c: int) -> dict:
        return next(ln for ln in per_corner[c] if ln["slot"] == slot)
    m0, m100, m0q = lane(0), lane(1), lane(2)
    live0 = m0["pole_hz"] > 20 and m0["pole_r"] >= 0.5
    live1 = m100["pole_hz"] > 20 and m100["pole_r"] >= 0.5
    feats = dict(voiced="voiced" if (live0 or live1) else "frame",
                 travel=travel_class(m0["pole_hz"], m100["pole_hz"]),
                 q=q_sign(m0q["pole_r"] - m0["pole_r"]))
    if live0:
        feats["register"] = register(m0["pole_hz"])
    if m0["zero_hz"] > 20 and m0["zero_r"] >= 0.9999:
        feats["zero"] = "silence line"
    return feats


def wavs_in(folder: Path) -> list[Path]:
    return sorted(list(folder.glob("*.wav")) + list(folder.glob("*.WAV")))


# ── ingest: endpoint objects in ─────────────────────────────────────────────

def table_from_wav(path: Path) -> tuple[dict, float]:
    """Reduce a WAV to a frequency table via find_peaks (never a fit).

    PITCHED MATERIAL TAKES THE HARMONIC-ENVELOPE LANE. A played note's
    spectrum is a comb of its own partials; peak-picking it measures the
    NOTE, not the filter the note was played through — a 303 loop reads back
    as four razor partials with 5 Hz "bandwidths" (r = 0.9997). Sampling the
    spectrum at n*f0 and interpolating between those samples removes the
    pitch structure and leaves the envelope, which is the filter
    (tools/batch_ingest.tonal_to_tf). Unpitched sources keep the direct
    averaged spectrum.

    The table is written as a sidecar (<stem>_peaks.json) in the same shape
    the compiler ingests — the "tables we should have made": per-formant
    fundamentals + bandwidths, the source the whole pipeline runs on.
    """
    x, sr = load_wav(path)
    f0, clarity = detect_f0(x, sr)
    if f0 and clarity >= 0.30:
        env_db, _ = tonal_to_tf(x, sr, f0=f0)
        lo, hi = max(BAND[0], TF_FREQS[0]), min(BAND[1], sr * 0.45, TF_FREQS[-1])
        if hi <= lo + 100.0:
            raise ValueError(f"{path.name}: band {lo:.0f}-{hi:.0f} Hz too narrow")
        grid = np.geomspace(lo, hi, LOG_POINTS)
        gdb = np.interp(np.log(grid), np.log(TF_FREQS), env_db)
        lane = (f"harmonic envelope over f0 {f0:.1f} Hz "
                f"(clarity {clarity:.2f}) — the filter, not the notes")
    else:
        f, db = averaged_spectrum_db(x, sr)
        lo, hi = max(BAND[0], f[0]), min(BAND[1], sr * 0.45, f[-1])
        if hi <= lo:
            raise ValueError(f"{path.name}: band {lo:.0f}-{hi:.0f} Hz unusable")
        sel = (f >= lo) & (f <= hi)
        grid = np.geomspace(f[sel][0], f[sel][-1], LOG_POINTS)
        gdb = np.interp(np.log(grid), np.log(f[sel]), db[sel])
        lane = "averaged spectrum (unpitched source)"
    peaks = [{"freq_hz": hz, "bw_hz": bw, "level_db": lvl}
             for hz, bw, lvl in extract(grid, gdb)]
    tbl = {"format": "peaks-v1",
           "source": path.name,
           "description": f"find_peaks fundamentals extracted from the WAV: {lane}",
           "rate_hz": float(sr),
           "peaks": peaks}
    sidecar = path.with_name(f"{path.stem}_peaks.json")
    sidecar.write_text(json.dumps(tbl, indent=1))
    print(f"  extracted {sidecar.name} "
          f"({len(peaks)} fundamentals "
          + ", ".join(f"{p['freq_hz']:.0f} Hz bw {p['bw_hz']:.0f}" for p in peaks)
          + ")")
    return tbl, float(sr)


def _formants_from_object(obj: dict) -> list[dict]:
    """Tolerate every table shape the pipeline has produced:
    acoustic-source-v1 objects ({formants|modes}), peaks-v1 ({peaks}),
    named-formant maps ({f1:..,f2:.., bw:{...}}), and flat peak dicts."""
    if not isinstance(obj, dict):
        raise SystemExit(f"table entries must be objects, got {type(obj)}")
    rows = (obj.get("formants") or obj.get("modes") or obj.get("peaks")
            or obj.get("rows"))
    if rows is None:
        # named-formant object: f1/f2/... with an optional bw group
        names = sorted((k for k in obj if re.match(r"^f\d+$", k)),
                       key=lambda k: int(k[1:]))
        if not names:
            raise SystemExit("no formants/modes/peaks/rows list and no f1/f2/... "
                             f"keys in object: {list(obj)[:6]}...")
        bws = obj.get("bw", {})
        out = []
        for n in names:
            b = bws.get(n.replace("f", "b"), obj.get("bw_hz", None))
            out.append({"mode_or_formant": n.upper(),
                        "frequency_hz": float(obj[n]),
                        "bandwidth_hz": (float(b) if b is not None else None)})
        return out
    norm = []
    for r in rows:
        if isinstance(r, dict):
            fr = r.get("frequency_hz") or r.get("freq_hz") or r.get("freq")
            mo = r.get("mode_or_formant") or r.get("mode")
            bw = r.get("bandwidth_hz", r.get("bw_hz", r.get("bw")))
            lvl = r.get("level_db", r.get("amplitude_db", 0.0))
            if fr is None:
                raise SystemExit(f"row missing frequency: {r}")
            norm.append({"mode_or_formant": mo, "frequency_hz": float(fr),
                         "bandwidth_hz": (float(bw) if bw is not None else None),
                         "level_db": float(lvl or 0.0),
                         "derivation": r.get("derivation")})
        else:
            norm.append({"mode_or_formant": None, "frequency_hz": float(r),
                         "bandwidth_hz": None, "level_db": 0.0})
    return norm


def split_ref(item: str) -> tuple[Path, str | None]:
    """`table.json#object_id` -> (path, object_id). The picker's selection
    syntax: ONE object of an acoustic-source-v1 table is one endpoint."""
    if "#" in item:
        p, _, oid = item.rpartition("#")
        return Path(p), (oid or None)
    return Path(item), None


class Refusal(SystemExit):
    """Loud ingest/plan refusal. Carries the machine-readable code first."""


def _numbers_in(obj, out: set[float]) -> set[float]:
    """Every finite number anywhere in a JSON document."""
    if isinstance(obj, bool):
        return out
    if isinstance(obj, (int, float)) and math.isfinite(obj):
        out.add(round(float(obj), 6))
    elif isinstance(obj, dict):
        for v in obj.values():
            _numbers_in(v, out)
    elif isinstance(obj, list):
        for v in obj:
            _numbers_in(v, out)
    return out


def assert_sourced(rows: list[dict], doc: dict, path: Path) -> None:
    """PROVENANCE ASSERTION (step 1 INGEST).

    A table that names its `sources` must be able to show every frequency and
    bandwidth it supplies inside one of them. Anything else is a constant
    somebody typed - the exact failure that put an invented 250 Hz throat and a
    fabricated 1668.5 Hz formant into two bodies with citations attached.

    A value that is legitimately COMPUTED (bandwidth from a measured radius,
    a resampled Hz) is still admissible, but it must say so: give that row a
    `derivation` string. Silence is refused, citations are checked.
    """
    sources = doc.get("sources")
    if not sources:
        return
    if isinstance(sources, str):
        sources = [sources]
    pool: set[float] = set()
    missing_files = []
    for ref in sources:
        src = Path(ref) if Path(ref).is_absolute() else ROOT / ref
        if not src.is_file():
            missing_files.append(ref)
            continue
        try:
            text = src.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            raise Refusal(f"UNSOURCED_CONSTANT: {path.name} cites {ref}, "
                          f"which cannot be read ({exc})")
        try:
            _numbers_in(json.loads(text), pool)
        except json.JSONDecodeError:
            # A cited source may be prose - a manual, a paper. The numbers in
            # it are still the citation: the Morpheus manual states its
            # paravowel peaks as "325, 700, 2530, 3500 and 4950Hz".
            for tok in re.findall(r"-?\d+(?:\.\d+)?", text):
                pool.add(round(float(tok), 6))
    if missing_files:
        raise Refusal(f"UNSOURCED_CONSTANT: {path.name} cites "
                      f"{', '.join(missing_files)}, which does not exist")
    unsourced = []
    for i, r in enumerate(rows):
        if r.get("derivation"):
            continue
        for field in ("frequency_hz", "bandwidth_hz"):
            v = r.get(field)
            if v is None:
                continue
            if round(float(v), 6) not in pool:
                unsourced.append(f"row {i + 1} {field}={v:g}")
    if unsourced:
        raise Refusal(
            f"UNSOURCED_CONSTANT: {path.name} cites {', '.join(sources)} but "
            f"these values are not in it: {'; '.join(unsourced)}. "
            "Put the number in a table, or give the row a `derivation` string "
            "naming how it was computed.")


def load_table(path: Path, object_id: str | None = None
               ) -> tuple[list[dict], float]:
    """Either a frequency-table JSON, or a WAV reduced to fundamentals.

    Returns the PEAK LIST of ONE endpoint object in (freq_hz, bw_hz,
    level_db) terms; the table may carry many objects (academic corpora),
    in which case `object_id` selects one (`table.json#id`) and the first
    object is the default.
    """
    if path.suffix.lower() == ".wav":
        tbl, sr = table_from_wav(path)
        peaks = tbl["peaks"]
        return ([{"freq_hz": p["freq_hz"], "bw_hz": p["bw_hz"],
                  "level_db": p["level_db"]} for p in peaks], sr)
    d = json.loads(path.read_text())
    sr = float(d.get("rate_hz", d.get("sample_rate", 48_000.0)))
    objects = d.get("objects")
    if objects is not None:
        if not isinstance(objects, list) or not objects:
            raise SystemExit(f"{path.name}: 'objects' must be a non-empty list")
        want = object_id or d.get("object_id")
        obj = objects[0]
        if want is not None:
            obj = next((o for o in objects if o.get("object_id") == want), None)
            if obj is None:
                raise SystemExit(f"{path.name}: no object '{want}' "
                                 f"(has {len(objects)}, e.g. "
                                 f"{objects[0].get('object_id')})")
        rows = _formants_from_object(obj)
        assert_sourced(rows, {**d, **obj}, path)
        return (_measured_peaks(rows, path), sr)
    rows = _formants_from_object(d)
    assert_sourced(rows, d, path)
    return (_measured_peaks(rows, path), sr)


def _measured_peaks(rows: list[dict], path: Path) -> list[dict]:
    """No recipe rides without a MEASURED bandwidth: a row missing one
    refuses loudly - no defaults, no Klatt fill-ins. r = exp(-pi*B/fs)
    is only true when B was measured."""
    missing = [str(r["mode_or_formant"] or f"row {i + 1}")
               for i, r in enumerate(rows) if r["bandwidth_hz"] is None]
    if missing:
        raise SystemExit(f"{path.name}: no measured bandwidth on "
                         f"{', '.join(missing)} - measure it; nothing is "
                         "authored without one")
    return [{"freq_hz": r["frequency_hz"], "bw_hz": r["bandwidth_hz"],
             "level_db": r.get("level_db", 0.0)} for r in rows]


def resolve_inputs(items: list[str], folder_arg: str | None
                   ) -> tuple[str, str, str]:
    """Return (m0_ref, m100_ref, label). Accepts two endpoint refs
    (`file` or `table.json#object_id`), or one folder with two WAVs."""
    if folder_arg:
        folder = Path(folder_arg)
        if not folder.is_dir():
            raise SystemExit(f"not a folder: {folder}")
        paths = wavs_in(folder)
        if len(paths) != 2:
            raise SystemExit(f"folder needs exactly 2 WAVs, got {len(paths)} "
                             f"({folder})")
        lo, hi = _morph_order(paths)
        return str(lo), str(hi), lo.stem + "_to_" + hi.stem
    if not items or len(items) != 2:
        raise SystemExit("pass two endpoint files (tables or WAVs), or one folder")
    p0, o0 = split_ref(items[0])
    p1, o1 = split_ref(items[1])
    if o0 is None and o1 is None \
            and p0.suffix.lower() == ".wav" and p1.suffix.lower() == ".wav":
        lo, hi = _morph_order([p0, p1])
        return str(lo), str(hi), lo.stem + "_to_" + hi.stem
    return items[0], items[1], (o0 or p0.stem) + "_to_" + (o1 or p1.stem)


def _morph_order(paths: list[Path]) -> tuple[Path, Path]:
    m = [re.search(r"(?i)(m0|m100)", p.stem) for p in paths]
    if all(mm and mm.group(1).lower() in ("m0", "m100") for mm in m):
        return min(paths, key=lambda p: m[paths.index(p)].group(1) == "m100"), \
               max(paths, key=lambda p: m[paths.index(p)].group(1) == "m100")
    return paths[0], paths[1]      # sorted() order = [M0, M100]


# ── lanes ───────────────────────────────────────────────────────────────────

def voice_radius(bw_hz: float, sr: float) -> float:
    """r = exp(-pi * B / fs)."""
    return math.exp(-math.pi * bw_hz / sr)


def place_zero(pole_hz: float, other_poles: list[float], sr: float,
               want_st: float = ZERO_ST_ABOVE_POLE) -> tuple[float, str]:
    """SERIAL CASCADE ZERO PLACEMENT (extractor.py law, rate-generalised).

    The wanted carve is the WANT, not the law: a zero multiplies through every
    downstream stage, so one landing near another pole deletes it. Search
    outward from it (upward first) for a spot >= MIN_ZERO_CLEARANCE_ST from
    every other pole; park at ZERO_PARK_HZ when nothing is clear.
    """
    want = pole_hz * 2 ** (want_st / 12.0)

    def clear(hz: float) -> bool:
        return all(abs(12 * math.log2(hz / p)) >= MIN_ZERO_CLEARANCE_ST
                   for p in other_poles if p > 0)

    if clear(want):
        return want, f"+{want_st:.1f} st"
    step = 0.25
    off = step
    while off <= ZERO_SEARCH_SPAN_ST:
        for cand in (want * 2 ** (off / 12.0), want * 2 ** (-off / 12.0)):
            if cand > pole_hz and cand < sr * 0.49 and clear(cand):
                moved = 12 * math.log2(cand / pole_hz)
                return cand, f"+{moved:.1f} st (moved clear)"
        off += step
    return min(ZERO_PARK_HZ, sr * 0.49), "parked (no clear carve)"


def grammar(seated: dict[int, tuple[int, tuple[float, float, float], bool]],
            sr: float, carve: tuple[float, float],
            terminal_zero_hz: float | None = None,
            carve_st: dict[int, float] | None = None,
            carve_zr: dict[int, float] | None = None) -> list[dict]:
    """Six lanes for one corner (P2K lane grammar).

    lane 1   = peripheral voice when seated: a measured pole whose zero may
               live registers away (the TalkingHedz S1 species - air pole,
               low-end carve). Left unseated it is the exact-passthrough
               sentinel; nothing is authored that was not measured.
    lanes 2-5 = the talkers: pole at the fundamental, r from the -3 dB
                bandwidth; the zero that PAYS for that pole sits at the
                corpus's measured interval above it (serial clearance
                enforced) and carries the corpus's measured zero radius -
                broader than the pole, so it eats the pole's upper skirt.
                `seated` maps slot -> (track, (hz, bw, level), reserved);
                a slot with no seated track is a SENTINEL identity lane
                (exact passthrough): the compiler does not demand a stage
                count, it pads what is missing.
    lane 6   = the terminal unit zero, always (measured-universal,
               132/132). Its FREQUENCY is authorable: parked out of band
               by default, a mid-band silence line when carved. Seated,
               the lane also carries a measured pole (the throat voice).
    SCALE = 1.0 at build time: broadband gain is a closed analytical
    unity-DC normalization applied post-hoc, never a search.
    """
    z_st, z_r = carve
    pole_hzs = [pk[0] for (_t, pk, _rv) in seated.values()]

    def voice(slot: int, role: str) -> dict:
        trk, (hz, bw, _lvl), tag = seated[slot]
        r = voice_radius(bw, sr)
        others = [p for p in pole_hzs if p != hz]
        # DIRECTIONAL CARVE. carve_law() is one global median (+3.03 st) and the
        # corpus does not agree with itself: measured per body over the exotic
        # 33, the median pole->zero interval runs from Dead Ringer at -3.5 st to
        # Cruz Pusher at +37.7 st, and four bodies carve BELOW their poles.
        # Deep Bouche is one of them (-2.8 st). A lane may therefore name its
        # own interval and side; unnamed lanes keep the global default.
        st = carve_st.get(slot, z_st) if carve_st else z_st
        zr = carve_zr.get(slot, z_r) if carve_zr else z_r
        if st >= 0:
            z_hz, note = place_zero(hz, others, sr, st)
        else:
            # Below-pole carve is placed directly: place_zero() searches upward
            # for serial clearance, which has no meaning under the pole.
            z_hz = max(20.0, min(hz * 2.0 ** (st / 12.0), sr * 0.49))
            note = f"{st:+.1f} st lower carve"
        return dict(slot=slot,
                    role=role.format(t=trk + 1) + (f" ({tag})" if tag else ""),
                    reserved=bool(tag), pole_hz=hz, pole_r=r,
                    zero_hz=z_hz, zero_r=zr, zero_note=note, scale=1.0)

    if 1 in seated:
        lanes = [voice(1, "S1 conditioning T{t}")]
    else:
        lanes = [dict(slot=1, role="S1 free lane (sentinel)",
                      pole_hz=0.0, pole_r=0.0, zero_hz=0.0, zero_r=0.0,
                      zero_note="sentinel - exact passthrough", scale=1.0)]
    for slot in (2, 3, 4, 5):
        if slot not in seated:
            lanes.append(dict(slot=slot, role=f"S{slot} sentinel (identity)",
                              pole_hz=0.0, pole_r=0.0,
                              zero_hz=0.0, zero_r=0.0,
                              zero_note="sentinel - exact passthrough",
                              scale=1.0))
            continue
        lanes.append(voice(slot, "talker T{t}"))
    if 6 in seated:
        trk, (hz, bw, _lvl), tag = seated[6]
        lanes.append(dict(slot=6,
                          role=f"S6 conditioning T{trk + 1} + unit zero"
                               + (f" ({tag})" if tag else ""),
                          reserved=bool(tag),
                          pole_hz=hz, pole_r=voice_radius(bw, sr),
                          zero_hz=min(terminal_zero_hz or ZERO_PARK_HZ, sr * 0.49),
                          zero_r=1.0,
                          zero_note=("terminal unit zero, AUTHORED "
                                     f"{terminal_zero_hz:.0f} Hz"
                                     if terminal_zero_hz else
                                     "terminal unit zero, parked "
                                     "(carve to move it)"),
                          scale=1.0))
    else:
        lanes.append(dict(slot=6, role="S6 terminator (unit zero)",
                          pole_hz=0.0, pole_r=0.0,
                          zero_hz=min(ZERO_PARK_HZ, sr * 0.49), zero_r=1.0,
                          zero_note="terminal unit zero, parked",
                          scale=1.0))
    return lanes


def apply_q_attitude(corner: list[dict], spec: str) -> tuple[list[dict], str]:
    """Derive the Q100 pose from a documented E-MU Q attitude (never measured).

    Radius deltas clamp inside (0, 0.9995) so the encoder never refuses;
    revoice moves a lane's pole Hz instead. Behaviors + evidence in
    tools/q_attitudes.py / docs/TRENCH_CORPUS_FINDINGS.md §6.
    """
    out = [dict(ln) for ln in corner]
    parts = spec.split(":")
    name = parts[0]

    def r(lane):
        return out[lane - 1]["pole_r"]

    if name in ("flat", "dead_q"):
        return out, "Q100 = Q0 (Q-collapsed; FuzziFace/DeepBouche dead-Q)"
    if name == "uniform-gentle":
        d = float(parts[1]) if len(parts) > 1 else 0.07
        for ln in out:
            if ln["pole_hz"] > 0:   # voices only; identity lanes stay identity
                ln["pole_r"] = min(0.9995, ln["pole_r"] + d)
        return out, f"uniform +{d} across all lanes (AcidRavage)"
    if name == "single-bloomer":
        lane, d = int(parts[1]), float(parts[2]) if len(parts) > 2 else 0.25
        if out[lane - 1]["pole_hz"] <= 0:  # voices only; identity lanes stay identity
            return out, f"S{lane} carries no pole (identity lane) - bloomer skipped"
        out[lane - 1]["pole_r"] = min(0.9995, r(lane) + d)
        return out, f"S{lane} +{d} only (BassTracer S6 +0.36)"
    if name == "asymmetric-relay":
        arm = [int(x) for x in parts[1].split(",")]
        dis = [int(x) for x in parts[2].split(",")]
        ad = float(parts[3]) if len(parts) > 3 else 0.22
        dd = float(parts[4]) if len(parts) > 4 else 0.40
        skipped = [i for i in arm + dis if out[i - 1]["pole_hz"] <= 0]
        for i in arm:
            if out[i - 1]["pole_hz"] > 0:
                out[i - 1]["pole_r"] = min(0.9995, r(i) + ad)
        for i in dis:
            if out[i - 1]["pole_hz"] > 0:
                out[i - 1]["pole_r"] = max(0.0, r(i) - dd)
        note = f"arm {arm} +{ad} / disarm {dis} -{dd} (MegaSweepz)"
        if skipped:
            note += f"; no pole on {skipped} - skipped"
        return out, note
    if name == "all-negative":
        d = float(parts[1]) if len(parts) > 1 else -0.04
        for ln in out:
            ln["pole_r"] = max(0.0, ln["pole_r"] + d)
        return out, f"all -{abs(d)} (RazorBlades: more Q = deeper cuts)"
    if name == "revoice":
        lane, _, hz = int(parts[1]), parts[2], float(parts[3])
        if out[lane - 1]["pole_hz"] <= 0:  # voices only; identity lanes stay identity
            return out, f"S{lane} carries no pole (identity lane) - revoice skipped"
        out[lane - 1]["pole_hz"] = hz
        return out, f"S{lane} revoiced to {hz:.0f} Hz at Q100 (DJAlkaline)"
    raise SystemExit(f"unknown --q-attitude '{name}' (see docstring)")


# The bench menu, spec + plain words. ONE SYSTEM: the workstation shows these
# entries verbatim and stores the spec; nothing about Q lives in the UI.
# Deltas are the measured behaviours (tools/q_attitudes.py); bloomer/relay
# target talker lanes because that is where this grammar's poles live.
Q_ATTITUDE_MENU = [
    ("flat", "no Q move - radii hold (FuzziFace, DeepBouche)"),
    ("uniform-gentle:0.07", "every voice lifts a little  +0.07 r  (AcidRavage)"),
    ("all-negative:-0.04", "every voice relaxes  -0.04 r  (RazorBlades)"),
    ("single-bloomer:5:0.36",
     "one voice takes the whole budget  S5 +0.36  (BassTracer's move)"),
    ("asymmetric-relay:5:2:0.22:0.66",
     "one arms while one defuses  S5 +0.22 / S2 -0.66  (MegaSweepz's move)"),
]


# ── compile ────────────────────────────────────────────────────────────────

def encode_stage(roots: list[float], sr: float) -> list[int]:
    """One lane -> 5 packed u16 words, through trench_core.dll."""
    row = (ctypes.c_uint16 * 5)()
    rc = lib.trench_stage_words_from_roots_at(
        (ctypes.c_double * 5)(*roots), ctypes.c_double(sr), row)
    if rc == 0:
        return list(row)
    raise ValueError(f"encoder refused {roots}; the blueprint is law - "
                     "no fallback rewrites the zero geometry")


def pack_body(per_corner: list[list[dict]], sr: float) -> bytes:
    """Corner-major lanes -> packed body, unity-DC normalized. The ONE
    encode->pack path: the bench preview and the saved body are the same
    bytes by construction. Refusals name their corner and slot."""
    words: list[int] = []
    refused: list[str] = []
    for cname, lanes in zip(CORNER_ORDER, per_corner):
        for ln in lanes:
            try:
                words.extend(encode_stage(
                    [ln["pole_hz"], ln["pole_r"], ln["zero_hz"],
                     ln["zero_r"], ln["scale"]], sr))
            except ValueError:
                refused.append(f"{cname} S{ln['slot']}")
                words.extend([0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000])
    if refused:
        raise SystemExit(f"encode refused {', '.join(refused)}; "
                         "tune the source, do not ship identity")

    buf = ctypes.create_string_buffer(240)
    rc = lib.trench_pack_body_from_corner_words(
        (ctypes.c_uint16 * 120)(*words), 120, buf)
    assert rc == 0, f"pack rc={rc}"

    # SCALE: closed analytical unity-DC normalization, one 1/6-root factor
    # per corner over the six SCALE words (DC at z=1 -> 1).
    return bytes(dc_anchor_body(buf.raw))


def compile_body(per_corner: list[list[dict]], sr: float) -> tuple[bytes, dict]:
    """pack_body + stability certification."""
    body = pack_body(per_corner, sr)

    max_r, fail_m, fail_q = (ctypes.c_double(), ctypes.c_double(),
                             ctypes.c_double())
    passed = ctypes.c_int()
    rc = lib.trench_certify_body(body, 240, 33, 1.0, ctypes.byref(passed),
                                 ctypes.byref(max_r), ctypes.byref(fail_m),
                                 ctypes.byref(fail_q))
    assert rc == 0, f"certify rc={rc}"
    if passed.value != 1:
        raise SystemExit(f"UNSTABLE body at morph {fail_m.value:.2f} "
                         f"q {fail_q.value:.2f} - do not ship")

    return body, {"max_r": max_r.value, "certify": "PASS",
                  "dc": "unity (closed analytical)"}


def write_blueprint(folder: Path, name: str, sr: float, per_corner: list,
                    tracks: dict, carve: tuple[float, float],
                    q_spec: str, q_note: str, label: str, lane_order: str,
                    body: bytes | None, info: dict,
                    seat_map: dict[int, int] | None = None,
                    reserved_slots: set[int] | None = None,
                    wires: dict[int, str] | None = None) -> None:
    json_path = folder / f"{name}_blueprint.json"
    txt_path = folder / f"{name}_corners.txt"
    slot_of = {t: s for s, t in (seat_map or {}).items()}

    doc = {
        "schema": "batch-lane-grammar-blueprint-v4",
        "name": name,
        "rate_hz": sr,
        "corner_order": CORNER_ORDER,
        "endpoints": label,
        "lane_law": "S1/S6 conditioning when seated (sentinel / bare "
                    "terminator when not) + S2-S5 measured talkers + S6 "
                    "terminal unit zero; every pole measured, no family "
                    "medians",
        "q_corners": {"law": q_spec, "note": q_note, "derived": True,
                      "measured": False},
        "lane_order": lane_order,
        "voice_zero": {"interval_st": carve[0],
                       "radius": carve[1],
                       "source": "median over every carving dossier lane, "
                                 "corpus-wide (derived statistic, never a "
                                 "copied row)"},
        "tracks": [
            {"lane": slot_of.get(t),      # null = measured but riding no lane
             "reserved": slot_of.get(t) in (reserved_slots or set()),
             "freq_hz": [tracks[cn][t][0] for cn in CORNER_ORDER],
             "bw_hz": [tracks[cn][t][1] for cn in CORNER_ORDER]}
            for t in range(len(tracks[CORNER_ORDER[0]]))
        ],
        "reservations": sorted(f"S{s}=T{seat_map[s] + 1}"
                               for s in (reserved_slots or set())),
        "wires": sorted((wires or {}).values()),
        "stages": {cn: lanes for cn, lanes in zip(CORNER_ORDER, per_corner)},
        "scale_policy": "closed analytical unity-DC, post-hoc",
        "compile": {"status": "done" if body is not None else "refused",
                    **info},
    }
    json_path.write_text(json.dumps(doc, indent=1))

    z_st, z_r = carve
    lines = [f"{name}  {label}  fs={sr:.0f}  lanes={lane_order}  "
             f"r=exp(-pi*B/fs)  voice zero {z_st:+.1f} st r {z_r:.4f}  "
             f"Q: {q_note}\n"]
    for ci, cname in enumerate(CORNER_ORDER):
        lines.append(f"\n{cname}")
        for ln in per_corner[ci]:
            lines.append(
                f"  S{ln['slot']}  {ln['role']:<28} pole {ln['pole_hz']:>9.1f} Hz"
                f"  r {ln['pole_r']:.4f}   zero {ln['zero_hz']:>9.1f} Hz"
                f"  r {ln['zero_r']:.4f}   {ln['zero_note']}")
    txt_path.write_text("\n".join(lines))
    print(f"  blueprint  {json_path.name}")
    print(f"  corners    {txt_path.name}")
    if body is not None:
        body_path = folder / f"{name}.body240"
        body_path.write_bytes(body)
        print(f"  body       {body_path.name}   certify {info['certify']} "
              f"max_r {info['max_r']:.6f}   scale {info['dc']}")


def referee(body: bytes, ref_path: str, sr: float) -> None:
    """The silhouette gate (docs/EVIDENCE_2026-08-06_resonance_gap.md).

    Whole-cascade judgment only: peak-above-DC per corner through the packed
    runtime path, ours vs the reference we claim to answer. PASS = our
    hottest corner never stands taller than the reference's hottest. On
    refusal the diagnostic is in the sections: name the rung where the
    ladders separate. Runs before the body touches disk — a refused compile
    ships nothing.
    """
    from cascade_ladder import stages, ladder, CORNERS, DIVERGE_DB
    ref = Path(ref_path).read_bytes()
    walks = []
    for cname, m, q in CORNERS:
        ro, _, _ = ladder(stages(body, m, q, sr))
        rr, _, _ = ladder(stages(ref, m, q, sr))
        walks.append((cname, ro, rr))
    ours_max = max(w[1][5] for w in walks)
    ref_max = max(w[2][5] for w in walks)
    if ours_max <= ref_max:
        print(f"  referee    PASS  answers {Path(ref_path).stem}: hottest "
              f"corner {ours_max:.1f} dB inside reference {ref_max:.1f} dB")
        return
    diag = []
    for cname, ro, rr in walks:
        sep = next((i for i in range(6)
                    if abs(ro[i] - rr[i]) >= DIVERGE_DB), None)
        if sep is not None:
            diag.append(f"{cname} ladders separate at S{sep + 1} "
                        f"({ro[sep] - rr[sep]:+.1f} dB)")
    raise SystemExit(
        f"REFEREE REFUSED  answers {Path(ref_path).stem}: hottest corner "
        f"{ours_max:.1f} dB stands over reference {ref_max:.1f} dB\n  "
        + "\n  ".join(diag))


def run(m0_ref: str, m100_ref: str, label: str, args) -> None:
    m0_path, m0_obj = split_ref(m0_ref)
    m100_path, m100_obj = split_ref(m100_ref)
    print(f"\n== {label} ==")
    print(f"  M0   <- {m0_path.name}" + (f"  [{m0_obj}]" if m0_obj else ""))
    print(f"  M100 <- {m100_path.name}" + (f"  [{m100_obj}]" if m100_obj else ""))

    m0, sr0 = load_table(m0_path, m0_obj)
    m100, sr1 = load_table(m100_path, m100_obj)
    if args.rate:
        # The AUTHORING rate is the runtime rate, not the rate the source
        # happened to be recorded at: packed words are authored for the rate
        # the engine will run at and consumed verbatim (AGENTS.md). Measured
        # frequencies and bandwidths are physical Hz and carry over unchanged.
        sr = float(args.rate)
    else:
        if abs(sr0 - sr1) > 0.5:
            raise SystemExit(f"endpoint rates differ: {sr0:.0f} vs {sr1:.0f} "
                             "- pass --rate to name the authoring rate")
        sr = float(sr0)
    print(f"  fs = {sr:.0f} Hz   band {BAND[0]:.0f}-{BAND[1]:.0f} Hz")

    tz = None
    if args.terminal_zero:
        try:
            a, b = (float(x) for x in args.terminal_zero.split(":"))
        except ValueError:
            raise SystemExit("--terminal-zero wants HZ_M0:HZ_M100, e.g. 20277:11460")
        if not (0 < a <= sr * 0.49 and 0 < b <= sr * 0.49):
            raise SystemExit(f"--terminal-zero must sit inside (0, {sr * 0.49:.0f}] Hz")
        tz = (a, b)
    locked = []
    for spec in args.lock_lane:
        m = re.fullmatch(r"[sS]([1-6])", spec)
        if not m:
            raise Refusal(f"--lock-lane wants S1..S6, got {spec!r}")
        locked.append(int(m.group(1)))
    plan = plan_body(m0, m100, sr, lane_order=args.lane_order,
                     reserve=args.reserve, wire=args.wire,
                     q_attitude=args.q_attitude, terminal_zero=tz,
                     locked_lanes=locked, shape_weight=args.shape_weight,
                     gate_static=True)
    for line in plan["notes"]:
        print("  " + line)

    out_dir = Path(args.out) if args.out else m0_path.parent
    out_dir.mkdir(parents=True, exist_ok=True)
    body, info = None, {"certify": "refused", "max_r": 0.0, "dc": "-"}
    if not args.blueprint_only:
        try:
            body, info = compile_body(plan["per_corner"], sr)
        except SystemExit as e:
            print(f"  COMPILE REFUSED: {e}")
        if body is not None and args.answers:
            try:
                referee(body, args.answers, sr)
            except SystemExit as e:
                print(f"  {e}")
                body, info = None, {"certify": "referee refused",
                                    "max_r": info["max_r"], "dc": info["dc"]}
    write_blueprint(out_dir, args.name or label, sr, plan["per_corner"],
                    plan["tracks"], plan["carve"], args.q_attitude,
                    plan["q_note"], label, args.lane_order, body, info,
                    plan["seat_map"], plan["reserved_slots"],
                    plan["wire_labels"])


def plan_body(m0: list, m100: list, sr: float, *, lane_order: str = "ascending",
              reserve: list[str] | None = None, wire: list[str] | None = None,
              q_attitude: str = "flat",
              terminal_zero: tuple[float, float] | None = None,
              locked_lanes: list[int] | None = None,
              carve_st: dict[int, float] | None = None,
              carve_zr: dict[int, float] | None = None,
              shape_weight: float = 0.0,
              gate_static: bool = False) -> dict:
    """Two measured endpoint row lists -> the four-corner lane plan.

    The one planning path: the CLI and the Workstation both call this.
    Refusals raise SystemExit with the reason - loud, never silent.
    """
    # TALKER COUNT IS ADAPTIVE. A source may carry 1..4 fundamentals (the
    # composer chooses how much of a source it wants); the compiler never
    # demands a fixed number. More than four is truncated to the first four,
    # ascending - SCHEMA agreement (extra modes are kept in the table, the
    # grammar reads the first four). Fewer than four pads exact-passthrough
    # sentinel lanes. The two endpoints are capped symmetrically so tracking
    # never has to guess.
    notes: list[str] = []
    n_cap = min(NVOICE, len(m0), len(m100))
    if n_cap < 1:
        raise SystemExit(f"need at least one fundamental per endpoint "
                         f"(M0={len(m0)}, M100={len(m100)})")
    capped = len(m0) > n_cap or len(m100) > n_cap
    m0, m100 = m0[:n_cap], m100[:n_cap]
    notes.append(f"measured rows: {n_cap} of up to {NVOICE} "
                 f"{'- capped from more' if capped else '(as sourced)'}")

    # Only the two morph endpoints are measured. Track -> lane fixed here.
    raw = {"M0_Q0": [(p["freq_hz"], p["bw_hz"], p["level_db"]) for p in m0],
           "M100_Q0": [(p["freq_hz"], p["bw_hz"], p["level_db"]) for p in m100]}
    tracks = track_formants(raw, shape_weight=shape_weight)

    # TRAVEL WIRES. S3=F2:F4 is the deliberate handoff: M0's F2 travels to
    # M100's F4, seated at S3. The two affected tracks swap their M100
    # partners - a pure permutation of measured rows, nothing invented -
    # and the seat is pinned a priori like every lane decision.
    wired_slots: dict[int, int] = {}                   # slot -> M0 row (track)
    wire_labels: dict[int, str] = {}
    for spec in (wire or []):
        m = re.fullmatch(r"[sS]([1-6])=[fF]([1-6]):[fF]([1-6])", spec)
        if not m:
            raise SystemExit(f"a wire wants S<1-6>=F<m0>:F<m100>, got {spec!r}")
        slot, a, b = int(m.group(1)), int(m.group(2)) - 1, int(m.group(3)) - 1
        if a >= n_cap or b >= n_cap:
            raise SystemExit(f"wire {spec}: source carries only {n_cap} "
                             "formants per endpoint")
        if slot in wired_slots or a in wired_slots.values():
            raise SystemExit(f"wire {spec}: seat or M0 formant already wired")
        want = raw["M100_Q0"][b]
        tb = next(t for t in range(n_cap) if tracks["M100_Q0"][t] == want)
        if tb != a:
            tracks["M100_Q0"][a], tracks["M100_Q0"][tb] = \
                tracks["M100_Q0"][tb], tracks["M100_Q0"][a]
        wired_slots[slot] = a
        wire_labels[slot] = spec.upper()
    # LANE ORDER is a lane-role decision, taken a priori — before a single
    # filter coefficient exists — and applied identically to every corner, so
    # correspondence never scrambles. Ascending is the default; descending is
    # the DeepBouche species signature (INTENT: "S1 = highest formant, S6 =
    # throat ... reverse the order and it stops being DeepBouche").
    if lane_order == "descending":
        for cn in list(tracks):
            tracks[cn] = list(reversed(tracks[cn]))
    # RESERVED SEATS. S4=T2 pins track 2's measured peak to slot 4, a priori
    # like every lane decision: the compiler seats the remaining tracks
    # around reservations (and wires), never over them.
    reserved_slots: set[int] = set()
    seat_map: dict[int, int] = dict(wired_slots)       # slot -> track index
    if lane_order == "descending":                     # reversal renumbers
        seat_map = {s: n_cap - 1 - t for s, t in seat_map.items()}
    for spec in (reserve or []):
        m = re.fullmatch(r"[sS]([1-6])=[tT]([1-6])", spec)
        if not m:
            raise SystemExit(f"a seat wants S<1-6>=T<1-6>, got {spec!r}")
        slot, trk = int(m.group(1)), int(m.group(2)) - 1
        if trk >= n_cap:
            raise SystemExit(f"seat {spec}: source carries only {n_cap} tracks")
        if slot in seat_map or trk in seat_map.values():
            raise SystemExit(f"seat {spec}: seat or track already taken")
        seat_map[slot] = trk
        reserved_slots.add(slot)
    free_slots = [s for s in (2, 3, 4, 5) if s not in seat_map]
    free_tracks = [t for t in range(n_cap) if t not in seat_map.values()]
    seat_map.update(zip(free_slots, free_tracks))

    # STATIC LANE GATE. Measured over the whole 50-body P2K bank: not one ROM
    # body parks a single pole — zero of 50 have even one lane whose (Hz, bw)
    # is identical at M0 and M100. A body whose lanes hold is not a morph, it
    # is one filter stored twice. Lanes that are MEANT to hold must say so.
    static = [s for s, t in sorted(seat_map.items())
              if tracks["M0_Q0"][t][:2] == tracks["M100_Q0"][t][:2]]
    unflagged = [s for s in static if s not in (locked_lanes or [])]
    if gate_static and len(unflagged) >= 2:
        detail = ", ".join(f"S{s} {tracks['M0_Q0'][seat_map[s]][0]:.0f} Hz"
                           for s in unflagged)
        raise Refusal(
            f"STATIC_LANE_DEFECT: {len(unflagged)} of 6 lanes do not move "
            f"({detail}). No ROM body parks a pole (0 of 50 measured). Give "
            "these lanes different coordinates at the two endpoints, or "
            "declare them with --lock-lane S<n> if holding is the intent.")

    # The Q100 corners are the same formants under the Q behaviour law —
    # derived, never measured. Mirrored only after every wire/reserve swap
    # is final, so all four corners hold identical track order.
    tracks["M0_Q100"] = list(tracks["M0_Q0"])
    tracks["M100_Q100"] = list(tracks["M100_Q0"])

    def pin_tag(slot: int) -> str:
        return ("wired" if slot in wired_slots
                else "reserved" if slot in reserved_slots else "")

    notes.append(f"tracks (pre-fit Hungarian, lane fixed before any filter "
                 f"math, {lane_order}):")
    for slot in sorted(seat_map):
        t = seat_map[slot]
        row = "  ".join(f"{tracks[cn][t][0]:>8.1f}" for cn in ("M0_Q0",
                                                               "M100_Q0"))
        tag = pin_tag(slot)
        notes.append(f"  T{t + 1} S{slot}   {row} Hz"
                     + (f"  {tag.upper()}" if tag else ""))

    def seated_for(cn: str) -> dict:
        return {s: (t, tracks[cn][t], pin_tag(s)) for s, t in seat_map.items()}

    carve = carve_law()
    # The S6 terminal zero's RADIUS is the universal ROM law (1.0, 132/132);
    # its FREQUENCY is authorable and the ROM authors it per endpoint (Deep
    # Bouche: 20277 Hz at M0, 11460 Hz at M100). Parked at ZERO_PARK_HZ unless
    # the two endpoint frequencies are given.
    tz0, tz1 = terminal_zero if terminal_zero else (None, None)
    m0_q0 = grammar(seated_for("M0_Q0"), sr, carve, tz0, carve_st, carve_zr)
    m100_q0 = grammar(seated_for("M100_Q0"), sr, carve, tz1, carve_st, carve_zr)
    # The Q variations of low/high morph are shaped by the P2K Q-corner
    # behaviour law (tools/q_attitudes.py + recipes/tables/
    # p2k_filter_q_behavior.json), never by measured audio.
    m0_q100, q_note = apply_q_attitude(m0_q0, q_attitude)
    m100_q100, _ = apply_q_attitude(m100_q0, q_attitude)
    notes.append(f"Q corners: {q_note}")
    per_corner = [m0_q0, m100_q0, m0_q100, m100_q100]
    s6 = per_corner[0][5]
    notes.append(f"{s6['role']}: unit zero parked {s6['zero_hz']:.0f} Hz  "
                 "(S1 sentinel; no conditioning lanes - every pole is measured)")

    return dict(per_corner=per_corner, tracks=tracks, seat_map=seat_map,
                reserved_slots=reserved_slots, wired_slots=wired_slots,
                wire_labels=wire_labels, carve=carve, q_note=q_note,
                n_cap=n_cap, notes=notes)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("items", nargs="*",
                    help="two endpoint tables/WAVs, or one folder with two WAVs")
    ap.add_argument("--q-attitude", default="flat",
                    help="Q-corner law (see docstring); default flat")
    ap.add_argument("--shape-weight", type=float, default=0.0, metavar="W",
                    help="morphological lane pairing: weight |d log2 Q| alongside "
                         "|d log2 f| in the tracker's cost. 0 (default) is pure "
                         "minimum-travel, which can never produce a crossing. Raise it "
                         "so a sharp voice tracks a sharp voice across a wide interval "
                         "- 31%% of measured ROM motion is contrary.")
    ap.add_argument("--lock-lane", action="append", metavar="S<n>", default=[],
                    help="declare a lane as deliberately holding, exempting it "
                         "from STATIC_LANE_DEFECT. Repeatable. Use only when a "
                         "held lane is the intent - no ROM body holds one.")
    ap.add_argument("--terminal-zero", metavar="HZ_M0:HZ_M100",
                    help="author the S6 terminal zero's FREQUENCY at each morph "
                         "endpoint (its radius stays the universal 1.0). The ROM "
                         "authors this per endpoint - Deep Bouche runs 20277 Hz "
                         "at M0 to 11460 Hz at M100, and that lane carries 41.7 dB "
                         "of its morph. Parked at ZERO_PARK_HZ when omitted.")
    ap.add_argument("--lane-order", default="ascending",
                    choices=["ascending", "descending"],
                    help="talker lane roles, fixed a priori: ascending "
                         "(S2=lowest formant) or descending (S2=highest, the "
                         "DeepBouche species signature)")
    ap.add_argument("--rate", type=float,
                    help="authoring/runtime rate in Hz (default: the source "
                         "table's rate). Measured Hz are physical and carry "
                         "over; only the encoding rate changes.")
    ap.add_argument("--reserve", action="append", metavar="S<slot>=T<track>",
                    help="reserve a seat: pin a measured track to a slot "
                         "S1-S6 (e.g. S4=T2; S1/S6 = the conditioning "
                         "lanes), repeatable; remaining tracks seat around "
                         "reservations into S2-S5")
    ap.add_argument("--wire", action="append", metavar="S<slot>=F<m0>:F<m100>",
                    help="wire a journey: M0's formant travels to M100's "
                         "formant in that seat (e.g. S3=F2:F4), repeatable; "
                         "the two affected tracks swap M100 partners")
    ap.add_argument("--blueprint-only", action="store_true",
                    help="stop after the blueprint (hold point)")
    ap.add_argument("--out", help="output folder")
    ap.add_argument("--name", help="body name (default: <m0>_to_<m100>)")
    ap.add_argument("--answers", metavar="REF.body240",
                    help="the referee: probe both bodies at the four corners "
                         "through the packed path; refuse the compile if our "
                         "hottest corner (peak above DC) stands taller than "
                         "the reference's hottest. On refusal the diagnostic "
                         "is in the sections: the rung where the ladders "
                         "separate is named.")
    args = ap.parse_args()

    m0, m100, label = resolve_inputs(args.items, args.items[0] if len(
        args.items) == 1 and Path(args.items[0]).is_dir() else None)
    run(m0, m100, label, args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())