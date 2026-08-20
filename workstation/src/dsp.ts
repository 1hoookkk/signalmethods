export type Lane = {
  pole_hz: number;
  pole_r: number;
  zero_hz: number;
  zero_r: number;
  scale: number;
};

export type StageWords = number[];
export type CornerWords = StageWords[];
export type Biquad = number[];
export type Kernel = number[];
export type Grid = ArrayLike<number>;

export function encode(value: number): number {
  if (value >= 1) return 0xffff;
  if (value <= 0) return 0x0000;
  const denormMant = Math.round(value * 134217728);
  if (denormMant > 0 && denormMant <= 0xfff) return (denormMant - 1) & 0xffff;
  const log2Val = Math.log2(value);
  let expStored = Math.min(Math.floor(log2Val) + 1, 0);
  if (expStored < -14) return 0x0000;
  let biasedExp = expStored + 15;
  let mantWithHidden = Math.round(value / 2 ** (expStored - 13));
  if (mantWithHidden >= 0x2000) {
    if (expStored < 0) {
      expStored += 1;
      biasedExp += 1;
      mantWithHidden = Math.round(value / 2 ** (expStored - 13));
      const mant = Math.min(mantWithHidden & 0xfff, 0xfff);
      return (((biasedExp << 12) | mant) - 1) & 0xffff;
    }
    return 0xffff;
  }
  const mant = Math.max(0, Math.min(mantWithHidden - 0x1000, 0xfff));
  return (((biasedExp << 12) | mant) - 1) & 0xffff;
}

export function lanesToWords(lanes: Lane[], sr: number): CornerWords {
  const TAU = 2 * Math.PI;
  return lanes.map((lane) => {
    const wz = (TAU * lane.zero_hz) / sr;
    const wp = (TAU * lane.pole_hz) / sr;
    const rz = lane.zero_r;
    const rp = lane.pole_r;
    const c0 = 2 - 2 * rz * Math.cos(wz);
    const c1 = 1 - rz * rz;
    const c2 = 2 - 2 * rp * Math.cos(wp);
    const c3 = 1 - rp * rp;
    const c4 = lane.scale;
    return [encode((c0 - c1) / 4), encode(c1), encode((c2 - c3) / 4), encode(c3), encode(c4 / 4)];
  });
}

export function lerpU16(a: number, b: number, frac: number): number {
  const diff = Math.fround(b - a);
  const prod = Math.fround(diff * Math.fround(frac));
  let d = prod | 0;
  d = (d << 16) >> 16;
  return (d + a) & 0xffff;
}

export function decode(word: number): number {
  const u = word + 1;
  if (u === 65536) return 1;
  if (u === 1) return 0;
  const e = (u >> 12) & 0xf;
  const m = u & 0xfff;
  const x = e === 0 ? m / 4096 : (m + 4096) / 8192;
  return x * 2 ** (e - 15);
}

export function stageWordsToKernel(w: StageWords): Kernel {
  const d0 = decode(w[0]);
  const d1 = decode(w[1]);
  const d2 = decode(w[2]);
  const d3 = decode(w[3]);
  const d4 = decode(w[4]);
  return [4 * d0 + d1, d1, 4 * d2 + d3, d3, 4 * d4];
}

export function kernelToBiquad(k: Kernel): Biquad {
  return [k[4], (k[0] - 2) * k[4], (1 - k[1]) * k[4], k[2] - 2, 1 - k[3]];
}

export function interpolateWords(corners: CornerWords[], morph: number, q: number, z: number): CornerWords {
  const out: CornerWords = [];
  for (let si = 0; si < 7; si++) {
    const row: StageWords = new Array(5);
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

export function wordsToBiquads(words: CornerWords): Biquad[] {
  return words.map((w) => kernelToBiquad(stageWordsToKernel(w)));
}

export function interpolateBiquad(corners: CornerWords[], morph: number, q: number, z: number): Biquad[] {
  return wordsToBiquads(interpolateWords(corners, morph, q, z));
}

export function sumDb(rows: Biquad[], grid: Grid, sr: number, out?: Float32Array): Float32Array {
  const buf = out && out.length === grid.length ? out : new Float32Array(grid.length);
  for (let i = 0; i < grid.length; i++) {
    let db = 0;
    for (let s = 0; s < rows.length; s++) db += rowDb(rows[s], grid[i], sr);
    buf[i] = db;
  }
  return buf;
}

export function rowDb(c: Biquad, hz: number, sr: number): number {
  const w = (2 * Math.PI * hz) / sr;
  const cw = Math.cos(w);
  const sw = Math.sin(w);
  const c2 = Math.cos(2 * w);
  const s2 = Math.sin(2 * w);
  const nr = c[0] + c[1] * cw + c[2] * c2;
  const ni = -(c[1] * sw + c[2] * s2);
  const dr = 1 + c[3] * cw + c[4] * c2;
  const di = -(c[3] * sw + c[4] * s2);
  return 10 * Math.log10(Math.max(nr * nr + ni * ni, 1e-30) / Math.max(dr * dr + di * di, 1e-30));
}
