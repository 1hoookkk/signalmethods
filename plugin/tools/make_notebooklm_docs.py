from __future__ import annotations

import io
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ARCH = ROOT / "recipes" / "architectures"
INTENT = ROOT / "recipes" / "INTENT.md"
OUT = ROOT / "dossiers" / "notebooklm"

def intent_blocks() -> dict[int, str]:
    text = io.open(INTENT, encoding="utf-8").read()
    blocks: dict[int, str] = {}
    pat = re.compile(r"^\*\*(\d+) (.+?)\*\*.*$", re.M)
    matches = list(pat.finditer(text))
    for i, m in enumerate(matches):
        end = matches[i + 1].start() if i + 1 < len(matches) else text.find("\n## ", m.start())
        if end == -1:
            end = len(text)
        blocks[int(m.group(1))] = text[m.start():end].strip()
    return blocks

def corner_row(d: dict, key: str):
    c = d[key]
    return c["M0_Q0"], c["M100_Q0"], c["M0_Q100"], c["M100_Q100"]

def anatomy_table(sections: list[dict]) -> str:
    lines = [
        "| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |",
        "|---|---|---|---|---|---|---|",
    ]
    for s in sections:
        p00, p10, p01, p11 = corner_row(s, "pole_hz")
        r00, r10, r01, r11 = corner_row(s, "pole_r")
        z00, z10, _, _ = corner_row(s, "zero_hz")
        zr00, zr10, _, _ = corner_row(s, "zero_r")
        sc = s.get("scale_db")
        sc_txt = "/".join(f"{v:.1f}" for v in corner_row(s, "scale_db")) if isinstance(sc, dict) else str(sc)
        lines.append(
            f"| S{s['slot']} | {p00:.0f} → {p10:.0f} | {r00:.3f} → {r10:.3f} "
            f"| {r01:.3f} → {r11:.3f} | {z00:.0f} → {z10:.0f} | {zr00:.3f} → {zr10:.3f} | {sc_txt} |")
    return "\n".join(lines)

