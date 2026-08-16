export function thumbwheel({ horizontal, onDelta }) {
  const el = document.createElement("div");
  el.className = horizontal ? "sgi-wheel sgi-wheel-h" : "sgi-wheel";
  let last = null;
  let offset = 0;
  el.addEventListener("pointerdown", (e) => {
    last = horizontal ? e.clientX : e.clientY;
    el.setPointerCapture(e.pointerId);
    e.preventDefault();
  });
  el.addEventListener("pointermove", (e) => {
    if (last === null) return;
    const now = horizontal ? e.clientX : e.clientY;
    const step = now - last;
    last = now;
    offset += step;
    el.style.backgroundPosition = horizontal ? `${offset}px 0` : `0 ${offset}px`;
    onDelta(horizontal ? step : -step, e.shiftKey);
  });
  const release = () => {
    last = null;
  };
  el.addEventListener("pointerup", release);
  el.addEventListener("pointercancel", release);
  return el;
}

export function evaluate(text, current) {
  const s = String(text).trim();
  if (!s) return null;
  if (!/^[-+*/0-9.() ]+$/.test(s)) return null;
  const expr = /^[-+*/]/.test(s) ? `${current}${s}` : s;
  try {
    const v = Function(`"use strict";return (${expr})`)();
    return Number.isFinite(v) ? v : null;
  } catch {
    return null;
  }
}

export function valueBox({ onCommit, currentValue }) {
  const input = document.createElement("input");
  input.className = "sgi-value";
  input.spellcheck = false;
  const send = () => {
    const v = evaluate(input.value, currentValue());
    if (v !== null) onCommit(v);
  };
  input.onkeydown = (e) => {
    if (e.key === "Enter") input.blur();
    e.stopPropagation();
  };
  input.onblur = send;
  return input;
}

export function button(label, onClick, extra) {
  const b = document.createElement("button");
  b.className = extra ? `sgi-btn ${extra}` : "sgi-btn";
  b.textContent = label;
  b.onclick = onClick;
  return b;
}

export function panel(title) {
  const box = document.createElement("div");
  box.className = "sgi-panel";
  const head = document.createElement("div");
  head.className = "sgi-panel-title";
  head.textContent = title;
  box.appendChild(head);
  const body = document.createElement("div");
  body.className = "sgi-panel-body";
  box.appendChild(body);
  return { box, body };
}
