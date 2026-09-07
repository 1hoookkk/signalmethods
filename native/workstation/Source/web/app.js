import { getNativeFunction } from "/juce/index.js";

const readState = getNativeFunction("state");
const send = getNativeFunction("dispatch");
const dispatch = (name, ...args) => send(name, ...args);

const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();
const C = { ink: css("--ink"), text: css("--text"), dim: css("--dim"), faint: css("--faint"), grid: css("--grid"), panel: css("--panel"), rule: css("--rule"), blue: css("--blue"), orange: css("--orange"), yellow: css("--yellow") };
const FONT = '10px "Segoe UI", system-ui, sans-serif';

let S = null;
const view = {};
const plotEl = document.getElementById("plot");

const sizes = new Map();
function fit(c) {
  const r = c.getBoundingClientRect(), s = devicePixelRatio || 1;
  const key = r.width + "x" + r.height + "@" + s;
  if (sizes.get(c) !== key) { c.width = Math.max(1, r.width * s); c.height = Math.max(1, r.height * s); sizes.set(c, key); }
  const g = c.getContext("2d"); g.setTransform(s, 0, 0, s, 0, 0);
  g.clearRect(0, 0, r.width, r.height);
  return { g, w: r.width, h: r.height };
}
function label(g, text, x, y, colour, align = "left") { g.fillStyle = colour; g.font = FONT; g.textAlign = align; g.textBaseline = "middle"; g.fillText(text, x, y); }

const octaveOf = (hz) => Math.log2(Math.max(20, Math.min(20000, hz)) / 20);
const hzOf = (octave) => 20 * Math.pow(2, Math.max(0, Math.min(10, octave)));
const resonanceDb = (r) => 20 * Math.log10(1 / Math.max(1e-5, 1 - Math.min(0.99999, r)));
const radiusOf = (db) => 1 - Math.pow(10, -Math.max(0, Math.min(100, db)) / 20);
const DB_MAX = 60;

function armadilloPoint(hz, r, a) {
  const theta = Math.PI * (1 - octaveOf(hz) / 10);
  const rho = a.R * Math.min(1, resonanceDb(r) / DB_MAX);
  return { x: a.cx + rho * Math.cos(theta), y: a.cy - rho * Math.sin(theta) };
}
function armadilloInverse(p, a) {
  const dx = p.x - a.cx, dy = a.cy - p.y;
  const rho = Math.min(a.R, Math.hypot(dx, dy)), theta = Math.atan2(Math.max(0, dy), dx);
  return { hz: hzOf(10 * (1 - theta / Math.PI)), r: radiusOf(DB_MAX * rho / a.R) };
}

function drawArmadillo(g, a) {
  g.strokeStyle = C.grid; g.lineWidth = 1;
  for (let db = 20; db <= DB_MAX; db += 20) { const rho = a.R * db / DB_MAX; g.beginPath(); g.arc(a.cx, a.cy, rho, Math.PI, 2 * Math.PI); g.stroke(); label(g, db + " dB", a.cx + 4, a.cy - rho - 7, C.dim); }
  for (let oct = 0; oct <= 10; oct += 1) {
    const theta = Math.PI * (1 - oct / 10);
    g.strokeStyle = oct % 5 === 0 ? C.faint : C.grid;
    g.beginPath(); g.moveTo(a.cx, a.cy); g.lineTo(a.cx + a.R * Math.cos(theta), a.cy - a.R * Math.sin(theta)); g.stroke();
    const hz = hzOf(oct);
    label(g, hz >= 1000 ? (hz / 1000) + "k" : String(Math.round(hz)), a.cx + (a.R + 14) * Math.cos(theta), a.cy - (a.R + 14) * Math.sin(theta), C.dim, "center");
  }
  g.strokeStyle = C.rule; g.beginPath(); g.moveTo(a.cx - a.R, a.cy + 0.5); g.lineTo(a.cx + a.R, a.cy + 0.5); g.stroke();
  const st = S.stage;
  st.sections.forEach((s, row) => {
    const lit = view.hot && view.hot.row === row;
    if (s.zero && s.zeroR > 0.01) {
      const z = armadilloPoint(s.zeroHz, s.zeroR, a);
      g.strokeStyle = lit && view.hot.zero ? C.ink : C.orange; g.lineWidth = 1.2;
      g.strokeRect(z.x - 4, z.y - 4, 8, 8);
      if (s.pole) { const p = armadilloPoint(s.poleHz, s.poleR, a); g.strokeStyle = C.faint; g.beginPath(); g.moveTo(p.x, p.y); g.lineTo(z.x, z.y); g.stroke(); }
    }
    if (s.pole && s.poleR > 0.01) {
      const p = armadilloPoint(s.poleHz, s.poleR, a);
      g.fillStyle = lit && !view.hot.zero ? C.ink : C.blue;
      g.beginPath(); g.arc(p.x, p.y, 4, 0, 2 * Math.PI); g.fill();
      label(g, String(row + 1), p.x + 7, p.y - 7, C.text);
    }
  });
}

