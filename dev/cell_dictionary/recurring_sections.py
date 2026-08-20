"""
Recurring sections and combinations-of-sections, across ALL of: the 33 P2K
presets (240-byte, 6-stage), the 560-byte 7-stage 'cube' family
(ref/cubes/*.body, recipes/hero/*.body|*.cube), AND the raw 289-cube
Morpheus corpus (ref/morpheus/records_stream.bin, 7-stage) -- one combined
census, not three separate ones, so a section or combination that recurs
across families is found too.

Matching happens in GEOMETRY space (Hz / radius / scale-dB), each object
decoded at ITS OWN correct sample-rate datum, not in raw-word space --
raw words are datum-relative, so two objects encoding the SAME physical
frequency at different datums have different words, and word-identity
would both miss real recurrence and manufacture false recurrence between
differently-datumed files. See the datum table below; anything not
established by this repository's own evidence is marked ASSUMED, not
VERIFIED, and is reported as such.

No movement/held/moved framing here at all -- this is purely: does this
exact (tolerance-quantized to the project's own documented packed-grid
precision) section, or exact combination of sections, occur more than
once anywhere in the corpus.

Read-only against the repository. Writes only under
dev/cell_dictionary/output/.
"""
import glob
import json
import os
import re
from collections import Counter, defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

import decode_lib as dl

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output")
os.makedirs(OUT_DIR, exist_ok=True)

ANALYSIS_BAND_LO_HZ = 40.0
ANALYSIS_BAND_HI_HZ = 18_000.0
FREQS = dl.log_grid_hz(ANALYSIS_BAND_LO_HZ, ANALYSIS_BAND_HI_HZ, 400)

# ---------------------------------------------------------------------------
# significance filter -- excludes sections/combinations with no audible
# feature before they ever enter the recurrence census, so junk/near-flat
# states can't dominate a ranking. Every threshold is a named constant;
# where a range or no number was given, the choice made here is flagged
# explicitly rather than picked silently.
# ---------------------------------------------------------------------------
MIN_PTP_DB = 1.0          # audible-band peak-to-peak magnitude floor
MIN_POLE_RADIUS = 0.6     # midpoint of the given ~0.5-0.7 range; ASSUMPTION
NYQUIST_GUARD_FRAC = 0.95  # feature frequency within 5% of Nyquist -> excluded; ASSUMPTION
CASCADE_RMS_MIN_DB = 1.0  # cascade RMS deviation from unity floor; ASSUMPTION (no
                           # number was given for this one, reused the section ptp floor)


def section_is_significant(geom, datum_sr):
    if dl.stage_is_identity(geom):
        return False, "degenerate"
    nyquist = datum_sr / 2.0
    for root, label in ((geom.pole, "pole"), (geom.zero, "zero")):
        if isinstance(root, dl.Conjugate) and root.hz > NYQUIST_GUARD_FRAC * nyquist:
            return False, f"{label}_near_nyquist"
    if isinstance(geom.pole, dl.Conjugate) and geom.pole.r < MIN_POLE_RADIUS:
        return False, "pole_radius_too_low"
    band_touched = False
    for root, label in ((geom.pole, "pole"), (geom.zero, "zero")):
        if isinstance(root, dl.Conjugate) and ANALYSIS_BAND_LO_HZ <= root.hz <= ANALYSIS_BAND_HI_HZ:
            band_touched = True
        elif isinstance(root, dl.RealPair):
            band_touched = True  # real roots have no single Hz; judged by ptp below instead
    if not band_touched:
        return False, "feature_outside_analysis_band"
    if isinstance(geom.pole, dl.Conjugate) and geom.pole.hz < 250 and geom.pole.r > 0.998:
        if isinstance(geom.zero, dl.Degenerate):
            return False, "structural_dc_shelf"
    bq = dl.stage_biquad(geom if geom.scale is not None else dl.StageGeometry(geom.pole, geom.zero, 1.0), datum_sr)
    db = dl.stage_response_db(bq, FREQS, datum_sr)
    ptp = max(db) - min(db)
    if ptp < MIN_PTP_DB:
        return False, "ptp_below_threshold"
    return True, "ok"


