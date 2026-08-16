export function lerpU16(a, b, frac) {
  const diff = Math.fround(b - a);
  const prod = Math.fround(diff * Math.fround(frac));
  let d = prod | 0;
  d = (d << 16) >> 16;
  return (d + a) & 0xffff;
}

export function decode(word) {
  const u = word + 1;
  if (u === 65536) return 1;
  if (u === 1) return 0;
  const e = (u >> 12) & 0xf;
  const m = u & 0xfff;
  const x = e === 0 ? m / 4096 : (m + 4096) / 8192;
  return x * Math.pow(2, e - 15);
}

export function stageWordsToKernel(w) {
  const d0 = decode(w[0]);
  const d1 = decode(w[1]);
  const d2 = decode(w[2]);
  const d3 = decode(w[3]);
  const d4 = decode(w[4]);
  return [4 * d0 + d1, d1, 4 * d2 + d3, d3, 4 * d4];
}

export function kernelToBiquad(k) {
  return [k[4], (k[0] - 2) * k[4], (1 - k[1]) * k[4], k[2] - 2, 1 - k[3]];
}

export function interpolateWords(corners, morph, q, z) {
  const out = [];
  for (let si = 0; si < 7; si++) {
    const row = new Array(5);
    for (let wi = 0; wi < 5; wi++) {
      const plane = [0, 0];
      for (let zi = 0; zi < 2; zi++) {
        const base = zi * 4;
        const edge0 = lerpU16(corners[base][si][wi], corners[base + 1][si][wi], morph);
        const edge1 = lerpU16(corners[base + 2][si][wi], corners[base + 3][si][wi], morph);
        plane[zi] = lerpU16(edge0, edge1, q);
      }
      row[wi] = lerpU16(plane[0], plane[1], z);
    }
    out.push(row);
  }
  return out;
}

export function wordsToBiquads(words) {
  return words.map((w) => kernelToBiquad(stageWordsToKernel(w)));
}

export function interpolateBiquad(corners, morph, q, z) {
  return wordsToBiquads(interpolateWords(corners, morph, q, z));
}

export function sumDb(rows, grid, sr, out) {
  const buf = out && out.length === grid.length ? out : new Float32Array(grid.length);
  for (let i = 0; i < grid.length; i++) {
    let db = 0;
    for (let s = 0; s < rows.length; s++) db += rowDb(rows[s], grid[i], sr);
    buf[i] = db;
  }
  return buf;
}

export function rowDb(c, hz, sr) {
  const w = (2 * Math.PI * hz) / sr;
  const cw = Math.cos(w);
  const sw = Math.sin(w);
  const c2 = Math.cos(2 * w);
  const s2 = Math.sin(2 * w);
  const nr = c[0] + c[1] * cw + c[2] * c2;
  const ni = -(c[1] * sw + c[2] * s2);
  const dr = 1 + c[3] * cw + c[4] * c2;
  const di = -(c[3] * sw + c[4] * s2);
  return (
    10 *
    Math.log10(
      Math.max(nr * nr + ni * ni, 1e-30) / Math.max(dr * dr + di * di, 1e-30)
    )
  );
}
