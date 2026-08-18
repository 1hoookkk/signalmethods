let context = null;
let node = null;
let analyser = null;
let probe = null;
let starting = null;
let rate = 39062.5;
let reference = null;
let source = "synth";
const pending = { corners: null, ride: [0, 0, 0], play: false, blend: 1, freq: 110, grit: 0 };

async function workletUrl() {
  const [dsp, cascade] = await Promise.all(
    ["js/dsp.js", "js/worklet.js"].map((p) => fetch(p, { cache: "reload" }).then((r) => r.text()))
  );
  const cleanDsp = dsp.replace(/^export\s+/gm, "");
  const cleanCascade = cascade.replace(/^import\s+[\s\S]*?;\r?\n?/gm, "");
  const source = cleanDsp + "\n" + cleanCascade;
  return URL.createObjectURL(new Blob([source], { type: "text/javascript" }));
}

export async function ensureAudio() {
  if (node) return node;
  if (starting) return starting;
  starting = (async () => {
    try {
      context = new AudioContext({ sampleRate: rate });
    } catch (e) {
      context = new AudioContext();
    }
    await context.audioWorklet.addModule(await workletUrl());
    node = new AudioWorkletNode(context, "trench-cascade", { outputChannelCount: [1] });
    analyser = context.createAnalyser();
    analyser.fftSize = 2048;
    probe = new Float32Array(analyser.fftSize);
    node.connect(analyser);
    analyser.connect(context.destination);
    node.port.postMessage(pending);
    return node;
  })();
  return starting;
}

export function setRate(hz) {
  if (hz > 0 && !context) rate = hz;
}

export function audioRate() {
  return context ? context.sampleRate : rate;
}

export function audioState() {
  return context ? context.state : "none";
}

export function outputRms() {
  if (!analyser) return null;
  analyser.getFloatTimeDomainData(probe);
  let sum = 0;
  for (let i = 0; i < probe.length; i++) sum += probe[i] * probe[i];
  return Math.sqrt(sum / probe.length);
}

function send(message) {
  Object.assign(pending, message);
  if (node) node.port.postMessage(message);
}

export function setCorners(corners) {
  send({ corners });
}

export function setRide(m, q, z) {
  send({ ride: [m, q, z] });
}

export async function setPlay(play) {
  pending.play = play;
  if (source === "dry" && reference) {
    if (node) node.port.postMessage({ play: false });
    if (play) await reference.play();
    else reference.pause();
    return;
  }
  if (reference) reference.pause();
  await ensureAudio();
  if (play && context.state !== "running") await context.resume();
  node.port.postMessage({ play });
}

export function setReference(url) {
  if (reference) reference.pause();
  reference = url ? new Audio(url) : null;
  if (reference) reference.loop = true;
}

export function setSource(next) {
  source = next === "dry" ? "dry" : "synth";
  if (reference) reference.pause();
  if (node) node.port.postMessage({ play: false });
}

export function setBlend(blend) {
  send({ blend });
}

export function setFreq(freq) {
  send({ freq });
}

export function setGrit(grit) {
  send({ grit });
}
