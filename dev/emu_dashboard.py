"""Self-contained Altair/Vega-Lite dashboard for the E-mu corpus.

Writes one HTML file with every chart's data embedded in the spec, so it
renders with no Python runtime. The marimo notebook (dev/emu_grammar.py) is
the interactive study environment; this is the static, always-renders view.
"""
import os
import math
import json
import numpy as np
import polars as pl
import altair as alt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "plotdata", "emu")
OUT = os.path.join(ROOT, "plots", "corpus", "emu_dashboard.html")
FS = 39062.5

alt.data_transformers.disable_max_rows()

sections = pl.read_parquet(os.path.join(DATA, "sections.parquet"))
states = pl.read_parquet(os.path.join(DATA, "states.parquet"))
subsets = pl.read_parquet(os.path.join(DATA, "subsets.parquet"))
scaffolds = pl.read_parquet(os.path.join(DATA, "scaffolds.parquet"))
lattice = pl.read_parquet(os.path.join(DATA, "lattice.parquet"))
edges = pl.read_parquet(os.path.join(DATA, "axis_edges.parquet"))

W = 900
INK = "#0b0b0b"


def titled(chart, t, sub=""):
    return chart.properties(
        title=alt.TitleParams(t, subtitle=sub, anchor="start",
                              fontSize=14, subtitleFontSize=10,
                              subtitleColor="#52514e", color=INK))


# ------------------------------------------------------------------ C: UpSet
up = (subsets.group_by("lane_mask", "n_lanes")
      .agg(pl.col("occurrences").sum().alias("occurrences"),
           pl.col("n_filters").max().alias("n_filters"),
           pl.col("state_sig").n_unique().alias("motifs"))
      .sort("occurrences", descending=True)
      .head(24)
      .with_columns(
          pl.col("lane_mask").map_elements(
              lambda m: "+".join(f"S{i+1}" for i, c in enumerate(m) if c == "1"),
              return_dtype=pl.Utf8).alias("lanes")))
order = up["lane_mask"].to_list()

c_bars = titled(
    alt.Chart(up).mark_bar(size=20, cornerRadiusTopLeft=3,
                           cornerRadiusTopRight=3).encode(
        x=alt.X("lane_mask:N", sort=order, axis=None),
        y=alt.Y("occurrences:Q", title="occurrences"),
        color=alt.Color("n_lanes:O", title="lanes",
                        scale=alt.Scale(scheme="blues")),
        tooltip=["lanes", "n_lanes", "occurrences", "n_filters", "motifs"],
    ).properties(width=W, height=230),
    "C — which lane combinations form the recurring grammar",
    "A motif is an exact set of section states in exact lanes, recurring in "
    "more than one filter. Bar = total occurrences.")

mrows = []
for r in up.iter_rows(named=True):
    for i, ch in enumerate(r["lane_mask"]):
        mrows.append({"lane_mask": r["lane_mask"], "lane": f"S{i+1}",
                      "on": ch == "1", "lanes": r["lanes"]})
c_dots = alt.Chart(pl.DataFrame(mrows)).mark_circle(size=120).encode(
    x=alt.X("lane_mask:N", sort=order, axis=None),
    y=alt.Y("lane:N", sort=[f"S{i+1}" for i in range(7)], title=None),
    color=alt.condition(alt.datum.on, alt.value("#2a78d6"),
                        alt.value("#e6e6e0")),
    tooltip=["lanes"],
).properties(width=W, height=150)

# ---------------------------------------------------------- A: primitives
top_states = (states.head(30)
              .with_columns(
                  (pl.col("pole_hz").round(0).cast(pl.Int64).cast(pl.Utf8)
                   + " Hz  r" + pl.col("pole_r").round(3).cast(pl.Utf8)
                   + "   zero " +
                   pl.when(pl.col("zero_r") > 0.02)
                   .then(pl.col("zero_hz").round(0).cast(pl.Int64).cast(pl.Utf8)
                         + " Hz  r" + pl.col("zero_r").round(3).cast(pl.Utf8))
                   .otherwise(pl.lit("off"))).alias("label")))
