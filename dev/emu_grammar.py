import marimo

__generated_with = "0.23.16"
app = marimo.App(width="full")


@app.cell
def _():
    import os
    import math
    import marimo as mo
    import numpy as np
    import polars as pl
    import altair as alt
    import networkx as nx

    ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    DATA = os.path.join(ROOT, "plotdata", "emu")
    FS = 39062.5

    sections = pl.read_parquet(os.path.join(DATA, "sections.parquet"))
    states = pl.read_parquet(os.path.join(DATA, "states.parquet"))
    subsets = pl.read_parquet(os.path.join(DATA, "subsets.parquet"))
    scaffolds = pl.read_parquet(os.path.join(DATA, "scaffolds.parquet"))

    alt.data_transformers.disable_max_rows()
    return DATA, FS, alt, math, mo, np, nx, pl, scaffolds, sections, states, subsets


@app.cell
def _(mo, scaffolds, sections, states, subsets):
    mo.md(
        f"""
        # E-mu z-plane grammar

        One row per section occurrence; every frequency at the native
        **39,062.5 Hz** datum, de-warped to continuous acoustic Hz by the
        inverse bilinear transform. Binary contract verified upstream —
        this notebook studies structure, not bytes.

        | table | rows | meaning |
        |---|---|---|
        | `sections` | {sections.height:,} | one section occurrence |
        | `states` | {states.height:,} | distinct live section states |
        | `subsets` | {subsets.height:,} | lane combinations recurring across filters |
        | `scaffolds` | {scaffolds.height:,} | "N shared + 1 replaced" edges |

        Live sections: **{sections.filter(sections['live']).height:,}** of
        {sections.height:,} — the rest are sentinels, self-cancelling pairs
        and idle-pole filler that cannot affect the response.
        """
    )
    return


@app.cell
def _(alt, mo, pl, subsets):
    upset_src = (
        subsets.group_by("lane_mask", "n_lanes", "contiguous")
        .agg(pl.col("occurrences").sum().alias("occurrences"),
             pl.col("n_filters").max().alias("n_filters"),
             pl.col("state_sig").n_unique().alias("distinct_motifs"))
        .sort("occurrences", descending=True)
        .head(24)
        .with_columns(pl.col("lane_mask").alias("mask"))
    )
    upset_order = upset_src["mask"].to_list()

    upset = mo.ui.altair_chart(
        alt.Chart(upset_src)
        .mark_bar(size=18, cornerRadiusTopLeft=3, cornerRadiusTopRight=3)
        .encode(
            x=alt.X("mask:N", sort=upset_order, axis=None),
            y=alt.Y("occurrences:Q", title="occurrences"),
            color=alt.Color("n_lanes:O", title="lanes",
                            scale=alt.Scale(scheme="blues")),
            tooltip=["mask", "n_lanes", "occurrences", "n_filters",
                     "distinct_motifs"],
        )
        .properties(height=230, width=780,
                    title="C — which lane combinations form the recurring "
                          "grammar   (click a bar)")
    )
    upset
    return upset, upset_order, upset_src


@app.cell
def _(alt, pl, upset_order, upset_src):
    _rows = []
    for _r in upset_src.iter_rows(named=True):
        for _i, _ch in enumerate(_r["lane_mask"]):
            _rows.append({"mask": _r["mask"], "lane": f"S{_i+1}",
                          "on": _ch == "1"})
    (alt.Chart(pl.DataFrame(_rows))
     .mark_circle(size=115)
     .encode(
         x=alt.X("mask:N", sort=upset_order, axis=None),
         y=alt.Y("lane:N", sort=[f"S{i+1}" for i in range(7)], title=None),
         color=alt.condition(alt.datum.on, alt.value("#2a78d6"),
                             alt.value("#e6e6e0")),
         tooltip=["mask", "lane"],
     )
     .properties(height=155, width=780))
    return


@app.cell
def _(mo, pl, subsets, upset):
    def _picked(sel, col):
        try:
            if sel is None or len(sel) == 0:
                return None
            v = sel[col]
            v = v.to_list() if hasattr(v, "to_list") else list(v)
            return str(v[0]) if v else None
        except Exception:
            return None

    picked_mask = _picked(upset.value, "mask")
    motifs = (subsets.filter(pl.col("lane_mask") == picked_mask)
              if picked_mask else subsets).sort("occurrences", descending=True)
    mo.md(f"**Selected lane combination:** "
          f"`{picked_mask or 'all lanes (click a bar above)'}` — "
          f"{motifs.height:,} distinct motifs, "
          f"{int(motifs['occurrences'].sum()):,} occurrences")
    return motifs, picked_mask


