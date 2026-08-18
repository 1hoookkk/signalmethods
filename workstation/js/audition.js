import { curveInto } from "./curves.js";
import { interpolateWords } from "./dsp.js";
import { initPad } from "./pad.js";
import { audioRate, audioState, outputRms, setBlend, setFreq, setPlay, setRide, setSource } from "./audio.js";

const LEVEL_MS = 250;

export function createAudition(el, deps) {
  const { doc, say, pushAudio, paintSpectrum } = deps;

  const ridePos = { m: 0, q: 0, z: 0 };
  let previewPending = false;
  let previewBuffer = null;
  let levelTimer = 0;
  let uniformWarned = false;

  function preview(m, q, z) {
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

  function hold(down) {
    if (down && !doc.fieldWords && !doc.words) {
      return say("nothing seated — load a body or seat sections first");
    }
    if (down && !doc.fieldWords) pushAudio();
    setPlay(down).catch((e) => say(`AUDIO: ${e.message}`));
    clearInterval(levelTimer);
    if (!down) return;
    levelTimer = setInterval(() => {
      const rms = outputRms();
      if (rms === null) return;
      const level = rms > 0 ? `${(20 * Math.log10(rms)).toFixed(1)} dBFS` : "silent";
      say(`hold — out ${level} · ${audioState()} ${audioRate()} Hz`);
    }, LEVEL_MS);
  }

  const pad = initPad(el, {
    onRide: (m, q, z) => {
      setRide(m, q, z);
      preview(m, q, z);
      if (doc.fieldUniform && !uniformWarned) {
        uniformWarned = true;
        say("playing one state — edit LO and HI to create motion");
      }
    },
    onHold: hold,
    onBlend: (b) => setBlend(b),
    onFreq: (f) => setFreq(f),
    onSource: (source) => setSource(source),
  });

  return {
    hold,
    preview,
    ridePos,
    paint: pad.paint,
    clearUniformWarning() {
      uniformWarned = false;
    },
  };
}
