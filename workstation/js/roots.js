import { scope } from "./render.js";

const STAGE_VARS = ["--s1", "--s2", "--s3", "--s4", "--s5", "--s6", "--s7"];
const RP_MAX = 60;
const HZ_LO = 40;
const HZ_HI = 18500;
const LOG_RATIO = Math.log(HZ_HI / HZ_LO);

function xOf(hz, w) {
  const t = Math.log(Math.max(HZ_LO, Math.min(HZ_HI, hz)) / HZ_LO) / LOG_RATIO;
  return Math.min(w - 3, Math.max(3, t * w));
}

function offAxis(hz) {
  return hz > HZ_HI || hz < HZ_LO;
}

function edgeTick(ctx, x, y, color) {
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(x, y - 7);
  ctx.lineTo(x, y + 7);
  ctx.stroke();
  ctx.lineWidth = 1;
}

function css(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

function rPrime(r) {
  if (r >= 1) return RP_MAX;
  if (r <= 0) return 0;
  return Math.min(RP_MAX, 20 * Math.log10(1 / (1 - r)));
}

function rOf(rp) {
  return 1 - Math.pow(10, -rp / 20);
}

function yOfR(r, h) {
  return Math.min(h - 3, Math.max(3, h - (rPrime(r) / RP_MAX) * h));
}

function hzOfX(x, w) {
  return HZ_LO * Math.pow(HZ_HI / HZ_LO, Math.min(1, Math.max(0, x / w)));
}

export function drawRoots(canvas, doc, ceiling) {
  scope(canvas).frame((ctx, w, h) => paintRoots(ctx, w, h, doc, ceiling));
}

function paintRoots(ctx, w, h, doc, ceiling) {
  ctx.fillStyle = css("--well");
  ctx.fillRect(0, 0, w, h);
  ctx.lineWidth = 1;
  for (const hz of [100, 1000, 10000, 18000]) {
    ctx.strokeStyle = css("--grat-minor");
    ctx.beginPath();
    ctx.moveTo(xOf(hz, w), 0);
    ctx.lineTo(xOf(hz, w), h);
    ctx.stroke();
  }
  for (let rp = 10; rp < RP_MAX; rp += 10) {
    ctx.strokeStyle = css("--grat-minor");
    const y = h - (rp / RP_MAX) * h;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }
  ctx.strokeStyle = css("--ceiling");
  ctx.setLineDash([4, 3]);
  ctx.beginPath();
  ctx.moveTo(0, yOfR(ceiling, h));
  ctx.lineTo(w, yOfR(ceiling, h));
  ctx.stroke();
  ctx.setLineDash([]);
  ctx.font = `10px ${css("--mono")}`;
  for (let i = 0; i < 7; i++) {
    const lane = doc.lanes[i];
    const color = css(STAGE_VARS[i]);
    const hasPole = lane.pole_r > 0;
    const hasZero = lane.zero_r > 0;
    const inert = (r) => rPrime(r) < 2;
    if (hasPole && hasZero && !inert(lane.pole_r) && !inert(lane.zero_r)) {
      ctx.strokeStyle = color;
      ctx.globalAlpha = 0.35;
      ctx.beginPath();
      ctx.moveTo(xOf(lane.pole_hz, w), yOfR(lane.pole_r, h));
      ctx.lineTo(xOf(lane.zero_hz, w), yOfR(lane.zero_r, h));
      ctx.stroke();
      ctx.globalAlpha = 1;
    }
    if (hasPole) {
      ctx.globalAlpha = inert(lane.pole_r) ? 0.3 : 1;
      ctx.fillStyle = color;
      ctx.beginPath();
      ctx.arc(xOf(lane.pole_hz, w), yOfR(lane.pole_r, h), 5, 0, 7);
      ctx.fill();
      ctx.fillText(`${i + 1}`, xOf(lane.pole_hz, w) + 7, yOfR(lane.pole_r, h) + 3);
      if (offAxis(lane.pole_hz)) edgeTick(ctx, xOf(lane.pole_hz, w), yOfR(lane.pole_r, h), color);
      ctx.globalAlpha = 1;
    }
    if (hasZero) {
      ctx.globalAlpha = inert(lane.zero_r) ? 0.3 : 1;
      ctx.strokeStyle = color;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.arc(xOf(lane.zero_hz, w), yOfR(lane.zero_r, h), 5, 0, 7);
      ctx.stroke();
      ctx.lineWidth = 1;
      ctx.fillStyle = color;
      ctx.fillText(`${i + 1}`, xOf(lane.zero_hz, w) + 7, yOfR(lane.zero_r, h) + 3);
      if (offAxis(lane.zero_hz)) edgeTick(ctx, xOf(lane.zero_hz, w), yOfR(lane.zero_r, h), color);
      ctx.globalAlpha = 1;
    }
  }
}

export function hitRoot(canvas, doc, px, py) {
  const w = canvas.clientWidth;
  const h = canvas.clientHeight;
  let best = null;
  let bestDist = 12;
  for (let i = 0; i < 7; i++) {
    const lane = doc.lanes[i];
    const roots = [];
    if (lane.pole_r > 0) roots.push(["pole", lane.pole_hz, lane.pole_r]);
    if (lane.zero_r > 0) roots.push(["zero", lane.zero_hz, lane.zero_r]);
    for (const [kind, hz, r] of roots) {
      const d = Math.hypot(xOf(hz, w) - px, yOfR(r, h) - py);
      if (d < bestDist) {
        bestDist = d;
        best = { lane: i, kind };
      }
    }
  }
  return best;
}

export function attachRoots(canvas, doc, cb) {
  let drag = null;
  const local = (e) => {
    const rect = canvas.getBoundingClientRect();
    return [e.clientX - rect.left, e.clientY - rect.top];
  };
  canvas.addEventListener("pointerdown", (e) => {
    const [px, py] = local(e);
    const hit = hitRoot(canvas, doc, px, py);
    if (!hit) return;
    if (cb.isLocked(hit.lane)) {
      cb.onLocked(hit.lane);
      return;
    }
    drag = hit;
    cb.onGrab(hit);
    canvas.setPointerCapture(e.pointerId);
  });
  canvas.addEventListener("pointermove", (e) => {
    if (!drag) return;
    const [px, py] = local(e);
    dragTo(canvas, doc, drag, px, py, cb.ceiling());
    cb.onMove(drag);
  });
  canvas.addEventListener("pointerup", () => {
    if (!drag) return;
    const held = drag;
    drag = null;
    cb.onRelease(held);
  });
}

export function dragTo(canvas, doc, hit, px, py, ceiling) {
  const w = canvas.clientWidth;
  const h = canvas.clientHeight;
  const hz = Math.min(HZ_HI, Math.max(HZ_LO, hzOfX(px, w)));
  let rp = (1 - Math.min(1, Math.max(0, py / h))) * RP_MAX;
  const lane = doc.lanes[hit.lane];
  if (hit.kind === "pole") {
    lane.pole_hz = hz;
    lane.pole_r = Math.min(rOf(rp), ceiling);
  } else {
    lane.zero_hz = hz;
    lane.zero_r = py <= 3 ? 1.0 : Math.min(rOf(rp), 1.0);
  }
}
