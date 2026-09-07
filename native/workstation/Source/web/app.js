import { getNativeFunction } from "/juce/index.js";

const readState = getNativeFunction("state");
const send = getNativeFunction("dispatch");
const openSpectrogram = getNativeFunction("spectrogram");
const dispatch = (name, ...args) => send(name, ...args);

const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();
const C = { ink: css("--ink"), text: css("--text"), dim: css("--dim"), faint: css("--faint"), grid: css("--grid"), panel: css("--panel"), blue: css("--blue"), orange: css("--orange"), yellow: css("--yellow"), purple: css("--purple"), ground: css("--ground") };
const FONT = '10px "Segoe UI", system-ui, sans-serif';

let S = null;
let anchorPick = -1;
const view = {};

function canvas(id) {
  const c = document.getElementById(id);
  const fit = () => { const r = c.getBoundingClientRect(); const s = devicePixelRatio || 1; c.width = Math.max(1, r.width * s); c.height = Math.max(1, r.height * s); const g = c.getContext("2d"); g.setTransform(s, 0, 0, s, 0, 0); return { g, w: r.width, h: r.height }; };
  return { el: c, fit };
}
const motherC = canvas("motherC"), stageC = canvas("stageC"), bodyC = canvas("bodyC"), paletteC = canvas("paletteC");

function xOfIndex(i, r) { return r.x + (i / 95) * r.w; }
function yOfDb(db, r) { return r.y + r.h * (1 - (Math.min(30, Math.max(-30, db)) + 30) / 60); }

function axes(g, r) {
  g.strokeStyle = C.grid; g.lineWidth = 1;
  for (let db = -30; db <= 30; db += 10) { const y = Math.round(yOfDb(db, r)) + 0.5; g.beginPath(); g.moveTo(r.x, y); g.lineTo(r.x + r.w, y); g.stroke(); }
  for (const hz of [20, 100, 1000, 10000, 20000]) { const i = 95 * Math.log(hz / 20) / Math.log(1000); const x = Math.round(xOfIndex(i, r)) + 0.5; g.beginPath(); g.moveTo(x, r.y); g.lineTo(x, r.y + r.h); g.stroke(); }
  g.strokeStyle = C.faint; const y0 = Math.round(yOfDb(0, r)) + 0.5; g.beginPath(); g.moveTo(r.x, y0); g.lineTo(r.x + r.w, y0); g.stroke();
  g.strokeStyle = C.rule; g.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
}

function curve(g, r, data, colour, width) {
  if (!data || data.length < 2) return;
  g.strokeStyle = colour; g.lineWidth = width; g.beginPath();
  data.forEach((db, i) => { const x = xOfIndex(i, r), y = yOfDb(db, r); if (i === 0) g.moveTo(x, y); else g.lineTo(x, y); });
  g.stroke();
}

function label(g, text, x, y, colour, align = "left") { g.fillStyle = colour; g.font = FONT; g.textAlign = align; g.textBaseline = "middle"; g.fillText(text, x, y); }

function drawStage() {
  const { g, w, h } = stageC.fit();
  const r = { x: 44, y: 30, w: w - 60, h: h - 58 };
  axes(g, r);
  for (let db = -30; db <= 30; db += 10) label(g, (db > 0 ? "+" : "") + db, r.x - 6, yOfDb(db, r), C.dim, "right");
  for (const hz of [20, 100, 1000, 10000, 20000]) { const i = 95 * Math.log(hz / 20) / Math.log(1000); label(g, hz >= 1000 ? (hz / 1000) + "k" : String(hz), xOfIndex(i, r), r.y + r.h + 10, C.dim, "center"); }
  curve(g, r, S.heard, C.blue, 1.6);
  label(g, S.label, 10, 14, C.dim);
  view.stage = r;
}

function railRects(w, h) {
  const top = Math.min(h - 120, Math.round((w - 60) / 2) + 40);
  return [0, 1, 2].map(i => ({ x: 20, y: top + i * 30, w: w - 120, h: 20 }));
}

