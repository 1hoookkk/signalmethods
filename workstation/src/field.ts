import type { StageCell, StageSource, VocabularyState } from "./api.js";
import { api } from "./api.js";
import { displaySr } from "./curves.js";
import type { Geometry, Law, Slot } from "./doc.js";
import { commit, doc, emptyLanes, freeLaw, IDENTITY_WORDS, isLocked } from "./doc.js";
import type { CornerWords, Lane } from "./dsp.js";
import { lanesToWords } from "./dsp.js";

export const AUTHORED_CORNERS = 4;
export const RUNTIME_CORNERS = 8;
export const STAGES = 7;

export type SeatEntry = { id: string; name?: string; type?: string };

// A section that is off keeps its geometry and contributes identity. Words are derived,
// never stashed, so turning it back on cannot lose anything. The derived array is cached
// so the curve cache (keyed by array identity) still hits between repaints.
function deriveWords(slot: Slot): CornerWords {
  const off = slot.off;
  if (!off?.some(Boolean)) return slot.words;
  const mask = off.map((v) => (v ? "1" : "0")).join("");
  if (slot.derived && slot.derivedMask === mask && slot.derivedFrom === slot.words) {
    return slot.derived;
  }
  slot.derived = slot.words.map((w, i) => (off[i] ? IDENTITY_WORDS.slice() : w));
  slot.derivedMask = mask;
  slot.derivedFrom = slot.words;
  return slot.derived;
}

export function effectiveWords(slot: Slot | null): CornerWords | null {
  return slot ? deriveWords(slot) : null;
}

export function authoredWords(): CornerWords | null {
  return doc.field[doc.selectedCorner]?.words ?? null;
}

export function isSectionOff(stage: number): boolean {
  return !!doc.field[doc.selectedCorner]?.off?.[stage];
}

export function toggleSection(stage: number): Slot | null {
  const slot = ensureCorner(doc.selectedCorner);
  if (!slot.off) slot.off = Array.from({ length: STAGES }, () => false);
  slot.off[stage] = !slot.off[stage];
  return selectCorner(doc.selectedCorner, false);
}

function blankCitations(): (string | null)[] {
  return Array.from({ length: STAGES }, () => null);
}

function geometries(slot: Slot): (Geometry | null)[] {
  if (!slot.geometry) slot.geometry = Array.from({ length: STAGES }, () => null);
  return slot.geometry;
}

export function ensureCorner(index: number): Slot {
  let slot = doc.field[index];
  if (!slot) {
    const lanes = emptyLanes();
    slot = {
      name: `C${index}`,
      lanes,
      words: lanesToWords(lanes, displaySr()),
      laws: lanes.map(() => freeLaw()),
      citations: blankCitations(),
    };
    doc.field[index] = slot;
  }
  if (!slot.citations) slot.citations = blankCitations();
  return slot;
}

export function selectCorner(index: number, create = true): Slot | null {
  const slot = create ? ensureCorner(index) : doc.field[index];
  doc.selectedCorner = index;
  if (!slot) return null;
  doc.lanes = slot.lanes;
  slot.words = slot.words || lanesToWords(slot.lanes, displaySr());
  doc.words = deriveWords(slot);
  doc.laws = slot.laws || slot.lanes.map(() => freeLaw());
  slot.laws = doc.laws;
  return slot;
}

export function commitLanes(lanes: Lane[] = doc.lanes, words: CornerWords | null = null): Slot {
  const slot = ensureCorner(doc.selectedCorner);
  const priorWords = slot.words;
  slot.lanes = lanes;
  const nextWords = words || lanesToWords(lanes, displaySr());
  if (priorWords) {
    for (let stage = 0; stage < STAGES; stage++) {
      if (slot.citations[stage]) nextWords[stage] = priorWords[stage].slice();
    }
  }
  slot.words = nextWords;
  slot.laws = doc.laws;
  doc.lanes = slot.lanes;
  doc.words = deriveWords(slot);
  doc.fieldWords = runtimeFieldWords();
  return slot;
}

function slotWords(slot: Slot | null): CornerWords | null {
  if (!slot) return null;
  if (slot.words) return deriveWords(slot);
  if (slot.lanes) return lanesToWords(slot.lanes, displaySr());
  return null;
}

