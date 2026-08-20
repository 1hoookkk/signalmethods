import type { CornerWords } from "./dsp.js";
import workletUrl from "./worklet.ts?worker&url";

type EngineMessage = {
  corners?: CornerWords[] | null;
  ride?: number[];
  play?: boolean;
  blend?: number;
  freq?: number;
};

let context: AudioContext | null = null;
let node: AudioWorkletNode | null = null;
let starting: Promise<AudioWorkletNode> | null = null;
let rate = 39062.5;
let reference: HTMLAudioElement | null = null;
let source: "synth" | "dry" = "synth";

const pending: Required<EngineMessage> = {
  corners: null,
  ride: [0, 0, 0],
  play: false,
  blend: 1,
  freq: 110,
};

async function ensureAudio(): Promise<AudioWorkletNode> {
  if (node) return node;
  if (starting) return starting;
  starting = (async () => {
    try {
      context = new AudioContext({ sampleRate: rate });
    } catch {
      context = new AudioContext();
    }
    await context.audioWorklet.addModule(workletUrl);
    node = new AudioWorkletNode(context, "trench-cascade", { outputChannelCount: [1] });
    node.connect(context.destination);
    node.port.postMessage(pending);
    return node;
  })();
  return starting;
}

export function setRate(hz: number) {
  if (hz > 0 && !context) rate = hz;
}

function send(message: EngineMessage) {
  Object.assign(pending, message);
  if (node) node.port.postMessage(message);
}

export function setCorners(corners: CornerWords[]) {
  send({ corners });
}

export function setRide(m: number, q: number, z: number) {
  send({ ride: [m, q, z] });
}

export async function setPlay(play: boolean) {
  pending.play = play;
  if (source === "dry" && reference) {
    if (node) node.port.postMessage({ play: false });
    if (play) await reference.play();
    else reference.pause();
    return;
  }
  if (reference) reference.pause();
  const live = await ensureAudio();
  if (play && context && context.state !== "running") await context.resume();
  live.port.postMessage({ play });
}

export function setReference(url: string | null) {
  if (reference) reference.pause();
  reference = url ? new Audio(url) : null;
  if (reference) reference.loop = true;
}

// Changing source has to re-assert the latched play state, and asking for the source
// that is already selected must do nothing at all — otherwise loading a target kills
// the sound.
export function setSource(next: string) {
  const wanted = next === "dry" ? "dry" : "synth";
  if (wanted === source) return;
  source = wanted;
  if (reference) reference.pause();
  if (node) node.port.postMessage({ play: false });
  if (pending.play) void setPlay(true);
}

export function setBlend(blend: number) {
  send({ blend });
}

export function setFreq(freq: number) {
  send({ freq });
}