function xOfIndex(i, r) { return r.x + (i / 95) * r.w; }
function yOfDb(db, r) { return r.y + r.h * (1 - (Math.min(30, Math.max(-30, db)) + 30) / 60); }
function drawResponse(g, r) {
  g.strokeStyle = C.grid; g.lineWidth = 1;
  for (let db = -30; db <= 30; db += 10) { const y = Math.round(yOfDb(db, r)) + 0.5; g.beginPath(); g.moveTo(r.x, y); g.lineTo(r.x + r.w, y); g.stroke(); label(g, (db > 0 ? "+" : "") + db, r.x - 6, y, C.dim, "right"); }
  for (const hz of [20, 100, 1000, 10000, 20000]) { const x = Math.round(xOfIndex(95 * Math.log(hz / 20) / Math.log(1000), r)) + 0.5; g.beginPath(); g.moveTo(x, r.y); g.lineTo(x, r.y + r.h); g.stroke(); label(g, hz >= 1000 ? (hz / 1000) + "k" : String(hz), x, r.y + r.h + 10, C.dim, "center"); }
  g.strokeStyle = C.faint; const y0 = Math.round(yOfDb(0, r)) + 0.5; g.beginPath(); g.moveTo(r.x, y0); g.lineTo(r.x + r.w, y0); g.stroke();
  g.strokeStyle = C.rule; g.strokeRect(r.x + 0.5, r.y + 0.5, r.w - 1, r.h - 1);
  const data = S.stage.curve;
  g.strokeStyle = S.stage.editable ? C.blue : C.dim; g.lineWidth = 1.6; g.beginPath();
  data.forEach((db, i) => { const x = xOfIndex(i, r), y = yOfDb(db, r); if (i === 0) g.moveTo(x, y); else g.lineTo(x, y); });
  g.stroke();
}

