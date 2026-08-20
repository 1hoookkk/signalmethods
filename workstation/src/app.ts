import { mountAnalyze } from "./analyze.js";
import type { BodyTarget, LibraryItem, StageSource, VowelRow } from "./api.js";
import { api } from "./api.js";
import { attachArmadillo, drawArmadillo } from "./armadillo.js";
import { setCorners, setRate, setReference, setSource } from "./audio.js";
import { createAudition } from "./audition.js";
import { displaySr, setDisplaySr, smoothOctave, stageCurves, sumCurve } from "./curves.js";
import type { Curve, Slot } from "./doc.js";
import {
  commit,
  currentTarget,
  doc,
  emptyLanes,
  freeLaw,
  IDENTITY_WORDS,
  isLocked,
  lawState,
  pinLaw,
  redo,
  toDrawGrid,
  undo,
} from "./doc.js";
import type { Lane } from "./dsp.js";
import { lanesToWords } from "./dsp.js";
import * as field from "./field.js";
import { createFit } from "./fit.js";
import { mountInspector } from "./inspector.js";
import { mountMorph } from "./morph.js";
import { attachResponse, drawResponse, NEW_SECTION_R } from "./response.js";
import { createSectionPicker } from "./section.picker.js";
import type { Space } from "./shell.js";
import { mountShell } from "./shell.js";
import type { ListGroup } from "./ui.js";
import { element, key, keys, list, panel } from "./ui.js";

// Seeded resonances land in S1..S5. S6 and S7 are where tilt and gain trim are handled,
// corroborated by the corpus. The slot is a position in the cascade, not a role.
const SEED_SLOTS = [1, 2, 3, 4];
const CORNER_HINT = ["M0 Q0", "M100 Q0", "M0 Q100", "M100 Q100"];

let ceiling = 0.9999;
let editVersion = 0;
let responseSeq = 0;
let space: Space = "AUTHOR";
let stageSources: StageSource[] = [];
const vowelRows = new Map<string, VowelRow>();

const shell = mountShell(document.getElementById("app") as HTMLElement, {
  onSpace: (next) => setSpace(next),
  onPlay: () => {
    audition.toggle();
    paint();
  },
  onFit: () => runFit(),
  onPalette: () => openPalette(),
});

const say = (text: string) => {
  shell.status.textContent = text;
};

function pushAudio() {
  const words = field.runtimeFieldWords();
  if (!words) return;
  doc.fieldWords = words;
  setCorners(words);
}

const audition = createAudition({ doc, say, pushAudio, paintSpectrum: () => paint() });

const fitter = createFit({
  say,
  paint: () => paint(),
  paintSpectrum: () => paint(),
  storeCorner: (lanes, words) => field.commitLanes(lanes, words),
  authoredWords: () => field.authoredWords(),
  pushAudio,
  clearCitation: (i) => {
    const slot = field.ensureCorner(doc.selectedCorner);
    if (slot.citations) slot.citations[i] = null;
  },
  isOff: (i) => field.isSectionOff(i),
  corner: () => doc.selectedCorner,
  generation: () => editVersion,
});

function assignTarget(curve: Curve | null, name: string | null) {
  const slot = field.ensureCorner(doc.selectedCorner);
  slot.target = curve ? Array.from(curve) : null;
  slot.targetName = name;
}

