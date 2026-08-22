from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "ref" / "morpheus_manual_zplane_descriptions.txt"
OUT = ROOT / "ref" / "morpheus_axis_grammar.json"

FURNITURE = re.compile(
    r"^(=== PDF page \d+ ===|Morpheus Operation Manual\s*\d*|"
    r"Z-PLANE FILTER DESCRIPTIONS|Chapter \d+: .*\d+|PRESET PROGRAMMING)\s*$")
FAMILY = re.compile(r"^[A-Z][A-Z /&'\-]{3,}$")
ENTRY = re.compile(r"^(F\d{3})[: ]\s*(.*)$")
FIELD = re.compile(r"^(Morph|Freq\. Tracking|Freq\. Track|Transform 2|Comments|"
                   r"Morph & Freq\. Tracking|Morph and Freq\. Tracking)\s*:\s*(.*)$")
KEY = {"Morph": "morph", "Freq. Tracking": "freq_tracking", "Freq. Track": "freq_tracking",
       "Transform 2": "transform2", "Comments": "comments",
       "Morph & Freq. Tracking": "morph_and_freq_tracking",
       "Morph and Freq. Tracking": "morph_and_freq_tracking"}


VOCAB: set[str] = set()


def join(parts):
    out = ""
    for p in parts:
        p = p.strip()
        if not p:
            continue
        if out.endswith("-") and p[:1].islower():
            head = re.split(r"[\s]", out)[-1][:-1]
            tail = re.split(r"[\s,.;:)]", p)[0]
            fused = (head + tail).lower()
            out = (out[:-1] + p) if fused in VOCAB else (out + p)
        elif out:
            out += " " + p
        else:
            out = p
    return out


def main() -> int:
    text = SRC.read_text(encoding="utf-8")
    VOCAB.update(w.lower() for w in re.findall(r"[A-Za-z]{3,}", text))
    lines = text.split("\n")
    entries, family, cur, field, buf = [], "", None, None, []

    def flush():
        if cur is not None and field is not None:
            cur[field] = join(buf)

    for i, raw in enumerate(lines):
        line = raw.rstrip()
        if FURNITURE.match(line):
            continue
        m = ENTRY.match(line)
        if m:
            flush()
            name = m.group(2).strip()
            cur = {"id": m.group(1), "name": name, "family": family,
                   "square": name.rstrip().endswith("4"), "line": i + 1,
                   "description": "", "morph": "", "freq_tracking": "",
                   "transform2": "", "comments": "", "morph_and_freq_tracking": ""}
            entries.append(cur)
            field, buf = "description", []
            continue
        if cur is None:
            if FAMILY.match(line):
                family = line.strip()
            continue
        f = FIELD.match(line)
        if f:
            flush()
            field, buf = KEY[f.group(1)], [f.group(2)]
            continue
        if FAMILY.match(line):
            flush()
            family, cur, field, buf = line.strip(), None, None, []
            continue
        buf.append(line)
    flush()

    OUT.write_text(json.dumps(entries, indent=1, ensure_ascii=False), encoding="utf-8")

    cubes = [e for e in entries if not e["square"]]
    squares = [e for e in entries if e["square"]]
    have = lambda es, k: sum(1 for e in es if e[k])
    print(f"{len(entries)} filters -> {OUT.relative_to(ROOT)}")
    print(f"  {len(cubes)} cubes, {len(squares)} squares")
    for k in ("description", "morph", "freq_tracking", "morph_and_freq_tracking",
              "transform2", "comments"):
        print(f"  {k:16} present on {have(entries, k):3d}/{len(entries)}"
              f"   (cubes {have(cubes, k):3d}/{len(cubes)})")
    axis = [e for e in cubes if e["transform2"] and not e["transform2"].lower().startswith("not used")]
    print(f"  cubes with a live Transform 2: {len(axis)}/{len(cubes)}")
    blind = [e for e in entries if not (e["morph"] or e["morph_and_freq_tracking"])]
    print(f"  no morph text at all: {len(blind)} -> " + ", ".join(e["id"] for e in blind))
    fams = {}
    for e in entries:
        fams[e["family"]] = fams.get(e["family"], 0) + 1
    print("  families: " + ", ".join(f"{k or '(none)'} {v}" for k, v in fams.items()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