def cascade_rms_db(stage_geoms, datum_sr):
    total = [0.0] * len(FREQS)
    for g in stage_geoms:
        gg = g if g.scale is not None else dl.StageGeometry(g.pole, g.zero, 1.0)
        bq = dl.stage_biquad(gg, datum_sr)
        db = dl.stage_response_db(bq, FREQS, datum_sr)
        total = [a + b for a, b in zip(total, db)]
    return dl.rms(total)

# ---------------------------------------------------------------------------
# datum table (see module docstring)
# ---------------------------------------------------------------------------
SR_39K = 39_062.5   # VERIFIED: P2K legacy decode datum, Morpheus raw-stream decode datum
SR_44K = 44_100.0   # VERIFIED as trench-core::compiler::DEFAULT_AUTHORING_SR; ASSUMED as the
                     # actual encode datum of the untraced 560-byte cube family below


class Source:
    def __init__(self, family, name, path, datum_sr, datum_status, n_corners, n_stages):
        self.family = family
        self.name = name
        self.path = path
        self.datum_sr = datum_sr
        self.datum_status = datum_status
        self.n_corners = n_corners
        self.n_stages = n_stages
        self.corners = []  # list[list[StageGeometry]]

    def corner_label(self, ci):
        if self.n_corners == 4:
            return dl.P2K_CORNER_LABELS[ci]
        if self.n_corners == 8:
            z, m, q = ci & 1, (ci >> 1) & 1, (ci >> 2) & 1
            return f"M{m*100}_Q{q*100}_Z{z*100}"
        return str(ci)


def load_p2k_sources():
    sources = []
    for path in sorted(glob.glob(os.path.join(REPO_ROOT, "ref", "presets", "P2k_*.bin"))):
        base = os.path.basename(path)
        m = re.match(r"P2k_(\d+)_(.+)\.bin$", base)
        name = m.group(2)
        with open(path, "rb") as f:
            data = f.read()
        src = Source("p2k", name, path, SR_39K, "VERIFIED", 4, 6)
        src.corners = dl.decode_p2k_body(data, SR_39K)
        sources.append(src)
    return sources


def load_cube_family_sources():
    """ref/cubes/*.body and recipes/hero/*.body|*.cube. Datum is UNKNOWN
    (no generator located in this repository's own evidence pass); this
    script ASSUMES 240-byte files use the legacy 39,062.5 Hz datum and
    560-byte files use the native-format authoring default of 44,100 Hz,
    per this project's own stated convention (CLAUDE.md, 'The object':
    'Native bodies ... The legacy corpus ... The authoring default is
    44,100 Hz'). This is a documented ASSUMPTION, not a verified fact."""
    sources = []
    paths = sorted(glob.glob(os.path.join(REPO_ROOT, "ref", "cubes", "*.body")))
    paths += sorted(glob.glob(os.path.join(REPO_ROOT, "recipes", "hero", "*.body")))
    paths += sorted(glob.glob(os.path.join(REPO_ROOT, "recipes", "hero", "*.cube")))
    for path in paths:
        name = os.path.splitext(os.path.basename(path))[0]
        with open(path, "rb") as f:
            data = f.read()
        if len(data) == dl.LEGACY_BODY_BYTES:
            src = Source("cube_legacy", name, path, SR_39K, "ASSUMED", 4, 6)
            src.corners = dl.decode_p2k_body(data, SR_39K)
            sources.append(src)
        elif len(data) == dl.BODY_BYTES:
            src = Source("cube_native", name, path, SR_44K, "ASSUMED", 8, 7)
            src.corners = dl.decode_native_body(data, SR_44K)
            sources.append(src)
        else:
            print(f"skipping {path}: unrecognized size {len(data)}")
    return sources


