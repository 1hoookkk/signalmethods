import { displaySr } from "./curves.js";
import type { Doc } from "./doc.js";
import type { StageWords } from "./dsp.js";
import { decode, encode } from "./dsp.js";
import { css, scope } from "./render.js";

// The E-mu encoded parameter space itself. A packed section is five 16-bit words; two
// carry the zero pair and two carry the pole pair, and the runtime interpolates exactly
// those numbers. So the axes here ARE the words: equal word steps are equal steps on the
// plot, and corner-to-corner travel is straight because lerpU16 is linear in this space
// and nowhere else.
//
// Word layout per section, from the encoder in dsp.ts:
//   0 zero frequency   1 zero resonance   2 pole frequency   3 pole resonance   4 scale
const FREQ_WORD = { pole: 2, zero: 0 } as const;
const RES_WORD = { pole: 3, zero: 1 } as const;
const WORD_MAX = 0xffff;
const TICK = 0x2000;
const PAD_L = 32;
const PAD_R = 6;
const PAD_T = 6;
const PAD_B = 14;

export type RootKind = "pole" | "zero";
export type RootHit = { lane: number; kind: RootKind };

export type ArmadilloHooks = {
  isLocked: (lane: number) => boolean;
  ceiling: () => number;
  onLocked: (lane: number) => void;
  onRefused: (lane: number) => void;
  onGrab: (hit: RootHit) => void;
  onMove: (hit: RootHit) => void;
  onRelease: (hit: RootHit) => void;
};

type Frame = { x0: number; y0: number; w: number; h: number };

function frame(w: number, h: number): Frame {
  return { x0: PAD_L, y0: PAD_T, w: Math.max(10, w - PAD_L - PAD_R), h: Math.max(10, h - PAD_T - PAD_B) };
}

const xOfWord = (f: Frame, word: number) => f.x0 + (word / WORD_MAX) * f.w;
const yOfWord = (f: Frame, word: number) => f.y0 + (word / WORD_MAX) * f.h;
const wordOfX = (f: Frame, x: number) =>
  Math.round(Math.min(WORD_MAX, Math.max(0, ((x - f.x0) / f.w) * WORD_MAX)));
const wordOfY = (f: Frame, y: number) =>
  Math.round(Math.min(WORD_MAX, Math.max(0, ((y - f.y0) / f.h) * WORD_MAX)));

// The one ceiling, put through the one encoder the machine uses.
const ceilingWord = (ceiling: number) => encode(1 - ceiling * ceiling);

// The inverse of the encoder in dsp.ts, using the same decode: given the two packed words
// for a root, recover the frequency and radius the encoder started from.
function rootFromWords(freqWord: number, resWord: number, sr: number) {
  const c1 = decode(resWord);
  const r = Math.sqrt(Math.max(0, 1 - c1));
  const c0 = 4 * decode(freqWord) + c1;
  const cos = r > 1e-9 ? (2 - c0) / (2 * r) : 1;
  return { hz: (Math.acos(Math.min(1, Math.max(-1, cos))) * sr) / (2 * Math.PI), r };
}

function wordsOf(doc: Doc, stage: number): StageWords | null {
  const words = doc.field[doc.selectedCorner]?.words ?? doc.words;
  return words?.[stage] ?? null;
}

const isRealPair = (doc: Doc, stage: number) => !!doc.field[doc.selectedCorner]?.geometry?.[stage]?.real_pair;

export function drawArmadillo(canvas: HTMLCanvasElement, doc: Doc, ceiling: number) {
  scope(canvas).frame((g, w, h) => paint(g, w, h, doc, ceiling));
}

function grid(g: CanvasRenderingContext2D, f: Frame) {
  g.font = `9px ${css("--ui")}`;
  g.lineWidth = 1;
  g.textBaseline = "middle";
  for (let word = 0; word <= WORD_MAX; word += TICK) {
    const x = Math.floor(xOfWord(f, word)) + 0.5;
    const y = Math.floor(yOfWord(f, word)) + 0.5;
    g.strokeStyle = css("--well-rule");
    g.beginPath();
    g.moveTo(x, f.y0);
    g.lineTo(x, f.y0 + f.h);
    g.moveTo(f.x0, y);
    g.lineTo(f.x0 + f.w, y);
    g.stroke();
    g.fillStyle = css("--well-dim");
    g.textAlign = "center";
    g.fillText(word.toString(16).padStart(4, "0"), x, f.y0 + f.h + 7);
    g.textAlign = "right";
    g.fillText(word.toString(16).padStart(4, "0"), f.x0 - 3, y);
  }
  g.strokeStyle = css("--well-grid");
  g.strokeRect(f.x0 + 0.5, f.y0 + 0.5, f.w, f.h);
  g.textAlign = "left";
  g.textBaseline = "alphabetic";
}

