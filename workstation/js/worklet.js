import { interpolateBiquad, rowDb } from "./dsp.js";

const SLEW_ALPHA = 0.0109;
const BASE_GAIN = 0.35;
const CEILING_FLOOR = 0.05;
const CEILING_WIDE = 2.0;
const DISTORT_THRESH_FLOOR = 0.02;
const DISTORT_THRESH_WIDE = 1.5;
const KNEE = 0.72;

function gritCeiling(grit) {
  const g = Math.min(1, Math.max(0, grit));
  return CEILING_WIDE + (CEILING_FLOOR - CEILING_WIDE) * g;
}

function gritThreshold(grit) {
  const g = Math.min(1, Math.max(0, grit));
  return (DISTORT_THRESH_WIDE - DISTORT_THRESH_FLOOR) * Math.exp(-4 * g) + DISTORT_THRESH_FLOOR;
}

function softClamp(x, ceiling) {
  const k = KNEE * ceiling;
  const a = Math.abs(x);
  if (a <= k) return x;
  const span = ceiling - k;
  return Math.sign(x) * (k + span * Math.tanh((a - k) / span));
}

class Cascade extends AudioWorkletProcessor {
  constructor() {
    super();
    this.corners = null;
    this.playing = false;
    this.ride = [0, 0, 0];
    this.slewed = [0, 0, 0];
    this.rows = null;
    this.targetRows = null;
    this.state = Array.from({ length: 7 }, () => [0, 0]);
    this.yPrev = new Float64Array(7);
    this.grit = 0;
    this.phase = 0;
    this.freq = 110;
    this.blend = 1;
    this.gain = 1;
    this.targetGain = 1;
    this.env = 0;
    this.duckCountdown = 0;
    this.pinkState = new Float64Array(3);
    this.noiseSeed = 22222;
    this.port.onmessage = (e) => {
      const m = e.data;
      if (m.corners) {
        this.corners = m.corners;
        this.duckCountdown = 0;
      }
      if (m.ride) this.ride = m.ride;
      if (m.play !== undefined) this.playing = m.play;
      if (m.freq) this.freq = m.freq;
      if (m.blend !== undefined) this.blend = m.blend;
      if (m.grit !== undefined) this.grit = m.grit;
    };
  }

  white() {
    this.noiseSeed = (this.noiseSeed * 196314165 + 907633515) >>> 0;
    return (this.noiseSeed / 4294967296) * 2 - 1;
  }

  pink() {
    const w = this.white();
    const p = this.pinkState;
    p[0] = 0.99765 * p[0] + w * 0.099046;
    p[1] = 0.963 * p[1] + w * 0.2965164;
    p[2] = 0.57 * p[2] + w * 1.0526913;
    return (p[0] + p[1] + p[2] + w * 0.1848) * 0.18;
  }

  saw(dt) {
    this.phase += dt;
    if (this.phase >= 1) this.phase -= 1;
    let v = 2 * this.phase - 1;
    const t = this.phase;
    if (t < dt) {
      const x = t / dt;
      v -= x + x - x * x - 1;
    } else if (t > 1 - dt) {
      const x = (t - 1) / dt;
      v -= x * x + x + x + 1;
    }
    return v * 0.5;
  }

  duck() {
    if (!this.rows) return;
    let peak = -120;
    for (let i = 0; i < 48; i++) {
      const hz = 40 * Math.pow(16000 / 40, i / 47);
      let db = 0;
      for (const r of this.rows) db += rowDb(r, hz, sampleRate);
      if (db > peak) peak = db;
    }
    this.targetGain = Math.pow(10, -Math.max(0, peak - 6) / 20);
  }

  process(inputs, outputs) {
    const out = outputs[0][0];
    if (!this.corners) {
      out.fill(0);
      return true;
    }
    for (let k = 0; k < 3; k++) {
      this.slewed[k] += SLEW_ALPHA * (this.ride[k] - this.slewed[k]);
    }
    this.targetRows = interpolateBiquad(
      this.corners,
      Math.fround(this.slewed[0]),
      Math.fround(this.slewed[1]),
      Math.fround(this.slewed[2])
    );
    if (!this.rows) this.rows = this.targetRows.map((r) => r.slice());
    if (this.duckCountdown <= 0) {
      this.duck();
      this.duckCountdown = 16;
    }
    this.duckCountdown--;
    if (!this.playing) {
      out.fill(0);
      this.rows = this.targetRows.map((r) => r.slice());
      return true;
    }
    const n = out.length;
    const dt = this.freq / sampleRate;
    const ramp = 1 / n;
    const ceiling = gritCeiling(this.grit);
    const vt = gritThreshold(this.grit);
    for (let i = 0; i < n; i++) {
      const f = (i + 1) * ramp;
      this.gain += (this.targetGain - this.gain) * 0.002;
      let x = (this.saw(dt) * (1 - this.blend) + this.pink() * this.blend) * 1.4 * this.gain;
      for (let s = 0; s < 7; s++) {
        const cur = this.rows[s];
        const tgt = this.targetRows[s];
        const b0 = cur[0] + (tgt[0] - cur[0]) * f;
        const b1 = cur[1] + (tgt[1] - cur[1]) * f;
        const b2 = cur[2] + (tgt[2] - cur[2]) * f;
        let a1 = cur[3] + (tgt[3] - cur[3]) * f;
        let a2 = cur[4] + (tgt[4] - cur[4]) * f;
        const vg = Math.abs(this.yPrev[s]);
        if (this.grit > 0 && vg > vt && a2 > 1e-9) {
          const r = Math.sqrt(a2);
          if (r > 1e-6 && r < 1) {
            const cosTheta = Math.min(1, Math.max(-1, -a1 / (2 * r)));
            const ratio = Math.min(vg - vt, 0.5);
            const rNew = Math.min(0.9999, Math.max(0, r + r * (1 - r) * ratio));
            a1 = -2 * rNew * cosTheta;
            a2 = rNew * rNew;
          }
        }
        const st = this.state[s];
        const y = b0 * x + st[0];
        if (this.grit <= 0) {
          if (!Number.isFinite(y)) {
            st[0] = 0;
            st[1] = 0;
            x = 0;
            continue;
          }
          st[0] = b1 * x - a1 * y + st[1];
          st[1] = b2 * x - a2 * y;
          x = y;
          continue;
        }
        if (!Number.isFinite(y)) {
          st[0] = 0;
          st[1] = 0;
          this.yPrev[s] = 0;
          x = 0;
          continue;
        }
        const w1 = softClamp(b1 * x - a1 * y + st[1], ceiling);
        const w2 = softClamp(b2 * x - a2 * y, ceiling);
        if (!Number.isFinite(w1) || !Number.isFinite(w2)) {
          st[0] = 0;
          st[1] = 0;
          this.yPrev[s] = 0;
        } else {
          st[0] = w1;
          st[1] = w2;
          this.yPrev[s] = y;
        }
        x = y;
      }
      this.env = Math.max(Math.abs(x), this.env * 0.9995);
      const g = Math.min(BASE_GAIN, 0.89 / Math.max(this.env, 1e-9));
      out[i] = Math.max(-1, Math.min(1, x * g));
    }
    this.rows = this.targetRows.map((r) => r.slice());
    return true;
  }
}

registerProcessor("trench-cascade", Cascade);
