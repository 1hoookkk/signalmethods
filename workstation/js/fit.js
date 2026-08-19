import { api } from "./api.js";
import { doc, commit, emptyLanes, lawState, DRAW_GRID, toDrawGrid } from "./doc.js";
import { lanesToWords, wordsToBiquads, sumDb } from "./dsp.js";
import { displaySr } from "./curves.js";

const STAGES = 7;

export function createFit(deps) {
  const { say, paint, paintSpectrum, storeCorner, pushAudio, clearCitation } = deps;

  let fitting = false;
  let fitToken = 0;
  let candidatePending = false;
  let candidateCount = 0;

  const isLocked = (i) => lawState(doc.laws[i]) !== "FREE";
  const isHeld = (i) => !doc.laws[i].writable;
  const isEmpty = (i) => doc.lanes[i].pole_r <= 0 && doc.lanes[i].zero_r <= 0;

  function showCandidate(candidate) {
    if (!fitting) return;
    candidateCount++;
    if (candidatePending) return;
    candidatePending = true;
    requestAnimationFrame(() => {
      candidatePending = false;
      const curve = sumDb(wordsToBiquads(candidate.words), DRAW_GRID, displaySr());
      const target = toDrawGrid(doc.target);
      let offset = 0;
      for (let i = 0; i < curve.length; i++) offset += target[i] - curve[i];
      offset /= curve.length;
      for (let i = 0; i < curve.length; i++) curve[i] += offset;
      doc.candidate = curve;
      say(`fitting… ${candidate.rms.toFixed(2)} dB rms (candidate ${candidateCount})`);
      paintSpectrum();
    });
  }

  function solverOrder() {
    const order = [];
    for (let i = 0; i < STAGES; i++) if (isLocked(i) && doc.lanes[i].pole_r > 0) order.push(i);
    for (let i = 0; i < STAGES; i++) if (!order.includes(i)) order.push(i);
    return order;
  }

  async function run() {
    if (!doc.target) return say("no target loaded");
    say("solving… (Escape abandons)");
    candidateCount = 0;
    fitting = true;
    const token = ++fitToken;
    const order = solverOrder();
    const laws = doc.laws.map((law, i) => ({
      ...law,
      grow: lawState(law) === "FREE" && isEmpty(i),
    }));
    try {
      const r = await api.fitStream(
        doc.target,
        order.map((i) => doc.lanes[i]),
        order.map((i) => laws[i]),
        false,
        order,
        showCandidate
      );
      if (token !== fitToken) return;
      const lanes = emptyLanes();
      const words = new Array(STAGES);
      order.forEach((lane, slot) => {
        lanes[lane] = r.lanes[slot];
        words[lane] = r.words[slot];
      });
      doc.proposal = {
        lanes,
        words,
        rms: r.target_rms_db,
        packing: r.intended_packed_rms_db,
        sections: r.sections_used,
      };
      doc.candidate = sumDb(wordsToBiquads(words), DRAW_GRID, displaySr());
      const held = [];
      for (let i = 0; i < STAGES; i++) if (isHeld(i)) held.push(`S${i + 1}`);
      const opened = (r.opened || []).map((i) => `S${i + 1}`);
      say(
        `${r.target_rms_db.toFixed(2)} dB rms over ${r.sections_used} sections, packing ${r.intended_packed_rms_db.toFixed(3)} dB${
          held.length ? ` — ${held.join(", ")} held, solved at order ${(STAGES - held.length) * 2}` : ""
        }${opened.length ? `, opened ${opened.join(", ")}` : ", opened nothing"} — Enter seats it, Escape discards`
      );
      paint();
    } finally {
      if (token === fitToken) fitting = false;
      if (!doc.proposal) doc.candidate = null;
    }
  }

  function accept() {
    const p = doc.proposal;
    if (!p) return;
    commit("fit");
    const lanes = structuredClone(doc.lanes);
    const base = doc.words || lanesToWords(doc.lanes, displaySr());
    const words = base.map((w) => (w ? w.slice() : w));
    const seated = [];
    for (let i = 0; i < STAGES; i++) {
      if (isHeld(i)) continue;
      if (doc.roles[i]) {
        const was = doc.lanes[i].pole_hz;
        const now = p.lanes[i].pole_r > 0 ? p.lanes[i].pole_hz : 0;
        if (!now || !was || Math.abs(now - was) > was * 0.02) doc.roles[i] = null;
      }
      lanes[i] = p.lanes[i];
      words[i] = p.words[i];
      clearCitation(i);
      seated.push(`S${i + 1}`);
    }
    storeCorner(lanes, words);
    doc.rms = p.rms;
    doc.rmsStale = false;
    doc.packing = p.packing;
    doc.proposal = null;
    doc.candidate = null;
    pushAudio();
    say(
      seated.length
        ? `seated ${seated.join(", ")} together at ${p.rms.toFixed(2)} dB rms`
        : "nothing seated — every lane is held"
    );
    paint();
  }

  function discard() {
    if (!doc.proposal) return false;
    doc.proposal = null;
    doc.candidate = null;
    say("fit discarded");
    paint();
    return true;
  }

  function abandon() {
    if (!fitting) return false;
    fitting = false;
    fitToken++;
    doc.candidate = null;
    doc.proposal = null;
    say("fit abandoned");
    paint();
    return true;
  }

  return {
    run,
    accept,
    discard,
    abandon,
    isFitting: () => fitting,
  };
}