const cubeEl = document.getElementById("cubeC");
let anchorPick = -1;
function drawCube() {
  const { g, w, h } = fit(cubeEl);
  const cw = Math.max(60, Math.round((w - 36) / 2));
  const ch = Math.max(50, Math.min(cw, h - 150));
  const cells = [{ x: 12, y: 26, w: cw, h: ch }, { x: w - 12 - cw, y: 26, w: cw, h: ch }];
  const names = [S.pair.a, S.pair.b], curves = [S.pair.aCurve, S.pair.bCurve], stars = [S.pair.aStar, S.pair.bStar];
  cells.forEach((c, i) => {
    const target = S.pair.anchorTarget === i, picking = anchorPick === i;
    label(g, (i === 0 ? "A  " : "B  ") + (names[i] || (picking ? "click a card" : "")), c.x, c.y - 10, target || picking ? C.ink : C.text);
    g.strokeStyle = target ? C.rule : picking ? C.orange : C.faint; g.lineWidth = 1; g.strokeRect(c.x + 0.5, c.y + 0.5, c.w - 1, c.h - 1);
    g.strokeStyle = C.faint; const y0 = Math.round(yOfDb(0, c)) + 0.5; g.beginPath(); g.moveTo(c.x, y0); g.lineTo(c.x + c.w, y0); g.stroke();
    if (curves[i]) { g.strokeStyle = target ? C.orange : C.blue; g.lineWidth = 1.2; g.beginPath(); curves[i].forEach((db, k) => { const x = xOfIndex(k, c), y = yOfDb(db, c); if (k === 0) g.moveTo(x, y); else g.lineTo(x, y); }); g.stroke(); }
  });
  const top = cells[0].y + ch + 28;
  const rails = [0, 1, 2].map(i => ({ x: 12, y: top + i * 34, w: w - 96, h: 20 }));
  const at = [S.pair.morph, S.pair.frequency, S.pair.stress];
  const words = ["MORPH", "FREQUENCY", "STRESS"];
  const readings = [Math.round(at[0] * 100), ((S.pair.octaves * at[1]) >= 0 ? "+" : "") + (S.pair.octaves * at[1]).toFixed(2) + " oct", Math.round(at[2] * 100)];
  rails.forEach((r, i) => {
    label(g, words[i], r.x, r.y - 8, C.dim);
    label(g, String(readings[i]), r.x + r.w, r.y - 8, S.pair.live ? C.text : C.dim, "right");
    g.strokeStyle = C.faint; g.lineWidth = 1; g.beginPath(); g.moveTo(r.x, r.y + r.h / 2 + 0.5); g.lineTo(r.x + r.w, r.y + r.h / 2 + 0.5); g.stroke();
    const x = r.x + r.w * at[i], y = r.y + r.h / 2;
    g.fillStyle = C.panel; g.strokeStyle = S.pair.live ? C.blue : C.dim; g.lineWidth = 1.8;
    g.beginPath(); g.moveTo(x, y - 8); g.lineTo(x + 8, y); g.lineTo(x, y + 8); g.lineTo(x - 8, y); g.closePath(); g.fill(); g.stroke();
  });
  label(g, (S.pair.octaves >= 0 ? "+" : "") + S.pair.octaves.toFixed(1) + " oct", rails[1].x + rails[1].w + 8, rails[1].y + 10, C.dim);
  label(g, "BAKE", w - 12, rails[2].y + 10, S.pair.live ? C.ink : C.faint, "right");
  label(g, "CUBE", 12, 12, C.dim);
  label(g, "1 and 2 make what plays an anchor", w - 12, 12, C.faint, "right");
  view.cube = { cells, rails, bake: { x: w - 52, y: rails[2].y, w: 40, h: 20 }, stars };
}

function drawPlot() {
  const { g, w, h } = fit(plotEl);
  const responseH = Math.max(90, Math.round(h * 0.3));
  const R = Math.max(40, Math.min((w - 80) / 2, h - responseH - 70));
  const a = { cx: w / 2, cy: 30 + R, R };
  drawArmadillo(g, a);
  const r = { x: 44, y: a.cy + 34, w: w - 60, h: h - a.cy - 34 - 24 };
  drawResponse(g, r);
  label(g, S.stage.editable ? "ARMADILLO" : "ARMADILLO  (the probe, listening only)", 12, 14, S.stage.editable ? C.ink : C.dim);
  label(g, "hold Z to hear", w - 12, 14, S.source.held >= 0 ? C.ink : C.dim, "right");
  view.a = a; view.response = r;
}

let wordsKey = "";
function drawWords() {
  const el = document.getElementById("words");
  const key = JSON.stringify(S.stage.sections) + (view.hot ? view.hot.row + ":" + view.hot.zero : "");
  if (key === wordsKey) return;
  wordsKey = key;
  const rows = [`<div class="h"></div><div class="h">POLE</div><div class="h">RESONANCE</div><div class="h">ZERO</div><div class="h">DEPTH</div><div class="h">WORDS</div>`];
  S.stage.sections.forEach((s, i) => {
    const lit = view.hot && view.hot.row === i ? " lit" : "";
    const pole = s.pole && s.poleR > 0.01 ? `${Math.round(s.poleHz)} Hz` : "";
    const res = s.pole && s.poleR > 0.01 ? `${resonanceDb(s.poleR).toFixed(1)} dB` : "";
    const zero = s.zero && s.zeroR > 0.01 ? `${Math.round(s.zeroHz)} Hz` : "";
    const depth = s.zero && s.zeroR > 0.01 ? `${resonanceDb(s.zeroR).toFixed(1)} dB` : "";
    const off = !(s.pole && s.poleR > 0.01) ? " off" : "";
    rows.push(`<div class="n${lit}">${i + 1}</div><div class="${lit}${off}">${pole}</div><div class="${lit}${off}">${res}</div><div class="${lit}${off}">${zero}</div><div class="${lit}${off}">${depth}</div><div class="n">${s.words.map(w => w.toString(16).padStart(4, "0")).join(" ")}</div>`);
  });
  el.innerHTML = rows.join("");
}

