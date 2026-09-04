import os
import sys
import json
import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "native", "python"))
import trench_core

FS = 44100.0
NPTS = 96
FREQS = 20.0 * np.power(1000.0, np.arange(NPTS) / (NPTS - 1))


def biquad(words):
    d = [trench_core.decode_word(w) for w in words]
    c0, c1, c2, c3, c4 = 4 * d[0] + d[1], d[1], 4 * d[2] + d[3], d[3], 4 * d[4]
    return [c4, (c0 - 2) * c4, (1 - c1) * c4, c2 - 2, 1 - c3]


def response_db(frame_words):
    return np.clip(trench_core.cascade_response_db([biquad(w) for w in frame_words], list(FREQS), FS), -60, 30)


LO, HI = 30.0, 16000.0


def strong_poles(frame_words):
    out = []
    for w in frame_words:
        mag, r2 = trench_core.decode_word(w[2]), trench_core.decode_word(w[3])
        q = 1 - r2
        pp = 4 * mag + r2 - 2
        disc = pp * pp - 4 * q
        if disc < 0 and q > 0:
            r = q ** 0.5
            hz = np.arccos(max(-1.0, min(1.0, -pp / (2 * r)))) / (2 * np.pi) * FS
            if r > 0.85 and LO <= hz <= HI:
                out.append((hz, r))
    out.sort()
    return out


def pair(mag, r2):
    q = 1 - r2
    pp = 4 * mag + r2 - 2
    disc = pp * pp - 4 * q
    if disc < 0 and q > 0:
        r = q ** 0.5
        return [round(float(np.arccos(max(-1.0, min(1.0, -pp / (2 * r)))) / (2 * np.pi) * FS), 1), round(float(r), 4)]
    return None


def rows_of(words):
    out = []
    for w in words:
        d = [trench_core.decode_word(x) for x in w]
        out.append({"p": pair(d[2], d[3]), "z": pair(d[0], d[1])})
    return out


def collect():
    with open(os.path.join(ROOT, "native", "python", "workstation", "frames_3d.json"), "r", encoding="utf-8") as fp:
        raw = json.load(fp)
    frames = []
    for f in raw:
        name = f["name"].replace("�", "·")
        words = f["words"][:6]
        frames.append({"name": name, "words": words, "rows": rows_of(words)})
    curves = np.array([response_db(f["words"]) for f in frames])
    mean = curves.mean(axis=0)
    _, sv, vt = np.linalg.svd(curves - mean, full_matrices=False)
    pc = (curves - mean) @ vt[:2].T
    lo, hi = pc.min(axis=0), pc.max(axis=0)
    for f, p in zip(frames, pc):
        f["pc"] = [round(float((p[0] - lo[0]) / (hi[0] - lo[0])), 4), round(float((p[1] - lo[1]) / (hi[1] - lo[1])), 4)]
    var = sv ** 2 / (sv ** 2).sum()
    print("pca variance", round(float(var[0]), 3), round(float(var[1]), 3))
    return frames


