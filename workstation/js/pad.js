import { scope, css } from "./render.js";

export function initPad(el, hooks) {
  const title = el.querySelector(".dock-title");
  if (title) title.textContent = "PLAY · MORPH";
  const canvas = document.createElement("canvas");
  canvas.style.cssText = "flex:1 1 auto;min-height:150px;width:100%;display:block;touch-action:none;cursor:crosshair";
  const readout = document.createElement("div");
  readout.style.cssText = "padding:5px 8px;background:var(--chrome);color:var(--ink);font-size:11px;display:flex;justify-content:space-between";
  const bar = document.createElement("div");
  bar.style.cssText = "display:flex;gap:6px;align-items:center;padding:4px 8px;background:var(--chrome);font-size:10px";
  bar.innerHTML = '<button id="pad-source" type="button">SYNTH</button><span>SAW</span><input id="pad-blend" type="range" min="0" max="1" step="0.05" value="1" style="flex:1"><span>PINK</span><input id="pad-freq" type="range" min="30" max="440" step="1" value="110" style="width:80px">';
  el.append(canvas, readout, bar);
  const pos = { m: 0, q: 0, z: 0, grit: 0 };
  let playing = false;


  const PAD = 22;

  function paint() {
    scope(canvas).frame((g, w, h) => {
      g.fillStyle = css("--well");
      g.fillRect(0, 0, w, h);
      const x0 = PAD, x1 = w - PAD, y0 = PAD, y1 = h - PAD;
      g.strokeStyle = css("--grat-minor");
      g.lineWidth = 1;
      for (let i = 0; i <= 4; i++) {
        const x = x0 + ((x1 - x0) * i) / 4;
        const y = y0 + ((y1 - y0) * i) / 4;
        g.beginPath(); g.moveTo(x, y0); g.lineTo(x, y1); g.stroke();
        g.beginPath(); g.moveTo(x0, y); g.lineTo(x1, y); g.stroke();
      }
      g.strokeStyle = css("--grat-major");
      g.lineWidth = 2;
      g.strokeRect(x0, y0, x1 - x0, y1 - y0);
      const x = x0 + (x1 - x0) * pos.m;
      const y = y1 - (y1 - y0) * pos.q;
      g.strokeStyle = css("--grat-major");
      g.lineWidth = 1;
      g.beginPath(); g.moveTo(x0, y); g.lineTo(x1, y); g.stroke();
      g.beginPath(); g.moveTo(x, y0); g.lineTo(x, y1); g.stroke();
      g.fillStyle = css("--active-corner");
      g.beginPath();
      g.arc(x, y, playing ? 11 : 9, 0, Math.PI * 2);
      g.fill();
      g.font = `10px ${css("--mono")}`;
      g.fillStyle = css("--axis-ink");
      g.fillText("M", x1 - 8, y1 + 14);
      g.fillText("Q", x0 - 14, y0 + 8);
    });
    readout.innerHTML = `<span>M ${pos.m.toFixed(2)}</span><span>Q ${pos.q.toFixed(2)}</span>`;
  }

  function setFromEvent(e) {
    const rect = canvas.getBoundingClientRect();
    const w = rect.width, h = rect.height;
    pos.m = Math.min(1, Math.max(0, (e.clientX - rect.left - PAD) / Math.max(1, w - 2 * PAD)));
    pos.q = Math.min(1, Math.max(0, 1 - (e.clientY - rect.top - PAD) / Math.max(1, h - 2 * PAD)));
    hooks.onRide(pos.m, pos.q, 0);
    paint();
  }

  let dragging = false;
  canvas.addEventListener("pointerdown", (e) => {
    dragging = true;
    canvas.setPointerCapture(e.pointerId);
    setFromEvent(e);
    if (!playing) {
      playing = true;
      hooks.onHold(true);
    }
    paint();
  });
  canvas.addEventListener("pointermove", (e) => { if (dragging) setFromEvent(e); });
  canvas.addEventListener("pointerup", () => { dragging = false; paint(); });
  canvas.addEventListener("pointercancel", () => { dragging = false; paint(); });
  canvas.addEventListener("dblclick", () => {
    playing = false;
    hooks.onHold(false);
    paint();
  });
  canvas.addEventListener("wheel", (e) => {
    e.preventDefault();
    pos.q = Math.min(1, Math.max(0, pos.q - Math.sign(e.deltaY) * 0.05));
    hooks.onRide(pos.m, pos.q, 0);
    paint();
  });
  bar.querySelector("#pad-blend").oninput = (e) => hooks.onBlend(Number(e.target.value));
  bar.querySelector("#pad-freq").oninput = (e) => hooks.onFreq(Number(e.target.value));
  const source = bar.querySelector("#pad-source");
  source.onclick = () => {
    const next = source.textContent === "SYNTH" ? "DRY" : "SYNTH";
    source.textContent = next;
    if (hooks.onSource) hooks.onSource(next.toLowerCase());
  };
  paint();
  window.addEventListener("resize", paint);
  return { pos, paint };
}