let listKey = "";
const openFamilies = new Set();
function drawList() {
  const families = [], byFamily = new Map();
  for (const c of S.cards) { if (!byFamily.has(c.family)) { byFamily.set(c.family, []); families.push(c.family); } byFamily.get(c.family).push(c); }
  const pinned = S.corners[0].star;
  const key = families.join("|") + "#" + S.cards.length + "#" + pinned + "#" + [...openFamilies].join(",");
  if (key === listKey) return;
  listKey = key;
  const list = document.getElementById("list");
  list.innerHTML = "";
  families.forEach((f) => {
    const open = openFamilies.has(f);
    const head = document.createElement("div");
    head.className = "head";
    head.innerHTML = `<span>${f.toUpperCase()}</span><span>${byFamily.get(f).length}</span>`;
    head.onclick = () => { if (openFamilies.has(f)) openFamilies.delete(f); else openFamilies.add(f); listKey = ""; drawList(); };
    list.appendChild(head);
    if (!open) return;
    for (const c of byFamily.get(f)) {
      const row = document.createElement("div");
      row.className = "card" + (c.star === pinned ? " on" : "");
      row.textContent = c.name;
      row.onclick = () => {
        if (anchorPick >= 0) { dispatch("setPair", anchorPick, c.star); anchorPick = -1; drawAll(); return; }
        dispatch("pinCorner", 0, c.star); dispatch("setPuck", 0, 100);
      };
      list.appendChild(row);
    }
  });
}

function drawAll() {
  if (!S) return;
  document.getElementById("name").textContent = S.corners[0].name || "";
  drawCube(); drawPlot(); drawWords(); drawList();
}

const localTo = (e, el) => { const b = el.getBoundingClientRect(); return { x: e.clientX - b.left, y: e.clientY - b.top }; };
const inside = (p, r) => r && p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
let railDrag = -1;
cubeEl.addEventListener("pointerdown", (e) => {
  const p = localTo(e, cubeEl), v = view.cube;
  if (!v) return;
  for (let i = 0; i < 2; i++) if (inside(p, v.cells[i]) || inside(p, { x: v.cells[i].x, y: v.cells[i].y - 20, w: v.cells[i].w, h: 20 })) {
    if (v.stars[i] >= 0 && anchorPick !== i) { anchorPick = -1; dispatch("editAnchor", i); }
    else anchorPick = anchorPick === i ? -1 : i;
    drawAll(); return;
  }
  if (inside(p, v.bake)) { dispatch("bake"); return; }
  for (let i = 0; i < 3; i++) if (inside(p, { x: v.rails[i].x, y: v.rails[i].y - 6, w: v.rails[i].w, h: v.rails[i].h + 12 })) { railDrag = i; cubeEl.setPointerCapture(e.pointerId); moveRail(i, p); return; }
});
let railPending = null, railInFlight = false;
function flushRail() {
  if (!railPending || railInFlight) return;
  const { i, t } = railPending; railPending = null; railInFlight = true;
  const call = i === 0 ? dispatch("sweep", t) : i === 1 ? dispatch("setProbe", S.pair.morph, t, S.pair.stress) : dispatch("setProbe", S.pair.morph, S.pair.frequency, t);
  Promise.resolve(call).finally(() => { railInFlight = false; if (railPending) requestAnimationFrame(flushRail); });
}
function moveRail(i, p) { const r = view.cube.rails[i]; railPending = { i, t: Math.min(1, Math.max(0, (p.x - r.x) / r.w)) }; requestAnimationFrame(flushRail); }
cubeEl.addEventListener("pointermove", (e) => { if (railDrag >= 0) moveRail(railDrag, localTo(e, cubeEl)); });
cubeEl.addEventListener("pointerup", () => { railDrag = -1; });
cubeEl.addEventListener("wheel", (e) => { const p = localTo(e, cubeEl), v = view.cube; if (v && inside(p, { x: v.rails[1].x, y: v.rails[1].y - 6, w: v.rails[1].w + 70, h: v.rails[1].h + 12 })) dispatch("setOctaves", S.pair.octaves + (e.deltaY > 0 ? -0.25 : 0.25)); });

