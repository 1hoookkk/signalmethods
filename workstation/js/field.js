import { api } from "./api.js";
import { displaySr } from "./curves.js";
import { commit, doc, emptyLanes, emptyRoles, freeLaw, lawState } from "./doc.js";
import { lanesToWords } from "./dsp.js";

export const AUTHORED_CORNERS = 4;
export const RUNTIME_CORNERS = 8;
export const STAGES = 7;

let onBind = () => {};

export function setBindHook(fn) {
  onBind = fn || (() => {});
}

export function isLocked(stage) {
  return !!doc.laws[stage] && lawState(doc.laws[stage]) !== "FREE";
}

function blankCitations() {
  return Array.from({ length: STAGES }, () => null);
}

function geometries(slot) {
  if (!slot.geometry) slot.geometry = Array.from({ length: STAGES }, () => null);
  return slot.geometry;
}

export function ensureCorner(index) {
  let slot = doc.field[index];
  if (!slot) {
    const lanes = emptyLanes();
    slot = {
      name: `C${index}`,
      lanes,
      words: lanesToWords(lanes, displaySr()),
      roles: emptyRoles(),
      laws: lanes.map(() => freeLaw()),
      citations: blankCitations(),
    };
    doc.field[index] = slot;
  }
  if (!slot.citations) slot.citations = blankCitations();
  return slot;
}

export function selectCorner(index, create = true) {
  const slot = create ? ensureCorner(index) : doc.field[index];
  doc.selectedCorner = index;
  if (!slot) return null;
  doc.lanes = slot.lanes;
  doc.words = slot.words || lanesToWords(slot.lanes, displaySr());
  slot.words = doc.words;
  doc.roles = slot.roles || emptyRoles();
  doc.laws = slot.laws || slot.lanes.map(() => freeLaw());
  slot.roles = doc.roles;
  slot.laws = doc.laws;
  onBind(slot);
  return slot;
}

export function commitLanes(lanes = doc.lanes, words = null) {
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
  slot.roles = doc.roles;
  slot.laws = doc.laws;
  doc.lanes = slot.lanes;
  doc.words = slot.words;
  doc.fieldWords = runtimeFieldWords();
  return slot;
}

function slotWords(slot) {
  if (!slot) return null;
  if (slot.words) return slot.words;
  if (slot.lanes) return lanesToWords(slot.lanes, displaySr());
  return null;
}

export function runtimeFieldWords() {
  const all = doc.field.map(slotWords);
  if (all.every(Boolean)) return all;
  const authored = all.slice(0, AUTHORED_CORNERS);
  const seed = authored.find(Boolean) || doc.words || null;
  if (!seed) return null;
  const filled = authored.map((words) => words || seed);
  return Array.from({ length: RUNTIME_CORNERS }, (_, i) => filled[i % AUTHORED_CORNERS]);
}

export function applyLanes(nextLanes, next = {}) {
  const slot = ensureCorner(doc.selectedCorner);
  const nextWords = lanesToWords(nextLanes, displaySr());
  const outWords = slot.words.slice();
  const held = [];
  for (let stage = 0; stage < STAGES; stage++) {
    if (isLocked(stage)) {
      held.push(stage + 1);
      continue;
    }
    slot.lanes[stage] = nextLanes[stage];
    outWords[stage] = nextWords[stage];
    slot.citations[stage] = null;
    geometries(slot)[stage] = null;
    if (next.roles) doc.roles[stage] = next.roles[stage];
    if (next.laws) doc.laws[stage] = next.laws[stage];
  }
  slot.words = outWords;
  slot.roles = doc.roles;
  slot.laws = doc.laws;
  doc.lanes = slot.lanes;
  doc.words = slot.words;
  doc.fieldWords = runtimeFieldWords();
  return held;
}

export function setSection(stage, cell) {
  const slot = ensureCorner(doc.selectedCorner);
  slot.words = slot.words.map((words, i) => (i === stage ? cell.words.slice() : words));
  slot.lanes[stage] = structuredClone(cell.lane);
  slot.citations[stage] = cell.citation || null;
  geometries(slot)[stage] = cell.geometry || null;
  return selectCorner(doc.selectedCorner, false);
}