export function runtimeFieldWords(): CornerWords[] | null {
  const all = doc.field.map(slotWords);
  if (all.every(Boolean)) return all as CornerWords[];
  const authored = all.slice(0, AUTHORED_CORNERS);
  const seed = authored.find(Boolean) || doc.words || null;
  if (!seed) return null;
  const filled = authored.map((words) => words || seed);
  return Array.from({ length: RUNTIME_CORNERS }, (_, i) => filled[i % AUTHORED_CORNERS]);
}

export function applyLanes(nextLanes: Lane[], nextLaws?: Law[]): number[] {
  const slot = ensureCorner(doc.selectedCorner);
  const nextWords = lanesToWords(nextLanes, displaySr());
  const outWords = slot.words.slice();
  const held: number[] = [];
  for (let stage = 0; stage < STAGES; stage++) {
    if (isLocked(stage)) {
      held.push(stage + 1);
      continue;
    }
    slot.lanes[stage] = nextLanes[stage];
    outWords[stage] = nextWords[stage];
    slot.citations[stage] = null;
    geometries(slot)[stage] = null;
    if (nextLaws) doc.laws[stage] = nextLaws[stage];
  }
  slot.words = outWords;
  slot.laws = doc.laws;
  doc.lanes = slot.lanes;
  doc.words = deriveWords(slot);
  doc.fieldWords = runtimeFieldWords();
  return held;
}

export function setSection(stage: number, cell: StageCell): Slot | null {
  const slot = ensureCorner(doc.selectedCorner);
  slot.words = slot.words.map((words, i) => (i === stage ? cell.words.slice() : words));
  slot.lanes[stage] = structuredClone(cell.lane);
  slot.citations[stage] = cell.citation || null;
  geometries(slot)[stage] = cell.geometry || null;
  return selectCorner(doc.selectedCorner, false);
}

export function clearSection(stage: number): Slot | null {
  const slot = ensureCorner(doc.selectedCorner);
  const blank = emptyLanes();
  const blankWords = lanesToWords(blank, displaySr());
  slot.lanes[stage] = blank[stage];
  slot.words = slot.words.map((words, i) => (i === stage ? blankWords[stage] : words));
  slot.citations[stage] = null;
  return selectCorner(doc.selectedCorner, false);
}

export function swapSections(from: number, to: number): Slot | null {
  for (const slot of doc.field) {
    if (!slot) continue;
    [slot.lanes[from], slot.lanes[to]] = [slot.lanes[to], slot.lanes[from]];
    slot.words = slot.words.map((w, i) => (i === from ? slot.words[to] : i === to ? slot.words[from] : w));
    if (slot.laws) [slot.laws[from], slot.laws[to]] = [slot.laws[to], slot.laws[from]];
    if (slot.citations) {
      [slot.citations[from], slot.citations[to]] = [slot.citations[to], slot.citations[from]];
    }
  }
  return selectCorner(doc.selectedCorner, false);
}

export function swapCorners(a: number, b: number): Slot | null {
  const held = doc.field[a];
  doc.field[a] = doc.field[b];
  doc.field[b] = held;
  return selectCorner(doc.selectedCorner, false);
}

export function flipSection(stage: number): Slot {
  const slot = ensureCorner(doc.selectedCorner);
  const lane = slot.lanes[stage];
  [lane.pole_hz, lane.zero_hz] = [lane.zero_hz, lane.pole_hz];
  [lane.pole_r, lane.zero_r] = [lane.zero_r, lane.pole_r];
  const w = slot.words[stage];
  slot.words = slot.words.map((words, i) => (i === stage ? [w[2], w[3], w[0], w[1], w[4]] : words));
  if (slot.citations[stage]) slot.citations[stage] = `${slot.citations[stage]} · P↔Z`;
  return commitLanes(slot.lanes, slot.words);
}

export function reset(): Slot | null {
  doc.field = Array.from({ length: RUNTIME_CORNERS }, () => null);
  doc.lanes = emptyLanes();
  doc.laws = doc.lanes.map(() => freeLaw());
  doc.words = null;
  doc.fieldWords = null;
  doc.bodyName = null;
  doc.preview = null;
  return selectCorner(0);
}