def load_morpheus_sources():
    records = dl.decode_all_morpheus_records_v2(os.path.join(REPO_ROOT, "ref", "morpheus", "records_stream.bin"))
    sources = []
    for idx, rec in enumerate(records):
        src = Source("morpheus", rec["name"], f"records_stream.bin#{idx}", SR_39K, "VERIFIED", 8, 7)
        corners = []
        for corner in rec["corners"]:
            stages = []
            for sec in corner["sections"]:
                pr, zr = sec["pole"]["r"], sec["zero"]["r"]
                raw = sec["raw"]
                is_null = (raw[0] == raw[2] and raw[1] == raw[3]) or (pr <= 1e-6 and zr <= 1e-6)
                pole = dl.Degenerate() if (is_null or pr <= 1e-6) else dl.Conjugate(sec["pole"]["hz"], pr)
                zero = dl.Degenerate() if (is_null or zr <= 1e-6) else dl.Conjugate(sec["zero"]["hz"], zr)
                stages.append(dl.StageGeometry(pole=pole, zero=zero, scale=None))
            corners.append(stages)
        src.corners = corners
        sources.append(src)
    return sources


# ---------------------------------------------------------------------------
# single-section recurrence
# ---------------------------------------------------------------------------

def census_sections(sources):
    occ = defaultdict(list)  # canonical_key (pole,zero only -- scale reported separately) -> [(src,ci,si)]
    reject_counts = Counter()
    for src in sources:
        for ci, stages in enumerate(src.corners):
            for si, g in enumerate(stages):
                sig, reason = section_is_significant(g, src.datum_sr)
                if not sig:
                    reject_counts[reason] += 1
                    continue
                pole_key, zero_key, _scale_key = dl.canonical_stage_key(g)
                key = (pole_key, zero_key)
                occ[key].append((src, ci, si))
    print(f"  section significance rejections: {dict(reject_counts)}")
    return occ


def _with_scale(geom):
    """For response plotting only: Morpheus sections carry no per-section
    scale (gain is corner-level there), so default to unity for the
    purpose of drawing this ONE section's own resonant shape. Never used
    for the canonical matching key (canonical_stage_key correctly leaves
    scale_key=None for these, so they never match on scale)."""
    if geom.scale is None:
        return dl.StageGeometry(pole=geom.pole, zero=geom.zero, scale=1.0)
    return geom


def plot_top_sections(occ, out_path, n_panels=6, grid=(3, 2), title_suffix=""):
    multi = {k: v for k, v in occ.items() if len(v) > 1}
    ranked = sorted(multi.items(), key=lambda kv: len(kv[1]), reverse=True)

    fig, axes = plt.subplots(*grid, figsize=(14, 12))
    axes = axes.flatten()
    for i in range(min(n_panels, len(ranked))):
        key, locs = ranked[i]
        ax = axes[i]
        src0, ci0, si0 = locs[0]
        g0 = src0.corners[ci0][si0]
        bq = dl.stage_biquad(_with_scale(g0), src0.datum_sr)
        db = dl.stage_response_db(bq, FREQS, src0.datum_sr)
        ax.plot(FREQS, db, color="#1f77b4", lw=2)

        families = sorted(set(l[0].family for l in locs))
        names = sorted(set(l[0].name for l in locs))
        datum_flags = sorted(set(f"{l[0].datum_sr:.0f}Hz({l[0].datum_status})" for l in locs))

        def root_str(p):
            if isinstance(p, dl.Conjugate):
                return f"{p.hz:.0f}Hz r={p.r:.4f}"
            if isinstance(p, dl.RealPair):
                return f"real(a={p.root_a:.4f},b={p.root_b:.4f})"
            return "degenerate"

        title = (f"Rank #{i+1}: {len(locs)} occurrences across {len(names)} objects, "
                 f"{len(families)} families\n"
                 f"P:{root_str(g0.pole)}  Z:{root_str(g0.zero)}  gain={20*__import__('math').log10(max(g0.scale or 1.0,1e-9)):.2f}dB\n"
                 f"datum: {', '.join(datum_flags)}\n"
                 f"objects: {', '.join(names[:5])}{' ...' if len(names) > 5 else ''}")
        ax.set_title(title, fontsize=7.8)
        ax.set_xscale("log")
        ax.grid(True, which="both", alpha=0.3)
        ax.set_ylim(-40, 40)
        ax.axhline(0, color="gray", lw=0.5, ls=":")

    for j in range(min(n_panels, len(ranked)), len(axes)):
        axes[j].axis("off")

    fig.suptitle(f"Top {n_panels} Recurring Sections{title_suffix}\n"
                 f"(each object decoded at its own datum; distinct raw words at different "
                 f"datums CAN represent this same geometric section)",
                 fontsize=12, fontweight="bold")
    plt.tight_layout(rect=[0, 0, 1, 0.88])
    plt.savefig(out_path, dpi=180)
    plt.close()
    print(f"wrote {out_path}  ({len(ranked)} distinct recurring sections, "
          f"{len(occ)} total distinct active sections)")
    return ranked


