import type { Curve, Doc, Reference } from "./doc.js";
import type { Lane } from "./dsp.js";
import {
  css,
  curveEval,
  formantInk,
  graticule,
  hzAt,
  hzOfX,
  rOf,
  rPrime,
  scope,
  trace as traceEval,
  xOf,
  yMap,
} from "./render.js";

const DB_LO = -48;
const DB_HI = 42;
const CROWN = 36;
const ERROR_DB = 12;
const ERROR_H = 84;
const AXIS_H = 14;

// Bell et al. 1961, Fig. 7: input, generated comparison, difference curve and the
// numerical error together. The error is physically attached to the response — one
// canvas, one frequency axis, no gutter between them.
function split(h: number) {
  const error = Math.min(ERROR_H, Math.max(40, Math.round(h * 0.22)));
  return { main: h - error - AXIS_H, axis: AXIS_H, error, errorTop: h - error };
}

export type ResponseView = {
  target: Curve | null;
  sum: Curve | null;
  reference?: Reference | null;
  preview?: Curve | null;
  candidate?: Curve | null;
  section?: Curve | null;
  lanes?: Lane[] | null;
  selected: number;
  rms?: number | null;
};

export type ResponseHooks = {
  isLocked: (lane: number) => boolean;
  ceiling: () => number;
  sum: () => Curve | null;
  onLocked: (lane: number) => void;
  onGrab: (lane: number) => void;
  onDrag: (lane: number, patch: Partial<Lane>) => void;
  onRelease: (lane: number) => void;
  onCreate: (hz: number) => void;
  onClearSection: (lane: number) => void;
};

type Handle = { lane: number; kind: "pole" | "zero"; x: number; y: number; coincident: boolean };

function yOf(db: number, h: number): number {
  return yMap(DB_LO, DB_HI, split(h).main).yOf(db);
}

function dbOfY(y: number, h: number): number {
  return yMap(DB_LO, DB_HI, split(h).main).dbOf(y);
}

function trace(g: CanvasRenderingContext2D, curve: Curve, w: number, h: number, color: string, width = 1.4) {
  traceEval(g, w, curveEval(curve), (db) => yOf(db, h), color, width);
}

// A section's two roots sit on the curve at their own frequencies. When a section is
// placed they coincide exactly and cancel, so the pair reads as a single node until it is
// opened.
function handles(lanes: Lane[] | null | undefined, sum: Curve | null | undefined, w: number, h: number) {
  const out: Handle[] = [];
  if (!lanes) return out;
  const ev = sum ? curveEval(sum) : null;
  const at = (hz: number) => yOf(ev ? ev(hz) : 0, h);
  for (let i = 0; i < 7; i++) {
    const lane = lanes[i];
    if (lane.pole_r <= 0 && lane.zero_r <= 0) continue;
    const zeroHz = lane.zero_r > 0 ? lane.zero_hz : lane.pole_hz;
    const coincident = Math.abs(lane.pole_hz - zeroHz) < 1 && Math.abs(lane.pole_r - lane.zero_r) < 1e-4;
    if (lane.pole_r > 0) {
      out.push({ lane: i, kind: "pole", x: xOf(lane.pole_hz, w), y: at(lane.pole_hz), coincident });
    }
    out.push({ lane: i, kind: "zero", x: xOf(zeroHz, w), y: at(zeroHz), coincident });
  }
  return out;
}

function hit(list: Handle[], px: number, py: number): Handle | null {
  let best: Handle | null = null;
  let bestD = 13;
  for (const handle of list) {
    const d = Math.hypot(handle.x - px, handle.y - py);
    if (d < bestD) {
      bestD = d;
      best = handle;
    }
  }
  return best;
}

function referenceBands(g: CanvasRenderingContext2D, reference: Reference, w: number, h: number) {
  g.font = `9px ${css("--ui")}`;
  g.textAlign = "center";
  reference.formants.forEach((f, k) => {
    if (!(f.hz > 0)) return;
    const half = Math.max(f.bandwidth_hz, 1) / 2;
    const x0 = xOf(f.hz - half, w);
    const x1 = xOf(f.hz + half, w);
    g.fillStyle = formantInk(k);
    g.globalAlpha = 0.12;
    g.fillRect(x0, 0, Math.max(2, x1 - x0), h);
    g.globalAlpha = 1;
    g.fillText(`F${k + 1}`, (x0 + x1) / 2, 9);
  });
  g.textAlign = "left";
}

function ground(g: CanvasRenderingContext2D, curve: Curve, w: number, h: number) {
  const ev = curveEval(curve);
  const pts = Math.min(512, Math.max(192, Math.round(w / 2)));
  g.beginPath();
  g.moveTo(0, split(h).main);
  for (let i = 0; i < pts; i++) g.lineTo((i / (pts - 1)) * w, yOf(ev(hzAt(i, pts)), h));
  g.lineTo(w, split(h).main);
  g.closePath();
  g.fillStyle = "rgba(168, 176, 180, 0.10)";
  g.fill();
  trace(g, curve, w, h, css("--target"), 1);
}

