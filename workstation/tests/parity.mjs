import { readFileSync } from "node:fs";
import { interpolateWords, interpolateBiquad, rowDb } from "../src/dsp.ts";

const file = process.argv[2];
if (!file) {
  console.error("usage: node parity.mjs <vectors.json>");
  process.exit(2);
}
const v = JSON.parse(readFileSync(file, "utf8"));
const grid = Array.from({ length: 128 }, (_, i) => 40 * Math.pow(16000 / 40, i / 127));

let worstCurve = 0;
for (const c of v.cases) {
  const words = interpolateWords(v.words, c.m, c.q, c.z);
  for (let si = 0; si < 7; si++) {
    for (let wi = 0; wi < 5; wi++) {
      if (words[si][wi] !== c.iwords[si][wi]) {
        console.error(
          `WORD MISMATCH at (${c.m},${c.q},${c.z}) S${si + 1} w${wi}: js ${words[si][wi]} rust ${c.iwords[si][wi]}`
        );
        process.exit(1);
      }
    }
  }
  const rows = interpolateBiquad(v.words, c.m, c.q, c.z);
  for (let si = 0; si < 7; si++) {
    for (let ci = 0; ci < 5; ci++) {
      if (rows[si][ci] !== c.rows[si][ci]) {
        console.error(
          `ROW MISMATCH at (${c.m},${c.q},${c.z}) S${si + 1} c${ci}: js ${rows[si][ci]} rust ${c.rows[si][ci]}`
        );
        process.exit(1);
      }
    }
  }
  for (let i = 0; i < grid.length; i++) {
    const db = rows.reduce((sum, r) => sum + rowDb(r, grid[i], v.sr), 0);
    const diff = Math.abs(db - c.curve[i]);
    if (diff > worstCurve) worstCurve = diff;
    if (diff > 1e-9) {
      console.error(`CURVE MISMATCH at (${c.m},${c.q},${c.z}) ${grid[i].toFixed(1)} Hz: diff ${diff}`);
      process.exit(1);
    }
  }
}
console.log(`PARITY OK — ${v.cases.length} cases, words exact, rows exact, curve worst ${worstCurve.toExponential(2)} dB`);
