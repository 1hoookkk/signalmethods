import { getNativeFunction } from "/juce/index.js";

const readState = getNativeFunction("state");
const send = getNativeFunction("dispatch");
const dispatch = (name, ...args) => send(name, ...args);

const pad = document.getElementById("pad");
let S = null, sizeKey = "", area = null;

const F1 = { lo: 200, hi: 1100 }, F2 = { lo: 600, hi: 3500 };
const xOfF2 = (f, r) => r.x + r.w * (1 - Math.log(Math.min(F2.hi, Math.max(F2.lo, f)) / F2.lo) / Math.log(F2.hi / F2.lo));
const yOfF1 = (f, r) => r.y + r.h * (Math.log(Math.min(F1.hi, Math.max(F1.lo, f)) / F1.lo) / Math.log(F1.hi / F1.lo));
const f2Of = (x, r) => F2.lo * Math.pow(F2.hi / F2.lo, 1 - Math.min(1, Math.max(0, (x - r.x) / r.w)));
const f1Of = (y, r) => F1.lo * Math.pow(F1.hi / F1.lo, Math.min(1, Math.max(0, (y - r.y) / r.h)));

function fit() {
  const b = pad.getBoundingClientRect(), s = devicePixelRatio || 1, key = b.width + "x" + b.height + "@" + s;
  if (sizeKey !== key) { pad.width = Math.max(1, b.width * s); pad.height = Math.max(1, b.height * s); sizeKey = key; }
  const g = pad.getContext("2d"); g.setTransform(s, 0, 0, s, 0, 0); g.clearRect(0, 0, b.width, b.height);
  return { g, w: b.width, h: b.height };
}

function draw() {
  if (!S) return;
  const { g, w, h } = fit();
  const r = { x: 8, y: 8, w: w - 16, h: h - 16 };
  area = r;
  g.font = '9px "Segoe UI", system-ui, sans-serif'; g.textBaseline = "middle";
  const m1 = S.lens.f1, m2 = S.lens.f2;
  g.strokeStyle = "rgba(210,213,217,0.28)"; g.lineWidth = 1;
  if (m2 > 0) { const x = Math.round(xOfF2(m2, r)) + 0.5; g.beginPath(); g.moveTo(x, r.y); g.lineTo(x, r.y + r.h); g.stroke(); g.fillStyle = "rgba(210,213,217,0.45)"; g.textAlign = "left"; g.fillText("F2", x + 3, r.y + 7); }
  if (m1 > 0) { const y = Math.round(yOfF1(m1, r)) + 0.5; g.beginPath(); g.moveTo(r.x, y); g.lineTo(r.x + r.w, y); g.stroke(); g.fillStyle = "rgba(210,213,217,0.45)"; g.textAlign = "left"; g.fillText("F1", r.x + 3, y - 7); }
  const p1 = S.lens.puckF1, p2 = S.lens.puckF2;
  if (p1 > 0 && p2 > 0) {
    const x = xOfF2(p2, r), y = yOfF1(p1, r);
    g.fillStyle = S.lens.on ? "#4fa3e6" : "#a8abb0";
    g.beginPath(); g.arc(x, y, 6, 0, 2 * Math.PI); g.fill();
    g.strokeStyle = "rgba(22,23,24,0.9)"; g.lineWidth = 1.5; g.beginPath(); g.arc(x, y, 3, 0, 2 * Math.PI); g.stroke();
  }
}

const local = (e) => { const b = pad.getBoundingClientRect(); return { x: e.clientX - b.left, y: e.clientY - b.top }; };
let dragging = false, pending = null, inFlight = false, last = null, velocity = { x: 0, y: 0 }, thrown = null;
function flush() {
  if (!pending || inFlight) return;
  const p = pending; pending = null; inFlight = true;
  Promise.resolve(dispatch("lens", p.f1, p.f2)).finally(() => { inFlight = false; if (pending) requestAnimationFrame(flush); });
}
function lensAt(p) { pending = { f1: f1Of(p.y, area), f2: f2Of(p.x, area) }; requestAnimationFrame(flush); }
pad.addEventListener("contextmenu", (e) => { e.preventDefault(); thrown = null; dispatch("lensReset"); });
pad.addEventListener("pointerdown", (e) => {
  if (e.button !== 0 || !area) return;
  thrown = null; dragging = true; pad.setPointerCapture(e.pointerId);
  const p = local(e); last = { ...p, t: performance.now() }; velocity = { x: 0, y: 0 }; lensAt(p);
});
pad.addEventListener("pointermove", (e) => {
  if (!dragging) return;
  const p = local(e), t = performance.now(), dt = Math.max(1, t - last.t);
  velocity = { x: 0.6 * velocity.x + 0.4 * (p.x - last.x) / dt, y: 0.6 * velocity.y + 0.4 * (p.y - last.y) / dt };
  last = { ...p, t }; lensAt(p);
});
pad.addEventListener("pointerup", () => {
  dragging = false;
  const speed = Math.hypot(velocity.x, velocity.y);
  if (speed > 0.6 && last) { thrown = { x: last.x, y: last.y, vx: velocity.x, vy: velocity.y }; requestAnimationFrame(fly); }
});
function fly() {
  if (!thrown) return;
  thrown.x += thrown.vx * 16; thrown.y += thrown.vy * 16;
  thrown.vx *= 0.9; thrown.vy *= 0.9;
  const r = area;
  if (thrown.x < r.x || thrown.x > r.x + r.w) { thrown.vx = -thrown.vx; thrown.x = Math.min(r.x + r.w, Math.max(r.x, thrown.x)); }
  if (thrown.y < r.y || thrown.y > r.y + r.h) { thrown.vy = -thrown.vy; thrown.y = Math.min(r.y + r.h, Math.max(r.y, thrown.y)); }
  lensAt(thrown);
  if (Math.hypot(thrown.vx, thrown.vy) > 0.02) requestAnimationFrame(fly); else thrown = null;
}

window.__JUCE__.backend.addEventListener("state", (s) => { S = s; draw(); });
readState().then((s) => { S = s; draw(); });
window.addEventListener("resize", () => { sizeKey = ""; draw(); });
