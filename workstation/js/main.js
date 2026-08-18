import { api } from "./api.js";
import { renderLibrary } from "./library.js";
import {
  doc,
  commit,
  undo,
  redo,
  emptyLanes,
  emptyRoles,
  lawState,
  pinLaw,
  freeLaw,
  fieldCorners,
  loadSnapshot,
  DRAW_GRID,
  toDrawGrid,
} from "./doc.js";
import { renderCorners, NAMES as CORNER_NAMES } from "./corners.js";
import { drawSpectrum, puckHit, dbOfY } from "./spectrum.js";
import { hzOfX } from "./render.js";
import { mountStages, drawStages } from "./stages.js";
import { createSectionPicker } from "./section.picker.js";
import * as field from "./field.js";
import { drawRoots, hitRoot, dragTo } from "./roots.js";
import { initPad } from "./pad.js";
import { FORMANT_SLOTS } from "./fit.roles.js";
import { setCorners, setRide, setPlay, setBlend, setFreq, setGrit, setRate, setReference, setSource, outputRms, audioRate, audioState } from "./audio.js";
import { lanesToWords, encode, interpolateWords, wordsToBiquads, sumDb, biquadFromWords, tfKey } from "./dsp.js";
import { curveInto, sumCurve, stageCurves, displaySr, setDisplaySr } from "./curves.js";
import { openWire } from "./wire.js";

const verdict = document.getElementById("verdict-text");
const spectrumDock = document.getElementById("spectrum");
const stagesDock = document.getElementById("stages");
const analysisDock = document.getElementById("analysis");
const targetPane = document.getElementById("analysis-body");

spectrumDock.style.position = "relative";
stagesDock.style.position = "relative";

const overlayStatus = document.createElement("div");
overlayStatus.className = "overlay-status";
const overlayName = document.createElement("span");
const clearOverlayButton = document.createElement("button");
clearOverlayButton.textContent = "CLEAR";
overlayStatus.append(overlayName, clearOverlayButton);
spectrumDock.appendChild(overlayStatus);
const recordingState = { item: null, average: null, current: null, timer: 0 };

let mouths = [];

const canvas = document.createElement("canvas");
canvas.style.cssText = "flex:1;min-height:0;width:100%";
spectrumDock.appendChild(canvas);

const stagesBody = document.createElement("div");
stagesBody.id = "stages-body";
stagesBody.style.cssText = "flex:1;display:flex;min-height:0";
stagesDock.appendChild(stagesBody);

const rootsCanvas = document.createElement("canvas");
rootsCanvas.style.cssText = "flex:1;min-height:0;width:100%;touch-action:none";
document.getElementById("roots").appendChild(rootsCanvas);

let ceiling = 0.9999;
let hoverCorner = null;
let uniformWarned = false;
const BOUND_INTERVAL_ST = 2;
let padFreq = 110;

function endpointName(i = doc.selectedCorner) {
  return i === 0 ? "LO" : "HI";
}

const pad = initPad(document.getElementById("pad"), {
  onRide: (m, q, z) => {
    setRide(m, q, z);
    ridePreview(m, q, z);
    if (doc.fieldUniform && !uniformWarned) {
      uniformWarned = true;
      say("playing one state — edit LO and HI to create motion");
    }
  },
  onHold: hold,
  onBlend: (b) => setBlend(b),
  onFreq: (f) => { padFreq = f; setFreq(f); },
  onGrit: (g) => setGrit(g),
  onSource: (source) => setSource(source),
  isSquare: () => squareField(),
  getCube: () => ({
    filled: doc.field.map((s) => !!s),
    active: doc.selectedCorner,
    names: doc.field.map((s) => (s ? s.name : "")),
    hover: hoverCorner,
  }),
});

let levelTimer = 0;
let playing = false;

