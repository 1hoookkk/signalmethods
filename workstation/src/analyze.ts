import type { CurveTarget } from "./api.js";
import { api } from "./api.js";
import { smoothOctave } from "./curves.js";
import type { Curve } from "./doc.js";
import { css, curveEval, graticule, scope, trace, yMap } from "./render.js";
import { element, key, keys, numeric, panel, slider } from "./ui.js";

const DB_LO = -66;
const DB_HI = 30;
const AXIS = 14;

export type AnalyzeHooks = {
  onTarget: (curve: Curve, name: string) => void;
  say: (text: string) => void;
};

export type Analyze = {
  work: HTMLElement;
  controls: HTMLElement;
  paint: () => void;
  load: (id: string, name: string) => Promise<void>;
};

// What part of this recording do I want the body to reproduce? Measured against smoothed,
// with the octave fraction that decides how much detail survives. The measurement is
// shown; nothing about it is interpreted into geometry.
export function mountAnalyze(hooks: AnalyzeHooks): Analyze {
  const spectrum = panel("SPECTRUM", { grow: true });
  spectrum.body.style.background = "var(--well)";
  const canvas = element("canvas", "surface");
  spectrum.body.appendChild(canvas);

  const controls = element("div");
  controls.style.cssText = "display:flex;flex-direction:column;min-height:0;flex:1";

  const selection = panel("SELECTION", { scroll: true });
  const at = numeric("AT", { unit: "s", digits: 2 });
  const sliceRow = element("div", "num");
  sliceRow.append(element("label", undefined, "SLICE"));
  const sliceHost = element("div");
  sliceHost.style.cssText = "display:flex;align-items:center";
  const slice = slider(0, 1, 0, (v) => {
    at.set(v);
    schedule(v);
  });
  sliceHost.appendChild(slice.root);
  sliceRow.appendChild(sliceHost);
  const averageKey = key("WHOLE RECORDING", () => fetchSlice(null));
  selection.body.append(sliceRow, at.root, keys(1, averageKey));

  const shaping = panel("SHAPING", { scroll: true });
  const octaveValue = numeric("BAND", { digits: 0 });
  const octaveRow = element("div", "num");
  octaveRow.append(element("label", undefined, "SMOOTH"));
  const octaveHost = element("div");
  octaveHost.style.cssText = "display:flex;align-items:center";
  const octave = slider(1, 12, 3, (v) => {
    fraction = Math.round(v);
    octaveValue.set(fraction);
    recompute();
  });
  octaveHost.appendChild(octave.root);
  octaveRow.appendChild(octaveHost);
  const assignKey = key("USE AS TARGET", () => {
    if (!smoothed) return hooks.say("nothing analysed");
    hooks.onTarget(smoothed, `${name} · 1/${fraction} oct`);
  });
  shaping.body.append(octaveRow, octaveValue.root, keys(1, assignKey));

  controls.append(selection.root, shaping.root);

  let id: string | null = null;
  let name = "";
  let raw: CurveTarget | null = null;
  let smoothed: Float32Array | null = null;
  let fraction = 3;
  let timer = 0;

  octaveValue.set(fraction);

  function recompute() {
    smoothed = raw ? smoothOctave(raw.curve, 1 / fraction) : null;
    paint();
  }

  function paint() {
    scope(canvas).frame((g, w, h) => {
      g.fillStyle = css("--well");
      g.fillRect(0, 0, w, h);
      const plot = h - AXIS;
      const { yOf } = yMap(DB_LO, DB_HI, plot);
      graticule(g, w, plot, yOf, DB_LO, DB_HI, DB_HI, h);
      if (!raw) {
        g.fillStyle = css("--well-dim");
        g.font = `11px ${css("--ui")}`;
        g.fillText("NO RECORDING SELECTED", 8, 20);
        return;
      }
      trace(g, w, curveEval(raw.curve), yOf, css("--target"), 1);
      if (smoothed) trace(g, w, curveEval(smoothed), yOf, css("--live"), 1.7);
    });
  }

  function schedule(seconds: number) {
    clearTimeout(timer);
    timer = setTimeout(() => fetchSlice(seconds), 120) as unknown as number;
  }

  function fetchSlice(seconds: number | null) {
    if (!id) return;
    api
      .target("recording", id, seconds === null ? {} : { slice_at_seconds: seconds })
      .then((t) => {
        raw = t;
        recompute();
        hooks.say(seconds === null ? `${name} whole` : `${name} ${seconds.toFixed(2)} s`);
      })
      .catch((e: Error) => hooks.say(`ERROR ${e.message}`));
  }

  return {
    work: spectrum.root,
    controls,
    paint,
    async load(nextId, nextName) {
      id = nextId;
      name = nextName;
      raw = await api.target("recording", nextId);
      slice.set(0);
      at.set(null);
      recompute();
    },
  };
}