function nodes(
  g: CanvasRenderingContext2D,
  lanes: Lane[] | null | undefined,
  sum: Curve | null | undefined,
  selected: number,
  w: number,
  h: number,
) {
  const list = handles(lanes, sum, w, h);
  for (const lane of new Set(list.map((n) => n.lane))) {
    const on = lane === selected;
    const pole = list.find((n) => n.lane === lane && n.kind === "pole");
    const zero = list.find((n) => n.lane === lane && n.kind === "zero");
    const color = css(`--s${lane + 1}`);
    if (pole && zero && !zero.coincident) {
      g.strokeStyle = color;
      g.globalAlpha = on ? 0.45 : 0.12;
      g.lineWidth = 1;
      g.beginPath();
      g.moveTo(pole.x, pole.y);
      g.lineTo(zero.x, zero.y);
      g.stroke();
    }
    g.globalAlpha = on ? 1 : 0.3;
    if (zero) {
      g.strokeStyle = color;
      g.lineWidth = on ? 1.8 : 1.4;
      g.beginPath();
      g.arc(zero.x, zero.y, on ? 5.5 : 4, 0, 7);
      g.stroke();
    }
    if (pole) {
      g.fillStyle = color;
      g.beginPath();
      g.arc(pole.x, pole.y, on ? 4.5 : 3.5, 0, 7);
      g.fill();
    }
    g.globalAlpha = 1;
    g.lineWidth = 1;
  }
}

function overCeiling(g: CanvasRenderingContext2D, curve: Curve, w: number) {
  let bi = 0;
  for (let i = 1; i < curve.length; i++) if (curve[i] > curve[bi]) bi = i;
  if (curve[bi] <= CROWN) return;
  const x = xOf(hzAt(bi, curve.length), w);
  g.fillStyle = css("--error");
  g.beginPath();
  g.moveTo(x, 1);
  g.lineTo(x - 4, 8);
  g.lineTo(x + 4, 8);
  g.closePath();
  g.fill();
}

function errorStrip(
  g: CanvasRenderingContext2D,
  target: Curve,
  sum: Curve,
  w: number,
  top: number,
  h: number,
  rms: number | null | undefined,
) {
  const mid = top + h / 2;
  g.strokeStyle = css("--well-grid");
  g.lineWidth = 1;
  g.beginPath();
  g.moveTo(0, Math.floor(mid) + 0.5);
  g.lineTo(w, Math.floor(mid) + 0.5);
  g.stroke();
  const t = curveEval(target);
  const s = curveEval(sum);
  const pts = Math.min(512, Math.max(192, Math.round(w / 2)));
  g.strokeStyle = css("--error");
  g.lineWidth = 1.2;
  g.beginPath();
  let started = false;
  for (let i = 0; i < pts; i++) {
    const hz = hzAt(i, pts);
    // residual = body - target, so above the line is too loud.
    const d = s(hz) - t(hz);
    if (!Number.isFinite(d)) continue;
    const y = mid - (Math.max(-ERROR_DB, Math.min(ERROR_DB, d)) / ERROR_DB) * (h / 2 - 2);
    const x = (i / (pts - 1)) * w;
    if (!started) {
      g.moveTo(x, y);
      started = true;
    } else {
      g.lineTo(x, y);
    }
  }
  g.stroke();
  g.font = `9px ${css("--ui")}`;
  g.fillStyle = css("--well-dim");
  g.fillText(`ERROR ±${ERROR_DB} dB`, 4, top + 9);
  if (typeof rms === "number" && Number.isFinite(rms)) {
    const text = `${rms.toFixed(2)} dB RMS`;
    g.fillStyle = css("--error");
    g.fillText(text, w - g.measureText(text).width - 5, top + 9);
  }
}

export function drawResponse(canvas: HTMLCanvasElement, view: ResponseView) {
  const { target, sum, reference, preview, candidate, section, lanes, selected, rms } = view;
  scope(canvas).frame((g, w, full) => {
    const s = split(full);
    g.fillStyle = css("--well");
    g.fillRect(0, 0, w, full);
    graticule(g, w, s.main, (db) => yMap(DB_LO, DB_HI, s.main).yOf(db), DB_LO, DB_HI, CROWN, s.main + s.axis);
    if (target) ground(g, target, w, full);
    if (reference?.formants.length) referenceBands(g, reference, w, s.main);
    if (section) {
      g.globalAlpha = 0.8;
      trace(g, section, w, full, css(`--s${selected + 1}`), 1.1);
      g.globalAlpha = 1;
    }
    if (preview) trace(g, preview, w, full, css("--preview"), 1.1);
    if (candidate) trace(g, candidate, w, full, css("--candidate"), 1.1);
    if (sum) {
      trace(g, sum, w, full, css("--live"), 1.7);
      overCeiling(g, sum, w);
    }
    nodes(g, lanes, sum, selected, w, full);
    if (target && sum) errorStrip(g, target, sum, w, s.errorTop, s.error, rms);
  });
}