a_chart = titled(
    alt.Chart(top_states).mark_bar(cornerRadiusEnd=3).encode(
        y=alt.Y("label:N", sort="-x", title=None,
                axis=alt.Axis(labelFontSize=9, labelLimit=300)),
        x=alt.X("uses:Q", title="occurrences"),
        color=alt.Color("role:N", title="role",
                        scale=alt.Scale(scheme="tableau10")),
        tooltip=["label", "uses", "filters", "corpora", "role", "modal_lane",
                 "pole_hz", "pole_hz_ac", "pole_r", "zero_hz", "zero_r"],
    ).properties(width=W, height=520),
    "A — primitive recurrence: most reused exact section states",
    f"{states.height:,} distinct live states; {states.filter(pl.col('filters') > 1).height:,} "
    f"appear in more than one filter. Native Hz shown; de-warped acoustic Hz in tooltip.")

# ---------------------------------------------------------- B: top motifs
top_motifs = subsets.sort("occurrences", descending=True).head(30)
b_chart = titled(
    alt.Chart(top_motifs).mark_bar(cornerRadiusEnd=3, color="#2a78d6").encode(
        y=alt.Y("state_sig:N", sort="-x", title=None,
                axis=alt.Axis(labels=False)),
        x=alt.X("occurrences:Q", title="occurrences"),
        color=alt.Color("lanes:N", title="lanes"),
        tooltip=["lanes", "occurrences", "n_filters", "n_corpora", "examples"],
    ).properties(width=W, height=420),
    "B — the most reused multi-lane motifs",
    "Hover for the lanes and the filters that share them.")

# ------------------------------------------------------- D: scaffold swaps
sw = (scaffolds.with_columns(
        pl.when(pl.col("replaced_lane") == "")
        .then(pl.lit("lane added/removed"))
        .otherwise(pl.col("replaced_lane")).alias("swap"))
      .group_by("swap").agg(pl.len().alias("edges"))
      .sort("edges", descending=True))
d_chart = titled(
    alt.Chart(sw).mark_bar(cornerRadiusEnd=3, color="#eb6834").encode(
        y=alt.Y("swap:N", sort="-x", title="replaced lane"),
        x=alt.X("edges:Q", title="preset pairs"),
        tooltip=["swap", "edges"],
    ).properties(width=W, height=240),
    "D — scaffold mutation: which lane gets swapped",
    f"{scaffolds.height} preset pairs share >=4 identical section states in the "
    f"same lanes and differ in at most one lane. "
    f"{scaffolds.filter(pl.col('src_corpus') != pl.col('dst_corpus')).height} "
    f"of them cross the Morpheus/P2K boundary.")

d_table = titled(
    alt.Chart(scaffolds.head(40)).mark_circle(size=90).encode(
        x=alt.X("shared_lanes:Q", title="identical lanes",
                scale=alt.Scale(domain=[3, 7])),
        y=alt.Y("src_filter:N", title=None,
                axis=alt.Axis(labelFontSize=8, labelLimit=200)),
        color=alt.Color("replaced_lane:N", title="replaced"),
        tooltip=["src_filter", "dst_filter", "scaffold", "replaced_lane",
                 "shared_lanes"],
    ).properties(width=W, height=430),
    "D2 — the shared chassis, preset by preset", "")

# ------------------------------------------- E: measured lattice spacing
lat = lattice.with_columns(
    (pl.col("hi") / pl.col("lo")).log(2).alias("span_oct"))
e_chart = titled(
    alt.Chart(lat.filter(pl.col("span_oct") >= 1.0)).mark_circle(
        size=34, opacity=0.5).encode(
        x=alt.X("cv_df:Q", title="CV of Δf  (linear spacing)",
                scale=alt.Scale(type="sqrt", domain=[0, 1.4])),
        y=alt.Y("cv_do:Q", title="CV of Δo  (octave spacing)",
                scale=alt.Scale(type="sqrt", domain=[0, 1.4])),
        color=alt.Color("kind:N", title="measured",
                        scale=alt.Scale(
                            domain=["linear comb", "octave lattice",
                                    "measured, unassigned"],
                            range=["#eb6834", "#2a78d6", "#c3c2b7"])),
        shape=alt.Shape("extremum:N", title="extrema"),
        tooltip=["filter", "corner", "extremum", "n", "lo", "hi", "mean_df",
                 "cv_df", "mean_do", "cv_do", "kind"],
    ).properties(width=W, height=460),
    "E — extrema spacing, measured not named",
    "Each point is one cascade's extremum series (>=4 extrema, >=1 octave "
    "span). Bottom-right = constant octave ratio; top-left = constant Hz "
    "spacing. Nothing here is classified from a preset name.")