export function copyCorner(from: number, to: number): Slot | null {
  const source = doc.field[from];
  if (!source) return null;
  doc.field[to] = {
    ...structuredClone(source),
    name: `C${to}`,
  };
  return doc.field[to];
}

export async function seatState(state: VocabularyState, stage: number) {
  if (isLocked(stage)) throw new Error(`S${stage + 1} LOCK`);
  const data = await api.target("stage", state.seat.id, { stage: state.seat.stage });
  const cell = data.cells.find((entry) => entry.corner === state.seat.corner);
  if (!cell) throw new Error(`no cell for ${state.seat.id} C${state.seat.corner}`);
  commit();
  setSection(stage, cell);
}

export async function seatCell(
  source: StageSource,
  sourceStage: number,
  sourceCorner: number,
  destination: number,
) {
  if (isLocked(destination)) throw new Error(`S${destination + 1} LOCK`);
  const data = await api.target("stage", source.id, { stage: sourceStage });
  const cell = data.cells.find((entry) => entry.corner === sourceCorner);
  if (!cell) throw new Error(`no cell for ${source.id} C${sourceCorner}`);
  commit();
  setSection(destination, cell);
}

export async function seatTrack(source: StageSource, sourceStage: number, destination: number) {
  if (isLocked(destination)) throw new Error(`S${destination + 1} LOCK`);
  const data = await api.target("stage", source.id, { stage: sourceStage });
  commit();
  for (const cell of data.cells) {
    const slot = ensureCorner(cell.corner);
    slot.words = slot.words.map((words, i) => (i === destination ? cell.words.slice() : words));
    slot.lanes[destination] = structuredClone(cell.lane);
    slot.citations[destination] = cell.citation;
    geometries(slot)[destination] = cell.geometry || null;
  }
  selectCorner(doc.selectedCorner, false);
}

export async function importFactory(entry: SeatEntry, stages: number, corners: number) {
  const data = await Promise.all(
    Array.from({ length: stages }, (_, stage) => api.target("stage", entry.id, { stage })),
  );
  commit();
  const blank = emptyLanes();
  const blankWords = lanesToWords(blank, displaySr());
  for (let corner = 0; corner < corners; corner++) {
    const slot = ensureCorner(corner);
    const nextWords: CornerWords = [];
    for (let stage = 0; stage < STAGES; stage++) {
      const cell = stage < stages ? data[stage].cells.find((one) => one.corner === corner) : null;
      nextWords.push(cell ? cell.words.slice() : blankWords[stage].slice());
      slot.lanes[stage] = cell ? structuredClone(cell.lane) : blank[stage];
      slot.citations[stage] = cell ? cell.citation : null;
      geometries(slot)[stage] = cell ? cell.geometry || null : null;
    }
    slot.words = nextWords;
    slot.name = `C${corner}`;
  }
  for (let corner = corners; corner < RUNTIME_CORNERS; corner++) doc.field[corner] = null;
  doc.bodyName = entry.name ?? null;
  doc.selectedCorner = 0;
  selectCorner(0, false);
  return { stages, corners };
}

export async function seatTemplate(entry: SeatEntry, stages: number) {
  const data = await Promise.all(
    Array.from({ length: stages }, (_, stage) => api.target("stage", entry.id, { stage })),
  );
  commit();
  const sr = displaySr();
  const slot = ensureCorner(doc.selectedCorner);
  const blank = emptyLanes();
  const blankWords = lanesToWords(blank, sr);
  const nextWords: CornerWords = [];
  const held: number[] = [];
  for (let stage = 0; stage < STAGES; stage++) {
    if (isLocked(stage)) {
      nextWords.push(slot.words[stage]);
      held.push(stage + 1);
      continue;
    }
    const cell = stage < stages ? data[stage].cells.find((one) => one.corner === 0) : null;
    nextWords.push(cell ? cell.words.slice() : blankWords[stage].slice());
    slot.lanes[stage] = cell ? structuredClone(cell.lane) : blank[stage];
    slot.citations[stage] = cell ? cell.citation : null;
    geometries(slot)[stage] = cell ? cell.geometry || null : null;
  }
  slot.words = nextWords;
  doc.bodyName = entry.name ?? null;
  selectCorner(doc.selectedCorner, false);
  return { stages, held };
}