// A placed section is exact identity: pole and zero at the same frequency and radius
// cancel, so placing one changes nothing you can hear. The drag opens it from there.
const BASE_RP = 18;
export const NEW_SECTION_R = rOf(BASE_RP);

// R' is a root's resonance height in dB, so a coincident pair contributes about
// R'pole - R'zero at its own frequency. Vertical drag sets that dB directly and the radii
// fall out of it. Scale is never touched: it moves the whole spectrum and has no place in
// a resonator gesture.
function differential(base: number, db: number, ceiling: number) {
  return {
    pole_r: Math.min(ceiling, rOf(base + Math.max(0, db))),
    zero_r: Math.min(1, rOf(base + Math.max(0, -db))),
  };
}

export function attachResponse(canvas: HTMLCanvasElement, doc: Doc, cb: ResponseHooks) {
  type Grab = {
    lane: number;
    x0: number;
    y0: number;
    poleHz: number;
    zeroHz: number;
    base: number;
    db: number;
  };
  let drag: Grab | null = null;
  const local = (e: PointerEvent | WheelEvent | MouseEvent): [number, number] => {
    const rect = canvas.getBoundingClientRect();
    return [e.clientX - rect.left, e.clientY - rect.top];
  };
  const under = (px: number, py: number) =>
    hit(handles(doc.lanes, cb.sum(), canvas.clientWidth, canvas.clientHeight), px, py);

  function grabOf(lane: number, px: number, py: number): Grab {
    const l = doc.lanes[lane];
    const polePrime = rPrime(l.pole_r);
    const zeroPrime = l.zero_r > 0 ? rPrime(l.zero_r) : polePrime;
    return {
      lane,
      x0: px,
      y0: py,
      poleHz: l.pole_hz,
      zeroHz: l.zero_r > 0 ? l.zero_hz : l.pole_hz,
      base: Math.min(polePrime, zeroPrime),
      db: polePrime - zeroPrime,
    };
  }

  function apply(g: Grab, base: number, db: number, shift: number) {
    const bound = (hz: number) => Math.min(16000, Math.max(40, hz * shift));
    cb.onDrag(g.lane, {
      pole_hz: bound(g.poleHz),
      zero_hz: bound(g.zeroHz),
      ...differential(base, db, cb.ceiling()),
    });
  }

  canvas.addEventListener("pointerdown", (e) => {
    const [px, py] = local(e);
    const handle = under(px, py);
    if (!handle) return;
    if (cb.isLocked(handle.lane)) return cb.onLocked(handle.lane);
    drag = grabOf(handle.lane, px, py);
    cb.onGrab(handle.lane);
    canvas.setPointerCapture(e.pointerId);
  });

  canvas.addEventListener("pointermove", (e) => {
    if (!drag) return;
    const [px, py] = local(e);
    const w = canvas.clientWidth;
    const h = canvas.clientHeight;
    const shift = hzOfX(px, w) / Math.max(1e-6, hzOfX(drag.x0, w));
    apply(drag, drag.base, drag.db + (dbOfY(py, h) - dbOfY(drag.y0, h)), shift);
  });

  const end = () => {
    if (!drag) return;
    const held = drag;
    drag = null;
    cb.onRelease(held.lane);
  };
  canvas.addEventListener("pointerup", end);
  canvas.addEventListener("pointercancel", end);
  canvas.addEventListener("lostpointercapture", end);

  // The wheel is width: both radii move together, so the pair keeps its dB and only gets
  // sharper or broader.
  canvas.addEventListener("wheel", (e) => {
    const [px, py] = local(e);
    const handle = under(px, py);
    if (!handle) return;
    e.preventDefault();
    if (cb.isLocked(handle.lane)) return cb.onLocked(handle.lane);
    const g = grabOf(handle.lane, px, py);
    cb.onGrab(handle.lane);
    apply(g, Math.max(0, g.base - Math.sign(e.deltaY) * 2), g.db, 1);
    cb.onRelease(handle.lane);
  });

  // Double-click is the create/destroy gesture, so a stray click cannot allocate a
  // section: empty space places one, a node removes the section it belongs to.
  canvas.addEventListener("dblclick", (e) => {
    const [px, py] = local(e);
    const handle = under(px, py);
    if (!handle) return cb.onCreate(hzOfX(px, canvas.clientWidth));
    if (cb.isLocked(handle.lane)) return cb.onLocked(handle.lane);
    cb.onClearSection(handle.lane);
  });
}
