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
  DRAW_GRID,
  toDrawGrid,
} from "./doc.js";
import { renderCorners, NAMES as CORNER_NAMES } from "./corners.js";
import { drawSpectrum } from "./spectrum.js";
import { renderStages } from "./stages.js";
import { drawRoots, hitRoot, dragTo } from "./roots.js";
import { initPad } from "./pad.js";
import { initFitRoom } from "./fitroom.js";
import { FORMANT_SLOTS, clampZeroHz } from "./fit.roles.js";
import { setCorners, setRide, setPlay, setBlend, setFreq, setRate, outputRms, audioRate, audioState } from "./audio.js";
import { interpolateWords, wordsToBiquads, sumDb } from "./dsp.js";
import { curveInto, sumCurve, stageCurves, displaySr, setDisplaySr } from "./curves.js";
import { openWire, wireReady, wordsOf } from "./wire.js";

const verdict = document.getElementById("verdict-text");
const spectrumDock = document.getElementById("spectrum");
const stagesDock = document.getElementById("stages");

const openButton = document.createElement("button");
openButton.textContent = "FIT";
openButton.style.cssText =
  "position:absolute;right:6px;top:2px;z-index:1;background:var(--chrome);border:2px solid;border-color:var(--chrome-hi) var(--chrome-lo) var(--chrome-lo) var(--chrome-hi);font:inherit;padding:0 12px;cursor:pointer";
spectrumDock.style.position = "relative";
spectrumDock.appendChild(openButton);
openButton.onclick = () => fitRoom.open();

const fitRoom = initFitRoom(document.getElementById("fitroom"), {
  onAutoPin: () => placeSkeleton().catch((e) => say(`ERROR: ${e.message}`)),
  onFit: () => runFit().catch((e) => say(`ERROR: ${e.message}`)),
  onCommit: (i) => {
    activeCorner = i;
    commitToCorner();
  },
  onSelect: (i) => {
    doc.selected = i;
    paint();
  },
  onLock: (i) => {
    commit(`lock S${i + 1}`);
    doc.laws[i] = isLocked(i) ? freeLaw() : pinLaw(doc.lanes[i]);
    say(`S${i + 1} ${isLocked(i) ? "locked" : "unlocked"}`);
    paint();
  },
  onPreset: (key) => {
    commit(`locks ${key}`);
    for (let i = 0; i < 7; i++) {
      const formant = !!doc.roles[i];
      const keep =
        key === "all" ? true : key === "formants" ? formant : i !== 5 && i !== 6;
      doc.laws[i] = keep && doc.lanes[i].pole_r > 0 ? pinLaw(doc.lanes[i]) : freeLaw();
    }
    say(
      key === "all"
        ? "all seated sections locked"
        : key === "formants"
          ? "formant sections locked"
          : "S6 and S7 free, the rest locked"
    );
    paint();
  },
  onEdit: (i, patch) => editLane(i, patch),
  onEditEnd: () => {
    refreshResponse().catch((e) => say(`ERROR: ${e.message}`));
  },
  onScaffold: () => paint(),
  onPlaceZero: (hz) => placeZero(hz),
  onBound: () => boundAndNormalize(),
  onPaint: () => paint(),
});

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
let activeCorner = 0;
const BOUND_INTERVAL_ST = 2;

const pad = initPad(document.getElementById("pad"), {
  onRide: (m, q, z) => {
    setRide(m, q, z);
    ridePreview(m, q, z);
  },
  onHold: hold,
  onBlend: (b) => setBlend(b),
  onFreq: (f) => setFreq(f),
  getCube: () => ({
    filled: doc.field.map((s) => !!s),
    active: activeCorner,
    names: doc.field.map((s) => (s ? s.name : "")),
  }),
});

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

const ridePos = { m: 0, q: 0, z: 0 };
let previewPending = false;
let previewBuffer = null;