function hold(down) {
  if (down && !doc.fieldWords && !doc.words) {
    return say("nothing seated — load a body or seat sections first");
  }
  if (down && !doc.fieldWords) pushAudio();
  playing = down;
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

const ridePos = { m: 0, q: 0, z: 0 };
let previewPending = false;
let previewBuffer = null;

function ridePreview(m, q, z) {
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

const ensureCorner = field.ensureCorner;
const bindCorner = field.selectCorner;
const storeCorner = field.commitLanes;
const squareField = field.isSquare;
const outputWords = field.runtimeFieldWords;

field.setBindHook((slot) => {
  if (!recordingState.item) return;
  doc.target = slot.target || null;
  doc.peaks = slot.peaks || [];
  doc.targetName = slot.targetName || recordingState.item.name;
});

function pushAudio() {
  const words = field.runtimeFieldWords();
  if (!words) {
    doc.fieldUniform = false;
    return;
  }
  doc.fieldWords = words;
  doc.fieldUniform = words.every((w) => w === words[0]);
  setCorners(words);
}

function commitToCorner() {
  if (doc.field[doc.selectedCorner] && doc.field[doc.selectedCorner].held) {
    return say(`${endpointName()} HELD — double-click it to free it`);
  }
  storeCorner();
  say(`${endpointName()} is already live — edits land directly`);
  pushAudio();
  paintCorners();
  pad.paint();
  paint();
}

function paintCorners() {
  renderCorners(document.getElementById("corners-body"), doc.field, doc.selectedCorner, {
    onHover: (i) => {
      hoverCorner = i;
      pad.paint();
    },
    onSelect: (i) => {
      if (i === doc.selectedCorner) return;
      commit(`select corner ${i}`);
      bindCorner(i);
      say(`${endpointName(i)} selected — edits land here`);
      paint();
      paintCorners();
      pad.paint();
    },
    onHoldCorner: (i) => {
      if (!doc.field[i]) return say(`${endpointName(i)} is empty`);
      commit(`hold corner ${i}`);
      doc.field[i].held = !doc.field[i].held;
      say(`${endpointName(i)} ${doc.field[i].held ? "HELD" : "free"}`);
      paintCorners();
    },
    onSwap: (a, b) => {
      commit(`swap C${a} C${b}`);
      const t = doc.field[a];
      doc.field[a] = doc.field[b];
      doc.field[b] = t;
      bindCorner(doc.selectedCorner, false);
      say(`C${a} ↔ C${b}`);
      pushAudio();
      paintCorners();
      pad.paint();
      paint();
    },
    onClear: () => {
      commit("new");
      doc.lanes = emptyLanes();
      doc.roles = emptyRoles();
      doc.laws = doc.lanes.map(() => freeLaw());
      doc.field = Array.from({ length: 8 }, () => null);
      doc.words = null;
      doc.fieldWords = null;
      doc.target = null;
      doc.targetName = null;
      doc.peaks = null;
      doc.preview = null;
      doc.rms = null;
      doc.selectedCorner = 0;
      bindCorner(0);
      uniformWarned = false;
      say("new — LO and HI cleared");
      paintCorners();
      pad.paint();
      paint();
    },
    onFill: () => {
      const src = doc.field[doc.selectedCorner];
      if (!src) return say("nothing posed yet — edit LO or HI first");
      if (!src.words && src.lanes) src.words = lanesToWords(src.lanes, displaySr());
      const to = doc.selectedCorner === 0 ? 1 : 0;
      commit(`copy endpoint ${doc.selectedCorner} to ${to}`);
      doc.field[to] = structuredClone(src);
      doc.field[to].name = to === 0 ? "LO MORPH" : "HI MORPH";
      doc.field[to].cloned = true;
      say(`${doc.selectedCorner === 0 ? "LO" : "HI"} copied to ${to === 0 ? "LO" : "HI"} — edit the destination`);
      pushAudio();
      paintCorners();
      pad.paint();
      paint();
    },
    onKeep: () => {
      api
        .writeFrame(doc.lanes, doc.laws, doc.targetName || "workstation")
        .then((r) => say(`kept ${r.name} → ${r.path}`))
        .catch((e) => say(`ERROR: ${e.message}`));
    },
    onAudit: () => {
      const words = outputWords();
      if (!words) return say("set a LO or HI endpoint first");
      api
        .auditWords(words)
        .then((r) =>
          say(
            `audit ${r.pass ? "PASS" : "FAIL"} — crown ${r.crown_min_db.toFixed(1)}..${r.crown_max_db.toFixed(1)} dB, parity ${r.parity_db.toFixed(1)}, interior ${r.interior_crown_db.toFixed(1)}${r.pass ? "" : " — " + r.failures.join("; ")}`
          )
        )
        .catch((e) => say(`ERROR: ${e.message}`));
    },
    onWrite: () => {
      const words = outputWords();
      if (!words) return say("set a LO or HI endpoint first");
      api
        .writeWords(words, "morph")
        .then((r) => say(`wrote ${r.bytes}B body → ${r.path} (interior crown ${r.report.interior_crown_db.toFixed(1)} dB)`))
        .catch((e) => say(`REFUSED: ${e.message}`));
    },
  }, doc.words);
}

function say(text) {
  verdict.textContent = text;
}

function isLocked(i) {
  return lawState(doc.laws[i]) !== "FREE";
}

let responseSeq = 0;

async function requestResponse(lanes) {
  const seq = ++responseSeq;
  const r = await api.response(lanes);
  return seq === responseSeq ? r : null;
}

let editVersion = 0;

// Guarded post-gesture parity reconcile. During a gesture the client encode
// (lanesToWords) is authoritative; this only adopts the server's snapped words
// AFTER a gesture, and never when a newer local edit has since landed.
function reconcile() {
  const v = editVersion;
  requestResponse(doc.lanes)
    .then((r) => {
      if (!r || v !== editVersion) return;
      storeCorner(doc.lanes, r.words);
      paint();
    })
    .catch(() => {});
}

function editLane(i, patch) {
  if (isLocked(i)) {
    say(`S${i + 1} HELD — nothing may move it; click HELD to free it`);
    return;
  }
  const lane = doc.lanes[i];
  doc.rmsStale = true;
  for (const key of Object.keys(patch)) {
    if (Number.isFinite(patch[key])) lane[key] = patch[key];
  }
  if (lane.pole_r > ceiling) lane.pole_r = ceiling;
  const slot = ensureCorner(doc.selectedCorner);
  if (slot.citations) slot.citations[i] = null;
  storeCorner(doc.lanes);
  paint();
  pushAudio();
  editVersion++;
}

function zeroHeld(i) {
  return doc.laws[i].freedom[2] === false;
}

async function boundAndNormalize() {
  if (!doc.words) return say("nothing seated yet");
  commit("bound and normalize");
  let bound = 0;
  for (let i = 0; i < 7; i++) {
    const lane = doc.lanes[i];
    if (lane.pole_r <= 0 || lane.zero_r > 0) continue;
    if (20 * Math.log10(1 / Math.max(1 - lane.pole_r, 1e-6)) < 20) continue;
    lane.zero_hz = Math.min(16000, lane.pole_hz * Math.pow(2, BOUND_INTERVAL_ST / 12));
    lane.zero_r = 0.9 * lane.pole_r;
    bound++;
  }
  let r = await requestResponse(doc.lanes);
  if (!r) return;
  storeCorner(r.lanes, r.words);
  const stages = stageCurves(doc.words);
  const active = [];
  for (let i = 0; i < 7; i++) if (doc.lanes[i].pole_r > 0 || doc.lanes[i].zero_r > 0) active.push(i);
  let total = 0;
  for (const i of active) {
    let peak = -Infinity;
    for (const v of stages[i]) if (v > peak) peak = v;
    total += peak;
    doc.lanes[i].scale *= Math.pow(10, -peak / 20);
  }
  const share = Math.pow(10, total / (20 * Math.max(active.length, 1)));
  for (const i of active) doc.lanes[i].scale *= share;
  r = await requestResponse(doc.lanes);
  if (!r) return;
  storeCorner(r.lanes, r.words);
  pushAudio();
  say(`${bound} bound pairs, cascade scale product preserved across ${active.length} sections`);
  paint();
}

function paintSpectrum() {
  const stale = doc.words ? sumCurve(doc.words) : null;
  const swap = fitting && doc.candidate;
  const view = {
    name: doc.targetName,
    target: doc.target,
    sum: swap ? doc.candidate : stale,
    peaks: doc.peaks,
    guides: guidesOf(),
    preview: doc.preview,
    candidate: swap ? stale : doc.candidate,
    lanes: doc.lanes,
    roles: doc.roles,
    zeroHeld: doc.laws.map((_, i) => zeroHeld(i)),
    locks: doc.laws.map((_, i) => isLocked(i)),
    selected: doc.selected,
    field: doc.field,
    active: doc.selectedCorner,
    packing: doc.packing,
    rms: doc.target && stale ? liveResidual(doc.target, stale) : null,
    rmsStale: doc.rmsStale,
    ceiling,
  };
  drawSpectrum(canvas, view);
}

function liveResidual(target, sum) {
  const t = toDrawGrid(target);
  let squared = 0;
  let count = 0;
  for (let i = 0; i < sum.length && i < t.length; i++) {
    if (!Number.isFinite(sum[i]) || !Number.isFinite(t[i])) continue;
    const d = sum[i] - t[i];
    squared += d * d;
    count++;
  }
  return count ? Math.sqrt(squared / count) : null;
}

let puckDrag = null;
canvas.style.touchAction = "none";
canvas.addEventListener("pointerdown", (e) => {
  const rect = canvas.getBoundingClientRect();
  const sum = doc.words ? sumCurve(doc.words) : null;
  const i = puckHit(
    doc.lanes,
    doc.roles,
    sum,
    canvas.clientWidth,
    canvas.clientHeight,
    e.clientX - rect.left,
    e.clientY - rect.top
  );
  if (i === null) return;
  if (isLocked(i)) {
    say(`S${i + 1} locked`);
    return;
  }
  commit(`drag ${doc.roles[i]} S${i + 1}`);
  doc.selected = i;
  puckDrag = {
    lane: i,
    y0: e.clientY - rect.top,
    rp0: 20 * Math.log10(1 / Math.max(1e-6, 1 - doc.lanes[i].pole_r)),
  };
  canvas.setPointerCapture(e.pointerId);
});
canvas.addEventListener("pointermove", (e) => {
  if (!puckDrag) return;
  const rect = canvas.getBoundingClientRect();
  const h = canvas.clientHeight;
  const py = e.clientY - rect.top;
  const hz = hzOfX(e.clientX - rect.left, canvas.clientWidth);
  const rp = puckDrag.rp0 + (dbOfY(py, h) - dbOfY(puckDrag.y0, h));
  const r = Math.min(ceiling, Math.max(0, 1 - Math.pow(10, -Math.max(0, rp) / 20)));
  editLane(puckDrag.lane, { pole_hz: hz, pole_r: r });
});
const endPuckDrag = () => {
  if (!puckDrag) return;
  const d = puckDrag;
  puckDrag = null;
  storeCorner(doc.lanes, lanesToWords(doc.lanes, displaySr()));
  pushAudio();
  paint();
  say(
    `S${d.lane + 1} ${doc.roles[d.lane] || ""}: ${doc.lanes[d.lane].pole_hz.toFixed(0)} Hz  r ${doc.lanes[d.lane].pole_r.toFixed(4)}`
  );
  reconcile();
};
canvas.addEventListener("pointerup", endPuckDrag);
canvas.addEventListener("pointercancel", endPuckDrag);

let paintQueued = false;
let levelDragUntil = 0;
let stripTimer = 0;

function paint() {
  if (paintQueued) return;
  paintQueued = true;
  requestAnimationFrame(() => {
    paintQueued = false;
    paintFrame();
  });
}

let mountedStages = null;

const stageCallbacks = {
  onSelect: (i) => {
    doc.selected = i;
    paint();
  },
  onLockClick: (i) => {
    commit(`lock S${i + 1}`);
    doc.laws[i] = isLocked(i) ? freeLaw() : pinLaw(doc.lanes[i]);
    say(`S${i + 1} ${isLocked(i) ? "locked" : "unlocked"}`);
    paint();
  },
  onHeader: null,
  onLevel: (i, deltaDb) => {
    const lane = doc.lanes[i];
    if (lane.pole_r <= 0 && lane.zero_r <= 0) return;
    editLane(i, { scale: lane.scale * Math.pow(10, deltaDb / 20) });
    say(`S${i + 1} scale ${(20 * Math.log10(Math.max(lane.scale, 1e-9))).toFixed(1)} dB`);
  },
  onDrag: (i, kind, hz, r) => {
    if (hz === null) {
      reconcile();
      return;
    }
    if (kind === "pole") editLane(i, { pole_hz: hz, pole_r: r });
    else editLane(i, { zero_hz: hz, zero_r: r });
  },
  onClear: (i) => {
    commit(`clear C${doc.selectedCorner} S${i + 1}`);
    doc.lanes[i] = emptyLanes()[0];
    const slot = ensureCorner(doc.selectedCorner);
    if (slot.citations) slot.citations[i] = null;
    storeCorner(doc.lanes);
    pushAudio();
    paintCorners();
    paint();
    say(`${endpointName()} S${i + 1} cleared to identity`);
  },
  onFlip: (i) => {
    if (isLocked(i)) return say(`S${i + 1} HELD`);
    commit(`flip C${doc.selectedCorner} S${i + 1}`);
    const lane = doc.lanes[i];
    [lane.pole_hz, lane.zero_hz] = [lane.zero_hz, lane.pole_hz];
    [lane.pole_r, lane.zero_r] = [lane.zero_r, lane.pole_r];
    const slot = ensureCorner(doc.selectedCorner);
    const words = slot.words[i].slice();
    slot.words[i] = [words[2], words[3], words[0], words[1], words[4]];
    if (slot.citations && slot.citations[i]) slot.citations[i] = `${slot.citations[i]} · P↔Z`;
    storeCorner(doc.lanes, slot.words);
    pushAudio();
    paintCorners();
    paint();
    say(`${endpointName()} S${i + 1} pole ↔ zero; scale preserved`);
  },
  onSwap: (from, to) => {
    commit(`swap S${from + 1} S${to + 1}`);
    for (const slot of doc.field) {
      if (!slot) continue;
      [slot.lanes[from], slot.lanes[to]] = [slot.lanes[to], slot.lanes[from]];
      [slot.words[from], slot.words[to]] = [slot.words[to], slot.words[from]];
      if (slot.roles) [slot.roles[from], slot.roles[to]] = [slot.roles[to], slot.roles[from]];
      if (slot.laws) [slot.laws[from], slot.laws[to]] = [slot.laws[to], slot.laws[from]];
      if (slot.citations) [slot.citations[from], slot.citations[to]] = [slot.citations[to], slot.citations[from]];
    }
    bindCorner(doc.selectedCorner, false);
    pushAudio();
    paintCorners();
    paint();
    say(`S${from + 1} ↔ S${to + 1} across all corners; travel preserved`);
  },
  onSeat: (i, rect) => openStageSourcePicker(i, rect),
};

function paintFrame() {
  overlayStatus.style.display = doc.target ? "flex" : "none";
  overlayName.textContent = doc.target ? `OVERLAY · ${doc.targetName || "UNTITLED"}` : "";
  paintSpectrum();
  drawRoots(rootsCanvas, doc, ceiling);
  if (!mountedStages) mountedStages = mountStages(stagesBody, doc, stageCallbacks);
  drawStages(mountedStages, doc);
}

async function refreshResponse() {
  const r = await requestResponse(doc.lanes);
  if (r) {
    storeCorner(r.lanes, r.words);
  }
  if (doc.words) pushAudio();
  paint();
}

let drag = null;

rootsCanvas.addEventListener("pointerdown", (e) => {
  const rect = rootsCanvas.getBoundingClientRect();
  const hit = hitRoot(rootsCanvas, doc, e.clientX - rect.left, e.clientY - rect.top);
  if (!hit) return;
  if (isLocked(hit.lane)) {
    say(`S${hit.lane + 1} HELD — nothing may move it; click HELD to free it`);
    return;
  }
  commit(`drag ${hit.kind} S${hit.lane + 1}`);
  const slot = ensureCorner(doc.selectedCorner);
  if (slot.citations) slot.citations[hit.lane] = null;
  drag = hit;
  rootsCanvas.setPointerCapture(e.pointerId);
});

rootsCanvas.addEventListener("pointermove", (e) => {
  if (!drag) return;
  const rect = rootsCanvas.getBoundingClientRect();
  dragTo(rootsCanvas, doc, drag, e.clientX - rect.left, e.clientY - rect.top, ceiling);
  storeCorner(doc.lanes);
  paint();
  pushAudio();
  editVersion++;
});

rootsCanvas.addEventListener("pointerup", () => {
  if (!drag) return;
  const d = drag;
  drag = null;
  storeCorner(doc.lanes, lanesToWords(doc.lanes, displaySr()));
  pushAudio();
  paint();
  say(`S${d.lane + 1} ${d.kind}: ${fmtLane(doc.lanes[d.lane], d.kind)}`);
  reconcile();
});

function fmtLane(lane, kind) {
  const hz = kind === "pole" ? lane.pole_hz : lane.zero_hz;
  const r = kind === "pole" ? lane.pole_r : lane.zero_r;
  return `${hz.toFixed(1)} Hz  r ${r.toFixed(4)} (snapped)`;
}

async function setTarget(item) {
  recordingState.item = null;
  analysisDock.hidden = true;
  setReference(null);
  setSource("synth");
  const t = await api.target("mouth", item.id);
  commit(`target ${t.name}`);
  doc.targetName = t.name;
  doc.target = t.curve;
  doc.peaks = t.peaks;
  say(`target set — ${t.name}, ${t.peaks.length} measured formants (cascade untouched)`);
  paint();
}

function applyRecordingTarget(t, label) {
  recordingState.current = t;
  doc.targetName = `${t.name} · ${label}`;
  doc.target = t.curve;
  doc.peaks = t.peaks;
  doc.rmsStale = false;
  say(`target ${doc.targetName} — ${t.peaks.length} measured peaks`);
  paint();
}

function clearWorkingOverlay() {
  commit("clear working overlay");
  doc.target = null;
  doc.targetName = recordingState.item ? recordingState.item.name : null;
  doc.peaks = [];
  recordingState.current = null;
  const slot = doc.field[doc.selectedCorner];
  if (slot) {
    slot.target = null;
    slot.targetName = null;
    slot.peaks = [];
  }
  say("working overlay cleared");
  paint();
}

clearOverlayButton.onclick = clearWorkingOverlay;

function landRecordingTarget() {
  if (!recordingState.current || !doc.target) return say("choose STATIONARY or a TIME SLICE first");
  const slot = ensureCorner(doc.selectedCorner);
  slot.target = doc.target.slice();
  slot.targetName = doc.targetName;
  slot.peaks = structuredClone(doc.peaks || []);
  say(`${doc.targetName} landed on ${endpointName()} — hand-place sections here`);
  paintCorners();
}

function seatAr(t) {
  if (!t || !t.ar_sections || !t.ar_sections.length) return say("no AR sections available");
  commit(`AR seed ${t.name}`);
  const n = Math.min(t.ar_sections.length, 7);
  for (let k = 0; k < n; k++) {
    const s = t.ar_sections[k];
    doc.lanes[k] = { pole_hz: s.pole_hz, pole_r: Math.min(s.pole_r, ceiling), zero_hz: s.zero_hz || 0, zero_r: s.zero_r || 0, scale: 1 };
  }
  storeCorner(doc.lanes);
  say(`${t.name} — AR(14) ${n} sections seated on ${endpointName()}`);
  refreshResponse().catch((e) => say(`ERROR: ${e.message}`));
}

async function prepareRecording(item, seedArNow = false) {
  commit(`sample ${item.name}`);
  recordingState.item = item;
  recordingState.average = await api.target("recording", item.id);
  recordingState.current = null;
  doc.target = null;
  doc.peaks = [];
  doc.targetName = item.name;
  setReference(`/api/audio?id=${item.id}`);
  setSource("synth");
  targetPane.textContent = "";
  const stationary = document.createElement("button");
  const slice = document.createElement("input");
  const at = document.createElement("span");
  const land = document.createElement("button");
  const peaks = document.createElement("button");
  const ar = document.createElement("button");
  stationary.textContent = "STATIONARY · AVERAGE";
  slice.type = "range";
  slice.min = "0";
  slice.max = String(recordingState.average.source_seconds || recordingState.average.seconds || 1);
  slice.step = "0.01";
  slice.value = String(Math.min(0.1, Number(slice.max) / 2));
  slice.style.width = "120px";
  at.textContent = `${Number(slice.value).toFixed(2)} s`;
  land.textContent = `LAND → ${endpointName()}`;
  peaks.textContent = "SEED PEAKS";
  ar.textContent = "SEED AR(14)";
  stationary.onclick = () => applyRecordingTarget(recordingState.average, "stationary average");
  slice.oninput = () => {
    at.textContent = `${Number(slice.value).toFixed(2)} s`;
    land.textContent = `LAND → ${endpointName()}`;
    clearTimeout(recordingState.timer);
    recordingState.timer = setTimeout(() => {
      api.target("recording", item.id, { slice_at_seconds: Number(slice.value) })
        .then((t) => applyRecordingTarget(t, `slice ${Number(slice.value).toFixed(2)} s`))
        .catch((e) => say(`ERROR: ${e.message}`));
    }, 120);
  };
  land.onclick = landRecordingTarget;
  peaks.onclick = () => placeSkeleton().catch((e) => say(`ERROR: ${e.message}`));
  ar.onclick = () => seatAr(recordingState.current || recordingState.average);
  stationary.className = "analysis-wide";
  const sliceRow = document.createElement("div");
  sliceRow.className = "analysis-slice analysis-wide";
  sliceRow.append(document.createTextNode("TIME SLICE"), slice, at);
  land.className = "analysis-wide";
  targetPane.append(stationary, sliceRow, land, peaks, ar);
  analysisDock.hidden = false;
  if (seedArNow) {
    applyRecordingTarget(recordingState.average, "stationary average");
    seatAr(recordingState.average);
  } else {
    say(`${item.name} loaded — user must choose STATIONARY or a TIME SLICE; no auto-classification`);
    paint();
  }
}


function guidesOf() {
  const g = [];
  for (let i = 0; i < 7; i++) {
    if (doc.roles[i] && /^F\d/.test(doc.roles[i]) && doc.lanes[i].pole_r > 0) {
      g.push({ hz: doc.lanes[i].pole_hz, label: doc.roles[i] });
    }
  }
  return g;
}

async function placeSkeleton() {
  if (!doc.target) return say("no target loaded");
  const r = await api.skeleton(doc.target, emptyLanes());
  commit("auto-pin skeleton");
  const seated = r.lanes.filter((lane) => lane.pole_r > 0).slice(0, FORMANT_SLOTS.length);
  doc.lanes = emptyLanes();
  doc.roles = emptyRoles();
  seated.forEach((lane, k) => {
    const slot = FORMANT_SLOTS[k];
    doc.lanes[slot] = lane;
    doc.roles[slot] = `F${k + 1}`;
  });
  doc.laws = doc.lanes.map((lane) => (lane.pole_r > 0 ? pinLaw(lane) : freeLaw()));
  storeCorner(doc.lanes);
  doc.selected = FORMANT_SLOTS[0];
  say(`${seated.length} measured formants placed and locked — ${doc.targetName}`);
  await refreshResponse();
  await boundAndNormalize();
}

let candidatePending = false;
let candidateCount = 0;
let fitting = false;
let fitToken = 0;

function showCandidate(candidate) {
  if (!fitting) return;
  candidateCount++;
  if (candidatePending) return;
  candidatePending = true;
  requestAnimationFrame(() => {
    candidatePending = false;
    const curve = sumDb(wordsToBiquads(candidate.words), DRAW_GRID, displaySr());
    const target = toDrawGrid(doc.target);
    let offset = 0;
    for (let i = 0; i < curve.length; i++) offset += target[i] - curve[i];
    offset /= curve.length;
    for (let i = 0; i < curve.length; i++) curve[i] += offset;
    doc.candidate = curve;
    say(`fitting… ${candidate.rms.toFixed(2)} dB rms (candidate ${candidateCount})`);
    paintSpectrum();
  });
}

function solverOrder() {
  const order = [];
  for (let i = 0; i < 7; i++) if (isLocked(i) && doc.lanes[i].pole_r > 0) order.push(i);
  for (let i = 0; i < 7; i++) if (!order.includes(i)) order.push(i);
  return order;
}

async function runFit() {
  if (!doc.target) return say("no target loaded");
  say("solving… (Escape abandons)");
  candidateCount = 0;
  fitting = true;
  const token = ++fitToken;
  const order = solverOrder();
  const laws = doc.laws;
  try {
    const r = await api.fitStream(
      doc.target,
      order.map((i) => doc.lanes[i]),
      order.map((i) => laws[i]),
      false,
      order,
      showCandidate
    );
    if (token !== fitToken) return;
    doc.candidate = null;
    commit("fit");
    const lanes = emptyLanes();
    const words = new Array(7);
    order.forEach((lane, slot) => {
      lanes[lane] = r.lanes[slot];
      words[lane] = r.words[slot];
    });
    const strayed = [];
    for (let i = 0; i < 7; i++) {
      if (!isLocked(i) || doc.lanes[i].pole_r <= 0) continue;
      const was = doc.lanes[i].pole_hz;
      const now = lanes[i].pole_r > 0 ? lanes[i].pole_hz : 0;
      if (Math.abs(now - was) > was * 0.01) strayed.push(`S${i + 1}`);
    }
    for (let i = 0; i < 7; i++) {
      if (!doc.roles[i] || isLocked(i)) continue;
      const was = doc.lanes[i].pole_hz;
      const now = lanes[i].pole_r > 0 ? lanes[i].pole_hz : 0;
      if (!now || !was || Math.abs(now - was) > was * 0.02) doc.roles[i] = null;
    }
    storeCorner(lanes, words);
    doc.rms = r.target_rms_db;
    doc.rmsStale = false;
    doc.packing = r.intended_packed_rms_db;
    pushAudio();
    say(
      `${r.target_rms_db.toFixed(2)} dB rms over ${r.sections_used} sections, packing ${r.intended_packed_rms_db.toFixed(3)} dB${
        strayed.length ? ` — the solver moved locked ${strayed.join(", ")}` : ""
      }`
    );
    paint();
  } finally {
    if (token === fitToken) fitting = false;
    doc.candidate = null;
  }
}

const briefPane = document.createElement("div");
briefPane.style.cssText =
  "max-height:30%;overflow-y:auto;background:var(--well);color:var(--well-ink);font-size:11px;padding:4px 8px;white-space:pre-wrap;display:none;border-top:1px solid var(--grat-major)";
document.getElementById("library").appendChild(briefPane);

let stageSources = [];

function afterFieldChange() {
  pushAudio();
  paintCorners();
  pad.paint();
  paint();
}

const sectionPicker = createSectionPicker(document.getElementById("work"), {
  onState: (state, destination) => {
    field
      .seatState(state, destination)
      .then(({ citation, verbatim }) => {
        afterFieldChange();
        say(`${endpointName()} S${destination + 1} ← ×${state.count}${verbatim ? "" : " · WORDS MISMATCH"} · ${citation}`);
      })
      .catch((e) => say(`ERROR: ${e.message}`));
  },
  onDetail: (text) => say(text),
});

async function loadTypeTemplate(entry, importAll) {
  const source = stageSources.find((s) => s.id === entry.id);
  const stageCount = source ? source.stage_count : 6;
  if (importAll) {
    const { stages, corners } = await field.importFactory(entry, stageCount, source ? source.corners : 4);
    afterFieldChange();
    return say(`${entry.type} imported — ${corners} corners × ${stages} sections, S${stages + 1} idle`);
  }
  const { stages, held } = await field.seatTemplate(entry, stageCount);
  afterFieldChange();
  const kept = held.length ? ` · S${held.join(",S")} HELD, kept` : "";
  say(`${entry.type} → ${endpointName()} · ${stages} sections, S${stages + 1} idle${kept}`);
}

function initTransplant(sources, vocabulary) {
  stageSources = sources || [];
  sectionPicker.setVocabulary(vocabulary);
}

function openStageSourcePicker(destination, anchorRect) {
  if (!stageSources.length) return say("no factory stage sources loaded");
  sectionPicker.open(destination, anchorRect);
}

function loadBodyObject(r, label) {
  doc.field = Array.from({ length: 8 }, (_, i) => {
    const lanes = structuredClone(r.corners[i]);
    return {
      name: `${r.name} c${i}`,
      lanes,
      words: r.words ? r.words[i] : lanesToWords(lanes, displaySr()),
      roles: emptyRoles(),
      laws: lanes.map(() => freeLaw()),
      citations: Array.from({ length: 7 }, () => null),
    };
  });
  doc.target = null;
  doc.targetName = r.name;
  doc.peaks = [];
  doc.preview = null;
  doc.rms = null;
  doc.rmsStale = false;
  bindCorner(0, false);
  pushAudio();
  paintCorners();
  pad.paint();
  paint();
  say(`${label}: ${r.name} — 8 authoritative packed corners loaded${r.real_pair_sections_skipped ? ` (${r.real_pair_sections_skipped} real-pair sections shown from packed words)` : ""}`);
}

async function pick(kind, item, right) {
  if (kind === "mouths") {
    await setTarget(item);
    if (right) await placeSkeleton();
    return;
  }
  if (kind === "recordings") {
    await prepareRecording(item, right);
    return;
  }
  if (kind === "poses") {
    const r = await api.target("pose", item.id);
    commit(`pose ${r.name}`);
    doc.targetName = r.name;
    const seated = r.lanes.filter((lane) => lane.pole_r > 0).slice(0, FORMANT_SLOTS.length);
    doc.lanes = emptyLanes();
    doc.roles = emptyRoles();
    seated.forEach((lane, k) => {
      doc.lanes[FORMANT_SLOTS[k]] = lane;
      doc.roles[FORMANT_SLOTS[k]] = `F${k + 1}`;
    });
    doc.laws = doc.lanes.map((lane) => (lane.pole_r > 0 ? pinLaw(lane) : freeLaw()));
    storeCorner(doc.lanes);
    say(`replaced cascade — ${r.name}, ${seated.length} formant poles placed and locked`);
    await refreshResponse();
    await boundAndNormalize();
    return;
  }
  if (kind === "frames") {
    const r = await api.frame(item.id);
    commit(`frame ${r.name}`);
    doc.targetName = r.name;
    doc.lanes = r.lanes;
    doc.laws = r.laws;
    doc.words = lanesToWords(r.lanes, displaySr());
    doc.roles = emptyRoles();
    storeCorner(doc.lanes, doc.words);
    say(`replaced cascade — frame ${r.name}, ${r.provenance}`);
    pushAudio();
    paint();
    refreshResponse().catch(() => {});
    return;
  }
  if (kind === "bodies") {
    const r = await api.target("body", item.id);
    commit(`body ${r.name}`);
    loadBodyObject(r, "body");
    return;
  }
  if (kind === "templates") {
    await loadTypeTemplate(item, right);
    return;
  }
  if (kind === "architectures") {
    const b = await api.brief(item.id);
    briefPane.style.display = "block";
    briefPane.textContent = `${b.name}${b.sentence ? "\n“" + b.sentence + "”" : ""}\n\n${b.intent}`;
    say(`brief: ${b.name}`);
    return;
  }
  say(`${kind}: ${item.name} (not wired yet)`);
}

async function start() {
  openWire();
  const lib = await api.library();
  setDisplaySr(lib.authoring_sr);
  setRate(lib.authoring_sr);
  const libEl = document.getElementById("library-list");
  renderLibrary(libEl, lib, (kind, item, right) => {
    pick(kind, item, right).catch((e) => say(`ERROR: ${e.message}`));
  });
  ceiling = lib.pole_ceiling_r || ceiling;
  initTransplant(lib.stage_sources, lib.vocabulary);
  mouths = lib.mouths || [];
  say(`library loaded — root ${lib.root}`);
  if (loadSnapshot()) say("Ready — Ctrl+L restores last session");
  paint();
  paintCorners();
  const q = new URLSearchParams(location.search);
  const bodyId = q.get("body");
  if (bodyId) {
    const item = (lib.bodies || []).find((b) => b.id === bodyId || b.name === bodyId);
    if (item) await pick("bodies", item, false);
  }
  const mouth = q.get("mouth");
  if (mouth) {
    const item = (lib.mouths || []).find((m) => m.id === mouth || m.name === mouth);
    if (item) {
      await setTarget(item);
      if (q.has("skeleton")) await placeSkeleton();
      if (q.has("fit")) await runFit();
      if (q.has("corners")) {
        for (let i = 0; i < 8; i++) {
          doc.field[i] = { name: `${doc.targetName} c${i}`, lanes: structuredClone(doc.lanes) };
        }
        paintCorners();
        pad.paint();
        const f = fieldCorners();
        const r = await api.audit(f.corners);
        say(
          `audit ${r.pass ? "PASS" : "FAIL"} — crown ${r.crown_min_db.toFixed(1)}..${r.crown_max_db.toFixed(1)} dB, parity ${r.parity_db.toFixed(1)}, interior ${r.interior_crown_db.toFixed(1)}`
        );
      }
    }
  }
}

window.addEventListener("keydown", (e) => {
  if (e.key === "Escape" && fitting) {
    fitting = false;
    fitToken++;
    doc.candidate = null;
    say("fit abandoned");
    paint();
    return;
  }
  if (e.ctrlKey && e.key === "l") {
    e.preventDefault();
    const snap = loadSnapshot();
    if (!snap || !snap.lanes) return say("no saved session");
    commit("restore session");
    doc.lanes = snap.lanes;
    if (snap.laws) doc.laws = snap.laws;
    if (snap.roles) doc.roles = snap.roles;
    if (snap.field) doc.field = snap.field;
    doc.selectedCorner = Number.isInteger(snap.selectedCorner) ? snap.selectedCorner : 0;
    bindCorner(doc.selectedCorner);
    doc.targetName = snap.targetName || null;
    say("restored last session");
    paintCorners();
    refreshResponse().catch(() => paint());
    return;
  }
  if (e.key === "Delete" && document.activeElement.tagName !== "INPUT") {
    commit("clear cascade");
    doc.lanes = emptyLanes();
    doc.roles = emptyRoles();
    doc.laws = doc.lanes.map(() => freeLaw());
    doc.words = null;
    doc.target = null;
    doc.targetName = null;
    doc.peaks = null;
    doc.preview = null;
    doc.rms = null;
    storeCorner(doc.lanes);
    say("cleared");
    paint();
    return;
  }
  if (e.code === "Space" && !e.ctrlKey && document.activeElement.tagName !== "INPUT") {
    e.preventDefault();
    const next = !playing;
    hold(next);
    if (!next) say("stopped");
    return;
  }
  if (e.ctrlKey && e.key === "z" && !e.shiftKey) {
    if (undo()) { bindCorner(doc.selectedCorner); say("undo"); paintCorners(); refreshResponse().catch(() => paint()); }
  } else if (e.ctrlKey && (e.key === "Z" || e.key === "y")) {
    if (redo()) { bindCorner(doc.selectedCorner); say("redo"); paintCorners(); refreshResponse().catch(() => paint()); }
  }
  if (document.activeElement.tagName === "INPUT") return;
  if (e.key === "s" && !e.ctrlKey) {
    e.preventDefault();
    say(`${endpointName()} is live — no SET step`);
    return;
  }
  if (e.key === "f" && !e.ctrlKey) {
    e.preventDefault();
    const src = doc.field.find((s) => s && !s.cloned);
    if (!src) return say("nothing posed — edit a corner first");
    if (!src.words && src.lanes) src.words = lanesToWords(src.lanes, displaySr());
    commit("fill field");
    let n = 0;
    for (let i = 0; i < 8; i++) {
      if (doc.field[i]) continue;
      doc.field[i] = structuredClone(src);
      doc.field[i].cloned = true;
      n++;
    }
    if (n) { pushAudio(); paintCorners(); pad.paint(); paint(); }
    say(n ? `${n} corners filled` : "field already full");
    return;
  }
  if (e.key === "w" && !e.ctrlKey) {
    e.preventDefault();
    const words = outputWords();
    if (!words) return say("set a LO or HI endpoint first");
    api.writeWords(words, "morph")
      .then((r) => say(`wrote ${r.bytes}B → ${r.path} (crown ${r.report.interior_crown_db.toFixed(1)} dB)`))
      .catch((er) => say(`REFUSED: ${er.message}`));
    return;
  }
  if (e.key === "n" && !e.ctrlKey) {
    e.preventDefault();
    commit("new");
    doc.lanes = emptyLanes();
    doc.roles = emptyRoles();
    doc.laws = doc.lanes.map(() => freeLaw());
    doc.field = Array.from({ length: 8 }, () => null);
    doc.words = null;
    doc.fieldWords = null;
    doc.target = null;
    doc.targetName = null;
    doc.peaks = null;
    doc.preview = null;
    doc.rms = null;
    doc.selectedCorner = 0;
    bindCorner(0);
    uniformWarned = false;
    say("new");
    paintCorners();
    pad.paint();
    paint();
    return;
  }
});

window.addEventListener("resize", paint);

start().catch((e) => say(`ERROR: ${e.message}`));
