import { scope, css } from "./render.js";

export function initPad(el, hooks) {
  const title = el.querySelector(".dock-title");
  if (title) title.textContent = "PLAY · MORPH";
  const canvas = document.createElement("canvas");
  canvas.style.cssText = "flex:1 1 auto;min-height:120px;width:100%;display:block;touch-action:none;cursor:ew-resize";
  const readout = document.createElement("div");
  readout.style.cssText = "padding:5px 8px;background:var(--chrome);color:var(--ink);font-size:11px;display:flex;justify-content:space-between";
  const bar = document.createElement("div");
  bar.style.cssText = "display:flex;gap:6px;align-items:center;padding:4px 8px;background:var(--chrome);font-size:10px";
  bar.innerHTML = '<button id="pad-source" type="button">SYNTH</button><span>SAW</span><input id="pad-blend" type="range" min="0" max="1" step="0.05" value="1" style="flex:1"><span>PINK</span><input id="pad-freq" type="range" min="30" max="440" step="1" value="110" style="width:80px">';
  el.append(canvas, readout, bar);
  const pos = { m: 0, q: 0, z: 0, grit: 0 };
  let playing = false;

  function paint() {
    scope(canvas).frame((g, w, h) => {
      g.fillStyle = css("--well");
      g.fillRect(0, 0, w, h);
      const y = h / 2;
      const x0 = 24;
      const x1 = w - 24;
      g.strokeStyle = css("--grat-major");
      g.lineWidth = 2;
      g.beginPath();
      g.moveTo(x0, y);
      g.lineTo(x1, y);
      g.stroke();
      for (let i = 0; i <= 10; i++) {
        const x = x0 + (x1 - x0) * i / 10;
        g.strokeStyle = css("--grat-minor");
        g.beginPath();
        g.moveTo(x, y - 8);
        g.lineTo(x, y + 8);
        g.stroke();
      }
      const x = x0 + (x1 - x0) * pos.m;
      g.fillStyle = css("--active-corner");
      g.beginPath();
      g.arc(x, y, playing ? 11 : 9, 0, Math.PI * 2);
      g.fill();
      g.font = `11px ${css("--mono")}`;
      g.fillStyle = css("--axis-ink");
      g.fillText("LO MORPH", x0, y - 20);
      const hi = "HI MORPH";
      g.fillText(hi, x1 - g.measureText(hi).width, y - 20);
    });
    readout.innerHTML = `<span>LO ${Math.round((1 - pos.m) * 100)}%</span><span>M ${pos.m.toFixed(3)}</span><span>HI ${Math.round(pos.m * 100)}%</span>`;
  }

  function setFromEvent(e) {
    const rect = canvas.getBoundingClientRect();
    pos.m = Math.min(1, Math.max(0, (e.clientX - rect.left - 24) / Math.max(1, rect.width - 48)));
    hooks.onRide(pos.m, 0, 0);
    paint();
  }

  canvas.addEventListener("pointerdown", (e) => {
    playing = true;
    canvas.setPointerCapture(e.pointerId);
    setFromEvent(e);
    hooks.onHold(true);
  });
  canvas.addEventListener("pointermove", (e) => { if (playing) setFromEvent(e); });
  canvas.addEventListener("pointerup", () => { playing = false; hooks.onHold(false); paint(); });
  canvas.addEventListener("pointercancel", () => { playing = false; hooks.onHold(false); paint(); });
  canvas.addEventListener("wheel", (e) => {
    e.preventDefault();
    pos.m = Math.min(1, Math.max(0, pos.m - Math.sign(e.deltaY) * 0.025));
    hooks.onRide(pos.m, 0, 0);
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
