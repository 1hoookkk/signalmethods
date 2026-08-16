function css(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

const VERTS = [
  [0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0],
  [0, 0, 1], [1, 0, 1], [0, 1, 1], [1, 1, 1],
];
const EDGES = [
  [0, 1], [2, 3], [4, 5], [6, 7],
  [0, 2], [1, 3], [4, 6], [5, 7],
  [0, 4], [1, 5], [2, 6], [3, 7],
];

function project(m, q, z, w, h) {
  const x = (m - 0.5) + 0.45 * (z - 0.5);
  const y = (0.5 - q) + 0.32 * (0.5 - z);
  return [w / 2 + x * w * 0.62, h / 2 + y * h * 0.62];
}

export function drawCube(canvas, pos, filled, active, names) {
  const ctx = canvas.getContext("2d");
  const w = canvas.width;
  const h = canvas.height;
  ctx.fillStyle = css("--well");
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = css("--grat-major");
  ctx.lineWidth = 1;
  for (const [a, b] of EDGES) {
    const [ax, ay] = project(...VERTS[a], w, h);
    const [bx, by] = project(...VERTS[b], w, h);
    ctx.beginPath();
    ctx.moveTo(ax, ay);
    ctx.lineTo(bx, by);
    ctx.stroke();
  }
  ctx.font = "9px monospace";
  for (let i = 0; i < 8; i++) {
    const m = i & 1;
    const q = (i >> 1) & 1;
    const z = (i >> 2) & 1;
    const [x, y] = project(m, q, z, w, h);
    const isActive = i === active;
    ctx.fillStyle = isActive
      ? css("--active-corner")
      : filled[i]
        ? css("--trace-live")
        : css("--ink-dim");
    ctx.beginPath();
    ctx.arc(x, y, isActive ? 5 : 3.5, 0, 7);
    ctx.fill();
    if (isActive) {
      ctx.strokeStyle = css("--active-corner");
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.arc(x, y, 9, 0, 7);
      ctx.stroke();
    }
    const name = names && names[i] ? names[i] : "";
    const text = `C${i}${name ? ": " + name.slice(0, 13) : ""}`;
    ctx.fillText(text, x + (m ? 8 : -8 - ctx.measureText(text).width), y + (q ? -7 : 11));
  }
  const [px, py] = project(pos.m, pos.q, pos.z, w, h);
  ctx.strokeStyle = css("--trace-ghost");
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.arc(px, py, 6, 0, 7);
  ctx.stroke();
}
