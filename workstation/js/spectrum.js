const DB_LO = -60;
const DB_HI = 45;
const CROWN = 36;

function css(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

export function xOf(hz, w) {
  return (Math.log(hz / 40) / Math.log(16000 / 40)) * w;
}

export function yOf(db, h) {
  return h - ((db - DB_LO) / (DB_HI - DB_LO)) * h;
}

function graticule(ctx, w, h) {
  ctx.fillStyle = css("--well");
  ctx.fillRect(0, 0, w, h);
  ctx.lineWidth = 1;
  for (let db = DB_LO; db <= DB_HI; db += 10) {
    ctx.strokeStyle = db === 0 ? css("--grat-major") : css("--grat-minor");
    ctx.beginPath();
    ctx.moveTo(0, yOf(db, h));
    ctx.lineTo(w, yOf(db, h));
    ctx.stroke();
  }
  for (const hz of [100, 1000, 10000]) {
    ctx.strokeStyle = hz === 1000 ? css("--grat-major") : css("--grat-minor");
    ctx.beginPath();
    ctx.moveTo(xOf(hz, w), 0);
    ctx.lineTo(xOf(hz, w), h);
    ctx.stroke();
  }
  ctx.strokeStyle = css("--alarm");
  ctx.setLineDash([6, 4]);
  ctx.beginPath();
  ctx.moveTo(0, yOf(CROWN, h));
  ctx.lineTo(w, yOf(CROWN, h));
  ctx.stroke();
  ctx.setLineDash([]);
}

export function trace(ctx, curve, w, h, color, width = 1.5) {
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.beginPath();
  const span = curve.length - 1;
  for (let i = 0; i < curve.length; i++) {
    const x = (i / span) * w;
    const y = yOf(curve[i], h);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.stroke();
}

function guideLanes(ctx, guides, w, h) {
  ctx.setLineDash([4, 4]);
  ctx.lineWidth = 1;
  ctx.font = "10px monospace";
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

export function drawSpectrum(canvas, { target, sum, peaks, guides, preview, candidate }) {
  const ctx = canvas.getContext("2d");
  const w = canvas.width;
  const h = canvas.height;
  graticule(ctx, w, h);
  if (guides && guides.length) guideLanes(ctx, guides, w, h);
  if (target) trace(ctx, target, w, h, css("--trace-ghost"), 1.2);
  if (preview) trace(ctx, preview, w, h, css("--ceiling"), 1.4);
  if (candidate) trace(ctx, candidate, w, h, css("--tilt"), 1.2);
  if (sum) trace(ctx, sum, w, h, css("--trace-live"), 1.8);
  if (peaks) {
    ctx.fillStyle = css("--trace-ghost");
    for (const p of peaks) {
      const x = xOf(p.hz, w);
      ctx.fillRect(x - 1, yOf(p.db, h) - 5, 2, 5);
    }
  }
}
