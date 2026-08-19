export const doc = {
  targetName: null,
  target: null,
  peaks: [],
  lanes: emptyLanes(),
  laws: emptyLaws(),
  roles: emptyRoles(),
  words: null,
  cornerWords: null,
  fieldWords: null,
  preview: null,
  candidate: null,
  proposal: null,
  rms: null,
  packing: null,
  selected: 1,
  selectedCorner: 0,
  field: Array.from({ length: 8 }, () => null),
};

export function fieldCorners() {
  const filled = doc.field.map((s) => !!s);
  const cube = filled.every(Boolean);
  const square = filled.slice(0, 4).every(Boolean) && filled.slice(4).every((f) => !f);
  if (!cube && !square) return null;
  const corners = doc.field.map((s, i) =>
    s ? s.lanes : doc.field[i - 4].lanes
  );
  return { corners, square };
}

export function emptyLaws() {
  return Array.from({ length: 7 }, () => ({
    writable: true, freedom: [true, true, true, true], zone: null,
  }));
}

export function emptyRoles() {
  return Array.from({ length: 7 }, () => null);
}

export function lawState(law) {
  if (!law.writable) return "HELD";
  if (!law.freedom[0]) return "PIN";
  return "FREE";
}

export function freeLaw() {
  return { writable: true, freedom: [true, true, true, true], zone: null };
}

export function pinLaw() {
  return { writable: false, freedom: [false, false, false, false], zone: null };
}

export function cycleLaw(law) {
  const state = lawState(law);
  if (state === "FREE") return { writable: true, freedom: [false, true, true, true], zone: null };
  if (state === "PIN") return { writable: false, freedom: [true, true, true, true], zone: null };
  return { writable: true, freedom: [true, true, true, true], zone: null };
}

export function emptyLanes() {
  return Array.from({ length: 7 }, () => ({
    pole_hz: 0, pole_r: 0, zero_hz: 0, zero_r: 0, scale: 1,
  }));
}

const past = [];
const future = [];
let lastLabel = "—";

const SNAPSHOT_KEY = "trench.session";

export function saveSnapshot() {
  try {
    localStorage.setItem(
      SNAPSHOT_KEY,
      JSON.stringify({
        lanes: doc.lanes,
        laws: doc.laws,
        roles: doc.roles,
        field: doc.field,
        targetName: doc.targetName,
        selectedCorner: doc.selectedCorner,
      })
    );
  } catch (e) {}
}

export function loadSnapshot() {
  try {
    const raw = localStorage.getItem(SNAPSHOT_KEY);
    return raw ? JSON.parse(raw) : null;
  } catch (e) {
    return null;
  }
}

export function commit(label) {
  past.push(structuredClone(doc));
  if (past.length > 100) past.shift();
  future.length = 0;
  lastLabel = label;
  setTimeout(saveSnapshot, 0);
}

export function undo() {
  const prev = past.pop();
  if (!prev) return false;
  future.push(structuredClone(doc));
  Object.assign(doc, prev);
  return true;
}

export function redo() {
  const next = future.pop();
  if (!next) return false;
  past.push(structuredClone(doc));
  Object.assign(doc, next);
  return true;
}

export function undoLabel() {
  return lastLabel;
}

export const GRID = Array.from({ length: 1024 }, (_, i) =>
  40 * Math.pow(16000 / 40, i / 1023)
);

export const DRAW_POINTS = 1024;

export const DRAW_GRID = Float64Array.from({ length: DRAW_POINTS }, (_, i) =>
  40 * Math.pow(16000 / 40, i / (DRAW_POINTS - 1))
);

export function toDrawGrid(curve) {
  const span = curve.length - 1;
  const out = new Float32Array(DRAW_POINTS);
  for (let i = 0; i < DRAW_POINTS; i++) {
    out[i] = curve[Math.round((i * span) / (DRAW_POINTS - 1))];
  }
  return out;
}