function drawMother() {
  const { g, w, h } = motherC.fit();
  const cw = Math.max(80, Math.round((w - 60) / 2));
  const ch = Math.min(cw, h - 130);
  const cells = [{ x: 20, y: 24, w: cw, h: ch }, { x: w - 20 - cw, y: 24, w: cw, h: ch }];
  const names = [S.pair.a, S.pair.b], curves = [S.pair.aCurve, S.pair.bCurve];
  cells.forEach((c, i) => {
    const lit = anchorPick === i;
    label(g, names[i] || "", c.x, c.y - 10, lit ? C.ink : C.text);
    label(g, "∨", c.x + c.w - 6, c.y - 10, C.dim, "right");
    const plot = { x: c.x, y: c.y, w: c.w, h: c.h };
    axes(g, plot);
    curve(g, plot, curves[i], lit ? C.orange : C.blue, 1.2);
  });
  const rails = railRects(w, h);
  const at = [S.pair.morph, S.pair.frequency, S.pair.stress];
  const words = ["MORPH", "FREQUENCY", "STRESS"];
  const readings = [Math.round(at[0] * 100), ((S.pair.octaves * at[1]) >= 0 ? "+" : "") + (S.pair.octaves * at[1]).toFixed(2) + " oct", Math.round(at[2] * 100)];
  rails.forEach((r, i) => {
    label(g, words[i], r.x, r.y - 8, C.dim);
    label(g, String(readings[i]), r.x + r.w, r.y - 8, S.pair.live ? C.text : C.dim, "right");
    g.strokeStyle = C.faint; g.beginPath(); g.moveTo(r.x, r.y + r.h / 2 + 0.5); g.lineTo(r.x + r.w, r.y + r.h / 2 + 0.5); g.stroke();
    const x = r.x + r.w * at[i], y = r.y + r.h / 2;
    g.fillStyle = C.panel; g.strokeStyle = S.pair.live ? C.blue : C.dim; g.lineWidth = 1.8;
    g.beginPath(); g.moveTo(x, y - 8); g.lineTo(x + 8, y); g.lineTo(x, y + 8); g.lineTo(x - 8, y); g.closePath(); g.fill(); g.stroke();
    if (S.pair.live) { g.fillStyle = C.blue; g.beginPath(); g.arc(x, y, 2, 0, Math.PI * 2); g.fill(); }
  });
  label(g, (S.pair.octaves >= 0 ? "+" : "") + S.pair.octaves.toFixed(1) + " oct", rails[1].x + rails[1].w + 8, rails[1].y + 10, C.dim);
  label(g, "BAKE", w - 20, rails[2].y + 10, S.pair.live ? C.ink : C.faint, "right");
  view.mother = { cells, rails, bake: { x: w - 60, y: rails[2].y, w: 40, h: 20 } };
}

