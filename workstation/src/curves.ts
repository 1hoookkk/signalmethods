import { DRAW_GRID } from "./doc.js";
import type { CornerWords } from "./dsp.js";
import { rowDb, sumDb, wordsToBiquads } from "./dsp.js";

type Entry = {
  stages: Float32Array[] | null;
  sum: Float32Array | null;
};

let sr = 39062.5;
let cache = new WeakMap<CornerWords, Entry>();

export function displaySr(): number {
  return sr;
}

export function setDisplaySr(hz: number) {
  if (!(hz > 0) || hz === sr) return;
  sr = hz;
  cache = new WeakMap();
}

function entry(words: CornerWords): Entry {
  let found = cache.get(words);
  if (!found) {
    found = { stages: null, sum: null };
    cache.set(words, found);
  }
  return found;
}

// The cascade sum, accumulated in f64 and rounded once. The live response and the
// interpolated preview go through this same function, so they cannot disagree.
export function curveInto(words: CornerWords, out: Float32Array | null): Float32Array {
  return sumDb(wordsToBiquads(words), DRAW_GRID, sr, out ?? undefined);
}

export function stageCurves(words: CornerWords): Float32Array[] {
  const held = entry(words);
  if (!held.stages) {
    held.stages = wordsToBiquads(words).map((row) => {
      const buf = new Float32Array(DRAW_GRID.length);
      for (let i = 0; i < buf.length; i++) buf[i] = rowDb(row, DRAW_GRID[i], sr);
      return buf;
    });
  }
  return held.stages;
}

export function sumCurve(words: CornerWords): Float32Array {
  const held = entry(words);
  if (!held.sum) held.sum = curveInto(words, null);
  return held.sum;
}

// DRAW_GRID is logarithmic, so a fixed number of points is a fixed fraction of an octave.
export const TARGET_OCTAVE = 1 / 3;

// A measured spectrum carries harmonics, analysis ripple and source structure that seven
// second-order sections cannot and should not reproduce. Smoothing to a fixed fraction of
// an octave leaves the envelope — the transfer-function shape actually being matched —
// and stops poles being spent on detail that is not the filter.
export function smoothOctave(curve: ArrayLike<number>, octave = TARGET_OCTAVE): Float32Array {
  const pointsPerOctave = (DRAW_GRID.length - 1) / Math.log2(16000 / 40);
  const half = Math.max(1, Math.round((octave * pointsPerOctave) / 2));
  const src = new Float32Array(DRAW_GRID.length);
  const span = curve.length - 1;
  for (let i = 0; i < src.length; i++) {
    src[i] = curve[Math.round((i * span) / (src.length - 1))];
  }
  const out = new Float32Array(src.length);
  let sum = 0;
  let count = 0;
  for (let i = 0; i < src.length + half; i++) {
    if (i < src.length && Number.isFinite(src[i])) {
      sum += src[i];
      count++;
    }
    const drop = i - 2 * half - 1;
    if (drop >= 0 && Number.isFinite(src[drop])) {
      sum -= src[drop];
      count--;
    }
    const at = i - half;
    if (at >= 0 && at < out.length) out[at] = count ? sum / count : src[at];
  }
  return out;
}
