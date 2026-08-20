import type { CornerWords, Lane } from "./dsp.js";

export type Law = { writable: boolean; freedom: boolean[]; zone: unknown };
export type Curve = ArrayLike<number>;

// A verified reference vowel chosen from a published table. Never inferred from audio.
export type Reference = { name: string; formants: { hz: number; bandwidth_hz: number }[] };

export type RootPair =
  | { kind: "off" }
  | { kind: "conj"; hz: number; r: number }
  | { kind: "real"; pair: number[] };

export type Geometry = {
  pole: RootPair;
  zero: RootPair;
  scale: number;
  real_pair: boolean;
};

export type Slot = {
  name: string;
  lanes: Lane[];
  words: CornerWords;
  laws: Law[];
  citations: (string | null)[];
  geometry?: (Geometry | null)[];
  off?: boolean[];
  derived?: CornerWords;
  derivedMask?: string;
  derivedFrom?: CornerWords;
  target?: Curve | null;
  targetName?: string | null;
};

export type Doc = {
  bodyName: string | null;
  lanes: Lane[];
  laws: Law[];
  words: CornerWords | null;
  fieldWords: CornerWords[] | null;
  preview: Float32Array | null;
  candidate: Float32Array | null;
  selected: number;
  selectedCorner: number;
  field: (Slot | null)[];
  reference: Reference | null;
};

export const doc: Doc = {
  bodyName: null,
  lanes: emptyLanes(),
  laws: Array.from({ length: 7 }, freeLaw),
  words: null,
  fieldWords: null,
  preview: null,
  candidate: null,
  selected: 1,
  selectedCorner: 0,
  reference: null,
  field: Array.from({ length: 8 }, () => null),
};

export const IDENTITY_WORDS = [0xdfff, 0xffff, 0xdfff, 0xffff, 0xdfff];

export function currentSlot(): Slot | null {
  return doc.field[doc.selectedCorner];
}

export function currentTarget(): Curve | null {
  return currentSlot()?.target ?? null;
}

export function currentTargetName(): string | null {
  return currentSlot()?.targetName ?? null;
}

export function fieldCorners(): { corners: Lane[][]; square: boolean } | null {
  const filled = doc.field.map((s) => !!s);
  const cube = filled.every(Boolean);
  const square = filled.slice(0, 4).every(Boolean) && filled.slice(4).every((f) => !f);
  if (!cube && !square) return null;
  const corners = doc.field.map((s, i) => (s ? s.lanes : (doc.field[i - 4] as Slot).lanes));
  return { corners, square };
}

export function lawState(law: Law): "HELD" | "PIN" | "FREE" {
  if (!law.writable) return "HELD";
  if (!law.freedom[0]) return "PIN";
  return "FREE";
}

export function isLocked(stage: number): boolean {
  const law = doc.laws[stage];
  return !!law && lawState(law) !== "FREE";
}

export function freeLaw(): Law {
  return { writable: true, freedom: [true, true, true, true], zone: null };
}

export function pinLaw(): Law {
  return { writable: false, freedom: [false, false, false, false], zone: null };
}

export function emptyLanes(): Lane[] {
  return Array.from({ length: 7 }, () => ({
    pole_hz: 0,
    pole_r: 0,
    zero_hz: 0,
    zero_r: 0,
    scale: 1,
  }));
}

const past: Doc[] = [];
const future: Doc[] = [];

const SNAPSHOT_KEY = "trench.session";

export type Snapshot = {
  lanes: Lane[];
  laws: Law[];
  field: (Slot | null)[];
  bodyName: string | null;
  selectedCorner: number;
};

export function saveSnapshot() {
  try {
    localStorage.setItem(
      SNAPSHOT_KEY,
      JSON.stringify({
        lanes: doc.lanes,
        laws: doc.laws,
        field: doc.field,
        bodyName: doc.bodyName,
        selectedCorner: doc.selectedCorner,
      }),
    );
  } catch {}
}

export function loadSnapshot(): Snapshot | null {
  try {
    const raw = localStorage.getItem(SNAPSHOT_KEY);
    return raw ? (JSON.parse(raw) as Snapshot) : null;
  } catch {
    return null;
  }
}

export function commit() {
  past.push(structuredClone(doc));
  if (past.length > 100) past.shift();
  future.length = 0;
  setTimeout(saveSnapshot, 0);
}

export function undo(): boolean {
  const prev = past.pop();
  if (!prev) return false;
  future.push(structuredClone(doc));
  Object.assign(doc, prev);
  return true;
}

export function redo(): boolean {
  const next = future.pop();
  if (!next) return false;
  past.push(structuredClone(doc));
  Object.assign(doc, next);
  return true;
}

export const DRAW_POINTS = 1024;

export const DRAW_GRID = Float64Array.from(
  { length: DRAW_POINTS },
  (_, i) => 40 * (16000 / 40) ** (i / (DRAW_POINTS - 1)),
);

export function toDrawGrid(curve: Curve): Float32Array {
  const span = curve.length - 1;
  const out = new Float32Array(DRAW_POINTS);
  for (let i = 0; i < DRAW_POINTS; i++) {
    out[i] = curve[Math.round((i * span) / (DRAW_POINTS - 1))];
  }
  return out;
}