function drawBody() {
  const { g, w, h } = bodyC.fit();
  const side = Math.max(80, Math.min(h - 96, 176));
  const cell = Math.round(side / 2) - 4;
  const grid = { x: 12, y: 30, w: side, h: side };
  const cells = [];
  S.corners.forEach((c, i) => {
    const col = i % 2, row = Math.floor(i / 2);
    const r = { x: grid.x + col * (cell + 8), y: grid.y + row * (cell + 8), w: cell, h: cell };
    cells.push(r);
    const lit = S.pad.editing === i || (S.pad.onCorner && S.pad.working === i);
    label(g, c.letter + "  " + c.name, r.x, r.y - 9, lit ? C.ink : C.dim);
    g.strokeStyle = lit ? C.rule : C.faint; g.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
    g.strokeStyle = C.faint; const y0 = Math.round(yOfDb(0, r)) + 0.5; g.beginPath(); g.moveTo(r.x, y0); g.lineTo(r.x + r.w, y0); g.stroke();
    curve(g, r, c.curve, C.blue, 1);
  });
  const pad = { x: grid.x, y: grid.y, w: cell * 2 + 8, h: cell * 2 + 8 };
  const px = pad.x + pad.w * S.pad.morph / 100, py = pad.y + pad.h * (1 - S.pad.q / 100);
  g.fillStyle = C.panel; g.strokeStyle = S.pad.complete ? C.blue : C.dim; g.lineWidth = 1.8;
  g.beginPath(); g.moveTo(px, py - 7); g.lineTo(px + 7, py); g.lineTo(px, py + 7); g.lineTo(px - 7, py); g.closePath(); g.fill(); g.stroke();
  const right = pad.x + pad.w + 20;
  label(g, S.label, right, 14, C.text);
  const noteName = (m) => ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][m % 12] + (Math.floor(m / 12) - 1);
  const sources = [["PLUCK", S.source.which === 3], ["SAW " + noteName(S.source.note), S.source.which === 0], ["NOISE", S.source.which === 1], ["LOOP", S.source.which === 2, !S.source.loop], [S.source.fixed ? "FIXED" : "KEY", false]];
  let x = right;
  const sourceRects = [];
  g.font = FONT;
  sources.forEach(([word, on, faint], i) => {
    const tw = g.measureText(word).width;
    label(g, word, x, 40, faint ? C.faint : on ? C.ink : i === 4 ? C.text : C.dim);
    sourceRects.push({ x, y: 32, w: tw, h: 16 });
    x += tw + (i === 3 ? 18 : 10);
  });
  label(g, "WRITE", w - 12, 40, C.dim, "right");
  const write = { x: w - 50, y: 32, w: 40, h: 16 };
  const keyboard = { x: 12, y: grid.y + grid.h + 12, w: w - 24, h: Math.max(44, h - grid.y - grid.h - 24) };
  drawKeyboard(g, keyboard);
  view.body = { pad, cells, sourceRects, write, keyboard };
}

const OFFSETS = [0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6];
const BLACK = new Set([1, 3, 6, 8, 10]);
function pianoKey(midi, r) {
  const pitch = midi % 12, white = Math.floor((midi - 36) / 12) * 7 + OFFSETS[pitch];
  const left = r.x + r.w * white / 21, rightEdge = r.x + r.w * (white + 1) / 21;
  if (BLACK.has(pitch)) return { x: rightEdge - (rightEdge - left) / 3, y: r.y, w: Math.max(4, (rightEdge - left) * 2 / 3), h: r.h * 3 / 5, black: true };
  return { x: left, y: r.y, w: rightEdge - left, h: r.h, black: false };
}
function noteAt(p, r) {
  for (let m = 36; m <= 71; m++) { const k = pianoKey(m, r); if (k.black && p.x >= k.x && p.x < k.x + k.w && p.y >= k.y && p.y < k.y + k.h) return m; }
  for (let m = 36; m <= 71; m++) { const k = pianoKey(m, r); if (p.x >= k.x && p.x < k.x + k.w && p.y >= k.y && p.y < k.y + k.h) return m; }
  return -1;
}
function drawKeyboard(g, r) {
  g.strokeStyle = C.faint; g.lineWidth = 1;
  g.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
  for (let white = 1; white < 21; white++) { const x = Math.round(r.x + r.w * white / 21) + 0.5; g.beginPath(); g.moveTo(x, r.y); g.lineTo(x, r.y + r.h); g.stroke(); }
  for (let m = 36; m <= 71; m++) { const k = pianoKey(m, r); if (!k.black) continue; g.fillStyle = C.panel; g.fillRect(k.x, k.y, k.w, k.h); g.strokeRect(k.x + 0.5, k.y + 0.5, k.w - 1, k.h - 1); }
  for (let m = 36; m <= 71; m += 12) { const k = pianoKey(m, r); label(g, "C" + (m / 12 - 1), k.x + 3, r.y + r.h - 8, C.dim); }
  if (S.source.held >= 0) { const k = pianoKey(S.source.held, r); g.fillStyle = k.black ? C.blue : C.ink; g.fillRect(k.x + 1, k.y + 1, k.w - 2, k.h - 2); }
}

function drawPalette() {
  const { g, w, h } = paletteC.fit();
  label(g, "Keep", w - 12, 14, S.pad.complete || S.pair.live ? C.ink : C.faint, "right");
  label(g, "drop .wav", w / 2, h - 12, C.dim, "center");
  g.strokeStyle = C.faint; g.strokeRect(8.5, h - 24.5, w - 16, 20);
  view.palette = { keep: { x: w - 50, y: 4, w: 40, h: 20 } };
}

