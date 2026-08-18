import {
  css,
  scope,
  xOf as xAt,
  hzOfX as xAtInverse,
  yMap,
  curveEval,
  trace as traceEval,
  graticule,
  formantTracks,
} from "./render.js";

const DB_LO = -60;
const DB_HI = 84;
const CROWN = 36;

export function xOf(hz, w) {
  return xAt(hz, w);
}

export function yOf(db, h) {
  return yMap(DB_LO, DB_HI, h).yOf(db);
}

export function trace(ctx, curve, w, h, color, width = 1.5) {
  traceEval(ctx, w, h, curveEval(curve), (db) => yOf(db, h), color, width);
}

function guideLanes(ctx, guides, w, h) {
  ctx.setLineDash([4, 4]);
  ctx.lineWidth = 1;
  ctx.font = `10px ${css("--mono")}`;
  for (const g of guides) {
    const x = xOf(g.hz, w);
    ctx.strokeStyle = css("--trace-cumulative");
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, h);
    ctx.stroke();
    ctx.fillStyle = css("--trace-cumulative");
    ctx.fillText(g.label, x + 3, 11);
  }
  ctx.setLineDash([]);
}

function ground(ctx, curve, w, h) {
  const ev = curveEval(curve);
  const pts = Math.min(512, Math.max(192, Math.round(w / 2)));
  ctx.beginPath();
  ctx.moveTo(0, h);
  for (let i = 0; i < pts; i++) {
    const hz = 40 * Math.pow(16000 / 40, i / (pts - 1));
    ctx.lineTo((i / (pts - 1)) * w, yOf(ev(hz), h));
  }
  ctx.lineTo(w, h);
  ctx.closePath();
  ctx.fillStyle = "rgba(160, 172, 168, 0.13)";
  ctx.fill();
  trace(ctx, curve, w, h, "rgba(160, 172, 168, 0.75)", 1);
}

function pucks(ctx, lanes, roles, sum, selected, w, h) {
  if (!lanes) return;
  const ev = sum ? curveEval(sum) : null;
  for (let i = 0; i < 7; i++) {
    if (lanes[i].pole_r <= 0) continue;
    const x = xOf(lanes[i].pole_hz, w);
    const yv = yOf(ev ? ev(lanes[i].pole_hz) : 0, h);
    ctx.fillStyle = css(`--s${i + 1}`);
    ctx.beginPath();
    ctx.arc(x, yv, i === selected ? 6 : 4.5, 0, 2 * Math.PI);
    ctx.fill();
    ctx.strokeStyle = css("--well");
    ctx.lineWidth = 1.5;
    ctx.stroke();
  }
}

export function puckHit(lanes, roles, sum, w, h, px, py) {
  if (!lanes) return null;
  const ev = sum ? curveEval(sum) : null;
  let best = null;
  let bestD = 14;
  for (let i = 0; i < 7; i++) {
    if (lanes[i].pole_r <= 0) continue;
    const d = Math.hypot(xOf(lanes[i].pole_hz, w) - px, yOf(ev ? ev(lanes[i].pole_hz) : 0, h) - py);
    if (d < bestD) {
      bestD = d;
      best = i;
    }
  }
  return best;
}

export function attachSpectrum(canvas, doc, cb) {
  let drag = null;
  canvas.style.touchAction = "none";
  canvas.addEventListener("pointerdown", (e) => {
    const rect = canvas.getBoundingClientRect();
    const py = e.clientY - rect.top;
    const lane = puckHit(doc.lanes, doc.roles, cb.sum(), canvas.clientWidth, canvas.clientHeight, e.clientX - rect.left, py);
    if (lane === null) return;
    if (cb.isLocked(lane)) {
      cb.onLocked(lane);
      return;
    }
    drag = { lane, y0: py, rp0: 20 * Math.log10(1 / Math.max(1e-6, 1 - doc.lanes[lane].pole_r)) };
    cb.onGrab(lane);
    canvas.setPointerCapture(e.pointerId);
  });
  canvas.addEventListener("pointermove", (e) => {
    if (!drag) return;
    const rect = canvas.getBoundingClientRect();
    const h = canvas.clientHeight;
    const py = e.clientY - rect.top;
    const hz = xAtInverse(e.clientX - rect.left, canvas.clientWidth);
    const rp = drag.rp0 + (dbOfY(py, h) - dbOfY(drag.y0, h));
    const r = Math.min(cb.ceiling(), Math.max(0, 1 - Math.pow(10, -Math.max(0, rp) / 20)));
    cb.onDrag(drag.lane, { pole_hz: hz, pole_r: r });
  });
  const end = () => {
    if (!drag) return;
    const held = drag;
    drag = null;
    cb.onRelease(held.lane);
  };
  canvas.addEventListener("pointerup", end);
  canvas.addEventListener("pointercancel", end);
}

