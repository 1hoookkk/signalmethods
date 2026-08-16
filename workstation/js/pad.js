function css(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

import { drawCube } from "./cube.js";

export function initPad(el, hooks) {
  const cubeCanvas = document.createElement("canvas");
  cubeCanvas.style.cssText = "height:150px;width:100%";
  el.appendChild(cubeCanvas);

  const canvas = document.createElement("canvas");
  canvas.style.cssText = "flex:1;min-height:0;width:100%;touch-action:none;cursor:crosshair";
  el.appendChild(canvas);

  const bar = document.createElement("div");
  bar.style.cssText = "display:flex;gap:6px;align-items:center;padding:3px 6px;background:var(--chrome);font-size:10px";
  bar.innerHTML =
    '<span>SAW</span><input id="pad-blend" type="range" min="0" max="1" step="0.05" value="0.5" style="flex:1"><span>PINK</span>' +
    '<input id="pad-freq" type="range" min="30" max="440" step="1" value="110" style="flex:1" title="exciter Hz">' +
    '<span>Z</span><input id="pad-z" type="range" min="0" max="1" step="0.05" value="0" style="flex:1" title="transform axis">';
  el.appendChild(bar);
  bar.querySelector("#pad-blend").oninput = (e) => hooks.onBlend(parseFloat(e.target.value));
  bar.querySelector("#pad-freq").oninput = (e) => hooks.onFreq(parseFloat(e.target.value));
  const zSlider = bar.querySelector("#pad-z");
  zSlider.oninput = (e) => {
    pos.z = parseFloat(e.target.value);
    hooks.onRide(pos.m, pos.q, pos.z);
    paint();
  };

  const pos = { m: 0, q: 0, z: 0 };
  let holding = false;

  function paint() {
    canvas.width = canvas.clientWidth;
    canvas.height = canvas.clientHeight;
    const ctx = canvas.getContext("2d");
    const w = canvas.width;
    const h = canvas.height;
    ctx.fillStyle = css("--well");
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = css("--grat-minor");
    for (let i = 1; i < 4; i++) {
      ctx.beginPath();
      ctx.moveTo((w * i) / 4, 0);
      ctx.lineTo((w * i) / 4, h);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(0, (h * i) / 4);
      ctx.lineTo(w, (h * i) / 4);
      ctx.stroke();
    }
    const x = pos.m * w;
    const y = (1 - pos.q) * h;
    ctx.strokeStyle = holding ? css("--trace-live") : css("--trace-ghost");
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.arc(x, y, 8, 0, 7);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(x - 12, y);
    ctx.lineTo(x + 12, y);
    ctx.moveTo(x, y - 12);
    ctx.lineTo(x, y + 12);
    ctx.stroke();
    ctx.fillStyle = css("--trace-cumulative");
    ctx.fillRect(w - 8, h - pos.z * h, 5, pos.z * h || 1);
    ctx.fillStyle = css("--s7");
    ctx.font = "10px monospace";
    ctx.fillText(`M ${pos.m.toFixed(2)}  Q ${pos.q.toFixed(2)}  Z ${pos.z.toFixed(2)}`, 6, 12);
    cubeCanvas.width = cubeCanvas.clientWidth;
    cubeCanvas.height = cubeCanvas.clientHeight;
    const cube = hooks.getCube ? hooks.getCube() : { filled: Array(8).fill(false) };
    drawCube(cubeCanvas, pos, cube.filled, cube.active, cube.names);
  }

  function setFromEvent(e) {
    const rect = canvas.getBoundingClientRect();
    pos.m = Math.min(1, Math.max(0, (e.clientX - rect.left) / rect.width));
    pos.q = Math.min(1, Math.max(0, 1 - (e.clientY - rect.top) / rect.height));
    hooks.onRide(pos.m, pos.q, pos.z);
    paint();
  }

  canvas.addEventListener("pointerdown", (e) => {
    holding = true;
    canvas.setPointerCapture(e.pointerId);
    setFromEvent(e);
    hooks.onHold(true);
  });
  canvas.addEventListener("pointermove", (e) => {
    if (holding) setFromEvent(e);
  });
  canvas.addEventListener("pointerup", () => {
    holding = false;
    hooks.onHold(false);
    paint();
  });
  canvas.addEventListener("wheel", (e) => {
    e.preventDefault();
    pos.z = Math.min(1, Math.max(0, pos.z - Math.sign(e.deltaY) * 0.05));
    zSlider.value = pos.z;
    hooks.onRide(pos.m, pos.q, pos.z);
    paint();
  });

  paint();
  window.addEventListener("resize", paint);
  return { pos, paint };
}
