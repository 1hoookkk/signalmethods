export const NAMES = ["M0 Q0 Z0", "M1 Q0 Z0", "M0 Q1 Z0", "M1 Q1 Z0", "M0 Q0 Z1", "M1 Q0 Z1", "M0 Q1 Z1", "M1 Q1 Z1"];

export function renderCorners(el, field, active, hooks) {
  el.textContent = "";
  const grid = document.createElement("div");
  grid.style.cssText = "display:grid;grid-template-columns:1fr 1fr;gap:2px;padding:3px;background:var(--well)";
  for (let i = 0; i < 8; i++) {
    const cell = document.createElement("div");
    const filled = !!field[i];
    cell.style.cssText = `padding:3px 6px;font-size:10px;cursor:pointer;border:1px solid var(--grat-major);color:${
      filled ? "var(--trace-live)" : "var(--ink-dim)"
    };background:${i === active ? "var(--titlebar)" : "transparent"}`;
    cell.textContent = `${NAMES[i]}${filled ? " ●" : " —"}`;
    cell.title = filled ? field[i].name : "empty — SET places the working lanes here";
    cell.onclick = () => hooks.onSelect(i);
    grid.appendChild(cell);
  }
  el.appendChild(grid);
  const bar = document.createElement("div");
  bar.style.cssText = "display:flex;gap:4px;padding:3px";
  for (const [label, fn, title] of [
    ["SET", hooks.onSet, "place the working lanes into the selected corner"],
    ["GET", hooks.onGet, "load the selected corner into the working lanes"],
    ["KEEP", hooks.onKeep, "save working lanes as a kept frame"],
    ["AUDIT", hooks.onAudit, "audit the full field"],
    ["WRITE", hooks.onWrite, "audit-gated 560B body write"],
  ]) {
    const b = document.createElement("button");
    b.textContent = label;
    b.title = title;
    b.style.cssText =
      "flex:1;background:var(--chrome);border:2px solid;border-color:var(--chrome-hi) var(--chrome-lo) var(--chrome-lo) var(--chrome-hi);font:inherit;font-size:10px;padding:2px 0;cursor:pointer";
    b.onclick = fn;
    bar.appendChild(b);
  }
  el.appendChild(bar);
}