export function dbOfY(y, h) {
  return yMap(DB_LO, DB_HI, h).dbOf(y);
}

function legend(ctx, entries, w) {
  ctx.font = `10px ${css("--mono")}`;
  ctx.textAlign = "right";
  ctx.textBaseline = "alphabetic";
  let x = w - 6;
  for (const [label, color] of entries.slice().reverse()) {
    ctx.fillStyle = color;
    ctx.fillText(label, x, 10);
    x -= ctx.measureText(label).width + 8;
  }
  ctx.textAlign = "left";
}

function overCeiling(ctx, curve, w) {
  let bi = 0;
  for (let i = 1; i < curve.length; i++) if (curve[i] > curve[bi]) bi = i;
  if (curve[bi] <= CROWN) return;
  const hz = 40 * Math.pow(16000 / 40, bi / (curve.length - 1));
  const x = xOf(hz, w);
  ctx.fillStyle = css("--alarm");
  ctx.beginPath();
  ctx.moveTo(x, 2);
  ctx.lineTo(x - 5, 10);
  ctx.lineTo(x + 5, 10);
  ctx.closePath();
  ctx.fill();
}

function deltaInk(d) {
  const a = Math.min(1, Math.max(0, (Math.abs(d) - 1.5) / 4.5));
  return `rgba(${Math.round(255 * a)}, ${Math.round(240 - 100 * a)}, ${Math.round(180 - 160 * a)}, 0.3)`;
}

function ribbon(ctx, target, sum, w, h) {
  const t = curveEval(target);
  const s = curveEval(sum);
  const pts = Math.min(384, Math.max(160, Math.round(w / 3)));
  for (let i = 0; i < pts - 1; i++) {
    const f0 = 40 * Math.pow(16000 / 40, i / (pts - 1));
    const f1 = 40 * Math.pow(16000 / 40, (i + 1) / (pts - 1));
    const t0 = t(f0);
    const t1 = t(f1);
    const s0 = s(f0);
    const s1 = s(f1);
    if (![t0, t1, s0, s1].every(Number.isFinite)) continue;
    const x0 = (i / (pts - 1)) * w;
    const x1 = ((i + 1) / (pts - 1)) * w;
    ctx.fillStyle = deltaInk((s0 - t0 + s1 - t1) / 2);
    ctx.beginPath();
    ctx.moveTo(x0, yOf(t0, h));
    ctx.lineTo(x1, yOf(t1, h));
    ctx.lineTo(x1, yOf(s1, h));
    ctx.lineTo(x0, yOf(s0, h));
    ctx.closePath();
    ctx.fill();
  }
}

export function drawSpectrum(canvas, { target, sum, peaks, guides, preview, candidate, lanes, roles, selected, rms }) {
  scope(canvas).frame((g, w, h) => {
    const { yOf: y } = yMap(DB_LO, DB_HI, h);
    graticule(g, w, h, y, { dbLo: DB_LO, dbHi: DB_HI, crown: CROWN });
    if (guides && guides.length) guideLanes(g, guides, w, h);
    if (peaks && peaks.length) formantTracks(g, w, h, peaks, null);
    if (target) ground(g, target, w, h);
    if (target && sum) ribbon(g, target, sum, w, h);
    if (preview) trace(g, preview, w, h, css("--ceiling"), 1.4);
    if (candidate) trace(g, candidate, w, h, css("--tilt"), 1.2);
    if (sum) trace(g, sum, w, h, css("--trace-live"), 1.8);
    if (sum) overCeiling(g, sum, w);
    pucks(g, lanes, roles, sum, selected, w, h);
    if (Number.isFinite(rms)) {
      g.font = `11px ${css("--mono")}`;
      g.fillStyle = css("--axis-ink");
      const text = `RESIDUAL ${rms.toFixed(2)} dB RMS`;
      g.fillText(text, w - g.measureText(text).width - 6, 13);
    }
  });
}
