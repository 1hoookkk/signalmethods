export const HZ_LO = 40;
export const HZ_HI = 16000;
export const SECTION_DB_LO = -60;
export const SECTION_DB_HI = 84;

const HZ_TICKS = [40, 100, 200, 500, 1000, 2000, 5000, 10000, 16000];
const LOG_RATIO = Math.log(HZ_HI / HZ_LO);

export type Curve = ArrayLike<number>;
export type YOf = (db: number) => number;

const vars: Record<string, string> = {};

export function css(name: string): string {
  if (!(name in vars)) {
    vars[name] = getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  }
  return vars[name];
}

export function xOf(hz: number, w: number): number {
  return (Math.log(Math.max(HZ_LO, Math.min(HZ_HI, hz)) / HZ_LO) / LOG_RATIO) * w;
}

export function hzOfX(x: number, w: number): number {
  return HZ_LO * (HZ_HI / HZ_LO) ** Math.min(1, Math.max(0, x / w));
}

export function yMap(dbLo: number, dbHi: number, h: number) {
  const span = dbHi - dbLo;
  return {
    yOf: (db: number) => h - ((db - dbLo) / span) * h,
    dbOf: (y: number) => dbLo + (1 - Math.min(1, Math.max(0, y / h))) * span,
  };
}

export function scope(canvas: HTMLCanvasElement) {
  return {
    frame(draw: (g: CanvasRenderingContext2D, w: number, h: number) => void) {
      const w = canvas.clientWidth;
      const h = canvas.clientHeight;
      if (!w || !h) return;
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      const dw = Math.round(w * dpr);
      const dh = Math.round(h * dpr);
      if (canvas.width !== dw || canvas.height !== dh) {
        canvas.width = dw;
        canvas.height = dh;
      }
      const g = canvas.getContext("2d");
      if (!g) return;
      g.setTransform(dpr, 0, 0, dpr, 0, 0);
      draw(g, w, h);
    },
  };
}

export function curveEval(curve: Curve): (hz: number) => number {
  const span = curve.length - 1;
  return (hz: number) => {
    const t = (Math.log(Math.max(HZ_LO, Math.min(HZ_HI, hz)) / HZ_LO) / LOG_RATIO) * span;
    const i = Math.min(span - 1, Math.max(0, Math.floor(t)));
    const f = Math.min(1, Math.max(0, t - i));
    return curve[i] * (1 - f) + curve[i + 1] * f;
  };
}

// The pole radius as Rossum's height mapping, R' = 20 log10(1/(1-R)). Every view that
// places a root vertically uses this one law and this one ceiling.
export const RP_MAX = 84;

export function rPrime(r: number): number {
  if (r >= 1) return RP_MAX;
  if (r <= 0) return 0;
  return Math.min(RP_MAX, 20 * Math.log10(1 / (1 - r)));
}

export function rOf(rp: number): number {
  return 1 - 10 ** (-Math.max(0, rp) / 20);
}

export function hzAt(i: number, pts: number): number {
  return HZ_LO * (HZ_HI / HZ_LO) ** (i / (pts - 1));
}

export function trace(
  g: CanvasRenderingContext2D,
  w: number,
  evalDb: (hz: number) => number,
  yOf: YOf,
  color: string,
  width = 1.6,
) {
  const pts = Math.min(512, Math.max(192, Math.round(w / 2)));
  g.strokeStyle = color;
  g.lineWidth = width;
  g.lineJoin = "round";
  g.lineCap = "round";
  g.beginPath();
  let valid = false;
  for (let i = 0; i < pts; i++) {
    const db = evalDb(hzAt(i, pts));
    if (!Number.isFinite(db)) continue;
    const x = (i / (pts - 1)) * w;
    const y = yOf(db);
    if (!valid) {
      g.moveTo(x, y);
      valid = true;
    } else {
      g.lineTo(x, y);
    }
  }
  g.stroke();
}

function hzLabel(hz: number): string {
  return hz >= 1000 ? `${hz / 1000}k` : `${hz}`;
}

export function graticule(
  g: CanvasRenderingContext2D,
  w: number,
  h: number,
  yOf: YOf,
  dbLo: number,
  dbHi: number,
  crown: number,
  axisBase = h,
) {
  const step = 10;
  g.lineWidth = 1;
  g.font = `10px ${css("--ui")}`;
  g.textBaseline = "middle";
  g.textAlign = "left";

  for (let db = Math.ceil(dbLo / step) * step; db <= dbHi; db += step) {
    const y = Math.floor(yOf(db)) + 0.5;
    g.strokeStyle = db === 0 ? css("--well-grid") : css("--well-rule");
    g.beginPath();
    g.moveTo(0, y);
    g.lineTo(w, y);
    g.stroke();
    g.fillStyle = css("--well-dim");
    g.fillText(`${db > 0 ? "+" : ""}${db}`, 4, y - 5);
  }

  g.textAlign = "center";
  g.textBaseline = "alphabetic";
  for (const hz of HZ_TICKS) {
    const x = Math.floor(xOf(hz, w)) + 0.5;
    g.strokeStyle = hz === 1000 ? css("--well-grid") : css("--well-rule");
    g.beginPath();
    g.moveTo(x, 0);
    g.lineTo(x, h);
    g.stroke();
    g.fillStyle = css("--well-dim");
    g.fillText(hzLabel(hz), Math.min(w - 12, Math.max(12, x)), axisBase - 4);
  }

  const y = Math.floor(yOf(crown)) + 0.5;
  g.strokeStyle = css("--error");
  g.lineWidth = 1;
  g.setLineDash([4, 4]);
  g.beginPath();
  g.moveTo(0, y);
  g.lineTo(w, y);
  g.stroke();
  g.setLineDash([]);
}

export function formantInk(k: number): string {
  return css(`--f${(k % 5) + 1}`);
}

// The well, the zero line and one trace: the shape every small section plot draws.
export function panel(g: CanvasRenderingContext2D, w: number, h: number, dbLo: number, dbHi: number) {
  const { yOf } = yMap(dbLo, dbHi, h);
  g.fillStyle = css("--well");
  g.fillRect(0, 0, w, h);
  const zero = Math.floor(yOf(0)) + 0.5;
  g.strokeStyle = css("--well-grid");
  g.lineWidth = 1;
  g.beginPath();
  g.moveTo(0, zero);
  g.lineTo(w, zero);
  g.stroke();
  return yOf;
}
