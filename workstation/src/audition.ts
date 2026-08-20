import { setBlend, setFreq, setPlay, setRide, setSource } from "./audio.js";
import { curveInto } from "./curves.js";
import type { Doc } from "./doc.js";
import { interpolateWords } from "./dsp.js";

export type AuditionDeps = {
  doc: Doc;
  say: (text: string) => void;
  pushAudio: () => void;
  paintSpectrum: () => void;
};

// The engine owns transport. Authoring never does: playback survives corner selection,
// M/Q movement, dragging, FIT, undo and corner swap.
export function createAudition(deps: AuditionDeps) {
  const { doc, say, pushAudio, paintSpectrum } = deps;

  const ridePos = { m: 0, q: 0, z: 0 };
  let previewPending = false;
  let previewBuffer: Float32Array | null = null;
  let playing = false;

  function preview(m: number, q: number, z: number) {
    ridePos.m = m;
    ridePos.q = q;
    ridePos.z = z;
    if (!doc.fieldWords && doc.words) pushAudio();
    if (previewPending || !doc.fieldWords) return;
    previewPending = true;
    requestAnimationFrame(() => {
      previewPending = false;
      if (!doc.fieldWords) return;
      const words = interpolateWords(doc.fieldWords, ridePos.m, ridePos.q, ridePos.z);
      previewBuffer = curveInto(words, previewBuffer);
      doc.preview = previewBuffer;
      paintSpectrum();
    });
  }

  function hold(down: boolean) {
    if (down && !doc.fieldWords && !doc.words) return say("nothing seated");
    playing = down;
    if (!down) {
      doc.preview = null;
      paintSpectrum();
    }
    if (down && !doc.fieldWords) pushAudio();
    setPlay(down).catch((e: Error) => say(`ERROR ${e.message}`));
  }

  return {
    ride(m: number, q: number, z: number) {
      setRide(m, q, z);
      preview(m, q, z);
    },
    cornerPosition(i: number) {
      return { m: i & 1, q: (i >> 1) & 1, z: (i >> 2) & 1 };
    },
    toggle: () => hold(!playing),
    isPlaying: () => playing,
    setBlend,
    setFreq,
    setSource,
    ridePos,
  };
}