FORMAT_DOC = """# trench-cascade-trace-v1 — the machine-readable authoring primitive

The unit is the TRANSITION, not the section in isolation:

    C_{s-1}(f)  --H_s(f)-->  C_s(f)

where each section is one SOS

    H_s(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)

and the cascade accumulates by multiplication — in dB, by addition:

    C_s_dB(f) = C_{s-1}_dB(f) + H_s_dB(f)

Each record carries: the exact five packed u16 words for the SOS, the
runtime-decoded [b0,b1,b2,a1,a2], tagged pole/zero geometry and scale, the
stage's own response, the cumulative response before it and after it, its
FIXED lane number (1..6, never frequency-sorted), and explicit morph / Q /
sample-rate coordinates. Complex responses are stored alongside magnitudes
because magnitude alone does not fully describe what an SOS does to a
signal.

## The laws

- BODY CONTRACT: 240 bytes = 4 corners x 6 sections x 5 LE u16 words.
  Corner order (M0,Q0), (M100,Q0), (M0,Q100), (M100,Q100). Word row =
  minifloat [zero-mag, zero-r^2, pole-mag, pole-r^2, SCALE].
- INTERPOLATION LAW: packed u16 words are interpolated LINEARLY IN ENCODED
  SPACE through the real runtime law (minifloat.rs lerp_u16: f32 fraction,
  truncation toward zero, i16 wrap; morph edge on each Q rail first, then
  q between the rails) and only THEN decoded. Pole frequencies, radii and
  curves are never interpolated independently.
- ONE CALCULATION PATH: tools/runtime_probe.py produces every number; the
  cascade inspector (tools/inspect_body.py) renders it and
  tools/export_cascade_trace.py serializes it, so the visual and numeric
  views cannot drift. The probe proves its Python word law against the
  DLL's own kernel on every export (verify_word_law), and the cascade
  identity after_dB = before_dB + stage_dB holds to < 0.001 dB in the
  emitted records.

## Files

- <body>_trace.json — metadata + per-state per-lane records with dB curves.
- <body>_trace.npz — dense arrays: states[S,2] (morph,q),
  frequency_hz[700] (20 Hz..20 kHz log), packed_words[S,6,5] u16,
  biquad[S,6,5], stage_db[S,6,700], cumulative_db[S,6,700],
  stage_complex[S,6,700,2], cumulative_complex[S,6,700,2] (re, im).

Produce one with:

    python tools/export_cascade_trace.py BODY.body240 48000 --npz

## The two ARMAdillo laws (Rossum, verified against this runtime)

1. PARTIAL DECOUPLING of radius and angle. The kernel-to-biquad map is
   a1 = 4*d2 + d3 - 2 and a2 = 1 - d3: the radius word (d3) sits inside
   the frequency decode. Sweeping Q at fixed morph SHIFTS pole frequency -
   measured 1-9% per lane on shipping bodies. Consequences: never spec a
   Q pose as "same frequencies, more radius" - state the Q100 frequencies
   explicitly (the corner encoder solves the coupled system exactly at
   poses; the drift lives between them). The corpus composes WITH this:
   the "Q-as-pitch" species uses the second axis as a tuner.

2. NO EXACT MID-SWEEP POLE-ZERO CANCELLATION. Words interpolate linearly
   in encoded space; the decode to the z-plane is exponential, so pole
   and zero paths with different endpoints trace different curved
   physical trajectories and never meet exactly mid-sweep. Identical
   endpoints cancel across the entire sweep. Therefore cancellation is a
   POSE property (exact at an authored corner) or a WHOLE-SWEEP property
   (the scaffold lanes, the S6 unit-zero rule) - never a mid-travel
   event. Order reduction mid-sweep is done asymptotically: NEAR
   cancellation plus scale ducking, which is exactly what the corpus's
   quiet lanes do.

3. THE APPROXIMATIONS BREAK DOWN OFF THE UNIT CIRCLE (Ding & Rossum).
   Measured on this runtime, not simulated: sweeping radius at constant
   intended 1500 Hz drifts x1.008 in the vocal regime (r 0.99->0.95) but
   x1.84 in the EQ regime (r 0.95->0.55) - the decoded pole balloons to
   2760 Hz mid-path and snaps back, non-monotonically. Morphing 500->4000
   Hz at fixed radius: r=0.99 tracks the geometric mean exactly (1414 Hz
   mid-path); r=0.60 lands at 2505 Hz. Consequences: for EQ and shelving
   species (low-radius lanes), corner-faith is worthless - interiors MUST
   be verified (the UW-AE bakeoff proved corners-only collapses mid-morph)
   and fits go through the joint packed-path optimizer. And the corpus
   plays this too: EarBender's bending lane travels four octaves at radii
   down to 0.48 - inside the warp regime; the lurch is part of the sound.
"""

def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    blocks = intent_blocks()
    index_lines = ["# P2K reference corpus — NotebookLM index", ""]
    count = 0
    for path in sorted(ARCH.glob("P2k_*.json")):
        d = json.loads(path.read_text(encoding="utf-8"))
        idx, name = d["index"], d["name"]
        doc = [f"# {name} — P2K reference {idx} [{d.get('x3_type', '?')}]", ""]
        doc.append(f"Architecture source: `{path.name}` (datum {d.get('datum_sr_hz', '?')} Hz). "
                   f"Lanes are fixed slot numbers; never frequency-sort them.")
        doc.append("")
        blk = blocks.get(idx)
        if blk:
            doc.append("## Dossier (verbatim, recipes/INTENT.md — manual quote included)")
            doc.append("")
            doc.append("```text")
            doc.append(blk)
            doc.append("```")
            doc.append("")
        doc.append("## Section anatomy (all numbers from the architecture)")
        doc.append("")
        doc.append(anatomy_table(d["sections"]))
        doc.append("")
        doc.append("## Machine block (architecture JSON, whole)")
        doc.append("")
        doc.append("```json")
        doc.append(json.dumps(d, indent=1))
        doc.append("```")
        out = OUT / (path.stem + ".md")
        io.open(out, "w", encoding="utf-8").write("\n".join(doc) + "\n")
        index_lines.append(f"- [{name}]({out.name}) — ref {idx}, {d.get('x3_type', '?')}")
        count += 1
    io.open(OUT / "trench-cascade-trace-format.md", "w", encoding="utf-8").write(FORMAT_DOC)
    index_lines += ["", "- [trench-cascade-trace-v1 format](trench-cascade-trace-format.md)"]
    io.open(OUT / "INDEX.md", "w", encoding="utf-8").write("\n".join(index_lines) + "\n")
    print(f"wrote {count} reference docs + format doc + index -> {OUT}")

if __name__ == "__main__":
    main()