@app.cell
def _(alt, mo, motifs):
    _top = motifs.head(30).with_columns(
        (motifs.head(30)["lanes"] + "  ×" +
         motifs.head(30)["occurrences"].cast(str)).alias("label"))
    combo = mo.ui.altair_chart(
        alt.Chart(_top)
        .mark_bar(cornerRadiusEnd=3, color="#2a78d6")
        .encode(
            y=alt.Y("state_sig:N", sort="-x", axis=alt.Axis(labels=False,
                                                            title=None)),
            x=alt.X("occurrences:Q", title="occurrences"),
            tooltip=["lanes", "occurrences", "n_filters", "n_corpora",
                     "examples"],
        )
        .properties(height=420, width=760,
                    title="B — recurring motifs inside the selected combination")
    )
    combo
    return (combo,)


@app.cell
def _(FS, alt, combo, math, mo, motifs, np, pl, sections):
    def _first(sel, col):
        try:
            if sel is None or len(sel) == 0:
                return None
            v = sel[col]
            v = v.to_list() if hasattr(v, "to_list") else list(v)
            return str(v[0]) if v else None
        except Exception:
            return None

    _sig = _first(combo.value, "state_sig") or (
        motifs["state_sig"][0] if motifs.height else None)
    _row = motifs.filter(pl.col("state_sig") == _sig) if _sig else motifs.head(0)

    def _members(sig):
        if not sig:
            return sections.head(0)
        keys = sig.split("|")
        hits = sections.filter(pl.col("state_key").is_in(keys) & pl.col("live"))
        return hits

    members = _members(_sig)

    _grid = np.geomspace(40.0, 16000.0, 700)
    _w = 2 * np.pi * _grid / FS

    def _sec_db(fp, rp, fz, rz):
        e1, e2 = np.exp(-1j * _w), np.exp(-2j * _w)
        num = 1 - 2 * rz * math.cos(2 * math.pi * fz / FS) * e1 + rz ** 2 * e2
        den = 1 - 2 * rp * math.cos(2 * math.pi * fp / FS) * e1 + rp ** 2 * e2
        return 20 * np.log10(np.maximum(np.abs(num), 1e-30) /
                             np.maximum(np.abs(den), 1e-30))

    _keys = _sig.split("|") if _sig else []
    _uniq = []
    for _k in _keys:
        _m = members.filter(pl.col("state_key") == _k)
        if _m.height:
            _uniq.append(_m.row(0, named=True))

    _rows = []
    _total = np.zeros_like(_grid)
    for _i, _s in enumerate(_uniq):
        _db = _sec_db(_s["pole_hz"], _s["pole_r"], _s["zero_hz"], _s["zero_r"])
        _total = _total + _db
        for _f, _v in zip(_grid[::4], _db[::4]):
            _rows.append({"hz": float(_f), "db": float(_v),
                          "trace": f'{_s["lane"]} factor'})
    for _f, _v in zip(_grid[::4], _total[::4]):
        _rows.append({"hz": float(_f), "db": float(_v), "trace": "product"})

    mag = (alt.Chart(pl.DataFrame(_rows))
           .mark_line()
           .encode(
               x=alt.X("hz:Q", scale=alt.Scale(type="log", domain=[40, 16000]),
                       title="Hz"),
               y=alt.Y("db:Q", scale=alt.Scale(domain=[-60, 40]), title="dB"),
               color=alt.Color("trace:N", title=None),
               size=alt.condition(alt.datum.trace == "product",
                                  alt.value(2.6), alt.value(1.0)),
           )
           .properties(height=280, width=470,
                       title="E — the motif as a product, not as bands"))

    _pz = []
    for _s in _uniq:
        if _s["pole_r"] > 0.02:
            _pz.append({"hz": _s["pole_hz_ac"], "rp": _s["pole_r"],
                        "kind": "pole", "lane": _s["lane"]})
        if _s["zero_r"] > 0.02:
            _pz.append({"hz": _s["zero_hz_ac"], "rp": _s["zero_r"],
                        "kind": "zero", "lane": _s["lane"]})
    _pzdf = pl.DataFrame(_pz) if _pz else pl.DataFrame(
        {"hz": [0.0], "rp": [0.0], "kind": ["pole"], "lane": ["S1"]})
    roots = (alt.Chart(_pzdf)
             .mark_point(size=140, filled=False, strokeWidth=2)
             .encode(
                 x=alt.X("hz:Q", scale=alt.Scale(type="log",
                                                 domain=[40, 20000]),
                         title="acoustic Hz (de-warped)"),
                 y=alt.Y("rp:Q", scale=alt.Scale(domain=[0, 1.02]),
                         title="radius"),
                 shape=alt.Shape("kind:N", title=None),
                 color=alt.Color("lane:N", title="lane"),
                 tooltip=["lane", "kind", "hz", "rp"],
             )
             .properties(height=280, width=470,
                         title="pole / zero geometry of the motif"))

    mo.hstack([roots, mag])
    return (members, mag, roots)