let listKey = "";
const openFamilies = new Set();
function drawList() {
  const families = [];
  const byFamily = new Map();
  for (const c of S.cards) { if (!byFamily.has(c.family)) { byFamily.set(c.family, []); families.push(c.family); } byFamily.get(c.family).push(c); }
  const key = families.join("|") + "#" + S.cards.length + "#" + S.selected + "#" + S.auditioning + "#" + [...openFamilies].join(",");
  if (key === listKey) return;
  listKey = key;
  const list = document.getElementById("list");
  list.innerHTML = "";
  families.forEach((f, i) => {
    const open = openFamilies.has(f) || (openFamilies.size === 0 && i === 0);
    const head = document.createElement("div");
    head.className = "head";
    head.innerHTML = `<span>${f.toUpperCase()}</span><span>${byFamily.get(f).length}</span>`;
    head.onclick = () => { if (openFamilies.has(f)) openFamilies.delete(f); else openFamilies.add(f); listKey = ""; };
    list.appendChild(head);
    if (!open) return;
    for (const c of byFamily.get(f)) {
      const row = document.createElement("div");
      row.className = "card" + (c.star === S.selected || c.star === S.auditioning ? " on" : "");
      row.innerHTML = `<span>${c.name}</span><span class="k">${c.kind}</span>`;
      row.onclick = () => { if (anchorPick >= 0) { dispatch("setPair", anchorPick, c.star); anchorPick = -1; } else dispatch("select", c.star); };
      list.appendChild(row);
    }
  });
}

function drawAll() {
  if (!S) return;
  drawStage(); drawMother(); drawBody(); drawPalette(); drawList();
  document.getElementById("status").textContent = S.status && (S.status.startsWith("cannot") || S.status.startsWith("no audio")) ? S.status : "";
}

const inside = (p, r) => r && p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
const local = (e, el) => { const b = el.getBoundingClientRect(); return { x: e.clientX - b.left, y: e.clientY - b.top }; };

let drag = null;
bodyC.el.addEventListener("pointerdown", (e) => {
  const p = local(e, bodyC.el), v = view.body;
  if (!v) return;
  if (inside(p, v.keyboard)) { const m = noteAt(p, v.keyboard); if (m >= 0) { dispatch("noteOn", m); drag = { kind: "key" }; bodyC.el.setPointerCapture(e.pointerId); } return; }
  for (let i = 0; i < v.sourceRects.length; i++) if (inside(p, v.sourceRects[i])) {
    if (i === 0) dispatch("setSource", 3); else if (i === 1) dispatch("setSource", 0); else if (i === 2) dispatch("setSource", 1);
    else if (i === 3) { if (S.source.loop) dispatch("setSource", 2); } else dispatch("setTracking", !S.source.fixed);
    return;
  }
  if (inside(p, v.write)) { dispatch("write"); return; }
  if (inside(p, v.pad)) { drag = { kind: "pad" }; bodyC.el.setPointerCapture(e.pointerId); movePad(p); }
});
function movePad(p) { const r = view.body.pad; dispatch("setPuck", 100 * (p.x - r.x) / r.w, 100 * (1 - (p.y - r.y) / r.h)); }
bodyC.el.addEventListener("pointermove", (e) => { if (drag && drag.kind === "pad") movePad(local(e, bodyC.el)); });
bodyC.el.addEventListener("pointerup", (e) => { if (drag && drag.kind === "key") dispatch("noteOff"); drag = null; });