function liveResidual(target: Curve, sum: Curve): number | null {
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

function cornerResidual(slot: Slot | null): number | null {
  if (!slot?.target) return null;
  const words = field.effectiveWords(slot);
  return words ? liveResidual(slot.target, sumCurve(words)) : null;
}

async function requestResponse(lanes: Lane[]) {
  const seq = ++responseSeq;
  const r = await api.response(lanes);
  return seq === responseSeq ? r : null;
}

function reconcile() {
  const v = editVersion;
  requestResponse(doc.lanes)
    .then((r) => {
      if (!r || v !== editVersion) return;
      field.commitLanes(doc.lanes, r.words);
      paint();
    })
    .catch(() => {});
}

async function refreshResponse() {
  const r = await requestResponse(doc.lanes);
  if (r) field.commitLanes(r.lanes, r.words);
  if (doc.words) pushAudio();
  paint();
}

function editLane(i: number, patch: Partial<Lane>) {
  if (isLocked(i)) return say(`S${i + 1} HELD`);
  const lane = doc.lanes[i];
  for (const [name, value] of Object.entries(patch) as [keyof Lane, number][]) {
    if (Number.isFinite(value)) lane[name] = value;
  }
  if (lane.pole_r > ceiling) lane.pole_r = ceiling;
  const slot = field.ensureCorner(doc.selectedCorner);
  if (slot.citations) slot.citations[i] = null;
  field.commitLanes(doc.lanes);
  paint();
  pushAudio();
  editVersion++;
}

const afterFieldChange = () => {
  pushAudio();
  paint();
};

function selectCorner(i: number) {
  if (i === doc.selectedCorner) return;
  field.selectCorner(i);
  audition.ride(i & 1, (i >> 1) & 1, (i >> 2) & 1);
  say(`C${i}`);
  paint();
}

function clearCorner() {
  commit();
  doc.lanes = emptyLanes();
  doc.laws = doc.lanes.map(() => freeLaw());
  doc.words = null;
  doc.preview = null;
  field.commitLanes(doc.lanes);
  assignTarget(null, null);
  say(`C${doc.selectedCorner} CLEAR`);
  afterFieldChange();
}

function sectionVerb(verb: string) {
  const i = doc.selected;
  if (verb === "hold") {
    commit();
    doc.laws[i] = isLocked(i) ? freeLaw() : pinLaw();
    say(`S${i + 1} ${isLocked(i) ? "HELD" : "FREE"}`);
    return paint();
  }
  if (verb === "bypass") {
    commit();
    field.toggleSection(i);
    say(`S${i + 1} ${field.isSectionOff(i) ? "BYPASS" : "ON"}`);
    return afterFieldChange();
  }
  if (verb === "flip") {
    if (isLocked(i)) return say(`S${i + 1} HELD`);
    commit();
    field.flipSection(i);
    say(`S${i + 1} P↔Z`);
    return afterFieldChange();
  }
  if (verb === "clear") {
    commit();
    field.clearSection(i);
    say(`C${doc.selectedCorner} S${i + 1} CLEAR`);
    return afterFieldChange();
  }
}

// ── browser dock ─────────────────────────────────────────────────────────────

const cornersPanel = panel("CORNERS");
const cornerKeys: HTMLButtonElement[] = [];
const cornerGrid = element("div", "keys c2");
for (let i = 0; i < 4; i++) {
  const k = key("", () => selectCorner(i), "corner");
  const name = element("b", undefined, `C${i}`);
  const value = element("i", undefined, CORNER_HINT[i]);
  k.replaceChildren(name, value);
  cornerGrid.appendChild(k);
  cornerKeys.push(k);
}
cornersPanel.body.style.display = "block";
cornersPanel.body.append(
  cornerGrid,
  keys(
    2,
    key("COPY", () => {
      if (!doc.field[doc.selectedCorner]) return say("corner empty");
      const to = doc.selectedCorner ^ 1;
      commit();
      field.copyCorner(doc.selectedCorner, to);
      say(`C${doc.selectedCorner} → C${to}`);
      afterFieldChange();
    }),
    key("CLEAR", clearCorner),
  ),
);

const sourcesPanel = panel("SOURCES", { grow: true, scroll: true });
const sources = list();
sourcesPanel.body.appendChild(sources.root);

const bodyPanel = panel("BODY");
bodyPanel.body.style.display = "block";
bodyPanel.body.append(
  keys(
    2,
    key("NEW", () => {
      commit();
      field.reset();
      shell.title.textContent = "—";
      say("NEW");
      afterFieldChange();
    }),
    key("AUDIT", () => {
      const words = field.runtimeFieldWords();
      if (!words) return say("nothing seated");
      api
        .auditWords(words)
        .then((r) =>
          say(
            `AUDIT ${r.pass ? "PASS" : "FAIL"} ${r.crown_min_db.toFixed(1)}..${r.crown_max_db.toFixed(1)} dB`,
          ),
        )
        .catch((e: Error) => say(`ERROR ${e.message}`));
    }),
  ),
  keys(
    1,
    key("WRITE BODY", () => {
      const words = field.runtimeFieldWords();
      if (!words) return say("nothing seated");
      api
        .writeWords(words, "morph")
        .then((r) => say(`WRITE ${r.bytes} B`))
        .catch((e: Error) => say(`REFUSED ${e.message}`));
    }),
  ),
);

shell.browser.append(cornersPanel.root, sourcesPanel.root, bodyPanel.root);

// ── work dock ────────────────────────────────────────────────────────────────

const responsePanel = panel("RESPONSE", { grow: true });
responsePanel.body.style.background = "var(--well)";
const responseCanvas = element("canvas", "surface pointer");
responsePanel.body.appendChild(responseCanvas);

const armadilloPanel = panel("ARMADILLO");
armadilloPanel.root.style.flex = "0 0 var(--armadillo-h, 220px)";
armadilloPanel.body.style.background = "var(--well)";
const armadilloCanvas = element("canvas", "surface pointer");
armadilloPanel.body.appendChild(armadilloCanvas);
const armadilloGrip = element("div", "grip row");

const morph = mountMorph({
  onCorner: selectCorner,
  onRide: (m, q, z) => {
    audition.ride(m, q, z);
    paint();
  },
});

const analyze = mountAnalyze({
  say,
  onTarget: (curve, name) => {
    commit();
    assignTarget(curve, name);
    say(`C${doc.selectedCorner} ← ${name}`);
    setSpace("AUTHOR");
  },
});

// A row grip resizes the engineering surface against the comparator.
{
  let start = 0;
  let base = 0;
  armadilloGrip.addEventListener("pointerdown", (e) => {
    start = e.clientY;
    base = armadilloPanel.root.getBoundingClientRect().height;
    armadilloGrip.setPointerCapture(e.pointerId);
    e.preventDefault();
  });
  armadilloGrip.addEventListener("pointermove", (e) => {
    if (!armadilloGrip.hasPointerCapture(e.pointerId)) return;
    const next = Math.min(560, Math.max(90, base - (e.clientY - start)));
    armadilloPanel.root.style.flex = `0 0 ${next}px`;
    paint();
  });
}

let armadilloOpen = false;

// ── inspector dock ───────────────────────────────────────────────────────────

const inspector = mountInspector({
  onEdit: (patch) => editLane(doc.selected, patch),
  onCommit: () => reconcile(),
  onHold: () => sectionVerb("hold"),
  onBypass: () => sectionVerb("bypass"),
  onFlip: () => sectionVerb("flip"),
  onClear: () => sectionVerb("clear"),
  onSeat: (rect) => {
    if (!stageSources.length) return say("no factory sources");
    sectionPicker.open(doc.selected, rect);
  },
  onClearTarget: () => {
    commit();
    assignTarget(null, null);
    paint();
  },
  onClearReference: () => {
    doc.reference = null;
    paint();
  },
});

const analyzeControls = element("div");
analyzeControls.style.cssText = "display:none;flex-direction:column;min-height:0;flex:1";
analyzeControls.appendChild(analyze.controls);
shell.inspector.append(inspector.root, analyzeControls);

// ── footer ───────────────────────────────────────────────────────────────────

const chain = element("div", "chain");
const sectionKeys: HTMLButtonElement[] = [];
chain.appendChild(element("span", "end", "IN"));
for (let i = 0; i < 7; i++) {
  if (i) chain.appendChild(element("span", "arrow", "▸"));
  const k = key(
    `S${i + 1}`,
    () => {
      doc.selected = i;
      paint();
    },
    "section",
  );
  k.style.color = `var(--s${i + 1})`;
  k.draggable = true;
  k.ondragstart = (e) => e.dataTransfer?.setData("text/stage", String(i));
  k.ondragover = (e) => e.preventDefault();
  k.ondrop = (e) => {
    e.preventDefault();
    const from = Number(e.dataTransfer?.getData("text/stage"));
    if (!Number.isInteger(from) || from === i) return;
    commit();
    field.swapSections(from, i);
    say(`S${from + 1} ↔ S${i + 1}`);
    afterFieldChange();
  };
  chain.appendChild(k);
  sectionKeys.push(k);
}
chain.appendChild(element("span", "end", "OUT"));

const positionRead = element("span", "pos");
shell.foot.append(chain, positionRead, shell.status);

// ── palette ──────────────────────────────────────────────────────────────────

const palettePanel = document.getElementById("palette") as HTMLElement;
const paletteInput = document.getElementById("palette-input") as HTMLInputElement;
const paletteList = document.getElementById("palette-list") as HTMLElement;
type Entry = { label: string; tag: string; run: () => void };
let paletteEntries: Entry[] = [];
let paletteShown: Entry[] = [];
let paletteCursor = 0;

function renderPalette() {
  const needle = paletteInput.value.trim().toLowerCase();
  paletteShown = paletteEntries.filter((e) => !needle || e.label.toLowerCase().includes(needle)).slice(0, 80);
  paletteCursor = Math.min(paletteCursor, Math.max(0, paletteShown.length - 1));
  paletteList.replaceChildren();
  paletteShown.forEach((entry, i) => {
    const row = element("div", i === paletteCursor ? "item on" : "item");
    row.append(element("span", undefined, entry.label), element("span", "tag", entry.tag));
    row.onclick = () => {
      closePalette();
      entry.run();
    };
    paletteList.appendChild(row);
  });
}

function openPalette() {
  palettePanel.hidden = false;
  paletteInput.value = "";
  paletteCursor = 0;
  renderPalette();
  paletteInput.focus();
}

const closePalette = () => {
  palettePanel.hidden = true;
  paletteInput.blur();
};

paletteInput.oninput = () => {
  paletteCursor = 0;
  renderPalette();
};

paletteInput.onkeydown = (e) => {
  e.stopPropagation();
  if (e.key === "Escape") return closePalette();
  if (e.key === "ArrowDown") {
    paletteCursor = Math.min(paletteCursor + 1, paletteShown.length - 1);
    return renderPalette();
  }
  if (e.key === "ArrowUp") {
    paletteCursor = Math.max(paletteCursor - 1, 0);
    return renderPalette();
  }
  if (e.key === "Enter" && paletteShown[paletteCursor]) {
    const entry = paletteShown[paletteCursor];
    closePalette();
    entry.run();
  }
};

// ── section transplant ───────────────────────────────────────────────────────

const sectionPicker = createSectionPicker(document.body, {
  corner: () => doc.selectedCorner,
  onDetail: say,
  onSeat: (grain, source, stage, corner, destination) => {
    const seat =
      grain === "track"
        ? field.seatTrack(source, stage, destination)
        : field.seatCell(source, stage, corner, destination);
    seat
      .then(() => {
        say(`S${destination + 1}`);
        afterFieldChange();
      })
      .catch((e: Error) => say(`ERROR ${e.message}`));
  },
  onSeatState: (state, destination) => {
    field
      .seatState(state, destination)
      .then(() => {
        say(`S${destination + 1}`);
        afterFieldChange();
      })
      .catch((e: Error) => say(`ERROR ${e.message}`));
  },
});

// ── render ───────────────────────────────────────────────────────────────────

function interiorPreview(): Float32Array | null {
  if (!doc.preview) return null;
  const { m, q } = audition.ridePos;
  const onCorner = (v: number) => v < 0.001 || v > 0.999;
  return onCorner(m) && onCorner(q) ? null : doc.preview;
}

function selectedSection(): Float32Array | null {
  if (!doc.words) return null;
  const curve = stageCurves(doc.words)[doc.selected];
  if (!curve) return null;
  for (let i = 0; i < curve.length; i++) if (Math.abs(curve[i]) > 0.05) return curve;
  return null;
}

function responseView(withNodes: boolean) {
  const target = currentTarget();
  const sum = doc.words ? sumCurve(doc.words) : null;
  const swap = fitter.isFitting() && doc.candidate;
  return {
    target,
    sum: swap ? doc.candidate : sum,
    reference: doc.reference,
    preview: interiorPreview(),
    candidate: swap ? sum : doc.candidate,
    section: withNodes ? selectedSection() : null,
    lanes: withNodes ? doc.lanes : null,
    selected: doc.selected,
    rms: target && sum ? liveResidual(target, sum) : null,
  };
}

let paintQueued = false;

// The single repaint path. Every visible surface is redrawn from the same authored state
// on the same frame, so no view can be left showing something stale.
function paint() {
  if (paintQueued) return;
  paintQueued = true;
  requestAnimationFrame(() => {
    paintQueued = false;
    const { m, q, z } = audition.ridePos;
    const target = currentTarget();
    const sum = doc.words ? sumCurve(doc.words) : null;
    const residual = target && sum ? liveResidual(target, sum) : null;

    shell.setPlaying(audition.isPlaying());
    shell.setFitEnabled(!!target);
    positionRead.innerHTML = `M <b>${m.toFixed(2)}</b>  Q <b>${q.toFixed(2)}</b>  Z <b>${z.toFixed(2)}</b>`;

    for (let i = 0; i < 4; i++) {
      const slot = doc.field[i];
      const delta = cornerResidual(slot);
      cornerKeys[i].classList.toggle("on", i === doc.selectedCorner);
      cornerKeys[i].classList.toggle("empty", !slot);
      (cornerKeys[i].lastChild as HTMLElement).textContent =
        delta === null ? CORNER_HINT[i] : `Δ ${delta.toFixed(2)} dB`;
    }

    const slot = doc.field[doc.selectedCorner];
    const authored = slot?.words ?? doc.words;
    for (let i = 0; i < 7; i++) {
      const w = authored?.[i];
      const unused = w
        ? w.every((v, k) => v === IDENTITY_WORDS[k])
        : doc.lanes[i].pole_r <= 0 && doc.lanes[i].zero_r <= 0;
      sectionKeys[i].classList.toggle("on", i === doc.selected);
      sectionKeys[i].classList.toggle("unused", unused);
      sectionKeys[i].classList.toggle("held", lawState(doc.laws[i]) !== "FREE");
      sectionKeys[i].classList.toggle("bypassed", !!slot?.off?.[i]);
    }

    if (space === "AUTHOR") {
      drawResponse(responseCanvas, responseView(true));
      responsePanel.value.textContent = residual === null ? "NO TARGET" : `${residual.toFixed(2)} dB RMS`;
    } else if (space === "MORPH") {
      drawResponse(morph.responseCanvas, responseView(false));
      morph.paint(doc, m, q, z, cornerResidual);
    } else {
      analyze.paint();
    }
    inspector.paint(doc, residual);
    if (armadilloOpen) drawArmadillo(armadilloCanvas, doc, ceiling);
  });
}

shell.onResize(paint);

function setSpace(next: Space) {
  space = next;
  shell.work.replaceChildren();
  if (next === "AUTHOR") {
    shell.work.append(responsePanel.root);
    if (armadilloOpen) shell.work.append(armadilloGrip, armadilloPanel.root);
  } else if (next === "MORPH") {
    shell.work.append(morph.fieldPanel, morph.responsePanel);
  } else {
    shell.work.append(analyze.work);
  }
  inspector.root.style.display = next === "ANALYZE" ? "none" : "flex";
  analyzeControls.style.display = next === "ANALYZE" ? "flex" : "none";
  shell.setSpace(next);
  paint();
}

// ── surfaces ─────────────────────────────────────────────────────────────────

attachResponse(responseCanvas, doc, {
  isLocked,
  ceiling: () => ceiling,
  sum: () => (doc.words ? sumCurve(doc.words) : null),
  onLocked: (i) => say(`S${i + 1} HELD`),
  onGrab: (i) => {
    commit();
    doc.selected = i;
  },
  onDrag: editLane,
  onRelease: (i) => {
    field.commitLanes(doc.lanes, lanesToWords(doc.lanes, displaySr()));
    pushAudio();
    paint();
    say(`S${i + 1} ${doc.lanes[i].pole_hz.toFixed(0)} Hz`);
    reconcile();
  },
  // Placing a section allocates a free one at exact identity: pole and zero coincide and
  // cancel, so it changes nothing you can hear. The drag opens it from there.
  onCreate: (hz) => {
    const free = doc.lanes.findIndex((l) => l.pole_r <= 0 && l.zero_r <= 0);
    if (free < 0) return say("all sections in use");
    commit();
    const r = Math.min(NEW_SECTION_R, ceiling);
    doc.lanes[free] = { pole_hz: hz, pole_r: r, zero_hz: hz, zero_r: r, scale: 1 };
    doc.selected = free;
    field.commitLanes(doc.lanes);
    say(`S${free + 1} ${hz.toFixed(0)} Hz`);
    afterFieldChange();
    reconcile();
  },
  onClearSection: () => sectionVerb("clear"),
});

attachArmadillo(armadilloCanvas, doc, {
  isLocked,
  ceiling: () => ceiling,
  onLocked: (i) => say(`S${i + 1} HELD`),
  onRefused: (i) => say(`S${i + 1} real pair`),
  onGrab: (hit) => {
    commit();
    doc.selected = hit.lane;
  },
  onMove: (hit) => {
    const slot = field.ensureCorner(doc.selectedCorner);
    if (slot.citations) slot.citations[hit.lane] = null;
    field.commitLanes(doc.lanes);
    paint();
    pushAudio();
    editVersion++;
  },
  onRelease: (hit) => {
    field.commitLanes(doc.lanes, lanesToWords(doc.lanes, displaySr()));
    pushAudio();
    paint();
    say(`S${hit.lane + 1} ${hit.kind}`);
    reconcile();
  },
});

// ── sources ──────────────────────────────────────────────────────────────────

function loadBody(r: BodyTarget) {
  doc.field = Array.from({ length: 8 }, (_, i) => {
    const lanes = structuredClone(r.corners[i]);
    return {
      name: `${r.name} c${i}`,
      lanes,
      words: r.words ? r.words[i] : lanesToWords(lanes, displaySr()),
      laws: lanes.map(() => freeLaw()),
      citations: Array.from({ length: 7 }, () => null),
    };
  });
  doc.bodyName = r.name;
  doc.preview = null;
  shell.title.textContent = r.name;
  field.selectCorner(0, false);
  say(`${r.corners.length} corners`);
  afterFieldChange();
}

async function pick(kind: string, item: LibraryItem) {
  if (kind === "mouths") {
    setReference(null);
    setSource("synth");
    const t = await api.target("mouth", item.id);
    commit();
    assignTarget(smoothOctave(t.curve), t.name);
    say(t.name);
    return paint();
  }
  if (kind === "recordings") {
    setReference(`/api/audio?id=${item.id}`);
    setSpace("ANALYZE");
    return analyze.load(item.id, item.name ?? item.id);
  }
  if (kind === "poses") {
    const r = await api.target("pose", item.id);
    commit();
    const seated = r.lanes.filter((l) => l.pole_r > 0).slice(0, SEED_SLOTS.length);
    const lanes = emptyLanes();
    seated.forEach((lane, k) => {
      lanes[SEED_SLOTS[k]] = lane;
    });
    field.applyLanes(
      lanes,
      lanes.map((l) => (l.pole_r > 0 ? pinLaw() : freeLaw())),
    );
    say(`SEED ${seated.length}`);
    return refreshResponse();
  }
  if (kind === "bodies") {
    const r = await api.target("body", item.id);
    commit();
    return loadBody(r);
  }
  if (kind === "templates" || kind === "factory") {
    const source = stageSources.find((s) => s.id === item.id);
    const { corners } = await field.importFactory(
      item,
      source ? source.stage_count : 6,
      source ? source.corners : 4,
    );
    shell.title.textContent = doc.bodyName ?? "—";
    say(`${corners} corners`);
    return afterFieldChange();
  }
  if (kind === "vowel") {
    const row = vowelRows.get(item.id);
    if (!row) return say("no reference");
    const formants = [
      [row.f1, row.b1],
      [row.f2, row.b2],
      [row.f3, row.b3],
      [row.f4, row.b4],
      [row.f5, row.b5],
    ]
      .filter(([f, b]) => typeof f === "number" && typeof b === "number")
      .map(([f, b]) => ({ hz: f as number, bandwidth_hz: b as number }));
    doc.reference = { name: item.name ?? item.id, formants };
    say(`${item.name} ${formants.map((f) => Math.round(f.hz)).join(" ")} Hz`);
    return paint();
  }
  if (kind === "architectures") {
    const b = await api.brief(item.id);
    return say(`${b.name} — ${b.intent}`);
  }
  say(`${kind} —`);
}

const runFit = () => fitter.run().catch((e: Error) => say(`ERROR ${e.message}`));

// ── keys ─────────────────────────────────────────────────────────────────────

window.addEventListener("keydown", (e) => {
  if ((e.target as HTMLElement)?.tagName === "INPUT") return;
  if (e.key === "Escape") {
    if (!palettePanel.hidden) return closePalette();
    if (fitter.cancel()) return;
  }
  if (e.key === "/") {
    e.preventDefault();
    return openPalette();
  }
  if (e.key === "a" && !e.ctrlKey) {
    armadilloOpen = !armadilloOpen;
    if (space === "AUTHOR") setSpace("AUTHOR");
    return;
  }
  if (e.code === "Space" && !e.ctrlKey) {
    e.preventDefault();
    audition.toggle();
    return paint();
  }
  if (e.ctrlKey && e.key === "z" && !e.shiftKey) {
    if (undo()) {
      field.selectCorner(doc.selectedCorner);
      say("UNDO");
      refreshResponse().catch(() => paint());
    }
    return;
  }
  if (e.ctrlKey && (e.key === "Z" || e.key === "y")) {
    if (redo()) {
      field.selectCorner(doc.selectedCorner);
      say("REDO");
      refreshResponse().catch(() => paint());
    }
    return;
  }
  if (e.key === "Delete") return sectionVerb("clear");
  if (e.key === "f" && !e.ctrlKey) {
    e.preventDefault();
    return runFit();
  }
  if (e.key >= "1" && e.key <= "7") {
    doc.selected = Number(e.key) - 1;
    paint();
  }
});

// ── start ────────────────────────────────────────────────────────────────────

async function start() {
  const lib = await api.library();
  const vowels: LibraryItem[] = [];
  try {
    const table = await api.vowelTable();
    for (const [id, row] of Object.entries(table.vowels)) {
      vowelRows.set(id, row);
      vowels.push({ id, name: `${row.ipa} ${id.split("_")[1] ?? id}` });
    }
  } catch {}
  setDisplaySr(lib.authoring_sr);
  setRate(lib.authoring_sr);
  ceiling = lib.pole_ceiling_r || ceiling;
  stageSources = lib.stage_sources ?? [];
  sectionPicker.setSources(stageSources);
  sectionPicker.setStates(lib.vocabulary?.states);

  const run = (kind: string, item: LibraryItem) => () => {
    pick(kind, item).catch((e: Error) => say(`ERROR ${e.message}`));
  };
  const groups: ListGroup[] = [];
  const addGroup = (title: string, kind: string, items: LibraryItem[]) => {
    if (!items.length) return;
    groups.push({
      title,
      entries: items.map((item) => ({
        id: `${kind}/${item.id}`,
        label: item.name ?? item.id,
        tag: item.gloss || item.id,
        onPick: run(kind, item),
      })),
    });
  };
  addGroup("BODIES", "bodies", lib.bodies ?? []);
  addGroup("MOUTHS", "mouths", lib.mouths ?? []);
  addGroup("RECORDINGS", "recordings", lib.recordings ?? []);
  addGroup("POSES", "poses", lib.poses ?? []);
  addGroup("VOWELS", "vowel", vowels);
  addGroup(
    "RUNTIME",
    "factory",
    (lib.stage_sources ?? []).filter((s) => s.family === "x3").map((s) => ({ id: s.id, name: s.name })),
  );
  addGroup(
    "FORMS",
    "factory",
    (lib.stage_sources ?? []).filter((s) => s.family === "md").map((s) => ({ id: s.id, name: s.name })),
  );
  addGroup("FACTORY", "architectures", lib.architectures ?? []);
  sources.render(groups);

  paletteEntries = [
    { label: "FIT CORNER", tag: "COMMAND", run: runFit },
    { label: "CLEAR CORNER", tag: "COMMAND", run: clearCorner },
    { label: "SEAT FACTORY SECTION", tag: "COMMAND", run: () => sectionPicker.open(doc.selected, null) },
    ...groups.flatMap((g) =>
      g.entries.map((entry) => ({ label: entry.label, tag: g.title, run: entry.onPick })),
    ),
  ];

  setSpace("AUTHOR");
  say("/ to find anything · A for ARMADILLO");
  const id = new URLSearchParams(location.search).get("body");
  if (id) {
    const item = (lib.bodies || []).find((b) => b.id === id || b.name === id);
    if (item) await pick("bodies", item);
  }
}

start().catch((e: Error) => say(`ERROR ${e.message}`));