@app.cell
def _(members, mo, pl):
    mo.vstack([
        mo.md("**Every filter that contains this motif**"),
        (members.group_by("corpus", "filter")
         .agg(pl.len().alias("sections"),
              pl.col("corner").n_unique().alias("corners"),
              pl.col("lane").unique().sort().str.join("+").alias("lanes"))
         .sort("sections", descending=True))
    ])
    return


@app.cell
def _(alt, mo, pl, states):
    prim = mo.ui.altair_chart(
        alt.Chart(states.head(40))
        .mark_bar(cornerRadiusEnd=3)
        .encode(
            y=alt.Y("state_key:N", sort="-x", title=None,
                    axis=alt.Axis(labelLimit=220, labelFontSize=8)),
            x=alt.X("uses:Q", title="occurrences"),
            color=alt.Color("role:N", title="role",
                            scale=alt.Scale(scheme="tableau10")),
            tooltip=["state_key", "uses", "filters", "corpora", "role",
                     "pole_hz", "pole_hz_ac", "pole_r",
                     "zero_hz", "zero_hz_ac", "zero_r"],
        )
        .properties(height=560, width=760,
                    title="A — primitive recurrence: the most reused exact "
                          "section states")
    )
    prim
    return (prim,)


@app.cell
def _(mo, pl, prim, sections):
    def _one(sel, col):
        try:
            if sel is None or len(sel) == 0:
                return None
            v = sel[col]
            v = v.to_list() if hasattr(v, "to_list") else list(v)
            return str(v[0]) if v else None
        except Exception:
            return None

    _k = _one(prim.value, "state_key")
    mo.vstack([
        mo.md(f"**Where `{_k or '(click a bar)'}` occurs**"),
        (sections.filter(pl.col("state_key") == _k)
         .group_by("corpus", "filter")
         .agg(pl.len().alias("uses"),
              pl.col("lane").unique().sort().str.join("+").alias("lanes"),
              pl.col("corner").unique().sort().cast(pl.List(pl.Utf8))
              .list.join(",").alias("corners"))
         .sort("uses", descending=True)
         if _k else sections.head(0))
    ])
    return


@app.cell
def _(alt, mo, nx, pl, scaffolds):
    _g = nx.Graph()
    for _r in scaffolds.iter_rows(named=True):
        _g.add_edge(_r["src_filter"], _r["dst_filter"],
                    lane=_r["replaced_lane"], shared=_r["shared_lanes"])
    _pos = nx.spring_layout(_g, seed=7, k=0.9, iterations=200) if _g else {}
    _nodes = pl.DataFrame([{"filter": n, "x": float(p[0]), "y": float(p[1]),
                            "degree": _g.degree(n)} for n, p in _pos.items()])
    _edges = pl.DataFrame([
        {"x": float(_pos[a][0]), "y": float(_pos[a][1]),
         "x2": float(_pos[b][0]), "y2": float(_pos[b][1]),
         "lane": d["lane"], "pair": f"{a} ↔ {b}"}
        for a, b, d in _g.edges(data=True)]) if _g.number_of_edges() else \
        pl.DataFrame({"x": [], "y": [], "x2": [], "y2": [], "lane": [],
                      "pair": []})

    _e = (alt.Chart(_edges)
          .mark_rule(strokeWidth=1.1, opacity=0.55)
          .encode(x=alt.X("x:Q", axis=None), y=alt.Y("y:Q", axis=None),
                  x2="x2:Q", y2="y2:Q",
                  color=alt.Color("lane:N", title="replaced lane"),
                  tooltip=["pair", "lane"]))
    _n = (alt.Chart(_nodes)
          .mark_circle(size=130, color="#0b0b0b", opacity=0.85)
          .encode(x="x:Q", y="y:Q",
                  size=alt.Size("degree:Q", legend=None,
                                scale=alt.Scale(range=[60, 420])),
                  tooltip=["filter", "degree"]))
    mo.vstack([
        mo.md("### D — scaffold mutation graph\n"
              "An edge joins two presets that share **≥4 identical section "
              "states in the same lanes** and differ in at most one lane. "
              "Edge colour is the lane that was replaced."),
        mo.ui.altair_chart((_e + _n).properties(height=520, width=980)
                           .configure_view(stroke=None)),
        scaffolds.group_by("replaced_lane").agg(pl.len().alias("edges"))
        .sort("edges", descending=True),
    ])
    return


if __name__ == "__main__":
    app.run()