# ---------------------------------------------------------------------------
# combination-of-sections recurrence (whole corner, active sections only,
# as a canonical multiset -- independent of total container stage count so
# a 6-stage and 7-stage corner CAN match if their active content matches)
# ---------------------------------------------------------------------------

def significant_stages(src, ci):
    """The stages of one corner that pass the significance filter --
    this is what 'active content' means for combination-matching, not
    merely non-identity."""
    return [g for g in src.corners[ci] if section_is_significant(g, src.datum_sr)[0]]


def canonical_corner_combo(src, ci):
    active_keys = [dl.canonical_stage_key(g) for g in significant_stages(src, ci)]
    active_keys.sort()
    return tuple(active_keys)


def census_combinations(sources, min_active=2):
    occ = defaultdict(list)
    reject_counts = Counter()
    for src in sources:
        for ci in range(len(src.corners)):
            combo = canonical_corner_combo(src, ci)
            if len(combo) < min_active:
                reject_counts["too_few_significant_stages"] += 1
                continue
            stages = significant_stages(src, ci)
            if cascade_rms_db(stages, src.datum_sr) < CASCADE_RMS_MIN_DB:
                reject_counts["cascade_rms_below_threshold"] += 1
                continue
            occ[combo].append((src, ci))
    print(f"  combination significance rejections: {dict(reject_counts)}")
    return occ


def plot_top_combinations(occ, out_path, n_panels=6, grid=(3, 2)):
    multi = {k: v for k, v in occ.items() if len(v) > 1}
    ranked = sorted(multi.items(), key=lambda kv: len(kv[1]), reverse=True)

    fig, axes = plt.subplots(*grid, figsize=(14, 12))
    axes = axes.flatten()
    for i in range(min(n_panels, len(ranked))):
        combo, locs = ranked[i]
        ax = axes[i]

        colors = plt.get_cmap("tab10")
        for j, (src, ci) in enumerate(locs[:8]):
            stages = significant_stages(src, ci)
            total_db = [0.0] * len(FREQS)
            for g in stages:
                bq = dl.stage_biquad(_with_scale(g), src.datum_sr)
                db = dl.stage_response_db(bq, FREQS, src.datum_sr)
                total_db = [a + b for a, b in zip(total_db, db)]
            label = f"{src.name}.{src.corner_label(ci)}"
            ax.plot(FREQS, total_db, color=colors(j % 10), lw=1.6, alpha=0.85, label=label)

        names = sorted(set(l[0].name for l in locs))
        title = (f"Rank #{i+1}: {len(combo)}-section combination, {len(locs)} occurrences "
                 f"across {len(names)} objects\nobjects: {', '.join(names[:6])}"
                 f"{' ...' if len(names) > 6 else ''}")
        ax.set_title(title, fontsize=8)
        ax.set_xscale("log")
        ax.grid(True, which="both", alpha=0.3)
        ax.set_ylim(-60, 40)
        ax.axhline(0, color="gray", lw=0.5, ls=":")
        ax.legend(loc="lower left", fontsize=6.5)

    for j in range(min(n_panels, len(ranked)), len(axes)):
        axes[j].axis("off")

    fig.suptitle("Top Recurring Combinations of Sections (whole active-corner content, "
                 "cascade response)", fontsize=12, fontweight="bold")
    plt.tight_layout(rect=[0, 0, 1, 0.92])
    plt.savefig(out_path, dpi=180)
    plt.close()
    print(f"wrote {out_path}  ({len(ranked)} distinct recurring combinations, "
          f"{len(occ)} total distinct combinations with >=2 active sections)")
    return ranked


