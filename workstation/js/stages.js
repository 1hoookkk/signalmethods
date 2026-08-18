import { lawState } from "./doc.js";
import { stageCurves } from "./curves.js";
import { hzOfX, curveEval, trace, yMap, SECTION_DB_LO, SECTION_DB_HI } from "./render.js";

const HZ_LO = 40;
const HZ_HI = 16000;
const RP_MAX = 60;

function css(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

function laneEmpty(l) {
  return l.pole_r <= 0 && l.zero_r <= 0;
}

function rPrime(r) {
  if (r >= 1) return RP_MAX;
  if (r <= 0) return 0;
  return Math.min(RP_MAX, 20 * Math.log10(1 / (1 - r)));
}

function rOfY(y, h) {
  const rp = (1 - Math.min(1, Math.max(0, y / h))) * RP_MAX;
  return 1 - Math.pow(10, -rp / 20);
}

function pxOf(hz, w) {
  const t = Math.log(Math.max(HZ_LO, Math.min(HZ_HI, hz)) / HZ_LO) / Math.log(HZ_HI / HZ_LO);
  return t * w;
}

function pyOfR(r, h) {
  return h - (rPrime(r) / RP_MAX) * h;
}

function fmtHz(hz) {
  if (hz <= 0) return "—";
  return hz >= 1000 ? `${(hz / 1000).toFixed(1)}k` : `${hz.toFixed(0)}`;
}

function cell(parent) {
  const canvas = document.createElement("canvas");
  canvas.style.cssText = "flex:0 0 96px;height:96px;width:100%;touch-action:none";
  parent.appendChild(canvas);
  return canvas;
}

function drawPanel(canvas, curves, lane, ink) {
  const cw = canvas.clientWidth;
  const ch = canvas.clientHeight;
  if (canvas.width !== cw) canvas.width = cw;
  if (canvas.height !== ch) canvas.height = ch;
  const w = canvas.width;
  const h = canvas.height;
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = css("--grat-minor");
  ctx.fillRect(0, 0, w, h);
  const { yOf } = yMap(SECTION_DB_LO, SECTION_DB_HI, h);
  const zero = yOf(0);
  ctx.strokeStyle = css("--grat-major");
  ctx.beginPath();
  ctx.moveTo(0, zero);
  ctx.lineTo(w, zero);
  ctx.stroke();
  for (const [curve, color, width] of curves) {
    if (curve) trace(ctx, w, h, curveEval(curve), yOf, color, width);
  }
  if (!lane) return;
  if (lane.pole_r > 0) {
    const px = pxOf(lane.pole_hz, w);
    const py = pyOfR(lane.pole_r, h);
    ctx.fillStyle = ink;
    ctx.beginPath();
    ctx.arc(px, py, 4, 0, 7);
    ctx.fill();
  }
  if (lane.zero_r > 0) {
    const zx = pxOf(lane.zero_hz, w);
    const zy = pyOfR(lane.zero_r, h);
    ctx.strokeStyle = ink;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(zx, zy, 4, 0, 7);
    ctx.stroke();
    ctx.lineWidth = 1;
  }
}

function hitDot(lane, px, py, w, h) {
  if (lane.pole_r > 0) {
    const dx = pxOf(lane.pole_hz, w) - px;
    const dy = pyOfR(lane.pole_r, h) - py;
    if (dx * dx + dy * dy < 64) return "pole";
  }
  if (lane.zero_r > 0) {
    const dx = pxOf(lane.zero_hz, w) - px;
    const dy = pyOfR(lane.zero_r, h) - py;
    if (dx * dx + dy * dy < 64) return "zero";
  }
  return null;
}

// Build the seven cascade columns once. DOM + handlers only; no per-frame work.
// Returns { cols } with references the per-frame draw path repaints.
export function mountStages(el, doc, cb) {
  el.textContent = "";
  const row = document.createElement("div");
  row.style.cssText =
    "display:grid;grid-template-columns:repeat(7,156px);gap:4px;flex:1;min-height:0;padding:4px;background:var(--well);overflow-x:auto;align-content:start";
  const cols = [];
  for (let i = 0; i < 7; i++) {
    const ink = css(`--s${i + 1}`);
    const col = document.createElement("div");
    col.style.cssText = "display:flex;flex-direction:column;min-height:0;cursor:pointer";
    col.draggable = true;
    col.ondragstart = (e) => e.dataTransfer.setData("text/stage", String(i));
    col.ondragover = (e) => e.preventDefault();
    col.ondrop = (e) => {
      e.preventDefault();
      const from = Number(e.dataTransfer.getData("text/stage"));
      if (Number.isInteger(from) && from !== i && cb.onSwap) cb.onSwap(from, i);
    };
    col.onclick = () => cb.onSelect && cb.onSelect(i);

    const header = document.createElement("div");
    header.style.cssText = `display:flex;gap:4px;align-items:baseline;color:${ink};font-size:11px;padding:2px 4px;white-space:nowrap;border:1px solid transparent;cursor:pointer`;
    const label = document.createElement("span");
    label.style.cssText = "flex:1;overflow:hidden;text-overflow:ellipsis";
    label.textContent = `S${i + 1}`;
    const lock = document.createElement("span");
    lock.style.cssText = "cursor:pointer;font-size:10px;padding:0 4px;border:1px solid transparent";
    lock.onclick = (e) => {
      e.stopPropagation();
      cb.onLockClick && cb.onLockClick(i);
    };
    header.onclick = (e) => {
      e.stopPropagation();
      const rect = header.getBoundingClientRect();
      cb.onSelect && cb.onSelect(i);
      cb.onHeader && cb.onHeader(i, rect);
    };
    const actions = document.createElement("div");
    actions.className = "stage-actions";
    const flip = document.createElement("button");
    flip.textContent = "P↔Z";
    flip.onclick = (e) => { e.stopPropagation(); if (cb.onFlip) cb.onFlip(i); };
    const seat = document.createElement("button");
    seat.textContent = "SECTION";
    seat.onclick = (e) => {
      e.stopPropagation();
      cb.onSelect && cb.onSelect(i);
      if (cb.onSeat) cb.onSeat(i, seat.getBoundingClientRect());
    };
    const clear = document.createElement("button");
    clear.textContent = "CLR";
    clear.onclick = (e) => { e.stopPropagation(); if (cb.onClear) cb.onClear(i); };
    header.append(label, lock);
    actions.append(seat, flip, clear);
    col.append(header, actions);

    const iso = cell(col);
    iso.onpointerdown = (e) => {
      const lane = doc.lanes[i];
      const locked = lawState(doc.laws[i]) !== "FREE";
      if (!cb.onDrag || locked) return;
      e.stopPropagation();
      const rect = iso.getBoundingClientRect();
      const px = e.clientX - rect.left;
      const py = e.clientY - rect.top;
      const scaleX = iso.width / iso.clientWidth;
      const scaleY = iso.height / iso.clientHeight;
      const kind = hitDot(lane, px * scaleX, py * scaleY, iso.width, iso.height);
      if (!kind) return;
      e.preventDefault();
      iso.setPointerCapture(e.pointerId);
      cb.onSelect && cb.onSelect(i);
      const move = (m) => {
        const mx = (m.clientX - rect.left) * scaleX;
        const my = (m.clientY - rect.top) * scaleY;
        const hz = hzOfX(mx, iso.width);
        const r = rOfY(my, iso.height);
        cb.onDrag(i, kind, hz, Math.min(r, 0.9999));
      };
      const up = () => {
        iso.removeEventListener("pointermove", move);
        iso.removeEventListener("pointerup", up);
        cb.onDrag(i, kind, null, null);
      };
      iso.addEventListener("pointermove", move);
      iso.addEventListener("pointerup", up);
    };

    const info = document.createElement("div");
    info.style.cssText =
      "color:var(--axis-ink);font-size:10px;padding:2px 4px;white-space:normal;line-height:13px;cursor:ns-resize;touch-action:none;overflow-wrap:anywhere";
    info.onclick = (e) => e.stopPropagation();
    info.onpointerdown = (e) => {
      if (!cb.onLevel) return;
      e.stopPropagation();
      e.preventDefault();
      let last = e.clientY;
      const move = (m) => {
        const step = (last - m.clientY) * 0.05;
        last = m.clientY;
        cb.onLevel(i, step);
      };
      const up = () => {
        window.removeEventListener("pointermove", move);
        window.removeEventListener("pointerup", up);
      };
      window.addEventListener("pointermove", move);
      window.addEventListener("pointerup", up);
    };
    col.appendChild(info);
    row.appendChild(col);
    cols.push({ header, lock, iso, info, ink });
  }
  el.appendChild(row);
  return { cols };
}

// Per-frame repaint of the mounted columns. Cheap enough to run every drag frame.
function realPairText(tag, root) {
  if (!root || root.kind === "off") return `${tag} —`;
  if (root.kind === "real") return `${tag} real ${root.pair[0].toFixed(4)} / ${root.pair[1].toFixed(4)}`;
  return `${tag} ${fmtHz(root.hz)} r ${root.r.toFixed(4)}`;
}

export function drawStages(mounted, doc) {
  const stages = doc.words ? stageCurves(doc.words) : null;
  for (let i = 0; i < 7; i++) {
    const c = mounted.cols[i];
    const lane = doc.lanes[i];
    const locked = lawState(doc.laws[i]) !== "FREE";
    const empty = laneEmpty(lane);
    c.header.style.borderColor = i === doc.selected ? css("--chrome-hi") : "transparent";
    c.lock.textContent = locked ? "HELD" : "free";
    c.lock.style.borderColor = locked ? css("--active-corner") : css("--grat-major");
    c.lock.style.color = locked ? css("--active-corner") : css("--axis-ink");
    drawPanel(c.iso, stages ? [[stages[i], c.ink, 1.3]] : [], empty ? null : lane, c.ink);
    const slot = doc.field && doc.field[doc.selectedCorner];
    const geom = slot && slot.geometry && slot.geometry[i];
    if (empty && geom && geom.real_pair) {
      c.info.textContent = `${realPairText("P", geom.pole)}  ${realPairText("Z", geom.zero)}  REAL PAIR — not editable here${slot.citations && slot.citations[i] ? `  SRC ${slot.citations[i]}` : ""}`;
      continue;
    }
    if (empty) {
      c.info.textContent = "";
      continue;
    }
    const parts = [];
    if (lane.pole_r > 0) parts.push(`P ${fmtHz(lane.pole_hz)} r ${lane.pole_r.toFixed(4)}`);
    if (lane.zero_r > 0) parts.push(`Z ${fmtHz(lane.zero_hz)} r ${lane.zero_r.toFixed(4)}`);
    const scaleDb = 20 * Math.log10(Math.max(lane.scale, 1e-12));
    let productDb = 0;
    for (let k = 0; k <= i; k++) productDb += 20 * Math.log10(Math.max(doc.lanes[k].scale, 1e-12));
    parts.push(`S ${scaleDb >= 0 ? "+" : ""}${scaleDb.toFixed(2)} dB  Σ ${productDb >= 0 ? "+" : ""}${productDb.toFixed(2)} dB`);
    const citation = doc.field && doc.field[doc.selectedCorner] && doc.field[doc.selectedCorner].citations && doc.field[doc.selectedCorner].citations[i];
    if (citation) parts.push(`SRC ${citation}`);
    c.info.textContent = parts.join("  ");
  }
}