function ridePreview(m, q, z) {
  ridePos.m = m;
  ridePos.q = q;
  ridePos.z = z;
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

function refreshCornerStages() {
  const a = doc.field[0];
  const b = doc.field[1];
  doc.cornerWords = a && b && a.words && b.words ? { a: a.words, b: b.words } : null;
}

function fieldWords() {
  if (doc.field.every((s) => s && s.words)) return doc.field.map((s) => s.words);
  const square =
    doc.field.slice(0, 4).every((s) => s && s.words) && doc.field.slice(4).every((s) => !s);
  if (square) return doc.field.map((s, i) => (s ? s.words : doc.field[i - 4].words));
  return null;
}

function pushAudio() {
  const words = fieldWords();
  const f = words ? null : fieldCorners();
  if (words) {
    doc.fieldWords = words;
    setCorners(words);
  } else if (f) {
    api
      .corners(f.corners)
      .then((r) => {
        doc.fieldWords = r.words;
        setCorners(r.words);
      })
      .catch(() => {});
  } else if (doc.words) {
    doc.fieldWords = Array.from({ length: 8 }, () => doc.words);
    setCorners(doc.fieldWords);
  }
}

function commitToCorner() {
  commit(`set corner ${activeCorner}`);
  doc.field[activeCorner] = {
    name: doc.targetName || `corner ${activeCorner}`,
    lanes: structuredClone(doc.lanes),
    words: doc.words,
  };
  say(`C${activeCorner} ${CORNER_NAMES[activeCorner]} ← ${doc.field[activeCorner].name}`);
  pushAudio();
  paintCorners();
  pad.paint();
  refreshCornerStages();
  paint();
}

function paintCorners() {
  renderCorners(document.getElementById("corners-body"), doc.field, activeCorner, {
    onSelect: (i) => {
      activeCorner = i;
      paintCorners();
      pad.paint();
    },
    onSet: commitToCorner,
    onGet: () => {
      if (!doc.field[activeCorner]) return say("corner is empty");
      commit(`get corner ${activeCorner}`);
      doc.lanes = structuredClone(doc.field[activeCorner].lanes);
      refreshResponse().catch((e) => say(`ERROR: ${e.message}`));
    },
    onKeep: () => {
      api
        .writeFrame(doc.lanes, doc.laws, doc.targetName || "workstation")
        .then((r) => say(`kept ${r.name} → ${r.path}`))
        .catch((e) => say(`ERROR: ${e.message}`));
    },
    onAudit: () => {
      const f = fieldCorners();
      if (!f) return say("field incomplete — fill corners 0-3 (square) or all 8 (cube)");
      api
        .audit(f.corners)
        .then((r) =>
          say(
            `audit ${r.pass ? "PASS" : "FAIL"} — crown ${r.crown_min_db.toFixed(1)}..${r.crown_max_db.toFixed(1)} dB, parity ${r.parity_db.toFixed(1)}, interior ${r.interior_crown_db.toFixed(1)}${r.pass ? "" : " — " + r.failures.join("; ")}`
          )
        )
        .catch((e) => say(`ERROR: ${e.message}`));
    },
    onWrite: () => {
      const f = fieldCorners();
      if (!f) return say("field incomplete — fill corners 0-3 (square) or all 8 (cube)");
      api
        .writeBody(f.corners, f.square ? "square" : "cube")
        .then((r) => say(`wrote ${r.bytes}B body → ${r.path} (interior crown ${r.report.interior_crown_db.toFixed(1)} dB)`))
        .catch((e) => say(`REFUSED: ${e.message}`));
    },
  });
}

function say(text) {
  verdict.textContent = text;
  fitRoom.setStatus(text);
}

function isLocked(i) {
  return lawState(doc.laws[i]) !== "FREE";
}

let editTimer = 0;

function editLane(i, patch) {
  const lane = doc.lanes[i];
  doc.rmsStale = true;
  for (const key of Object.keys(patch)) {
    if (Number.isFinite(patch[key])) lane[key] = patch[key];
  }
  if (lane.pole_r > ceiling) lane.pole_r = ceiling;
  paint();
  const now = performance.now();
  if (now - editTimer < 120) return;
  editTimer = now;
  api
    .response(doc.lanes)
    .then((r) => {
      doc.words = r.words;
      paint();
    })
    .catch(() => {});
}

function zeroHeld(i) {
  return doc.laws[i].freedom[2] === false;
}

function placeZero(hz) {
  const free = [];
  for (let i = 0; i < 7; i++) {
    if (doc.lanes[i].zero_r > 0) continue;
    free.push(i);
  }
  if (!free.length) {
    say("every section already carries a zero");
    return null;
  }
  const lane = free.find((i) => !doc.roles[i]) ?? free[0];
  commit(`zero S${lane + 1}`);
  doc.lanes[lane].zero_hz = clampZeroHz(hz, lane, doc.lanes, doc.roles);
  doc.lanes[lane].zero_r = 0.9;
  doc.laws[lane] = { ...doc.laws[lane], freedom: [doc.laws[lane].freedom[0], true, false, true] };
  doc.selected = lane;
  say(`S${lane + 1} zero at ${doc.lanes[lane].zero_hz.toFixed(0)} Hz, held`);
  editLane(lane, {});
  return lane;
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
  let r = await api.response(doc.lanes);
  doc.lanes = r.lanes;
  doc.words = r.words;
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
  r = await api.response(doc.lanes);
  doc.lanes = r.lanes;
  doc.words = r.words;
  pushAudio();
  say(`${bound} bound pairs, level spread over ${active.length} sections`);
  paint();
}

function paintSpectrum() {
  canvas.width = canvas.clientWidth;
  canvas.height = canvas.clientHeight;
  const view = {
    name: doc.targetName,
    target: doc.target,
    sum: doc.words ? sumCurve(doc.words) : null,
    peaks: doc.peaks,
    guides: guidesOf(),
    preview: doc.preview,
    candidate: doc.candidate,
    lanes: doc.lanes,
    roles: doc.roles,
    zeroHeld: doc.laws.map((_, i) => zeroHeld(i)),
    locks: doc.laws.map((_, i) => isLocked(i)),
    selected: doc.selected,
    field: doc.field,
    active: activeCorner,
    packing: doc.packing,
    rms: doc.rms,
    rmsStale: doc.rmsStale,
    ceiling,
  };
  drawSpectrum(canvas, view);
  fitRoom.paint(view);
}

function paint() {
  paintSpectrum();
  rootsCanvas.width = rootsCanvas.clientWidth;
  rootsCanvas.height = rootsCanvas.clientHeight;
  drawRoots(rootsCanvas, doc, ceiling);
  renderStages(
    stagesBody,
    doc,
    (i) => {
      doc.selected = i;
      paint();
    },
    (i) => {
      commit(`lock S${i + 1}`);
      doc.laws[i] = isLocked(i) ? freeLaw() : pinLaw(doc.lanes[i]);
      say(`S${i + 1} ${isLocked(i) ? "locked" : "unlocked"}`);
      paint();
    }
  );
}

async function refreshResponse() {
  const r = await api.response(doc.lanes);
  doc.lanes = r.lanes;
  doc.words = r.words;
  pushAudio();
  paint();
}

let drag = null;
let dragTimer = 0;
let wirePending = false;

rootsCanvas.addEventListener("pointerdown", (e) => {
  const rect = rootsCanvas.getBoundingClientRect();
  const hit = hitRoot(rootsCanvas, doc, e.clientX - rect.left, e.clientY - rect.top);
  if (!hit) return;
  commit(`drag ${hit.kind} S${hit.lane + 1}`);
  drag = hit;
  rootsCanvas.setPointerCapture(e.pointerId);
});

rootsCanvas.addEventListener("pointermove", (e) => {
  if (!drag) return;
  const rect = rootsCanvas.getBoundingClientRect();
  dragTo(rootsCanvas, doc, drag, e.clientX - rect.left, e.clientY - rect.top, ceiling);
  drawRoots(rootsCanvas, doc, ceiling);
  if (wireReady()) {
    if (!wirePending) {
      wirePending = true;
      wordsOf(doc.lanes).then((words) => {
        wirePending = false;
        doc.words = words;
        paint();
      });
    }
    return;
  }
  const now = performance.now();
  if (now - dragTimer > 150) {
    dragTimer = now;
    api.response(doc.lanes).then((r) => {
      doc.words = r.words;
      paint();
    }).catch(() => {});
  }
});

rootsCanvas.addEventListener("pointerup", () => {
  if (!drag) return;
  const d = drag;
  drag = null;
  refreshResponse()
    .then(() => say(`S${d.lane + 1} ${d.kind}: ${fmtLane(doc.lanes[d.lane], d.kind)}`))
    .catch((e) => say(`ERROR: ${e.message}`));
});

function fmtLane(lane, kind) {
  const hz = kind === "pole" ? lane.pole_hz : lane.zero_hz;
  const r = kind === "pole" ? lane.pole_r : lane.zero_r;
  return `${hz.toFixed(1)} Hz  r ${r.toFixed(4)} (snapped)`;
}

async function setTarget(item) {
  const t = await api.target("mouth", item.id);
  commit(`target ${t.name}`);
  doc.targetName = t.name;
  doc.target = t.curve;
  doc.peaks = t.peaks;
  say(`target: ${t.name} — ${t.peaks.length} measured formants`);
  paint();
}


function guidesOf() {
  const g = [];
  for (let i = 0; i < 7; i++) {
    if (doc.roles[i] && doc.lanes[i].pole_r > 0) {
      g.push({ hz: doc.lanes[i].pole_hz, label: doc.roles[i] });
    }
  }
  return g;
}

async function placeSkeleton() {
  if (!doc.target) return say("no target loaded");
  if (!fitRoom.isOpen()) fitRoom.open();
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
  doc.selected = FORMANT_SLOTS[0];
  say(`${seated.length} measured formants placed and locked — ${doc.targetName}`);
  await refreshResponse();
}

let candidatePending = false;
let candidateCount = 0;

function showCandidate(candidate) {
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
  say("solving…");
  candidateCount = 0;
  const order = solverOrder();
  try {
    const r = await api.fitStream(
      doc.target,
      order.map((i) => doc.lanes[i]),
      order.map((i) => doc.laws[i]),
      false,
      showCandidate
    );
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
    doc.lanes = lanes;
    doc.words = words;
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
    doc.candidate = null;
  }
}

const briefPane = document.createElement("div");
briefPane.style.cssText =
  "max-height:30%;overflow-y:auto;background:var(--well);color:var(--trace-ghost);font-size:10px;padding:4px 8px;white-space:pre-wrap;display:none;border-top:1px solid var(--grat-major)";
document.getElementById("library").appendChild(briefPane);

async function pick(kind, item, right) {
  if (kind === "mouths") {
    await setTarget(item);
    if (right) await placeSkeleton();
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
    say(`${r.name}: ${seated.length} formant poles placed and locked`);
    await refreshResponse();
    return;
  }
  if (kind === "frames") {
    const r = await api.frame(item.id);
    commit(`frame ${r.name}`);
    doc.targetName = r.name;
    doc.lanes = r.lanes;
    doc.laws = r.laws;
    doc.roles = emptyRoles();
    say(`frame ${r.name} loaded — ${r.provenance}`);
    await refreshResponse();
    return;
  }
  if (kind === "bodies") {
    const r = await api.target("body", item.id);
    commit(`body ${r.name}`);
    for (let i = 0; i < 8; i++) {
      doc.field[i] = { name: `${r.name} c${i}`, lanes: r.corners[i], words: r.words && r.words[i] };
    }
    doc.lanes = structuredClone(r.corners[0]);
    doc.roles = emptyRoles();
    say(
      `body ${r.name} → 8 corners loaded${r.real_pair_sections_skipped ? ` (${r.real_pair_sections_skipped} real-pair sections the lanes cannot hold — plots and ride follow the body's own words)` : ""}`
    );
    paintCorners();
    await refreshResponse();
    if (r.words) doc.words = r.words[0];
    refreshCornerStages();
    pushAudio();
    paint();
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
  renderLibrary(document.getElementById("library-list"), lib, (kind, item, right) => {
    pick(kind, item, right).catch((e) => say(`ERROR: ${e.message}`));
  });
  ceiling = lib.pole_ceiling_r || ceiling;
  say(`library loaded — root ${lib.root}`);
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
  if (e.ctrlKey && e.key === "z" && !e.shiftKey) {
    if (undo()) { say("undo"); paintCorners(); refreshResponse().catch(() => paint()); }
  } else if (e.ctrlKey && (e.key === "Z" || e.key === "y")) {
    if (redo()) { say("redo"); paintCorners(); refreshResponse().catch(() => paint()); }
  }
});

window.addEventListener("resize", paint);

start().catch((e) => say(`ERROR: ${e.message}`));