hist = titled(
    alt.Chart(lat.filter((pl.col("kind") == "octave lattice") &
                         (pl.col("span_oct") >= 1.0)))
    .mark_bar(color="#2a78d6").encode(
        x=alt.X("mean_do:Q", bin=alt.Bin(maxbins=60),
                title="mean Δo (octaves) of the octave lattices"),
        y=alt.Y("count():Q", title="series"),
    ).properties(width=W, height=220),
    "E2 — the ratios actually used", "")

# ------------------------------------------------- F: axis edge behaviour
mut = (edges.group_by("axis", "mutated").agg(pl.len().alias("n"))
       .join(edges.group_by("axis").agg(pl.len().alias("tot")), on="axis")
       .with_columns((100 * pl.col("n") / pl.col("tot")).alias("pct")))
f_chart = titled(
    alt.Chart(mut).mark_bar(cornerRadiusEnd=2).encode(
        x=alt.X("mutated:O", title="stages that changed across the edge"),
        y=alt.Y("pct:Q", title="% of that axis's edges"),
        color=alt.Color("axis:N", title="axis",
                        scale=alt.Scale(domain=["M", "Q", "T"],
                                        range=["#2a78d6", "#1baf7a", "#eb6834"])),
        xOffset="axis:N",
        tooltip=["axis", "mutated", "n", alt.Tooltip("pct:Q", format=".1f")],
    ).properties(width=W, height=280),
    "F — how much each axis rewrites the cascade",
    f"{edges.height} cube edges where both corners are authored. "
    f"Transform 2 has the highest single-stage substitution rate; Morph and Q "
    f"most often rewrite every stage at once.")

g_chart = titled(
    alt.Chart(edges.filter(pl.col("med_dst_p").is_not_null())).mark_circle(
        size=22, opacity=0.35).encode(
        x=alt.X("med_dst_p:Q", title="median pole shift across the edge (st)",
                scale=alt.Scale(domain=[-72, 72])),
        y=alt.Y("med_dr_p:Q", title="median pole radius change",
                scale=alt.Scale(domain=[-0.7, 0.7])),
        color=alt.Color("axis:N", title="axis",
                        scale=alt.Scale(domain=["M", "Q", "T"],
                                        range=["#2a78d6", "#1baf7a", "#eb6834"])),
        tooltip=["name", "axis", "src", "dst", "mutated", "med_dst_p",
                 "med_dr_p", "med_dst_z", "med_dr_z"],
    ).properties(width=W, height=420),
    "G — the axis operators themselves",
    "Each point is one cube edge. Vertical bands at ±24 and ±60 semitones are "
    "the discrete transposition quanta; the isolated ΔR ≈ ∓0.60 group is the "
    "S1-only radius operator.")

chart = alt.vconcat(
    c_bars, c_dots, a_chart, b_chart, d_chart, d_table, e_chart, hist,
    f_chart, g_chart
).resolve_scale(color="independent", shape="independent").properties(
    title=alt.TitleParams(
        "E-mu z-plane corpus — recurring modular grammar",
        subtitle=[
            f"{sections.height:,} section occurrences  ·  "
            f"{sections.filter(pl.col('live')).height:,} live  ·  "
            f"289 Morpheus cubes (14th order) + 33 P2K architectures "
            f"(12th order)  ·  native {FS:g} Hz datum",
            "Frequencies de-warped to continuous acoustic Hz by the inverse "
            "bilinear transform where marked. No topology is inferred from a "
            "preset name.",
        ],
        anchor="start", fontSize=19, subtitleFontSize=11,
        subtitleColor="#52514e")
).configure_view(strokeWidth=0).configure_axis(
    labelColor="#52514e", titleColor="#52514e", grid=True,
    gridColor="#e1e0d9")

chart.save(OUT)
raw = open(OUT, encoding="utf-8").read()
print(f"wrote {OUT}")
print(f"  bytes {len(raw):,}   embedded datasets {raw.count('datasets')}   "
      f"data rows {raw.count(chr(123) + chr(34))}")
for name, df in (("sections", sections), ("states", states),
                 ("subsets", subsets), ("scaffolds", scaffolds),
                 ("lattice", lattice)):
    print(f"  {name:10s} {df.height:6,} rows")