export function clearSection(stage) {
  const slot = ensureCorner(doc.selectedCorner);
  const blank = emptyLanes();
  const blankWords = lanesToWords(blank, displaySr());
  slot.lanes[stage] = blank[stage];
  slot.words = slot.words.map((words, i) => (i === stage ? blankWords[stage] : words));
  slot.citations[stage] = null;
  return selectCorner(doc.selectedCorner, false);
}

export function swapSections(from, to) {
  for (const slot of doc.field) {
    if (!slot) continue;
    [slot.lanes[from], slot.lanes[to]] = [slot.lanes[to], slot.lanes[from]];
    [slot.words[from], slot.words[to]] = [slot.words[to], slot.words[from]];
    if (slot.roles) [slot.roles[from], slot.roles[to]] = [slot.roles[to], slot.roles[from]];
    if (slot.laws) [slot.laws[from], slot.laws[to]] = [slot.laws[to], slot.laws[from]];
    if (slot.citations) [slot.citations[from], slot.citations[to]] = [slot.citations[to], slot.citations[from]];
  }
  return selectCorner(doc.selectedCorner, false);
}

export function flipSection(stage) {
  const slot = ensureCorner(doc.selectedCorner);
  const lane = slot.lanes[stage];
  [lane.pole_hz, lane.zero_hz] = [lane.zero_hz, lane.pole_hz];
  [lane.pole_r, lane.zero_r] = [lane.zero_r, lane.pole_r];
  const w = slot.words[stage];
  slot.words = slot.words.map((words, i) => (i === stage ? [w[2], w[3], w[0], w[1], w[4]] : words));
  if (slot.citations[stage]) slot.citations[stage] = `${slot.citations[stage]} · P↔Z`;
  return commitLanes(slot.lanes, slot.words);
}

export function swapCorners(a, b) {
  const held = doc.field[a];
  doc.field[a] = doc.field[b];
  doc.field[b] = held;
  return selectCorner(doc.selectedCorner, false);
}

export function reset() {
  doc.field = Array.from({ length: RUNTIME_CORNERS }, () => null);
  doc.lanes = emptyLanes();
  doc.roles = emptyRoles();
  doc.laws = doc.lanes.map(() => freeLaw());
  doc.words = null;
  doc.fieldWords = null;
  doc.target = null;
  doc.targetName = null;
  doc.peaks = null;
  doc.preview = null;
  doc.rms = null;
  return selectCorner(0);
}

export function copyCorner(from, to) {
  const source = doc.field[from];
  if (!source) return null;
  doc.field[to] = {
    ...structuredClone(source),
    name: `C${to}`,
  };
  return doc.field[to];
}

export async function seatState(state, stage) {
  if (isLocked(stage)) throw new Error(`S${stage + 1} HELD — click HELD to free it`);
  const data = await api.target("stage", state.seat.id, { stage: state.seat.stage });
  const cell = data.cells.find((entry) => entry.corner === state.seat.corner);
  if (!cell) throw new Error(`no cell for ${state.seat.id} C${state.seat.corner}`);
  const verbatim = cell.words.length === state.words.length && cell.words.every((word, i) => word === state.words[i]);
  commit(`seat S${stage + 1}`);
  setSection(stage, cell);
  return { citation: cell.citation, verbatim };
}

export async function importFactory(entry, stages, corners) {
  const data = await Promise.all(
    Array.from({ length: stages }, (_, stage) => api.target("stage", entry.id, { stage }))
  );
  commit(`import ${entry.type}`);
  const blank = emptyLanes();
  const blankWords = lanesToWords(blank, displaySr());
  for (let corner = 0; corner < corners; corner++) {
    const slot = ensureCorner(corner);
    const nextWords = [];
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
  doc.targetName = entry.name;
  doc.selectedCorner = 0;
  selectCorner(0, false);
  return { stages, corners };
}

export async function seatTemplate(entry, stages) {
  const data = await Promise.all(
    Array.from({ length: stages }, (_, stage) => api.target("stage", entry.id, { stage }))
  );
  commit(`template ${entry.type}`);
  const slot = ensureCorner(doc.selectedCorner);
  const blank = emptyLanes();
  const blankWords = lanesToWords(blank, displaySr());
  const nextWords = [];
  const held = [];
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
  doc.targetName = entry.name;
  selectCorner(doc.selectedCorner, false);
  return { stages, held };
}
