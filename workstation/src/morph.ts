import type { Doc, Slot } from "./doc.js";
import { css, scope } from "./render.js";
import { element, panel } from "./ui.js";

const PAD = 34;

const CORNER_AT: [number, number][] = [
  [0, 0],
  [1, 0],
  [0, 1],
  [1, 1],
];

export type MorphHooks = {
  onCorner: (i: number) => void;
  onRide: (m: number, q: number, z: number) => void;
};

export type Morph = {
  fieldPanel: HTMLElement;
  responsePanel: HTMLElement;
  responseCanvas: HTMLCanvasElement;
  paint: (doc: Doc, m: number, q: number, z: number, residual: (slot: Slot | null) => number | null) => void;
};

// What happens between authored endpoints. The corner field is the object here and the
// response is what it produces; no roots are edited in this workspace.
export function mountMorph(hooks: MorphHooks): Morph {
  const fieldSection = panel("CORNER FIELD", { grow: true });
  fieldSection.body.style.background = "var(--well)";
  const canvas = element("canvas", "surface pointer");
  fieldSection.body.appendChild(canvas);

  const responseSection = panel("RESPONSE AT POSITION");
  responseSection.root.style.flex = "0 0 42%";
  responseSection.body.style.background = "var(--well)";
  const responseCanvas = element("canvas", "surface");
  responseSection.body.appendChild(responseCanvas);

  const position = { m: 0, q: 0, z: 0 };
  let doc: Doc | null = null;
  let residual: (slot: Slot | null) => number | null = () => null;

  const box = (w: number, h: number) => ({ x0: PAD, y0: PAD, x1: w - PAD, y1: h - PAD });
  const place = (b: ReturnType<typeof box>, m: number, q: number): [number, number] => [
    b.x0 + (b.x1 - b.x0) * m,
    b.y1 - (b.y1 - b.y0) * q,
  ];

  function paintField() {
    scope(canvas).frame((g, w, h) => {
      const b = box(w, h);
      g.fillStyle = css("--well");
      g.fillRect(0, 0, w, h);
      g.font = `9px ${css("--ui")}`;
      g.fillStyle = css("--well-dim");
      g.fillText("Q", b.x0 - 14, b.y0 - 7);
      g.fillText("M", b.x1 + 5, b.y1 + 4);

      g.strokeStyle = css("--well-grid");
      g.lineWidth = 1;
      g.strokeRect(b.x0 + 0.5, b.y0 + 0.5, b.x1 - b.x0, b.y1 - b.y0);

      const [cx, cy] = place(b, position.m, position.q);
      g.strokeStyle = css("--accent");
      g.globalAlpha = 0.35;
      g.beginPath();
      g.moveTo(b.x0, cy);
      g.lineTo(b.x1, cy);
      g.moveTo(cx, b.y0);
      g.lineTo(cx, b.y1);
      g.stroke();
      g.globalAlpha = 1;

      for (let i = 0; i < 4; i++) {
        const [m, q] = CORNER_AT[i];
        const [x, y] = place(b, m, q);
        const slot = doc?.field[i] ?? null;
        const on = i === doc?.selectedCorner;
        g.fillStyle = slot ? css("--live") : css("--well-dim");
        g.strokeStyle = css("--well-dim");
        g.beginPath();
        g.arc(x, y, on ? 7 : 5, 0, 7);
        if (slot) g.fill();
        else g.stroke();
        const delta = residual(slot);
        g.fillStyle = on ? css("--well-ink") : css("--well-dim");
        g.textAlign = m ? "right" : "left";
        g.textBaseline = q ? "bottom" : "top";
        g.fillText(
          `C${i}${delta === null ? "" : `  Δ ${delta.toFixed(2)} dB`}`,
          x + (m ? -11 : 11),
          y + (q ? -3 : 3),
        );
        g.textAlign = "left";
        g.textBaseline = "alphabetic";
      }

      g.fillStyle = css("--accent");
      g.beginPath();
      g.arc(cx, cy, 5, 0, 7);
      g.fill();
    });
  }

  function nearestCorner(m: number, q: number): number | null {
    for (let i = 0; i < 4; i++) {
      const [cm, cq] = CORNER_AT[i];
      if (Math.hypot(m - cm, q - cq) < 0.1) return i;
    }
    return null;
  }

  let dragging = false;
  function at(e: PointerEvent): [number, number] {
    const rect = canvas.getBoundingClientRect();
    const b = box(rect.width, rect.height);
    return [
      Math.min(1, Math.max(0, (e.clientX - rect.left - b.x0) / Math.max(1, b.x1 - b.x0))),
      Math.min(1, Math.max(0, 1 - (e.clientY - rect.top - b.y0) / Math.max(1, b.y1 - b.y0))),
    ];
  }

  canvas.addEventListener("pointerdown", (e) => {
    const [m, q] = at(e);
    const corner = nearestCorner(m, q);
    if (corner !== null && corner !== doc?.selectedCorner) return hooks.onCorner(corner);
    dragging = true;
    canvas.setPointerCapture(e.pointerId);
    hooks.onRide(m, q, position.z);
  });
  canvas.addEventListener("pointermove", (e) => {
    if (!dragging) return;
    const [m, q] = at(e);
    hooks.onRide(m, q, position.z);
  });
  const release = () => {
    dragging = false;
  };
  canvas.addEventListener("pointerup", release);
  canvas.addEventListener("pointercancel", release);
  canvas.addEventListener("lostpointercapture", release);

  return {
    fieldPanel: fieldSection.root,
    responsePanel: responseSection.root,
    responseCanvas,
    paint(nextDoc, m, q, z, nextResidual) {
      doc = nextDoc;
      residual = nextResidual;
      position.m = m;
      position.q = q;
      position.z = z;
      fieldSection.value.textContent = `C${nextDoc.selectedCorner} · ${nextDoc.field.filter(Boolean).length} of 8 seated`;
      responseSection.value.textContent = `M ${m.toFixed(2)}  Q ${q.toFixed(2)}  Z ${z.toFixed(2)}`;
      paintField();
    },
  };
}
