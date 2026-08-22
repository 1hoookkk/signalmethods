"""Decode every native 7-section body to geometry at its own datum and write one
file per filter so each can be analysed separately.

Datum is 39,062.5 Hz for this whole lineage.  Nothing here is pooled with the
44,100 Hz P2K bodies; the two are different machines and the geometry only means
anything against the right rate.
"""
import json, math, pathlib, struct, sys
import numpy as np
sys.path.insert(0, r"C:\Users\hooki\trench-native\out\build\windows-msvc-release\native\research")
import trench_native_research as core

SR = core.kMorpheusDatumHz
SECTIONS, CORNERS = 7, 8
IDENT = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)
SRC = pathlib.Path(r"C:\Users\hooki\trench-authoring\ref\morpheus\bodies")
SCRATCH = pathlib.Path(r"C:\WINDOWS\TEMP\claude\C--Users-hooki-trench-native\c1331e8e-ca6d-4249-8dd3-5d4540d80e20\scratchpad")
OUT = pathlib.Path(__file__).resolve().parents[1] / "ref/morpheus_decoded"

GRID = [20.0 * (0.49 * SR / 20.0) ** (i / 255.0) for i in range(256)]


def load_manual():
    try:
        names = json.load(open(SCRATCH / "up_filters.json"))
        fams = json.load(open(SCRATCH / "up_families.json"))
    except OSError:
        return {}, {}
    return {int(k): v for k, v in names.items()}, {int(k): v for k, v in fams.items()}


def root(d):
    if d["kind"] == "conjugate":
        return {"kind": "conjugate", "hz": round(d["hz"], 3), "radius": round(d["radius"], 9)}
    if d["kind"] == "real":
        return {"kind": "real", "root_a": round(d["root_a"], 9), "root_b": round(d["root_b"], 9)}
    return {"kind": "degenerate"}


def main():
    names, fams = load_manual()
    OUT.mkdir(parents=True, exist_ok=True)
    index = []
    for path in sorted(SRC.glob("*.body")):
        raw = path.read_bytes()
        if len(raw) != 560:
            continue
        try:
            num = int(path.stem.split("_")[0])
        except ValueError:
            num = None
        w = struct.unpack("<280H", raw)
        family = fams.get(num, "UNLISTED")
        manual_name = names.get(num)
        # square vs cube is NOT derivable from the 560 bytes: no corner-index bit
        # is flat for .4 filters (scratchpad/axis_probe.py -- 0/58 exact on every
        # bit, and the flattest-axis distribution does not separate the two
        # labels).  The property lives in the instrument's filter table.  The
        # manual name suffix is therefore the only authority, and a body with no
        # manual entry is "unknown", never "cube".
        if manual_name is None:
            geometry = "unknown"
        elif manual_name.rstrip().endswith(".4") or manual_name.rstrip().endswith(" 4"):
            geometry = "square"
        else:
            geometry = "cube"

        corners, active_any = [], set()
        for c in range(CORNERS):
            words = [list(w[c * 35 + s * 5:c * 35 + s * 5 + 5]) for s in range(SECTIONS)]
            secs = []
            for s, row in enumerate(words):
                g = core.geometry(row, SR)
                db = np.asarray(core.section_db(row, GRID, SR))
                span = float(db.max() - db.min()) if np.all(np.isfinite(db)) else 0.0
                if span > 0.5:
                    active_any.add(s)
                secs.append({
                    "section": s + 1,
                    "words": row,
                    "hex": " ".join(f"{v:04X}" for v in row),
                    "identity": tuple(row) == IDENT,
                    "pole": root(g["pole"]),
                    "zero": root(g["zero"]),
                    "scale": round(g["scale"], 9),
                    "scale_db": round(20.0 * math.log10(max(g["scale"], 1e-12)), 3),
                    "span_db": round(span, 3),
                })
            flat = [v for row in words for v in row]
            total = np.asarray(core.cascade_db(flat, GRID, SR))
            corners.append({
                "corner": c,
                "sections": secs,
                "response_db": [round(float(v), 4) for v in total],
            })

        doc = {
            "schema": "trench-native-decoded-v1",
            "source_file": path.name,
            "filter_number": num,
            "manual_name": manual_name,
            "family": family,
            "geometry": geometry,
            "geometry_source": "manual name suffix" if manual_name else "not determined",
            "datum_hz": SR,
            "sections": SECTIONS,
            "corners": CORNERS,
            "active_sections": len(active_any),
            "frequency_grid_hz": [round(f, 4) for f in GRID],
            "corner_data": corners,
        }
        sub = OUT / family.lower().replace(" ", "_")
        sub.mkdir(parents=True, exist_ok=True)
        (sub / f"{path.stem}.json").write_text(json.dumps(doc, indent=1))
        index.append({
            "filter_number": num,
            "source_file": path.name,
            "manual_name": manual_name,
            "family": family,
            "geometry": geometry,
            "active_sections": len(active_any),
            "path": f"{sub.name}/{path.stem}.json",
        })

    (OUT / "index.json").write_text(json.dumps(
        {"schema": "trench-native-decoded-index-v1", "datum_hz": SR,
         "sections": SECTIONS, "corners": CORNERS, "count": len(index),
         "filters": index}, indent=1))
    print(f"wrote {len(index)} decoded filters to {OUT}")
    by_fam = {}
    for row in index:
        by_fam.setdefault(row["family"], []).append(row)
    for fam, rows in sorted(by_fam.items()):
        sq = sum(1 for r in rows if r["geometry"] == "square")
        cu = sum(1 for r in rows if r["geometry"] == "cube")
        un = sum(1 for r in rows if r["geometry"] == "unknown")
        print(f"  {fam:<22} {len(rows):>4}   square {sq:>3}   cube {cu:>3}   unknown {un:>3}")


if __name__ == "__main__":
    main()
