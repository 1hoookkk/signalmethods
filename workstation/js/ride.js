import { api } from "./api.js";
import { initPad } from "./pad.js";
import { setCorners, setRide, setPlay, setBlend, setFreq, outputRms, audioRate, audioState } from "./audio.js";

const pick = document.getElementById("ride-pick");
const verdict = document.getElementById("ride-verdict");
let loaded = Array(8).fill(false);

function say(text) {
  verdict.textContent = text;
}

let levelTimer = 0;

function hold(down) {
  setPlay(down).catch((e) => say(`AUDIO: ${e.message}`));
  clearInterval(levelTimer);
  if (!down) return;
  levelTimer = setInterval(() => {
    const rms = outputRms();
    if (rms === null) return;
    const level = rms > 0 ? `${(20 * Math.log10(rms)).toFixed(1)} dBFS` : "silent";
    say(`hold — out ${level} · ${audioState()} ${audioRate()} Hz`);
  }, 250);
}

initPad(document.getElementById("ride-pad"), {
  onRide: (m, q, z) => setRide(m, q, z),
  onHold: hold,
  onBlend: (b) => setBlend(b),
  onFreq: (f) => setFreq(f),
  getCube: () => ({ filled: loaded, active: -1, names: null }),
});

async function load(id) {
  const r = await api.target("body", id);
  setCorners(r.words || (await api.corners(r.corners)).words);
  loaded = Array(8).fill(true);
  say(`${r.name} — 8 corners`);
}

async function start() {
  const lib = await api.library();
  pick.textContent = "";
  const placeholder = document.createElement("option");
  placeholder.textContent = "— pick a body —";
  placeholder.value = "";
  pick.appendChild(placeholder);
  for (const b of lib.bodies || []) {
    const o = document.createElement("option");
    o.value = b.id;
    o.textContent = b.name;
    pick.appendChild(o);
  }
  pick.onchange = () => {
    if (pick.value) load(pick.value).catch((e) => say(`ERROR: ${e.message}`));
  };
}

start().catch((e) => say(`ERROR: ${e.message}`));
