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

function fit(c) {
  const r = c.getBoundingClientRect(), s = devicePixelRatio || 1;
  c.width = Math.max(1, r.width * s); c.height = Math.max(1, r.height * s);
  const g = c.getContext("2d"); g.setTransform(s, 0, 0, s, 0, 0);
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

function drawPlot() {
  const { g, w, h } = fit(plotEl);
  const responseH = Math.max(90, Math.round(h * 0.3));
  const R = Math.max(40, Math.min((w - 80) / 2, h - responseH - 70));
  const a = { cx: w / 2, cy: 30 + R, R };
  drawArmadillo(g, a);
  const r = { x: 44, y: a.cy + 34, w: w - 60, h: h - a.cy - 34 - 24 };
  drawResponse(g, r);
  label(g, S.stage.editable ? "ARMADILLO" : "ARMADILLO  (between corners, listening only)", 12, 14, S.stage.editable ? C.ink : C.dim);
  label(g, "hold Z to hear", w - 12, 14, S.source.held >= 0 ? C.ink : C.dim, "right");
  view.a = a; view.response = r;
}

function drawWords() {
  const el = document.getElementById("words");
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
      row.onclick = () => { dispatch("pinCorner", 0, c.star); dispatch("setPuck", 0, 100); };
      list.appendChild(row);
    }
  });
}

function drawAll() {
  if (!S) return;
  document.getElementById("name").textContent = S.corners[0].name || "";
  drawPlot(); drawWords(); drawList();
}

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
plotEl.addEventListener("pointermove", (e) => {
  const p = local(e);
  if (drag) {
    const s = S.stage.sections[drag.row], g = armadilloInverse(p, view.a);
    if (drag.zero) dispatch("setSection", S.stage.target, drag.row, s.poleHz, s.poleR, g.hz, g.r);
    else dispatch("setSection", S.stage.target, drag.row, g.hz, g.r, s.zeroHz, s.zeroR);
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
});
window.addEventListener("keyup", (e) => { if (e.code === "KeyZ" && down) { down = false; dispatch("noteOff"); } });

window.__JUCE__.backend.addEventListener("state", (s) => { S = s; drawAll(); });
readState().then((s) => { S = s; if (S.source.which !== 3) dispatch("setSource", 3); drawAll(); });
window.addEventListener("resize", drawAll);
