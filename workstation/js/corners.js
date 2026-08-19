import { css, scope, trace, curveEval, xOf, yMap } from "./render.js";
import { sumCurve } from "./curves.js";
import { decode } from "./dsp.js";

export const CELL_LABELS = ["LO Q0", "HI Q0", "LO Q100", "HI Q100"];

export const NAMES = ["M0 Q0 Z0", "M1 Q0 Z0", "M0 Q1 Z0", "M1 Q1 Z0", "M0 Q0 Z1", "M1 Q0 Z1", "M0 Q1 Z1", "M1 Q1 Z1"];

function levelDb(words) {
  if (!words) return null;
  return words.reduce((db, row) => db + 20 * Math.log10(Math.max(4 * decode(row[4]), 1e-12)), 0);
}

function miniplot(canvas, words, on, label, name, cloned, held) {
  scope(canvas).frame((g, w, h) => {
    const curve = words ? sumCurve(words) : null;
    const low = curve ? Math.min(-24, ...curve) : -24;
    const high = curve ? Math.max(24, ...curve) : 24;
    const dbLo = Math.floor((low - 4) / 12) * 12;
    const dbHi = Math.ceil((high + 4) / 12) * 12;
    const { yOf } = yMap(dbLo, dbHi, h);
    g.fillStyle = css("--well");
    g.fillRect(0, 0, w, h);
    g.strokeStyle = css("--grat");
    g.lineWidth = 1;
    for (const hz of [100, 1000, 10000]) {
      const x = Math.floor(xOf(hz, w)) + 0.5;
      g.beginPath();
      g.moveTo(x, 0);
      g.lineTo(x, h);
      g.stroke();
    }
    const zero = Math.floor(yOf(0)) + 0.5;
    g.strokeStyle = css("--grat-major");
    g.beginPath();
    g.moveTo(0, zero);
    g.lineTo(w, zero);
    g.stroke();
    if (words) {
      if (cloned) g.globalAlpha = 0.45;
      trace(g, w, h, curveEval(curve), yOf, css("--trace-live"), 1.3);
      g.globalAlpha = 1;
    }
    g.font = `10px ${css("--mono")}`;
    g.fillStyle = words ? css("--axis-ink") : css("--ink-dim");
    g.fillText(label, 4, 10);
    const level = levelDb(words);
    if (level !== null) {
      const text = `${level >= 0 ? "+" : ""}${level.toFixed(2)} dB`;
      g.fillText(text, w - g.measureText(text).width - 4, 10);
    }
    if (held) {
      g.fillStyle = css("--active-corner");
      g.fillText("HELD", w - 34, 10);
    }
    if (cloned) {
      g.fillStyle = css("--ink-dim");
      g.fillText("mirror", 4, h - 5);
    } else if (name && !/^corner \d+$/.test(name)) {
      g.fillStyle = css("--axis-ink");
      g.fillText(name.slice(0, 18), 4, h - 5);
    }
    g.strokeStyle = on ? css("--active-corner") : css("--grat-major");
    g.lineWidth = on ? 2 : 1;
    g.strokeRect(0.5, 0.5, w - 1, h - 1);
  });
}

export function renderCorners(el, field, active, hooks, live) {
  el.textContent = "";
  const bar = document.createElement("div");
  bar.className = "verb-bar";
  for (const group of [
    [["NEW", hooks.onClear]],
    [["COPY", hooks.onFill]],
    [["KEEP", hooks.onKeep]],
    [
      ["AUDIT", hooks.onAudit],
      ["WRITE", hooks.onWrite],
    ],
  ]) {
    const cluster = document.createElement("div");
    cluster.className = "verb-group";
    cluster.style.flex = `${group.length}`;
    for (const [label, fn] of group) {
      const b = document.createElement("button");
      b.className = label === "WRITE" ? "verb verb-terminal" : "verb";
      b.textContent = label;
      b.onclick = fn;
      cluster.appendChild(b);
    }
    bar.appendChild(cluster);
  }
  el.appendChild(bar);
  const cells = [];
  const heads = document.createElement("div");
  heads.style.cssText = "display:grid;grid-template-columns:1fr 1fr;gap:4px;padding:6px 4px 2px;background:var(--well);font-size:11px;color:var(--axis-ink)";
  heads.innerHTML = "<div>LO MORPH</div><div style='text-align:right'>HI MORPH</div>";
  el.appendChild(heads);
  const grid = document.createElement("div");
  grid.style.cssText = "display:grid;grid-template-columns:1fr 1fr;gap:6px;padding:4px;background:var(--well)";
  for (const i of [0, 1, 2, 3]) {
    const canvas = document.createElement("canvas");
    canvas.style.cssText = "width:100%;aspect-ratio:1.35;cursor:pointer";
    canvas.onclick = () => hooks.onSelect(i);
    canvas.ondblclick = () => hooks.onHoldCorner && hooks.onHoldCorner(i);
    canvas.draggable = true;
    canvas.ondragstart = (e) => e.dataTransfer.setData("text/plain", String(i));
    canvas.ondragover = (e) => e.preventDefault();
    canvas.ondrop = (e) => {
      e.preventDefault();
      const from = parseInt(e.dataTransfer.getData("text/plain"), 10);
      if (Number.isInteger(from) && from !== i && hooks.onSwap) hooks.onSwap(from, i);
    };
    grid.appendChild(canvas);
    cells.push([canvas, i]);
  }
  el.appendChild(grid);
  requestAnimationFrame(() => {
    for (const [canvas, i] of cells) {
      const slot = field[i];
      const cloned = !!(slot && slot.cloned);
      const words = slot ? (cloned && live ? live : slot.words) : null;
      miniplot(canvas, words, i === active, CELL_LABELS[i], slot && slot.name, cloned, !!(slot && slot.held));
    }
  });
}
