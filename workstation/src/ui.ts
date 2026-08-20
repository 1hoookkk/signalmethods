// Trench's own widget set. Every control here is built and styled by the workstation;
// no browser-default input, button or scrollbar survives into the interface. These are
// ordinary DOM — canvas is reserved for measurement surfaces.

export function element<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  className?: string,
  text?: string,
): HTMLElementTagNameMap[K] {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

export type Panel = { root: HTMLElement; body: HTMLElement; head: HTMLElement; value: HTMLElement };

// A panel is a hairline rule, a label and a body. It has no title bar, no frame and no
// background of its own; the body decides whether it is chassis or measurement well.
export function panel(title: string, options: { grow?: boolean; scroll?: boolean } = {}): Panel {
  const root = element("section", options.grow ? "panel grow" : "panel");
  const head = element("header");
  const label = element("span", undefined, title);
  const value = element("span", "value");
  head.append(label, value);
  const body = element("div", options.scroll ? "body scroll" : "body");
  root.append(head, body);
  return { root, body, head, value };
}

export function key(label: string, onClick: () => void, className = ""): HTMLButtonElement {
  const node = element("button", `key ${className}`.trim(), label);
  node.type = "button";
  node.onclick = onClick;
  return node;
}

export function button(label: string, onClick: () => void, className = ""): HTMLButtonElement {
  const node = element("button", `btn ${className}`.trim(), label);
  node.type = "button";
  node.onclick = onClick;
  return node;
}

export function keys(columns: 1 | 2 | 4, ...children: HTMLElement[]): HTMLElement {
  const node = element("div", `keys c${columns}`);
  node.append(...children);
  return node;
}

export type NumericOptions = {
  unit?: string;
  digits?: number;
  min?: number;
  max?: number;
  // Movement per pixel of horizontal drag, in the field's own units.
  step?: number;
  // Multiplicative scrub suits frequency, where a fixed step is useless across decades.
  ratio?: boolean;
  onChange?: (value: number) => void;
};

export type Numeric = { root: HTMLElement; set: (value: number | null) => void };

// Drag to scrub, double-click to type. Read-only when no onChange is supplied, which is
// how derived quantities are shown without pretending to be editable.
export function numeric(label: string, options: NumericOptions = {}): Numeric {
  const { unit = "", digits = 2, min = -Infinity, max = Infinity, step = 1, ratio = false } = options;
  const root = element("div", "num");
  const name = element("label", undefined, label);
  const field = element("div", options.onChange ? "field" : "field flat");
  const value = element("span", "v", "—");
  const units = element("span", "u", unit);
  field.append(value, units);
  root.append(name, field);

  let current: number | null = null;

  const show = (v: number | null) => {
    current = v;
    value.textContent = v === null ? "—" : v.toFixed(digits);
  };

  if (options.onChange) {
    const commit = (v: number) => {
      const clamped = Math.min(max, Math.max(min, v));
      show(clamped);
      options.onChange?.(clamped);
    };

    let from = 0;
    let start = 0;
    let dragging = false;
    field.addEventListener("pointerdown", (e) => {
      if (current === null) return;
      dragging = true;
      from = e.clientX;
      start = current;
      field.setPointerCapture(e.pointerId);
      e.preventDefault();
    });
    field.addEventListener("pointermove", (e) => {
      if (!dragging) return;
      const dx = e.clientX - from;
      commit(ratio ? start * (1 + step) ** dx : start + dx * step);
    });
    const end = () => {
      dragging = false;
    };
    field.addEventListener("pointerup", end);
    field.addEventListener("pointercancel", end);
    field.addEventListener("lostpointercapture", end);

    field.addEventListener("dblclick", () => {
      const entry = element("input");
      entry.value = current === null ? "" : String(current);
      field.replaceChildren(entry);
      entry.focus();
      entry.select();
      const close = (apply: boolean) => {
        const typed = Number(entry.value);
        field.replaceChildren(value, units);
        if (apply && Number.isFinite(typed)) commit(typed);
        else show(current);
      };
      entry.onblur = () => close(true);
      entry.onkeydown = (e) => {
        if (e.key === "Enter") close(true);
        if (e.key === "Escape") close(false);
        e.stopPropagation();
      };
    });
  }

  return { root, set: show };
}

export type Slider = { root: HTMLElement; set: (value: number) => void };

export function slider(min: number, max: number, value: number, onInput: (v: number) => void): Slider {
  const root = element("div", "slider");
  const track = element("div", "track");
  const fill = element("div", "fill");
  const knob = element("div", "knob");
  root.append(track, fill, knob);

  let current = value;
  const show = (v: number) => {
    current = Math.min(max, Math.max(min, v));
    const t = (current - min) / (max - min || 1);
    fill.style.width = `${t * 100}%`;
    knob.style.left = `${t * 100}%`;
  };
  show(value);

  let dragging = false;
  const fromEvent = (e: PointerEvent) => {
    const rect = root.getBoundingClientRect();
    const t = Math.min(1, Math.max(0, (e.clientX - rect.left) / Math.max(1, rect.width)));
    show(min + t * (max - min));
    onInput(current);
  };
  root.addEventListener("pointerdown", (e) => {
    dragging = true;
    root.setPointerCapture(e.pointerId);
    fromEvent(e);
  });
  root.addEventListener("pointermove", (e) => {
    if (dragging) fromEvent(e);
  });
  const end = () => {
    dragging = false;
  };
  root.addEventListener("pointerup", end);
  root.addEventListener("pointercancel", end);
  root.addEventListener("lostpointercapture", end);

  return { root, set: show };
}

export type ListEntry = { id: string; label: string; tag?: string; onPick: () => void };
export type ListGroup = { title: string; entries: ListEntry[] };
export type List = { root: HTMLElement; render: (groups: ListGroup[]) => void; select: (id: string) => void };

// A collapsible source list. Groups remember whether they are open, so the tree does not
// reset every time the library is rebuilt.
export function list(): List {
  const root = element("div", "list");
  const open = new Set<string>();
  let selected = "";
  let groups: ListGroup[] = [];

  function render(next: ListGroup[] = groups) {
    groups = next;
    root.replaceChildren();
    for (const group of groups) {
      const head = element("div", "group");
      const caret = element("span", "caret", open.has(group.title) ? "▾" : "▸");
      const title = element("span", undefined, group.title);
      const count = element("span", "n", String(group.entries.length));
      head.append(caret, title, count);
      head.onclick = () => {
        if (open.has(group.title)) open.delete(group.title);
        else open.add(group.title);
        render();
      };
      root.appendChild(head);
      if (!open.has(group.title)) continue;
      for (const entry of group.entries) {
        const item = element("div", entry.id === selected ? "item on" : "item", entry.label);
        item.title = entry.tag ?? entry.label;
        item.onclick = () => {
          selected = entry.id;
          render();
          entry.onPick();
        };
        root.appendChild(item);
      }
    }
  }

  return {
    root,
    render,
    select(id) {
      selected = id;
      render();
    },
  };
}