function paint(g: CanvasRenderingContext2D, w: number, h: number, doc: Doc, ceiling: number) {
  const f = frame(w, h);
  g.fillStyle = css("--well");
  g.fillRect(0, 0, w, h);
  grid(g, f);

  const ceilY = Math.floor(yOfWord(f, ceilingWord(ceiling))) + 0.5;
  g.strokeStyle = css("--ceiling");
  g.setLineDash([4, 3]);
  g.beginPath();
  g.moveTo(f.x0, ceilY);
  g.lineTo(f.x0 + f.w, ceilY);
  g.stroke();
  g.setLineDash([]);

  // Where the selected section sits at every authored corner. The joins are straight
  // because the machine lerps these words; a bent path would mean the plot is lying.
  const sel = doc.selected;
  const track: [number, number][] = [];
  for (let corner = 0; corner < 4; corner++) {
    const words = doc.field[corner]?.words?.[sel];
    if (words) track.push([xOfWord(f, words[FREQ_WORD.pole]), yOfWord(f, words[RES_WORD.pole])]);
  }
  if (track.length > 1) {
    g.strokeStyle = css(`--s${sel + 1}`);
    g.globalAlpha = 0.35;
    g.setLineDash([3, 3]);
    g.beginPath();
    g.moveTo(track[0][0], track[0][1]);
    for (const [x, y] of track.slice(1)) g.lineTo(x, y);
    g.stroke();
    g.setLineDash([]);
    g.globalAlpha = 1;
  }

  g.font = `9px ${css("--ui")}`;
  for (let i = 0; i < 7; i++) {
    const words = wordsOf(doc, i);
    if (!words) continue;
    const on = i === sel;
    const color = css(`--s${i + 1}`);
    const px = xOfWord(f, words[FREQ_WORD.pole]);
    const py = yOfWord(f, words[RES_WORD.pole]);
    const zx = xOfWord(f, words[FREQ_WORD.zero]);
    const zy = yOfWord(f, words[RES_WORD.zero]);
    g.strokeStyle = color;
    g.globalAlpha = on ? 0.5 : 0.12;
    g.lineWidth = 1;
    g.beginPath();
    g.moveTo(px, py);
    g.lineTo(zx, zy);
    g.stroke();
    g.globalAlpha = on ? 1 : 0.26;
    g.fillStyle = color;
    g.beginPath();
    g.arc(px, py, on ? 5 : 3.5, 0, 7);
    g.fill();
    g.strokeStyle = color;
    g.lineWidth = 1.8;
    g.beginPath();
    g.arc(zx, zy, on ? 4.5 : 3, 0, 7);
    g.stroke();
    g.lineWidth = 1;
    if (on) {
      g.fillText(`${i + 1}`, px + 7, py + 3);
      g.fillText(`${i + 1}`, zx + 7, zy + 3);
    }
    g.globalAlpha = 1;
  }
}

function hitRoot(canvas: HTMLCanvasElement, doc: Doc, px: number, py: number): RootHit | null {
  const f = frame(canvas.clientWidth, canvas.clientHeight);
  let best: RootHit | null = null;
  let bestD = 11;
  for (let i = 0; i < 7; i++) {
    const words = wordsOf(doc, i);
    if (!words) continue;
    for (const kind of ["pole", "zero"] as const) {
      const d = Math.hypot(xOfWord(f, words[FREQ_WORD[kind]]) - px, yOfWord(f, words[RES_WORD[kind]]) - py);
      if (d < bestD) {
        bestD = d;
        best = { lane: i, kind };
      }
    }
  }
  return best;
}

export function attachArmadillo(canvas: HTMLCanvasElement, doc: Doc, cb: ArmadilloHooks) {
  let drag: RootHit | null = null;
  const local = (e: PointerEvent): [number, number] => {
    const rect = canvas.getBoundingClientRect();
    return [e.clientX - rect.left, e.clientY - rect.top];
  };
  canvas.addEventListener("pointerdown", (e) => {
    const [px, py] = local(e);
    const hit = hitRoot(canvas, doc, px, py);
    if (!hit) return;
    if (cb.isLocked(hit.lane)) return cb.onLocked(hit.lane);
    // A real-axis pair has no conjugate frequency/radius to write back into, so it is
    // shown here but not dragged. Refusing beats silently deleting the geometry.
    if (isRealPair(doc, hit.lane)) return cb.onRefused(hit.lane);
    drag = hit;
    cb.onGrab(hit);
    canvas.setPointerCapture(e.pointerId);
  });
  canvas.addEventListener("pointermove", (e) => {
    if (!drag) return;
    const [px, py] = local(e);
    dragTo(canvas, doc, drag, px, py, cb.ceiling());
    cb.onMove(drag);
  });
  const end = () => {
    if (!drag) return;
    const held = drag;
    drag = null;
    cb.onRelease(held);
  };
  canvas.addEventListener("pointerup", end);
  canvas.addEventListener("pointercancel", end);
  canvas.addEventListener("lostpointercapture", end);
}

// Pointer position -> packed words -> the geometry those words decode to. The lane is
// re-encoded by the same encoder on commit, so what is dragged is a legal packed value
// and not a UI approximation of one.
function dragTo(canvas: HTMLCanvasElement, doc: Doc, hit: RootHit, px: number, py: number, ceiling: number) {
  const f = frame(canvas.clientWidth, canvas.clientHeight);
  const resWord = Math.max(wordOfY(f, py), hit.kind === "pole" ? ceilingWord(ceiling) : 0);
  const { hz, r } = rootFromWords(wordOfX(f, px), resWord, displaySr());
  const lane = doc.lanes[hit.lane];
  if (hit.kind === "pole") {
    lane.pole_hz = hz;
    lane.pole_r = Math.min(r, ceiling);
  } else {
    lane.zero_hz = hz;
    lane.zero_r = Math.min(r, 1);
  }
}
