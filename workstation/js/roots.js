import { xOf } from "./spectrum.js";

const STAGE_VARS = ["--s1", "--s2", "--s3", "--s4", "--s5", "--s6", "--s7"];
const RP_MAX = 60;

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
  return h - (rPrime(r) / RP_MAX) * h;
}

function hzOfX(x, w) {
  return 40 * Math.pow(16000 / 40, Math.min(1, Math.max(0, x / w)));
}

export function drawRoots(canvas, doc, ceiling) {
  const ctx = canvas.getContext("2d");
  const w = canvas.width;
  const h = canvas.height;
  ctx.fillStyle = css("--well");
  ctx.fillRect(0, 0, w, h);
  ctx.lineWidth = 1;
  for (const hz of [100, 1000, 10000]) {
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
  ctx.font = "9px monospace";
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
      ctx.globalAlpha = 1;
    }
  }
}

export function hitRoot(canvas, doc, px, py) {
  const w = canvas.width;
  const h = canvas.height;
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

export function dragTo(canvas, doc, hit, px, py, ceiling) {
  const w = canvas.width;
  const h = canvas.height;
  const hz = Math.min(16000, Math.max(40, hzOfX(px, w)));
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
