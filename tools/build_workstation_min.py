import os
import sys
import json
import numpy as np
from scipy.spatial import Delaunay

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


def collect():
    with open(os.path.join(ROOT, "native", "python", "workstation", "frames_3d.json"), "r", encoding="utf-8") as fp:
        raw = json.load(fp)
    frames = [{"name": f["name"].replace("�", "·"), "words": f["words"][:6]} for f in raw]
    span = np.log2(HI / LO)
    pts = []
    for f in frames:
        poles = strong_poles(f["words"])
        if len(poles) >= 2:
            f1, f2 = poles[0][0], poles[1][0]
        elif len(poles) == 1:
            f1 = f2 = poles[0][0]
        else:
            curve = response_db(f["words"])
            f1 = f2 = float(FREQS[int(np.argmax(curve))])
        u, v = np.log2(f1 / LO) / span, np.log2(f2 / LO) / span
        f["xy"] = [round(float(u), 4), round(float(v), 4)]
        f["f12"] = [round(float(f1)), round(float(f2))]
        pts.append([u, v])
    pts = np.array(pts)
    rng = np.random.default_rng(7)
    tri = Delaunay(pts + rng.normal(0, 1e-4, pts.shape))
    return frames, tri.simplices.tolist()


PAGE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>TRENCH workstation</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { height: 100%; }
body { background: #000; color: #ddd; font: 11px/1.2 "Courier New", Courier, monospace; user-select: none; overflow: hidden; }
#stage { width: 100vw; height: 100vh; display: block; touch-action: none; }
text { font-family: "Courier New", Courier, monospace; font-size: 10px; fill: #bbb; pointer-events: none; }
.rule { stroke: #666; shape-rendering: crispEdges; }
.title { fill: #000; font-weight: bold; }
.titlebar { fill: #bbb; shape-rendering: crispEdges; }
.axis { stroke: #bbb; fill: none; shape-rendering: crispEdges; }
.tick { stroke: #bbb; shape-rendering: crispEdges; }
.dots { stroke: #444; stroke-dasharray: 1 3; shape-rendering: crispEdges; }
.zero { stroke: #888; shape-rendering: crispEdges; }
.tri { stroke: #0ff; fill: #0ff; fill-opacity: .08; }
.node { fill: #aaa; cursor: crosshair; }
.node.cap { fill: #ff0; }
.node.held { fill: #fff; }
.link { stroke: #0ff; }
.probe { fill: none; stroke: #fff; stroke-width: 1; pointer-events: none; }
.edge { stroke: #bbb; fill: none; }
.edge.back { stroke: #555; }
.floor { stroke: #0ff; stroke-dasharray: 2 2; pointer-events: none; }
.wheel { fill: #ff0; pointer-events: none; }
.thumb-box { fill: #000; stroke: #777; shape-rendering: crispEdges; }
.thumb-box.armed { stroke: #0ff; }
.thumb-box.hot { stroke: #fff; stroke-width: 2; }
.thumb { stroke: #ddd; fill: none; }
.thumb-empty { stroke: #444; fill: none; }
.curve { stroke: #fff; stroke-width: 1.5; fill: none; pointer-events: none; }
.ghost { stroke: #0ff; stroke-width: 1; fill: none; pointer-events: none; }
.bar-box { fill: #000; stroke: #777; shape-rendering: crispEdges; cursor: ew-resize; }
.bar-f { fill: #f0f; } .bar-m { fill: #0cf; } .bar-t { fill: #f44; }
.key { fill: #222; stroke: #999; shape-rendering: crispEdges; cursor: pointer; }
.key-text { fill: #ddd; }
.thread { stroke: #0ff; stroke-dasharray: 2 3; pointer-events: none; }
.drag { fill: none; stroke: #fff; stroke-width: 1.5; pointer-events: none; }
.status { fill: #0ff; }
</style>
</head>
<body>
<svg id="stage"></svg>
<script>
const FRAMES = __FRAMES__;
const TRIS = __TRIS__;
const FS = 44100;
let captured = [];
try { captured = JSON.parse(localStorage.getItem('captured') || '[]'); } catch (e) { captured = []; }
let corners = [null, null, null, null, null, null, null, null];
let armed = 0;
let morph = 0.5, q = 0.5, z = 0.5;
let yaw = -0.55, pitch = 0.42;
let probe = null;
let status = 'ready';
const AXES = [['high', 'low'], ['closed', 'open'], ['relaxed', 'stressed']];
const poseName = i => [AXES[0][i & 1], AXES[1][i & 2 ? 1 : 0], AXES[2][i & 4 ? 1 : 0]].join(' ');

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
function cubeWeights() {
  const w = [];
  for (let i = 0; i < 8; i++) w.push((i & 1 ? morph : 1 - morph) * (i & 2 ? q : 1 - q) * (i & 4 ? z : 1 - z));
  return w;
}
function cubeReady() { return corners.every(c => c); }
function weightsAt(m, qq, zz) {
  const w = [];
  for (let i = 0; i < 8; i++) w.push((i & 1 ? m : 1 - m) * (i & 2 ? qq : 1 - qq) * (i & 4 ? zz : 1 - zz));
  return w;
}
function cubeXYAt(m, qq, zz) {
  const w = weightsAt(m, qq, zz);
  return [0, 1].map(k => corners.reduce((a, c, i) => a + c.xy[k] * w[i], 0));
}
function solveWheel(u, v) {
  let best = [morph, q], bd = Infinity;
  const err = (m, qq) => { const p = cubeXYAt(m, qq, z); return Math.hypot(p[0] - u, p[1] - v); };
  for (let a = 0; a <= 16; a++) for (let b = 0; b <= 16; b++) {
    const d = err(a / 16, b / 16);
    if (d < bd) { bd = d; best = [a / 16, b / 16]; }
  }
  let step = 1 / 32;
  for (let it = 0; it < 30; it++) {
    let improved = false;
    for (const [dm, dq] of [[step, 0], [-step, 0], [0, step], [0, -step]]) {
      const m = Math.max(0, Math.min(1, best[0] + dm)), qq = Math.max(0, Math.min(1, best[1] + dq));
      const d = err(m, qq);
      if (d < bd) { bd = d; best = [m, qq]; improved = true; }
    }
    if (!improved) step /= 2;
  }
  return best;
}
function bary(p, a, b, c) {
  const v0 = [b[0] - a[0], b[1] - a[1]], v1 = [c[0] - a[0], c[1] - a[1]], v2 = [p[0] - a[0], p[1] - a[1]];
  const den = v0[0] * v1[1] - v1[0] * v0[1];
  if (Math.abs(den) < 1e-12) return null;
  const v = (v2[0] * v1[1] - v1[0] * v2[1]) / den, w = (v0[0] * v2[1] - v2[0] * v0[1]) / den;
  return [1 - v - w, v, w];
}
function planeBlend(u, v) {
  for (const t of TRIS) {
    const b = bary([u, v], FRAMES[t[0]].xy, FRAMES[t[1]].xy, FRAMES[t[2]].xy);
    if (b && b.every(x => x >= -1e-9)) return { parents: t.map(i => FRAMES[i]), weights: b };
  }
  const near = FRAMES.map((f, i) => ({ i, d: Math.hypot(f.xy[0] - u, f.xy[1] - v) })).sort((a, b) => a.d - b.d).slice(0, 3);
  const ws = near.map(n => 1 / (n.d + 1e-6)), sum = ws.reduce((a, b) => a + b, 0);
  return { parents: near.map(n => FRAMES[n.i]), weights: ws.map(w => w / sum) };
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
  const topH = Math.round(VH * 0.5);
  const cubeW = Math.round(VW * 0.42);
  L.plane = { x: 0, y: 0, w: VW, h: topH, bar };
  L.cube = { x: 0, y: topH, w: cubeW, h: VH - topH, bar };
  L.resp = { x: cubeW, y: topH, w: VW - cubeW, h: Math.round((VH - topH) * 0.62), bar };
  L.bars = { x: cubeW, y: topH + L.resp.h, w: VW - cubeW, h: VH - topH - L.resp.h, bar };
  L.planeAx = { x: 44, y: bar + 10, w: VW - 60, h: topH - bar - 34 };
  L.respAx = { x: cubeW + 44, y: topH + bar + 10, w: VW - cubeW - 60, h: L.resp.h - bar - 34 };
}
const pu = u => L.planeAx.x + u * L.planeAx.w, pv = v => L.planeAx.y + (1 - v) * L.planeAx.h;
const rx = f => L.respAx.x + Math.log10(f / 20) / 3 * L.respAx.w, ry = d => L.respAx.y + (30 - d) / 60 * L.respAx.h;
function proj(m, qq, zz) {
  const x = m - 0.5, y = qq - 0.5, d = zz - 0.5;
  const cy = Math.cos(yaw), sy = Math.sin(yaw), cp = Math.cos(pitch), sp = Math.sin(pitch);
  const x1 = x * cy + d * sy, d1 = -x * sy + d * cy;
  const y2 = y * cp - d1 * sp, d2 = y * sp + d1 * cp;
  const c = L.cube, s = Math.min(c.w, c.h - c.bar) * 0.5;
  return [c.x + c.w * 0.5 + x1 * s, c.y + c.bar + (c.h - c.bar) * 0.5 - y2 * s, d2];
}
const cornerXYZ = i => [i & 1 ? 1 : 0, i & 2 ? 1 : 0, i & 4 ? 1 : 0];
const cornerPos = i => proj(...cornerXYZ(i));

function panel(r, title, parent) {
  el('rect', { x: r.x + 0.5, y: r.y + 0.5, width: r.w - 1, height: r.h - 1, fill: 'none', class: 'rule' }, parent);
  el('rect', { x: r.x + 1, y: r.y + 1, width: r.w - 2, height: r.bar - 1, class: 'titlebar' }, parent);
  txt(r.x + 6, r.y + 11, title, parent, 'title');
}
function axes(a, xt, yt, parent) {
  el('rect', { x: a.x + 0.5, y: a.y + 0.5, width: a.w, height: a.h, class: 'axis' }, parent);
  xt.forEach(([px, label]) => {
    el('line', { x1: px, y1: a.y + a.h, x2: px, y2: a.y + a.h + 4, class: 'tick' }, parent);
    el('line', { x1: px, y1: a.y, x2: px, y2: a.y + a.h, class: 'dots' }, parent);
    txt(px - label.length * 3, a.y + a.h + 14, label, parent);
  });
  yt.forEach(([py, label, cls]) => {
    el('line', { x1: a.x - 4, y1: py, x2: a.x, y2: py, class: 'tick' }, parent);
    el('line', { x1: a.x, y1: py, x2: a.x + a.w, y2: py, class: cls || 'dots' }, parent);
    txt(a.x - 8 - label.length * 6, py + 3, label, parent);
  });
}
let g = {};
function build() {
  svg.innerHTML = '';
  layout();
  ['plane', 'planeLive', 'cube', 'cubeLive', 'resp', 'respLive', 'bars', 'barsLive', 'drag'].forEach(k => { g[k] = el('g', {}, svg); });
  panel(L.plane, 'FRAME SPACE', g.plane);
  const a = L.planeAx;
  const octs = [[32.7, 'C1'], [65.4, 'C2'], [130.8, 'C3'], [261.6, 'C4'], [523.3, 'C5'], [1046.5, 'C6'], [2093, 'C7'], [4186, 'C8'], [8372, 'C9']];
  const lg = f => Math.log2(f / 30) / Math.log2(16000 / 30);
  axes(a, octs.map(([f, n]) => [pu(lg(f)), n]), octs.map(([f, n]) => [pv(lg(f)), n]), g.plane);
  txt(a.x + a.w - 90, a.y + a.h - 6, '1st resonance', g.plane);
  txt(a.x + 4, a.y + 10, '2nd resonance', g.plane);
  captured.concat(FRAMES).forEach((f, i) => {
    const cap = i < captured.length;
    const n = el('rect', { x: pu(f.xy[0]) - 2, y: pv(f.xy[1]) - 2, width: 4, height: 4, class: 'node' + (cap ? ' cap' : '') }, g.plane);
    n.dataset.idx = i;
    n.addEventListener('pointerenter', () => { status = f.name + '  ' + f.f12[0] + ' / ' + f.f12[1] + ' Hz'; drawStatus(); });
  });
  panel(L.cube, 'CUBE', g.cube);
  panel(L.resp, 'RESPONSE', g.resp);
  const r = L.respAx;
  axes(r, [50, 100, 200, 500, 1000, 2000, 5000, 10000].map(f => [rx(f), f >= 1000 ? (f / 1000) + 'k' : '' + f]),
    [[ry(20), '+20'], [ry(10), '+10'], [ry(0), '0', 'zero'], [ry(-10), '-10'], [ry(-20), '-20']], g.resp);
  panel(L.bars, 'CONTROL', g.bars);
  drawAll();
}
function drawStatus() {
  const old = g.plane.querySelector('.status');
  if (old) old.remove();
  txt(L.planeAx.x + L.planeAx.w * 0.55, L.plane.y + L.plane.h - 6, status, g.plane, 'status');
}
function thumb(words, x, y, w, h, parent, cls) {
  const pts = curveOf(words, 28).map(([f, d], i) => (x + i / 28 * w).toFixed(1) + ',' + (y + h - (d + 30) / 60 * h).toFixed(1));
  el('polyline', { points: pts.join(' '), class: cls }, parent);
}
function drawCube() {
  g.cubeLive.innerHTML = '';
  const edges = [];
  for (let i = 0; i < 8; i++) for (const b of [1, 2, 4]) if (!(i & b)) edges.push([i, i | b]);
  const P = [];
  for (let i = 0; i < 8; i++) P.push(cornerPos(i));
  edges.sort((e1, e2) => (P[e1[0]][2] + P[e1[1]][2]) - (P[e2[0]][2] + P[e2[1]][2]));
  edges.forEach(([i, j]) => el('line', { x1: P[i][0], y1: P[i][1], x2: P[j][0], y2: P[j][1], class: 'edge' + ((P[i][2] + P[j][2]) < 0 ? ' back' : '') }, g.cubeLive));
  if (cubeReady()) {
    const w = proj(morph, q, z), f = proj(morph, q, 0), fl = proj(morph, 0, z), fm = proj(0, q, z);
    [f, fl, fm].forEach(p => el('line', { x1: w[0], y1: w[1], x2: p[0], y2: p[1], class: 'floor' }, g.cubeLive));
    el('rect', { x: w[0] - 3, y: w[1] - 3, width: 6, height: 6, class: 'wheel' }, g.cubeLive);
  }
  const tw = 54, th = 22;
  for (let i = 0; i < 8; i++) {
    const [cx, cy, depth] = P[i];
    const dx = cx - (L.cube.x + L.cube.w / 2), dy = cy - (L.cube.y + L.cube.bar + (L.cube.h - L.cube.bar) / 2);
    const len = Math.hypot(dx, dy) || 1;
    const bx = cx + dx / len * 46 - tw / 2, by = cy + dy / len * 30 - th / 2;
    const box = el('rect', { x: bx, y: by, width: tw, height: th, class: 'thumb-box' + (i === armed ? ' armed' : '') }, g.cubeLive);
    box.dataset.corner = i;
    box.addEventListener('pointerdown', e => { e.stopPropagation(); armed = i; drawAll(); });
    el('line', { x1: cx, y1: cy, x2: bx + tw / 2, y2: by + th / 2, class: 'edge back' }, g.cubeLive);
    if (corners[i]) thumb(corners[i].words, bx + 2, by + 2, tw - 4, th - 4, g.cubeLive, 'thumb');
    else el('line', { x1: bx + 2, y1: by + th / 2, x2: bx + tw - 2, y2: by + th / 2, class: 'thumb-empty' }, g.cubeLive);
    txt(bx, by - 3, poseName(i), g.cubeLive);
  }
}
function drawBars() {
  g.barsLive.innerHTML = '';
  const b = L.bars, x0 = b.x + 34, w = Math.round(b.w * 0.3), h = 10, y0 = b.y + b.bar + 10;
  [['M', morph, 'bar-m'], ['Q', q, 'bar-f'], ['Z', z, 'bar-t']].forEach(([k, v, cls], i) => {
    const y = y0 + i * (h + 8);
    txt(b.x + 10, y + 9, k, g.barsLive);
    const box = el('rect', { x: x0, y, width: w, height: h, class: 'bar-box' }, g.barsLive);
    box.dataset.bar = i;
    el('rect', { x: x0 + 1, y: y + 1, width: Math.max(0, (w - 2) * v), height: h - 2, class: cls }, g.barsLive);
    txt(x0 + w + 8, y + 9, v.toFixed(3) + '  ' + AXES[i][0] + ' > ' + AXES[i][1], g.barsLive);
  });
  const kx = x0 + w + 180, ky = y0;
  const key = el('rect', { x: kx, y: ky, width: 96, height: 16, class: 'key' }, g.barsLive);
  key.dataset.key = 'capture';
  txt(kx + 6, ky + 12, 'CAPTURE', g.barsLive, 'key-text');
  const key2 = el('rect', { x: kx + 104, y: ky, width: 96, height: 16, class: 'key' }, g.barsLive);
  key2.dataset.key = 'clear';
  txt(kx + 110, ky + 12, 'CLEAR CAPS', g.barsLive, 'key-text');
}
function drawAll() {
  g.planeLive.innerHTML = '';
  g.respLive.innerHTML = '';
  if (probe && cubeReady()) {
    const w = cubeXYAt(morph, q, z);
    corners.forEach(c => el('line', { x1: pu(w[0]), y1: pv(w[1]), x2: pu(c.xy[0]), y2: pv(c.xy[1]), class: 'link' }, g.planeLive));
    el('rect', { x: pu(w[0]) - 4, y: pv(w[1]) - 4, width: 8, height: 8, class: 'probe' }, g.planeLive);
    status = 'M ' + morph.toFixed(2) + ' ' + AXES[0][0] + '>' + AXES[0][1] + '   Q ' + q.toFixed(2) + ' ' + AXES[1][0] + '>' + AXES[1][1] + '   Z ' + z.toFixed(2) + ' ' + AXES[2][0] + '>' + AXES[2][1];
  }
  else if (probe) {
    const { parents, weights } = planeBlend(probe[0], probe[1]);
    el('polygon', { points: parents.map(p => pu(p.xy[0]).toFixed(1) + ',' + pv(p.xy[1]).toFixed(1)).join(' '), class: 'tri' }, g.planeLive);
    parents.forEach(p => el('line', { x1: pu(probe[0]), y1: pv(probe[1]), x2: pu(p.xy[0]), y2: pv(p.xy[1]), class: 'link' }, g.planeLive));
    el('rect', { x: pu(probe[0]) - 4, y: pv(probe[1]) - 4, width: 8, height: 8, class: 'probe' }, g.planeLive);
    const pts = curveOf(blendWords(parents.map(p => p.words), weights), 200).map(([f, d]) => rx(f).toFixed(1) + ',' + ry(d).toFixed(1));
    el('polyline', { points: pts.join(' '), class: 'ghost' }, g.respLive);
    status = parents.map((p, i) => Math.round(weights[i] * 100) + '% ' + p.name).join('  |  ');
  }
  for (let i = 0; i < 8; i++) if (corners[i]) {
    const [cx, cy] = cornerPos(i);
    el('line', { x1: pu(corners[i].xy[0]), y1: pv(corners[i].xy[1]), x2: cx, y2: cy, class: 'thread' }, g.planeLive);
  }
  if (cubeReady()) {
    const pts = curveOf(wheelWords(), 240).map(([f, d]) => rx(f).toFixed(1) + ',' + ry(d).toFixed(1));
    el('polyline', { points: pts.join(' '), class: 'curve' }, g.respLive);
  }
  drawCube();
  drawBars();
  drawStatus();
}
function setCorner(i, frame) { corners[i] = frame; armed = (i + 1) % 8; status = 'corner ' + i + ' = ' + frame.name; drawAll(); }
function capture() {
  if (!cubeReady()) { status = 'fill all eight corners first'; drawStatus(); return; }
  const words = wheelWords(), w = cubeWeights();
  const xy = [0, 1].map(k => corners.reduce((a, c, i) => a + c.xy[k] * w[i], 0));
  const name = 'cap' + (captured.length + 1) + ' m' + morph.toFixed(2) + ' q' + q.toFixed(2) + ' z' + z.toFixed(2);
  captured.push({ name, words, xy: xy.map(v => Math.round(v * 1e4) / 1e4) });
  try { localStorage.setItem('captured', JSON.stringify(captured)); } catch (e) {}
  status = 'captured ' + name;
  build();
}
let mode = null, dragFrame = null, dragPos = null, last = null, barIdx = -1;
function inRect(r, x, y) { return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h; }
function toSvg(e) { const r = svg.getBoundingClientRect(); return [(e.clientX - r.left) / r.width * VW, (e.clientY - r.top) / r.height * VH]; }
function nearestCorner(x, y) {
  let best = -1, bd = 1e9;
  for (let i = 0; i < 8; i++) { const [cx, cy] = cornerPos(i); const d = Math.hypot(cx - x, cy - y); if (d < bd) { bd = d; best = i; } }
  return bd < 40 ? best : -1;
}
function setProbe(x, y) {
  probe = [Math.max(0, Math.min(1, (x - L.planeAx.x) / L.planeAx.w)), Math.max(0, Math.min(1, 1 - (y - L.planeAx.y) / L.planeAx.h))];
  if (cubeReady()) [morph, q] = solveWheel(probe[0], probe[1]);
  drawAll();
}
function setBar(x) {
  const b = L.bars, x0 = b.x + 34, w = Math.round(b.w * 0.3);
  const v = Math.max(0, Math.min(1, (x - x0) / w));
  if (barIdx === 0) morph = v; else if (barIdx === 1) q = v; else z = v;
  drawAll();
}
svg.addEventListener('pointerdown', e => {
  const [x, y] = toSvg(e);
  svg.setPointerCapture(e.pointerId);
  const t = e.target;
  if (t.dataset && t.dataset.key === 'capture') { capture(); return; }
  if (t.dataset && t.dataset.key === 'clear') { captured = []; try { localStorage.removeItem('captured'); } catch (er) {} build(); return; }
  if (t.dataset && t.dataset.bar !== undefined) { mode = 'bar'; barIdx = +t.dataset.bar; setBar(x); return; }
  if (t.classList && t.classList.contains('node')) {
    mode = 'drag'; dragFrame = captured.concat(FRAMES)[+t.dataset.idx]; dragPos = [x, y]; t.classList.add('held'); drawDrag(); return;
  }
  if (inRect(L.planeAx, x, y)) { mode = 'probe'; setProbe(x, y); return; }
  if (inRect(L.cube, x, y)) { mode = 'orbit'; last = [x, y]; return; }
});
function drawDrag() {
  g.drag.innerHTML = '';
  if (!dragPos) return;
  el('line', { x1: pu(dragFrame.xy[0]), y1: pv(dragFrame.xy[1]), x2: dragPos[0], y2: dragPos[1], class: 'thread' }, g.drag);
  thumb(dragFrame.words, dragPos[0] - 27, dragPos[1] - 11, 54, 22, g.drag, 'drag');
  const hot = nearestCorner(dragPos[0], dragPos[1]);
  g.cubeLive.querySelectorAll('.thumb-box').forEach(r => r.classList.toggle('hot', +r.dataset.corner === hot));
  status = 'drag ' + dragFrame.name + (hot >= 0 ? '  ->  corner ' + hot : '');
  drawStatus();
}
svg.addEventListener('pointermove', e => {
  if (!mode) return;
  const [x, y] = toSvg(e);
  if (mode === 'drag') { dragPos = [x, y]; drawDrag(); }
  else if (mode === 'probe') setProbe(x, y);
  else if (mode === 'orbit') { yaw += (x - last[0]) * 0.01; pitch = Math.max(-1.4, Math.min(1.4, pitch + (y - last[1]) * 0.01)); last = [x, y]; drawAll(); }
  else if (mode === 'bar') setBar(x);
});
svg.addEventListener('pointerup', e => {
  if (mode === 'drag') {
    const [x, y] = toSvg(e);
    const hot = nearestCorner(x, y);
    const moved = Math.hypot(x - pu(dragFrame.xy[0]), y - pv(dragFrame.xy[1])) > 6;
    if (hot >= 0) setCorner(hot, dragFrame);
    else if (!moved) setCorner(armed, dragFrame);
    g.drag.innerHTML = '';
    g.plane.querySelectorAll('.held').forEach(n => n.classList.remove('held'));
    dragFrame = null; dragPos = null;
    drawAll();
  }
  mode = null;
});
window.addEventListener('resize', build);
build();
</script>
</body>
</html>
"""


def build():
    frames, tris = collect()
    html = PAGE.replace("__FRAMES__", json.dumps(frames, separators=(",", ":"), ensure_ascii=False)).replace(
        "__TRIS__", json.dumps(tris, separators=(",", ":")))
    out = os.path.join(ROOT, "workstation_min.html")
    with open(out, "w", encoding="utf-8") as fp:
        fp.write(html)
    print(out, len(html), "bytes", len(frames), "frames", len(tris), "triangles")


if __name__ == "__main__":
    build()
