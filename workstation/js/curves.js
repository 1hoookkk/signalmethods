import { DRAW_GRID } from "./doc.js";
import { wordsToBiquads, rowDb } from "./dsp.js";

let sr = 44100;
let cache = new WeakMap();
let recomputes = 0;

export function displaySr() {
  return sr;
}

export function setDisplaySr(hz) {
  if (!(hz > 0) || hz === sr) return;
  sr = hz;
  cache = new WeakMap();
}

function entry(words) {
  let found = cache.get(words);
  if (!found) {
    found = { stages: null, sum: null, cumulative: new Array(words.length).fill(null) };
    cache.set(words, found);
  }
  return found;
}

export function curveInto(words, out) {
  const rows = wordsToBiquads(words);
  const buf = out && out.length === DRAW_GRID.length ? out : new Float32Array(DRAW_GRID.length);
  for (let i = 0; i < buf.length; i++) {
    let db = 0;
    for (let s = 0; s < rows.length; s++) db += rowDb(rows[s], DRAW_GRID[i], sr);
    buf[i] = db;
  }
  recomputes++;
  return buf;
}

export function stageCurves(words) {
  const held = entry(words);
  if (!held.stages) {
    held.stages = wordsToBiquads(words).map((row) => {
      const buf = new Float32Array(DRAW_GRID.length);
      for (let i = 0; i < buf.length; i++) buf[i] = rowDb(row, DRAW_GRID[i], sr);
      return buf;
    });
    recomputes++;
  }
  return held.stages;
}

export function sumCurve(words) {
  const held = entry(words);
  if (!held.sum) {
    held.sum = cumulativeCurve(words, words.length - 1);
  }
  return held.sum;
}

export function cumulativeCurve(words, upTo) {
  const held = entry(words);
  if (!held.cumulative[upTo]) {
    const stages = stageCurves(words);
    const buf = new Float32Array(DRAW_GRID.length);
    for (let s = 0; s <= upTo; s++) {
      for (let i = 0; i < buf.length; i++) buf[i] += stages[s][i];
    }
    held.cumulative[upTo] = buf;
  }
  return held.cumulative[upTo];
}

export function recomputeCount() {
  return recomputes;
}