motherC.el.addEventListener("pointerdown", (e) => {
  const p = local(e, motherC.el), v = view.mother;
  if (!v) return;
  for (let i = 0; i < 2; i++) if (inside(p, v.cells[i]) || inside(p, { x: v.cells[i].x, y: v.cells[i].y - 20, w: v.cells[i].w, h: 20 })) {
    anchorPick = anchorPick === i ? -1 : i;
    const star = i === 0 ? S.pair.aStar : S.pair.bStar;
    if (anchorPick >= 0 && star >= 0) dispatch("select", star);
    return;
  }
  if (inside(p, v.bake)) { dispatch("bake"); return; }
  for (let i = 0; i < 3; i++) if (inside(p, { x: v.rails[i].x, y: v.rails[i].y - 6, w: v.rails[i].w, h: v.rails[i].h + 12 })) { drag = { kind: "rail", i }; motherC.el.setPointerCapture(e.pointerId); moveRail(i, p); return; }
});
function moveRail(i, p) {
  const r = view.mother.rails[i], t = Math.min(1, Math.max(0, (p.x - r.x) / r.w));
  if (i === 0) dispatch("sweep", t); else if (i === 1) dispatch("setProbe", S.pair.morph, t, S.pair.stress); else dispatch("setProbe", S.pair.morph, S.pair.frequency, t);
}
motherC.el.addEventListener("pointermove", (e) => { if (drag && drag.kind === "rail") moveRail(drag.i, local(e, motherC.el)); });
motherC.el.addEventListener("pointerup", () => { drag = null; });
motherC.el.addEventListener("wheel", (e) => { const p = local(e, motherC.el), v = view.mother; if (v && inside(p, { x: v.rails[1].x, y: v.rails[1].y - 6, w: v.rails[1].w + 70, h: v.rails[1].h + 12 })) dispatch("setOctaves", S.pair.octaves + (e.deltaY > 0 ? -0.25 : 0.25)); });

paletteC.el.addEventListener("pointerdown", (e) => { const p = local(e, paletteC.el); if (view.palette && inside(p, view.palette.keep)) dispatch("keep"); });

const ROW = { KeyZ: 0, KeyS: 1, KeyX: 2, KeyD: 3, KeyC: 4, KeyV: 5, KeyG: 6, KeyB: 7, KeyH: 8, KeyN: 9, KeyJ: 10, KeyM: 11, Comma: 12, KeyL: 13, Period: 14 };
let keyOctave = 48, downKey = null;
window.addEventListener("keydown", (e) => {
  if (e.repeat) return;
  if (e.ctrlKey && e.code === "KeyG") { openSpectrogram(); return; }
  if (e.ctrlKey && e.code === "KeyK") { dispatch("keep"); return; }
  if (e.ctrlKey && e.code === "KeyZ") { dispatch("undo"); return; }
  if (e.ctrlKey && e.code === "KeyY") { dispatch("redo"); return; }
  if (e.ctrlKey && e.code === "KeyW") { dispatch("write"); return; }
  if (e.code === "Space") { dispatch("setPlaying", !S.source.playing); e.preventDefault(); return; }
  if (e.code === "PageUp") { keyOctave = Math.min(96, keyOctave + 12); return; }
  if (e.code === "PageDown") { keyOctave = Math.max(0, keyOctave - 12); return; }
  if (e.code === "Digit1" || e.code === "Digit2" || e.code === "Digit3" || e.code === "Digit4") { dispatch("toCorner", Number(e.code.slice(-1)) - 1); return; }
  if (e.code === "Delete") { dispatch("removeAdded"); return; }
  if (e.code === "Escape") { anchorPick = -1; return; }
  if (e.code === "ArrowLeft") dispatch("nudge", e.ctrlKey ? -0.2 : -1, 0);
  if (e.code === "ArrowRight") dispatch("nudge", e.ctrlKey ? 0.2 : 1, 0);
  if (e.code === "ArrowUp") dispatch("nudge", 0, e.ctrlKey ? 0.2 : 1);
  if (e.code === "ArrowDown") dispatch("nudge", 0, e.ctrlKey ? -0.2 : -1);
  if (e.code in ROW && !e.ctrlKey && downKey === null) { downKey = e.code; dispatch("noteOn", keyOctave + ROW[e.code]); }
});
window.addEventListener("keyup", (e) => { if (e.code === downKey) { downKey = null; dispatch("noteOff"); } });

window.__JUCE__.backend.addEventListener("state", (s) => { S = s; drawAll(); });
readState().then((s) => { S = s; drawAll(); });
window.addEventListener("resize", drawAll);
