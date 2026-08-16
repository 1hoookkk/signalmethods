import { lawState } from "./doc.js";
import { stageCurves, cumulativeCurve } from "./curves.js";
import { roleOf } from "./fit.roles.js";
import { yOf, trace } from "./spectrum.js";

function css(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

function laneEmpty(l) {
  return l.pole_r <= 0 && l.zero_r <= 0;
}

function cell(parent) {
  const canvas = document.createElement("canvas");
  canvas.style.cssText = "flex:1;min-height:0;width:100%";
  parent.appendChild(canvas);
  return canvas;
}

function drawPanel(canvas, curves) {
  canvas.width = canvas.clientWidth;
  canvas.height = canvas.clientHeight;
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = css("--grat-minor");
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  const zero = yOf(0, canvas.height);
  ctx.strokeStyle = css("--grat-major");
  ctx.beginPath();
  ctx.moveTo(0, zero);
  ctx.lineTo(canvas.width, zero);
  ctx.stroke();
  for (const [curve, color, width] of curves) {
    if (curve) trace(ctx, curve, canvas.width, canvas.height, color, width);
  }
}

export function renderStages(el, doc, onSelect, onLockClick) {
  el.textContent = "";
  const row = document.createElement("div");
  row.style.cssText =
    "display:grid;grid-template-columns:repeat(7,1fr);gap:2px;flex:1;min-height:0;padding:2px;background:var(--well)";
  const stages = doc.words ? stageCurves(doc.words) : null;
  for (let i = 0; i < 7; i++) {
    const col = document.createElement("div");
    col.style.cssText = "display:flex;flex-direction:column;min-height:0;cursor:pointer";
    col.onclick = () => onSelect && onSelect(i);
    const lane = doc.lanes[i];
    const role = roleOf(i, doc.roles);
    const locked = lawState(doc.laws[i]) !== "FREE";
    const empty = laneEmpty(lane);
    const ink = css(role.color);
    const header = document.createElement("div");
    header.style.cssText = `display:flex;gap:4px;align-items:baseline;color:${ink};font-size:10px;padding:1px 3px;white-space:nowrap;border:1px solid ${
      i === doc.selected ? css("--sgi-hi") : "transparent"
    }`;
    const label = document.createElement("span");
    label.style.cssText = "flex:1;overflow:hidden;text-overflow:ellipsis";
    label.textContent = `S${i + 1} ${role.label}${
      empty ? "" : `  ${lane.pole_r > 0 ? lane.pole_hz.toFixed(0) : `z${lane.zero_hz.toFixed(0)}`}`
    }`;
    const lock = document.createElement("span");
    lock.style.cssText = `cursor:pointer;opacity:${locked ? 1 : 0.35}`;
    lock.textContent = locked ? "🔒" : "🔓";
    lock.onclick = (e) => {
      e.stopPropagation();
      onLockClick && onLockClick(i);
    };
    header.append(label, lock);
    col.appendChild(header);

    const iso = cell(col);
    const caption = document.createElement("div");
    caption.style.cssText = "color:var(--ink-dim);font-size:9px;padding:0 4px;white-space:nowrap";
    col.appendChild(caption);
    const cum = cell(col);
    row.appendChild(col);

    requestAnimationFrame(() => {
      if (stages) {
        drawPanel(iso, [[stages[i], ink, 1.3]]);
        drawPanel(cum, [[cumulativeCurve(doc.words, i), css("--trace-cumulative"), 1.1]]);
        const level = stages[i].reduce((s, v) => s + v, 0) / stages[i].length;
        caption.textContent = `level ${level >= 0 ? "+" : ""}${level.toFixed(0)} dB`;
      } else {
        drawPanel(iso, []);
        drawPanel(cum, []);
        caption.textContent = "";
      }
    });
  }
  el.appendChild(row);
}