PAGE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>TRENCH workstation</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { height: 100%; }
body { background: #000; color: #ddd; font: 11px/1.2 Consolas, "Lucida Console", Menlo, monospace; user-select: none; overflow: hidden; }
#stage { width: 100vw; height: 100vh; display: block; touch-action: none; }
text { font-family: Consolas, "Lucida Console", Menlo, monospace; font-size: 11px; fill: #bbb; pointer-events: none; }
.rule { stroke: #666; shape-rendering: crispEdges; }
.title { fill: #000; font-weight: bold; }
.titlebar { fill: #bbb; shape-rendering: crispEdges; }
.axis { stroke: #bbb; fill: none; shape-rendering: crispEdges; }
.tick { stroke: #bbb; shape-rendering: crispEdges; }
.dots { stroke: #333; stroke-dasharray: 1 3; shape-rendering: crispEdges; }
.zero { stroke: #888; shape-rendering: crispEdges; }
.anchor { stroke: none; cursor: grab; }
.anchor.hot { stroke: #fff; stroke-width: 1.5; }
.link { stroke: #0ff; }
.tri { fill: #0ff; fill-opacity: .06; stroke: #0ff; stroke-opacity: .5; }
.probe { fill: #ff0; pointer-events: none; }
.probe-ring { fill: none; stroke: #ff0; pointer-events: none; }
.riser { stroke: #f0f; stroke-dasharray: 2 3; pointer-events: none; }
.curve { stroke: #fff; stroke-width: 1.6; fill: none; pointer-events: none; }
.bar-box { fill: #000; stroke: #777; shape-rendering: crispEdges; cursor: ew-resize; }
.bar-f { fill: #f0f; } .bar-m { fill: #0cf; } .bar-t { fill: #f44; }
.key { fill: #222; stroke: #999; shape-rendering: crispEdges; cursor: pointer; }
.key-on { fill: #0ff; }
.key-text { fill: #ddd; }
.list-box { fill: #000; stroke: #777; shape-rendering: crispEdges; }
.list-item { fill: #aaa; cursor: grab; }
.list-cap { fill: #ff0; cursor: grab; }
.layer-item { fill: #aaa; }
.layer-on { fill: #0ff; }
.mini { fill: #050505; stroke: #444; cursor: pointer; }
.mini-on { stroke: #0ff; }
.status { fill: #0ff; }
</style>
</head>
<body>
<svg id="stage"></svg>
<script>
const FRAMES = __FRAMES__;
const FS = 44100;
let captured = [];
try { captured = JSON.parse(localStorage.getItem('captured') || '[]'); } catch (e) { captured = []; }
let layers = [], active = 0;
let lit = null;
let probe = null;
let z = 0;
const groupOf = n => { for (const [g, t] of GROUPS) if (t(n)) return g; return 'INSTRUMENTS'; };
let status = 'ready';
let bodies = [], loose = [];
let trayScroll = 0;
let pan = [0, 0], zoom = 1;
let auto = [false, false, false], autoT0 = 0;
let keys = [], playhead = 0, playing_ = false, loop = true, playT0 = 0, playFrom = 0;
const DUR = 8;
const AUTO_RATE = [0.35, 0.23, 0.17];
const POSES = [['high', 'low', 'pitch'], ['closed', 'open', 'spread'], ['relaxed', 'stressed', 'resonance'], ['few', 'many', 'stages'], ['plain', 'carved', 'zeros'], ['dark', 'bright', 'ceiling'], ['-', '+', 'martens 1'], ['-', '+', 'martens 2']];
let axisX = 0, axisY = 2;
const TAGS = ['M0 Q0', 'M1 Q0', 'M0 Q1', 'M1 Q1'];
const GROUPS = [
  ['P2K', n => / · M[01] Q[01]$/.test(n)],
  ['MORPHEUS', n => /^MORPHEUS/.test(n)],
  ['X3', n => /^(X3|LADDER)/.test(n)],
  ['VOWELS', n => /^(sung|hedz|vowel|[aeiou]{1,3}$)/i.test(n)],
  ['HEADS', n => /ear az/i.test(n)],
  ['XL-1', n => /^Aud /.test(n)],
  ['INSTRUMENTS', n => true],
];

function decode(w) {
  const u = w + 1;
  if (u === 65536) return 1;
  if (u === 1) return 0;
  const e = (u >> 12) & 15, m = u & 4095;
  const x = e === 0 ? m / 4096 : (m + 4096) / 8192;
  return x * Math.pow(2, e - 15);
}
function biquad(w) {
  const d = w.map(decode);
  const c0 = 4 * d[0] + d[1], c1 = d[1], c2 = 4 * d[2] + d[3], c3 = d[3], c4 = 4 * d[4];
  return { b0: c4, b1: (c0 - 2) * c4, b2: (1 - c1) * c4, a1: c2 - 2, a2: 1 - c3 };
}
function pair(mag, r2) {
  const qq = 1 - r2, p = 4 * mag + r2 - 2, disc = p * p - 4 * qq;
  if (disc < 0 && qq > 0) { const r = Math.sqrt(qq); return [Math.acos(Math.max(-1, Math.min(1, -p / (2 * r)))) / (2 * Math.PI) * FS, r]; }
  return null;
}
function magDb(bq, f) {
  const w = 2 * Math.PI * f / FS, c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
  const nr = bq.b0 + bq.b1 * c1 + bq.b2 * c2, ni = -bq.b1 * s1 - bq.b2 * s2;
  const dr = 1 + bq.a1 * c1 + bq.a2 * c2, di = -bq.a1 * s1 - bq.a2 * s2;
  return 10 * Math.log10((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-30);
}
function cascadeDb(bqs, f) { let g = 0; for (const b of bqs) g += magDb(b, f); return g; }
function curveOf(words, n) {
  const bqs = words.map(biquad), out = [];
  for (let i = 0; i <= n; i++) { const f = 20 * Math.pow(1000, i / n); out.push([f, Math.max(-30, Math.min(30, cascadeDb(bqs, f)))]); }
  return out;
}
function blendWords(parents, weights) {
  const out = [];
  for (let s = 0; s < 6; s++) {
    const row = [];
    for (let k = 0; k < 5; k++) { let acc = 0; parents.forEach((p, i) => { acc += p[s][k] * weights[i]; }); row.push(Math.trunc(acc)); }
    out.push(row);
  }
  return out;
}
function resDb(r) { return Math.min(60, 20 * Math.log10(1 / Math.max(1e-3, 1 - r))); }
function rowsFor(f) { if (!f.rows) f.rows = f.words.map(w => { const d = w.map(decode); return { p: pair(d[2], d[3]), z: pair(d[0], d[1]) }; }); return f.rows; }
function measures(f) {
  if (f.m) return f.m;
  const rows = rowsFor(f);
  const ps = rows.filter(r => r.p).map(r => r.p);
  const strong = ps.filter(p => p[1] > 0.85);
  const use = strong.length ? strong : ps;
  const octs = use.map(p => Math.log2(Math.max(30, Math.min(16000, p[0])) / 30));
  const pitchOct = octs.length ? octs.reduce((a, b) => a + b, 0) / octs.length : 4.5;
  const spread = octs.length ? Math.max(...octs) - Math.min(...octs) : 0;
  const res = ps.length ? ps.reduce((a, p) => a + resDb(p[1]), 0) / ps.length : 0;
  const stages = rows.filter(r => r.p && r.p[1] > 0.5).length;
  const zeros = rows.filter(r => r.z && r.z[1] > 0.5 && r.z[0] < 15000).length;
  const zs = rows.filter(r => r.z).map(r => r.z[0]);
  const ceiling = zs.length ? Math.log2(Math.max(30, Math.min(16000, Math.max(...zs))) / 30) / 9 : 1;
  f.m = [pitchOct / 9, Math.min(1, spread / 6), Math.min(1, res / 40), stages / 6, zeros / 6, ceiling, f.pc ? f.pc[0] : 0.5, f.pc ? f.pc[1] : 0.5];
  return f.m;
}
function coordOf(f) { const m = measures(f); return [axisX === 0 ? 1 - m[0] : m[axisX], axisY === 0 ? 1 - m[0] : m[axisY]]; }
function hueHz(hz) { const t = Math.log2(Math.max(30, Math.min(16000, hz)) / 30) / Math.log2(16000 / 30); return `hsl(${Math.round(t * 270)} 90% 55%)`; }
function hueOf(f) { return `hsl(${Math.round(measures(f)[0] * 270)} 90% 55%)`; }
function glyph(f, cx, cy, scale, parent, cls, data) {
  const rows = rowsFor(f), bw = 3 * scale, gap = 1 * scale, h = 14 * scale;
  const x0 = cx - (6 * bw + 5 * gap) / 2;
  const back = el('rect', { x: x0 - 2, y: cy - h / 2 - 2, width: 6 * bw + 5 * gap + 4, height: h + 4, class: 'glyph-back ' + (cls || '') }, parent);
  if (data) for (const k in data) back.dataset[k] = data[k];
  rows.forEach((r, s) => {
    const x = x0 + s * (bw + gap);
    if (r.p) el('rect', { x, y: cy + h / 2 - Math.max(1.5 * scale, resDb(r.p[1]) / 60 * h), width: bw, height: Math.max(1.5 * scale, resDb(r.p[1]) / 60 * h), fill: hueHz(r.p[0]), 'pointer-events': 'none' }, parent);
    else el('rect', { x, y: cy + h / 2 - 1, width: bw, height: 1, fill: '#555', 'pointer-events': 'none' }, parent);
    if (r.z && r.z[1] > 0.5) el('rect', { x, y: cy + h / 2 + 1, width: bw, height: 1.5 * scale, fill: '#888', 'pointer-events': 'none' }, parent);
  });
}

function delaunay(pts) {
  const n = pts.length;
  if (n < 3) return [];
  const p = pts.map(q => [q[0] + (Math.random() - 0.5) * 1e-6, q[1] + (Math.random() - 0.5) * 1e-6]);
  p.push([-10, -10], [10, -10], [0, 10]);
  const circ = (a, b, c) => {
    const [ax, ay] = p[a], [bx, by] = p[b], [cx, cy] = p[c];
    const d = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (Math.abs(d) < 1e-18) return { a, b, c, r2: -1 };
    const ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d;
    const uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d;
    return { a, b, c, ux, uy, r2: (ax - ux) ** 2 + (ay - uy) ** 2 };
  };
  let tris = [circ(n, n + 1, n + 2)];
  for (let i = 0; i < n; i++) {
    const edges = [], keep = [];
    for (const t of tris) {
      if (t.r2 >= 0 && (p[i][0] - t.ux) ** 2 + (p[i][1] - t.uy) ** 2 < t.r2) edges.push([t.a, t.b], [t.b, t.c], [t.c, t.a]);
      else keep.push(t);
    }
    tris = keep;
    for (let e = 0; e < edges.length; e++) {
      let shared = false;
      for (let f = 0; f < edges.length; f++) if (e !== f && ((edges[e][0] === edges[f][0] && edges[e][1] === edges[f][1]) || (edges[e][0] === edges[f][1] && edges[e][1] === edges[f][0]))) { shared = true; break; }
      if (!shared) tris.push(circ(edges[e][0], edges[e][1], i));
    }
  }
  return tris.filter(t => t.a < n && t.b < n && t.c < n).map(t => [t.a, t.b, t.c]);
}
function bary(px, py, a, b, c) {
  const v0x = b[0] - a[0], v0y = b[1] - a[1], v1x = c[0] - a[0], v1y = c[1] - a[1], v2x = px - a[0], v2y = py - a[1];
  const den = v0x * v1y - v1x * v0y;
  if (Math.abs(den) < 1e-14) return null;
  const v = (v2x * v1y - v1x * v2y) / den, w = (v0x * v2y - v2x * v0y) / den;
  return [1 - v - w, v, w];
}
function retri(lay) { lay.tris = delaunay(lay.anchors.map(a => a.p)); }
function blendOn(k, u, v) {
  const lay = layers[k];
  if (!lay || lay.anchors.length < 3) return null;
  for (const t of lay.tris) {
    const b = bary(u, v, lay.anchors[t[0]].p, lay.anchors[t[1]].p, lay.anchors[t[2]].p);
    if (b && b.every(x => x >= -1e-9)) return { idx: t, w: b };
  }
  return null;
}
function column(u, v) { const col = []; for (let k = 0; k < layers.length; k++) if (blendOn(k, u, v)) col.push(k); return col; }
function columnPos() {
  if (!probe) return null;
  const col = column(probe[0], probe[1]);
  if (!col.length) return null;
  if (col.length === 1) return { col, a: col[0], b: col[0], t: 0, pos: 0 };
  const pos = z * (col.length - 1), a = Math.min(col.length - 2, Math.floor(pos));
  return { col, a: col[a], b: col[a + 1], t: pos - a, pos };
}
function playing() {
  const cp = columnPos();
  if (!cp) return null;
  const A = blendOn(cp.a, probe[0], probe[1]), B = blendOn(cp.b, probe[0], probe[1]);
  const parents = [], weights = [];
  A.idx.forEach((i, n) => { parents.push(layers[cp.a].anchors[i].f.words); weights.push(A.w[n] * (1 - cp.t)); });
  if (cp.t > 0) B.idx.forEach((i, n) => { parents.push(layers[cp.b].anchors[i].f.words); weights.push(B.w[n] * cp.t); });
  return { words: blendWords(parents, weights), A, B, cp };
}
function sortLayer(k) { const lay = layers[k]; lay.anchors = lay.frames.map(f => ({ f, p: coordOf(f) })); retri(lay); }
function buildLibrary() {
  bodies = []; loose = []; layers = [];
  const fam = new Map();
  FRAMES.forEach(f => {
    const parts = f.name.split(' · ');
    if (parts.length === 2 && TAGS.includes(parts[1])) { if (!fam.has(parts[0])) fam.set(parts[0], {}); fam.get(parts[0])[parts[1]] = f; }
    else loose.push(f);
  });
  fam.forEach((m, name) => { if (TAGS.every(t => m[t])) bodies.push({ name, corners: TAGS.map(t => m[t]) }); else TAGS.forEach(t => { if (m[t]) loose.push(m[t]); }); });
  layers.push({ name: 'ALL', frames: FRAMES.concat(captured), anchors: [], tris: [] });
  captured.forEach(c => loose.unshift(c));
  layers.forEach((l, k) => sortLayer(k));
}

const svg = document.getElementById('stage');
function el(tag, attrs, parent) {
  const e = document.createElementNS('http://www.w3.org/2000/svg', tag);
  for (const k in attrs) e.setAttribute(k, attrs[k]);
  parent.appendChild(e);
  return e;
}
function txt(x, y, s, parent, cls) { const t = el('text', { x, y, class: cls || '' }, parent); t.textContent = s; return t; }
let VW = 1280, VH = 800, L = {};
function layout() {
  VW = window.innerWidth; VH = window.innerHeight;
  svg.setAttribute('viewBox', `0 0 ${VW} ${VH}`);
  const bar = 14;
  const topH = Math.round(VH * 0.62);
  const tlH = 120;
  const respW = Math.round(VW * 0.62);
  L.plane = { x: 0, y: 0, w: VW, h: topH, bar };
  L.resp = { x: 0, y: topH, w: respW, h: VH - topH - tlH, bar };
  L.bars = { x: respW, y: topH, w: VW - respW, h: VH - topH - tlH, bar };
  L.tl = { x: 0, y: VH - tlH, w: VW, h: tlH, bar };
  L.tlAx = { x: 90, y: VH - tlH + bar + 22, w: VW - 110, h: tlH - bar - 40 };
  L.floors = { x: 8, y: bar + 8, w: 120, h: 120 };
  L.tray = { x: 8, y: bar + 136, w: 180, h: topH - bar - 144 };
  L.field = { x: 240, y: bar + 44, w: VW - 260, h: topH - bar - 74 };
  L.respAx = { x: 44, y: topH + bar + 8, w: respW - 60, h: VH - topH - tlH - bar - 30 };
}
const rx = f => L.respAx.x + Math.log10(f / 20) / 3 * L.respAx.w, ry = d => L.respAx.y + (30 - d) / 60 * L.respAx.h;
function w2s(u, v) { const F = L.field; return [F.x + (u * F.w) * zoom + pan[0], F.y + F.h - (v * F.h) * zoom + pan[1]]; }
function s2w(sx, sy) { const F = L.field; return [(sx - F.x - pan[0]) / (F.w * zoom), (F.y + F.h - sy + pan[1]) / (F.h * zoom)]; }
function panel(r, title, parent) {
  el('rect', { x: r.x + 0.5, y: r.y + 0.5, width: r.w - 1, height: r.h - 1, fill: 'none', class: 'rule' }, parent);
  el('rect', { x: r.x + 1, y: r.y + 1, width: r.w - 2, height: r.bar - 1, class: 'titlebar' }, parent);
  txt(r.x + 6, r.y + 11, title, parent, 'title');
}
function axes(a, xt, yt, parent) {
  el('rect', { x: a.x + 0.5, y: a.y + 0.5, width: a.w, height: a.h, class: 'axis' }, parent);
  xt.forEach(([px, label]) => { el('line', { x1: px, y1: a.y + a.h, x2: px, y2: a.y + a.h + 4, class: 'tick' }, parent); el('line', { x1: px, y1: a.y, x2: px, y2: a.y + a.h, class: 'dots' }, parent); txt(px - label.length * 3, a.y + a.h + 14, label, parent); });
  yt.forEach(([py, label, cls]) => { el('line', { x1: a.x - 4, y1: py, x2: a.x, y2: py, class: 'tick' }, parent); el('line', { x1: a.x, y1: py, x2: a.x + a.w, y2: py, class: cls || 'dots' }, parent); txt(a.x - 8 - label.length * 6, py + 3, label, parent); });
}
function key(x, y, w, label, data, parent, on) {
  const k = el('rect', { x, y, width: w, height: 14, class: 'key' + (on ? ' key-on' : '') }, parent);
  for (const n in data) k.dataset[n] = data[n];
  txt(x + 6, y + 11, label, parent, on ? 'title' : 'key-text');
  return k;
}
function axisTicks(which) {
  const m = which === 'x' ? axisX : axisY;
  if (m === 0) return [1, 2, 3, 4, 5, 6, 7, 8, 9].map(i => [1 - (Math.log2(32.703 * Math.pow(2, i - 1) / 30) / 9), 'C' + i]);
  if (m === 1) return [0, 1, 2, 3, 4, 5, 6].map(o => [o / 6, o + ' oct']);
  if (m === 2) return [0, 10, 20, 30, 40].map(d => [d / 40, d + ' dB']);
  if (m === 3 || m === 4) return [0, 1, 2, 3, 4, 5, 6].map(n => [n / 6, '' + n]);
  if (m === 5) return [1, 2, 3, 4, 5, 6, 7, 8, 9].map(i => [Math.log2(32.703 * Math.pow(2, i - 1) / 30) / 9, 'C' + i]);
  return [0, 0.25, 0.5, 0.75, 1].map(v => [v, v.toFixed(2)]);
}
let g = {};
function build() {
  svg.innerHTML = '';
  layout();
  ['plane', 'planeLive', 'resp', 'respLive', 'bars', 'barsLive', 'tl', 'tlLive', 'drag'].forEach(k => { g[k] = el('g', {}, svg); });
  panel(L.plane, 'FRAME SPACE', g.plane);
  panel(L.resp, 'RESPONSE', g.resp);
  panel(L.tl, 'TIMELINE', g.tl);
  const r = L.respAx;
  axes(r, [50, 100, 200, 500, 1000, 2000, 5000, 10000].map(f => [rx(f), f >= 1000 ? (f / 1000) + 'k' : '' + f]),
    [[ry(20), '+20'], [ry(10), '+10'], [ry(0), '0', 'zero'], [ry(-10), '-10'], [ry(-20), '-20']], g.resp);
  drawAll();
}
function drawPlane() {
  g.planeLive.innerHTML = '';
  const kx = L.field.x, ky = L.plane.y + L.plane.bar + 4;
  txt(kx, ky + 11, 'across', g.planeLive);
  POSES.forEach((p, i) => key(kx + 52 + i * 78, ky, 74, p[2], { ax: i }, g.planeLive, axisX === i));
  txt(kx, ky + 27, 'up', g.planeLive);
  POSES.forEach((p, i) => key(kx + 52 + i * 78, ky + 16, 74, p[2], { ay: i }, g.planeLive, axisY === i));
  key(kx + 700, ky, 52, 'SORT', { key: 'sort' }, g.planeLive);
  key(kx + 700, ky + 16, 52, 'CLEAR', { key: 'clearlayer' }, g.planeLive);
  const fl = L.floors;
  el('rect', { x: fl.x, y: fl.y, width: fl.w, height: fl.h, class: 'list-box' }, g.planeLive);
  GROUPS.map(gr => gr[0]).forEach((name, n) => {
    const t = txt(fl.x + 8, fl.y + 12 + n * 13, name, g.planeLive, lit === name ? 'layer-on' : 'layer-item');
    t.style.pointerEvents = 'auto';
    t.dataset.lit = name;
  });
  const lineH = 13, tr = L.tray;
  el('rect', { x: tr.x, y: tr.y, width: tr.w, height: tr.h, class: 'list-box' }, g.planeLive);
  const items = bodies.map(b => ({ body: b })).concat(loose.map(f => ({ frame: f })));
  const maxRows = Math.floor((tr.h - 4) / lineH);
  trayScroll = Math.max(0, Math.min(trayScroll, Math.max(0, items.length - maxRows)));
  items.slice(trayScroll, trayScroll + maxRows).forEach((it, i) => {
    const y = tr.y + 11 + i * lineH;
    if (it.body) {
      [[0, 0], [1, 0], [0, 1], [1, 1]].forEach(([a, b], k) => el('rect', { x: tr.x + 5 + a * 5, y: y - 8 + (1 - b) * 5, width: 4, height: 4, fill: hueOf(it.body.corners[k]) }, g.planeLive));
      const t = txt(tr.x + 18, y, it.body.name.slice(0, 21), g.planeLive, 'list-item');
      t.style.pointerEvents = 'auto'; t.dataset.body = bodies.indexOf(it.body);
    } else {
      el('circle', { cx: tr.x + 9, cy: y - 4, r: 3.5, fill: hueOf(it.frame) }, g.planeLive);
      const t = txt(tr.x + 18, y, it.frame.name.slice(0, 21), g.planeLive, it.frame.capture ? 'list-cap' : 'list-item');
      t.style.pointerEvents = 'auto'; t.dataset.loose = loose.indexOf(it.frame);
    }
  });
  const F = L.field;
  axes(F, axisTicks('x').map(([u, l]) => [w2s(u, 0)[0], l]).filter(([x]) => x >= F.x && x <= F.x + F.w), axisTicks('y').map(([v, l]) => [w2s(0, v)[1], l]).filter(([y]) => y >= F.y && y <= F.y + F.h), g.planeLive);
  const clip = el('clipPath', { id: 'fieldClip' }, g.planeLive);
  el('rect', { x: F.x, y: F.y, width: F.w, height: F.h }, clip);
  const field = el('g', { 'clip-path': 'url(#fieldClip)' }, g.planeLive);
  const lay = layers[active];
  const pl = playing();
  if (pl) {
    const A = pl.A;
    const pts = A.idx.map(i => w2s(...lay.anchors[i].p));
    const tri = el('polygon', { points: pts.map(p => p.map(v => v.toFixed(1)).join(',')).join(' '), class: 'tri' }, field);
    const [px, py] = w2s(...probe);
    pts.forEach(p => el('line', { x1: px, y1: py, x2: p[0], y2: p[1], class: 'link' }, field));
  }
  lay.anchors.forEach((a, i) => {
    const [cx, cy] = w2s(...a.p);
    const on = !lit || groupOf(a.f.name) === lit || a.f.capture;
    const d = el('circle', { cx, cy, r: hotAnchor === i ? 6 : (on ? 4 : 2.5), fill: a.f.capture ? '#ff0' : hueOf(a.f), 'fill-opacity': on ? 1 : 0.35, class: 'anchor' + (hotAnchor === i ? ' hot' : '') }, field);
    d.dataset.anchor = i;
  });
  const ks = keys.slice().sort((k1, k2) => k1.t - k2.t);
  for (let i = 0; i < ks.length - 1; i++) { const a1 = w2s(...ks[i].p), b1 = w2s(...ks[i + 1].p); el('line', { x1: a1[0], y1: a1[1], x2: b1[0], y2: b1[1], class: 'riser' }, field); }
  ks.forEach(k => { const [kx2, ky2] = w2s(...k.p); el('rect', { x: kx2 - 4, y: ky2 - 4, width: 8, height: 8, fill: 'none', stroke: k.color, transform: `rotate(45 ${kx2} ${ky2})`, 'pointer-events': 'none' }, field); });
  if (probe) {
    const [px, py] = w2s(...probe);
    el('circle', { cx: px, cy: py, r: 4, class: 'probe' }, field);
    el('circle', { cx: px, cy: py, r: 8, class: 'probe-ring' }, field);
  }
  txt(L.field.x, L.plane.y + L.plane.h - 6, status, g.planeLive, 'status');
}
function drawBars() {
  g.barsLive.innerHTML = '';
  panel(L.bars, 'CONTROL', g.barsLive);
  const b = L.bars, x0 = b.x + 64, w = Math.round(b.w * 0.42), h = 10, y0 = b.y + b.bar + 12;
  const cp = columnPos();
  const names = ['X', 'Y'], vals = [probe ? probe[0] : 0, probe ? probe[1] : 0], cls = ['bar-m', 'bar-f'];
  const ends = [POSES[axisX], POSES[axisY]];
  for (let i = 0; i < 2; i++) {
    const y = y0 + i * (h + 8);
    txt(b.x + 34, y + 9, names[i], g.barsLive);
    const box = el('rect', { x: x0, y, width: w, height: h, class: 'bar-box' }, g.barsLive);
    box.dataset.bar = i;
    el('rect', { x: x0 + 1, y: y + 1, width: Math.max(0, (w - 2) * vals[i]), height: h - 2, class: cls[i] }, g.barsLive);
    txt(x0 + w + 8, y + 9, vals[i].toFixed(3) + '  ' + ends[i][0] + ' > ' + ends[i][1], g.barsLive);
    const tick = el('rect', { x: x0 - 14, y, width: 10, height: 10, class: 'key' + (auto[i] ? ' key-on' : '') }, g.barsLive);
    tick.dataset.auto = i;
  }
  key(b.x + 34, y0 + 44, 96, 'CAPTURE', { key: 'capture' }, g.barsLive);
  key(b.x + 138, y0 + 44, 96, 'CLEAR CAPS', { key: 'clear' }, g.barsLive);
}
const tx = t => L.tlAx.x + t / DUR * L.tlAx.w;
function pathAt(t) {
  if (!keys.length) return null;
  const ks = keys.slice().sort((a, b) => a.t - b.t);
  if (t <= ks[0].t) return ks[0].p.slice();
  if (t >= ks[ks.length - 1].t) return ks[ks.length - 1].p.slice();
  for (let i = 0; i < ks.length - 1; i++) if (t >= ks[i].t && t <= ks[i + 1].t) { const f = (t - ks[i].t) / Math.max(1e-9, ks[i + 1].t - ks[i].t); return [ks[i].p[0] + (ks[i + 1].p[0] - ks[i].p[0]) * f, ks[i].p[1] + (ks[i + 1].p[1] - ks[i].p[1]) * f]; }
  return null;
}
function drawTimeline() {
  g.tlLive.innerHTML = '';
  const a = L.tlAx;
  key(L.tl.x + 8, a.y - 4, 44, playing_ ? 'STOP' : 'PLAY', { key: 'play' }, g.tlLive, playing_);
  key(L.tl.x + 8, a.y + 14, 44, 'LOOP', { key: 'loop' }, g.tlLive, loop);
  key(L.tl.x + 8, a.y + 32, 44, '+ KEY', { key: 'addkey' }, g.tlLive);
  key(L.tl.x + 8, a.y + 50, 44, 'CLEAR', { key: 'clearkeys' }, g.tlLive);
  el('rect', { x: a.x, y: a.y, width: a.w, height: a.h, class: 'list-box' }, g.tlLive);
  for (let t = 0; t <= DUR; t++) { el('line', { x1: tx(t), y1: a.y, x2: tx(t), y2: a.y + a.h, class: 'dots' }, g.tlLive); txt(tx(t) - 3, a.y + a.h + 12, t + 's', g.tlLive); }
  const ks = keys.slice().sort((k1, k2) => k1.t - k2.t);
  for (let i = 0; i < ks.length - 1; i++) el('line', { x1: tx(ks[i].t), y1: a.y + a.h / 2, x2: tx(ks[i + 1].t), y2: a.y + a.h / 2, class: 'link' }, g.tlLive);
  keys.forEach((k, i) => {
    const d = el('rect', { x: tx(k.t) - 5, y: a.y + a.h / 2 - 5, width: 10, height: 10, fill: k.color, class: 'anchor' + (dragKey === i ? ' hot' : ''), transform: `rotate(45 ${tx(k.t)} ${a.y + a.h / 2})` }, g.tlLive);
    d.dataset.tkey = i;
  });
  const hit = el('rect', { x: a.x, y: a.y, width: a.w, height: a.h, fill: 'transparent' }, g.tlLive);
  hit.dataset.scrub = 1;
  el('line', { x1: tx(playhead), y1: a.y, x2: tx(playhead), y2: a.y + a.h, stroke: '#ff0' }, g.tlLive);
  txt(tx(playhead) + 4, a.y + 10, playhead.toFixed(2) + 's', g.tlLive, 'status');
}
function drawAll() {
  g.respLive.innerHTML = '';
  drawTimeline();
  const pl = playing();
  if (pl) el('polyline', { points: curveOf(pl.words, 240).map(([f, d]) => rx(f).toFixed(1) + ',' + ry(d).toFixed(1)).join(' '), class: 'curve' }, g.respLive);
  drawPlane();
  drawBars();
}
function capture() {
  const pl = playing();
  if (!pl) { status = 'press inside the anchors first'; drawAll(); return; }
  const name = 'cap' + (captured.length + 1) + ' ' + probe[0].toFixed(2) + ',' + probe[1].toFixed(2);
  const f = { name, words: pl.words, capture: true };
  captured.push(f);
  try { localStorage.setItem('captured', JSON.stringify(captured)); } catch (e) {}
  loose.unshift(f);
  layers[0].frames.push(f);
  layers[0].anchors.push({ f, p: probe.slice() });
  retri(layers[0]);
  status = 'captured ' + name;
  drawAll();
}
let mode = null, dragFrame = null, dragBody = null, dragAnchor = -1, dragKey = -1, dragPos = null, dragStart = null, last = null, barIdx = -1, hotAnchor = -1;
function inRect(r, x, y) { return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h; }
function toSvg(e) { const r = svg.getBoundingClientRect(); return [(e.clientX - r.left) / r.width * VW, (e.clientY - r.top) / r.height * VH]; }
function setProbe(x, y) {
  if (!inRect(L.field, x, y)) return;
  probe = s2w(x, y);
  const cp = columnPos();
  const pl = playing();
  status = pl ? pl.A.idx.map((i, n) => Math.round(pl.A.w[n] * 100) + '% ' + layers[0].anchors[i].f.name).join('   ') : 'outside the anchors';
  drawAll();
}
function setBar(x) {
  const b = L.bars, x0 = b.x + 64, w = Math.round(b.w * 0.42);
  const v = Math.max(0, Math.min(1, (x - x0) / w));
  if (!probe) probe = [0.5, 0.5]; probe[barIdx] = v;
  drawAll();
}
svg.addEventListener('pointerdown', e => {
  const [x, y] = toSvg(e);
  svg.setPointerCapture(e.pointerId);
  const d = (e.target && e.target.dataset) || {};
  if (d.key === 'capture') { capture(); return; }
  if (d.key === 'play') { playing_ = !playing_; if (playing_) { playT0 = performance.now(); playFrom = playhead >= DUR ? 0 : playhead; requestAnimationFrame(playStep); } drawTimeline(); return; }
  if (d.key === 'loop') { loop = !loop; drawTimeline(); return; }
  if (d.key === 'addkey') { if (!probe) probe = [0.5, 0.5]; keys.push({ t: playhead, p: probe.slice(), color: `hsl(${Math.round(Math.random() * 360)} 80% 60%)` }); drawAll(); return; }
  if (d.key === 'clearkeys') { keys = []; drawAll(); return; }
  if (d.tkey !== undefined) { mode = 'tkey'; dragKey = +d.tkey; dragStart = [x, y]; return; }
  if (d.scrub !== undefined) { mode = 'scrub'; scrubTo(x); return; }
  if (d.key === 'clear') { captured = []; try { localStorage.removeItem('captured'); } catch (er) {} loose = loose.filter(f => !f.capture); layers[0].frames = layers[0].frames.filter(f => !f.capture); layers[0].anchors = layers[0].anchors.filter(a => !a.f.capture); retri(layers[0]); drawAll(); return; }
  if (d.key === 'sort') { sortLayer(active); status = 'sorted ' + layers[active].name + ' by ' + POSES[axisX][2] + ' and ' + POSES[axisY][2]; drawAll(); return; }
  if (d.key === 'clearlayer') { layers[active].anchors = []; layers[active].tris = []; drawAll(); return; }
  if (d.ax !== undefined) { axisX = +d.ax; drawAll(); return; }
  if (d.ay !== undefined) { axisY = +d.ay; drawAll(); return; }
  if (d.lit !== undefined) { lit = lit === d.lit ? null : d.lit; drawAll(); return; }
  if (d.auto !== undefined) { auto[+d.auto] = !auto[+d.auto]; if (auto.some(a => a) && !autoT0) { autoT0 = performance.now(); requestAnimationFrame(autoStep); } drawBars(); return; }
  if (d.bar !== undefined) { mode = 'bar'; barIdx = +d.bar; setBar(x); return; }
  if (d.loose !== undefined) { mode = 'frame'; dragFrame = loose[+d.loose]; dragPos = [x, y]; dragStart = [x, y]; return; }
  if (d.body !== undefined) { mode = 'body'; dragBody = bodies[+d.body]; dragPos = [x, y]; dragStart = [x, y]; return; }
  if (d.anchor !== undefined) { mode = 'anchor'; dragAnchor = +d.anchor; dragPos = [x, y]; dragStart = [x, y]; return; }
  if (inRect(L.field, x, y)) { mode = 'probe'; setProbe(x, y); return; }
});
svg.addEventListener('wheel', e => {
  const [x, y] = toSvg(e);
  if (inRect(L.tray, x, y)) { trayScroll += e.deltaY > 0 ? 3 : -3; drawPlane(); e.preventDefault(); }
  else if (inRect(L.field, x, y)) { const [u, v] = s2w(x, y); zoom = Math.max(0.5, Math.min(6, zoom * (e.deltaY > 0 ? 0.9 : 1.1))); const [nx, ny] = w2s(u, v); pan[0] += x - nx; pan[1] += y - ny; drawPlane(); e.preventDefault(); }
}, { passive: false });
function scrubTo(x) {
  playhead = Math.max(0, Math.min(DUR, (x - L.tlAx.x) / L.tlAx.w * DUR));
  const p = pathAt(playhead);
  if (p) { probe = p; const pl = playing(); status = pl ? pl.A.idx.map((i, n) => Math.round(pl.A.w[n] * 100) + '% ' + layers[0].anchors[i].f.name).join('   ') : 'outside the anchors'; }
  drawAll();
}
function playStep(now) {
  if (!playing_) return;
  let t = playFrom + (now - playT0) / 1000;
  if (t >= DUR) { if (loop) { playT0 = now; playFrom = 0; t = 0; } else { playing_ = false; t = DUR; } }
  playhead = t;
  const p = pathAt(t);
  if (p) probe = p;
  drawAll();
  if (playing_) requestAnimationFrame(playStep);
}
function drawDrag() {
  g.drag.innerHTML = '';
  if (!dragPos) return;
  if (mode === 'frame') el('circle', { cx: dragPos[0], cy: dragPos[1], r: 6, fill: hueOf(dragFrame), class: 'anchor hot' }, g.drag);
  else if (mode === 'anchor') el('circle', { cx: dragPos[0], cy: dragPos[1], r: 6, fill: hueOf(layers[active].anchors[dragAnchor].f), class: 'anchor hot' }, g.drag);
  else if (mode === 'body') [[0, 0], [1, 0], [0, 1], [1, 1]].forEach(([a, b], k) => el('circle', { cx: dragPos[0] - 8 + a * 16, cy: dragPos[1] + 8 - b * 16, r: 4, fill: hueOf(dragBody.corners[k]), class: 'anchor hot' }, g.drag));
}
svg.addEventListener('pointermove', e => {
  if (!mode) return;
  const [x, y] = toSvg(e);
  if (mode === 'frame' || mode === 'body' || mode === 'anchor') { dragPos = [x, y]; drawDrag(); }
  else if (mode === 'probe') setProbe(x, y);
  else if (mode === 'scrub') scrubTo(x);
  else if (mode === 'tkey') { keys[dragKey].t = Math.max(0, Math.min(DUR, (x - L.tlAx.x) / L.tlAx.w * DUR)); drawTimeline(); }
  else if (mode === 'bar') setBar(x);
});
svg.addEventListener('pointerup', e => {
  const [x, y] = toSvg(e);
  const moved = dragStart && Math.hypot(x - dragStart[0], y - dragStart[1]) > 6;
  const lay = layers[active];
  if (mode === 'frame' && inRect(L.field, x, y) && moved) { lay.anchors.push({ f: dragFrame, p: s2w(x, y) }); retri(lay); status = dragFrame.name + ' anchored on ' + lay.name; }
  else if (mode === 'body' && inRect(L.field, x, y) && moved) { dragBody.corners.forEach(f => lay.anchors.push({ f, p: coordOf(f) })); retri(lay); status = dragBody.name + ' anchored on ' + lay.name + ' at its measures'; }
  else if (mode === 'anchor') {
    if (inRect(L.tlAx, x, y) && moved) { keys.push({ t: Math.max(0, Math.min(DUR, (x - L.tlAx.x) / L.tlAx.w * DUR)), p: lay.anchors[dragAnchor].p.slice(), color: hueOf(lay.anchors[dragAnchor].f) }); status = 'key at ' + lay.anchors[dragAnchor].f.name; }
    else if (inRect(L.field, x, y) && moved) { lay.anchors[dragAnchor].p = s2w(x, y); retri(lay); }
    else if (!inRect(L.field, x, y) && moved) { lay.anchors.splice(dragAnchor, 1); retri(lay); status = 'anchor removed'; }
  }
  g.drag.innerHTML = '';
  if (mode === 'tkey' && !moved) { probe = keys[dragKey].p.slice(); playhead = keys[dragKey].t; }
  dragFrame = null; dragBody = null; dragAnchor = -1; dragKey = -1; dragPos = null; dragStart = null; hotAnchor = -1;
  mode = null;
  drawAll();
});
function autoStep(now) {
  if (!auto.some(a => a)) { autoT0 = 0; return; }
  const t = (now - autoT0) / 1000;
  if (!probe) probe = [0.5, 0.5];
  if (auto[0]) probe[0] = 0.5 + 0.45 * Math.sin(t * AUTO_RATE[0]);
  if (auto[1]) probe[1] = 0.5 + 0.45 * Math.sin(t * AUTO_RATE[1]);
  drawAll();
  requestAnimationFrame(autoStep);
}
window.addEventListener('resize', build);
buildLibrary();
if (location.hash === '#demo') { axisX = 6; axisY = 7; sortLayer(0); const a = layers[0].anchors; probe = [a.reduce((s, x) => s + x.p[0], 0) / a.length, a.reduce((s, x) => s + x.p[1], 0) / a.length]; lit = 'P2K'; keys = [{ t: 0.5, p: [0.3, 0.35], color: '#0cf' }, { t: 3, p: [0.55, 0.6], color: '#f0f' }, { t: 6.5, p: [0.4, 0.75], color: '#ff0' }]; playhead = 2.1; probe = pathAt(playhead); }
build();
</script>
</body>
</html>
"""


def build():
    frames = collect()
    html = PAGE.replace("__FRAMES__", json.dumps(frames, separators=(",", ":"), ensure_ascii=False))
    out = os.path.join(ROOT, "workstation_min.html")
    with open(out, "w", encoding="utf-8") as fp:
        fp.write(html)
    print(out, len(html), "bytes", len(frames), "frames")


if __name__ == "__main__":
    build()