const local = (e) => { const b = plotEl.getBoundingClientRect(); return { x: e.clientX - b.left, y: e.clientY - b.top }; };
function handleAt(p) {
  let best = null, bestD = 9;
  S.stage.sections.forEach((s, row) => {
    if (s.pole && s.poleR > 0.01) { const q = armadilloPoint(s.poleHz, s.poleR, view.a); const d = Math.hypot(q.x - p.x, q.y - p.y); if (d < bestD) { bestD = d; best = { row, zero: false }; } }
    if (s.zero && s.zeroR > 0.01) { const q = armadilloPoint(s.zeroHz, s.zeroR, view.a); const d = Math.hypot(q.x - p.x, q.y - p.y); if (d < bestD) { bestD = d; best = { row, zero: true }; } }
  });
  return best;
}
let drag = null;
plotEl.addEventListener("pointerdown", (e) => {
  if (!S || !S.stage.editable) return;
  const p = local(e), h = handleAt(p);
  if (!h) return;
  drag = h; view.hot = h; plotEl.setPointerCapture(e.pointerId);
});
let pending = null, inFlight = false;
function flush() {
  if (!pending || inFlight) return;
  const { row, zero, g } = pending; pending = null; inFlight = true;
  const s = S.stage.sections[row];
  const call = zero ? dispatch("setSection", S.stage.target, row, s.poleHz, s.poleR, g.hz, g.r) : dispatch("setSection", S.stage.target, row, g.hz, g.r, s.zeroHz, s.zeroR);
  Promise.resolve(call).finally(() => { inFlight = false; if (pending) requestAnimationFrame(flush); });
}
plotEl.addEventListener("pointermove", (e) => {
  const p = local(e);
  if (drag) {
    pending = { row: drag.row, zero: drag.zero, g: armadilloInverse(p, view.a) };
    requestAnimationFrame(flush);
    return;
  }
  const h = view.a ? handleAt(p) : null;
  if ((h && h.row) !== (view.hot && view.hot.row) || (h && h.zero) !== (view.hot && view.hot.zero)) { view.hot = h; drawAll(); }
});
plotEl.addEventListener("pointerup", () => { drag = null; });

let down = false;
window.addEventListener("keydown", (e) => {
  if (e.repeat) return;
  if (e.code === "KeyZ" && !e.ctrlKey && !down) { down = true; dispatch("noteOn", 48); }
  if (e.ctrlKey && e.code === "KeyZ") dispatch("undo");
  if (e.ctrlKey && e.code === "KeyY") dispatch("redo");
  if (e.ctrlKey && e.code === "KeyW") dispatch("write");
  if (e.code === "Digit1" || e.code === "Digit2") { anchorPick = -1; dispatch("anchorFromPlays", Number(e.code.slice(-1)) - 1); }
  if (e.code === "Escape") { anchorPick = -1; drawAll(); }
});
window.addEventListener("keyup", (e) => { if (e.code === "KeyZ" && down) { down = false; dispatch("noteOff"); } });

window.__JUCE__.backend.addEventListener("state", (s) => { S = s; drawAll(); });
readState().then((s) => { S = s; if (S.source.which !== 3) dispatch("setSource", 3); drawAll(); });
window.addEventListener("resize", () => { sizes.clear(); drawAll(); });
