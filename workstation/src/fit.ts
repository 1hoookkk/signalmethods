import type { FitCandidate, FitResult } from "./api.js";
import { api } from "./api.js";
import { displaySr } from "./curves.js";
import { commit, currentTarget, DRAW_GRID, doc, emptyLanes, isLocked, toDrawGrid } from "./doc.js";
import type { CornerWords, Lane } from "./dsp.js";
import { lanesToWords, sumDb, wordsToBiquads } from "./dsp.js";

const STAGES = 7;

// A bypassed section is offered to the solver as a held identity stage: it contributes
// nothing to the comparator and the solver may not touch it. What you hear is what is
// being fitted.
const IDENTITY_LANE: Lane = { pole_hz: 0, pole_r: 0, zero_hz: 0, zero_r: 0, scale: 1 };

export type FitDeps = {
  say: (text: string) => void;
  paint: () => void;
  paintSpectrum: () => void;
  storeCorner: (lanes: Lane[], words?: CornerWords | null) => unknown;
  authoredWords: () => CornerWords | null;
  pushAudio: () => void;
  clearCitation: (stage: number) => void;
  isOff: (stage: number) => boolean;
  corner: () => number;
  generation: () => number;
};

export function createFit(deps: FitDeps) {
  const {
    say,
    paint,
    paintSpectrum,
    storeCorner,
    authoredWords,
    pushAudio,
    clearCitation,
    isOff,
    corner,
    generation,
  } = deps;

  let fitting = false;
  let fitToken = 0;
  let candidatePending = false;

  const isHeld = (i: number) => !doc.laws[i].writable;
  const isEmpty = (i: number) => doc.lanes[i].pole_r <= 0 && doc.lanes[i].zero_r <= 0;

  function showCandidate(candidate: FitCandidate) {
    if (!fitting) return;
    if (candidatePending) return;
    candidatePending = true;
    requestAnimationFrame(() => {
      candidatePending = false;
      const target = currentTarget();
      if (!fitting || !target) return;
      const curve = sumDb(wordsToBiquads(candidate.words), DRAW_GRID, displaySr());
      const grid = toDrawGrid(target);
      let offset = 0;
      for (let i = 0; i < curve.length; i++) offset += grid[i] - curve[i];
      offset /= curve.length;
      for (let i = 0; i < curve.length; i++) curve[i] += offset;
      doc.candidate = curve;
      say(`FIT ${candidate.rms.toFixed(2)} dB`);
      paintSpectrum();
    });
  }

  function solverOrder(): number[] {
    const order: number[] = [];
    for (let i = 0; i < STAGES; i++) if (isLocked(i) && doc.lanes[i].pole_r > 0) order.push(i);
    for (let i = 0; i < STAGES; i++) if (!order.includes(i)) order.push(i);
    return order;
  }

  // One authored transaction. Authored words come from the corner slot, never from the
  // derived view, so a bypassed section is never baked into the body by fitting.
  function install(order: number[], r: FitResult) {
    const solved = emptyLanes();
    const solvedWords: CornerWords = new Array(STAGES);
    order.forEach((lane, slot) => {
      solved[lane] = r.lanes[slot];
      solvedWords[lane] = r.words[slot];
    });
    commit();
    const lanes = structuredClone(doc.lanes);
    const base = authoredWords() || lanesToWords(doc.lanes, displaySr());
    const words = base.map((w) => (w ? w.slice() : w));
    for (let i = 0; i < STAGES; i++) {
      if (isHeld(i) || isOff(i)) continue;
      lanes[i] = solved[i];
      words[i] = solvedWords[i];
      clearCitation(i);
    }
    storeCorner(lanes, words);
    doc.candidate = null;
    pushAudio();
    paint();
  }

  async function run() {
    if (fitting) return;
    const target = currentTarget();
    if (!target) return say("no target");
    const startCorner = corner();
    const startGeneration = generation();
    say("FIT…");
    fitting = true;
    const token = ++fitToken;
    const order = solverOrder();
    const laws = doc.laws.map((law, i) => ({
      ...law,
      grow: !isLocked(i) && isEmpty(i),
    }));
    try {
      const r = await api.fitStream(
        target,
        order.map((i) => (isOff(i) ? IDENTITY_LANE : doc.lanes[i])),
        order.map((i) => (isOff(i) ? { ...laws[i], writable: false, grow: false } : laws[i])),
        order,
        showCandidate,
      );
      if (token !== fitToken) return;
      if (corner() !== startCorner || generation() !== startGeneration) {
        return say("FIT —");
      }
      install(order, r);
      say(`FIT ${r.target_rms_db.toFixed(2)} dB`);
    } catch (e) {
      if (token === fitToken) say(`ERROR ${(e as Error).message}`);
    } finally {
      if (token === fitToken) {
        fitting = false;
        doc.candidate = null;
      }
    }
  }

  function cancel(): boolean {
    if (!fitting) return false;
    fitting = false;
    fitToken++;
    doc.candidate = null;
    say("FIT —");
    paint();
    return true;
  }

  return {
    run,
    cancel,
    isFitting: () => fitting,
  };
}