def main():
    p2k_sources = load_p2k_sources()
    cube_sources = load_cube_family_sources()
    morph_sources = load_morpheus_sources()

    print("=== datum table ===")
    for label, srcs in [("P2K", p2k_sources), ("cube family", cube_sources), ("Morpheus", morph_sources)]:
        statuses = sorted(set((s.datum_sr, s.datum_status) for s in srcs))
        print(f"  {label}: {len(srcs)} objects, datum(s): {statuses}")

    all_sources = p2k_sources + cube_sources + morph_sources
    print(f"\ntotal sources: {len(all_sources)} "
          f"(p2k={len(p2k_sources)}, cube_family={len(cube_sources)}, morpheus={len(morph_sources)})")

    sec_occ = census_sections(all_sources)
    plot_top_sections(sec_occ, os.path.join(OUT_DIR, "recurring_sections_all_families.png"),
                       n_panels=6, grid=(3, 2),
                       title_suffix=" -- P2K (6-stage) + cube family (7-stage) + Morpheus (7-stage)")

    combo_occ = census_combinations(all_sources, min_active=2)
    plot_top_combinations(combo_occ, os.path.join(OUT_DIR, "recurring_combinations_all_families.png"),
                           n_panels=6, grid=(3, 2))

    # also split Morpheus out on its own, since it dwarfs the other two
    # families in count and could otherwise swamp every ranking
    morph_sec_occ = census_sections(morph_sources)
    plot_top_sections(morph_sec_occ, os.path.join(OUT_DIR, "recurring_sections_morpheus_only.png"),
                       n_panels=6, grid=(3, 2), title_suffix=" -- Morpheus only")
    morph_combo_occ = census_combinations(morph_sources, min_active=2)
    plot_top_combinations(morph_combo_occ, os.path.join(OUT_DIR, "recurring_combinations_morpheus_only.png"),
                           n_panels=6, grid=(3, 2))

    p2k_cube_sec_occ = census_sections(p2k_sources + cube_sources)
    plot_top_sections(p2k_cube_sec_occ, os.path.join(OUT_DIR, "recurring_sections_p2k_cube_only.png"),
                       n_panels=6, grid=(3, 2), title_suffix=" -- P2K + cube family only")
    p2k_cube_combo_occ = census_combinations(p2k_sources + cube_sources, min_active=2)
    plot_top_combinations(p2k_cube_combo_occ, os.path.join(OUT_DIR, "recurring_combinations_p2k_cube_only.png"),
                           n_panels=6, grid=(3, 2))

    summary = {
        "datum_table": {
            "p2k": {"datum_hz": SR_39K, "status": "VERIFIED"},
            "cube_family_240B": {"datum_hz": SR_39K, "status": "ASSUMED"},
            "cube_family_560B": {"datum_hz": SR_44K, "status": "ASSUMED"},
            "morpheus_raw_stream": {"datum_hz": SR_39K, "status": "VERIFIED"},
        },
        "counts": {
            "all_families": {"distinct_sections": len(sec_occ),
                              "recurring_sections": len([1 for v in sec_occ.values() if len(v) > 1]),
                              "distinct_combinations": len(combo_occ),
                              "recurring_combinations": len([1 for v in combo_occ.values() if len(v) > 1])},
            "morpheus_only": {"distinct_sections": len(morph_sec_occ),
                               "recurring_sections": len([1 for v in morph_sec_occ.values() if len(v) > 1]),
                               "distinct_combinations": len(morph_combo_occ),
                               "recurring_combinations": len([1 for v in morph_combo_occ.values() if len(v) > 1])},
            "p2k_cube_only": {"distinct_sections": len(p2k_cube_sec_occ),
                               "recurring_sections": len([1 for v in p2k_cube_sec_occ.values() if len(v) > 1]),
                               "distinct_combinations": len(p2k_cube_combo_occ),
                               "recurring_combinations": len([1 for v in p2k_cube_combo_occ.values() if len(v) > 1])},
        },
    }
    with open(os.path.join(OUT_DIR, "recurring_sections_summary.json"), "w") as f:
        json.dump(summary, f, indent=1)
    print(f"\nwrote outputs to {OUT_DIR}")


if __name__ == "__main__":
    main()
