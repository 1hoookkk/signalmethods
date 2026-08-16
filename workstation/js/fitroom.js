import { xOf, yOf, trace } from "./spectrum.js";
import { roleOf } from "./fit.roles.js";
import { thumbwheel, valueBox, button, panel } from "./view.chrome.js";

const DB_LO = -60;
const DB_HI = 45;
const CROWN = 36;
const RP_MAX = 60;
const FIELDS = [
  ["pole_hz", "POLE", "Hz"],
  ["pole_r", "POLE", "R"],
  ["zero_hz", "ZERO", "Hz"],
  ["zero_r", "ZERO", "R"],
  ["scale", "LEVEL", "dB"],
];

function css(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

function rPrime(r) {
  if (r >= 1) return RP_MAX;
  if (r <= 0) return 0;
  return Math.min(RP_MAX, 20 * Math.log10(1 / (1 - r)));
}

function rOf(rp) {
  return 1 - Math.pow(10, -rp / 20);
}

function hzOfX(x, w) {
  return 40 * Math.pow(16000 / 40, Math.min(1, Math.max(0, x / w)));
}

function dbOfY(y, h) {
  return DB_LO + (1 - y / h) * (DB_HI - DB_LO);
}

function sampleAt(curve, hz) {
  if (!curve) return 0;
  const t = Math.log(hz / 40) / Math.log(16000 / 40);
  const i = Math.round(Math.min(1, Math.max(0, t)) * (curve.length - 1));
  return curve[i];
}

function residualOf(target, sum) {
  if (!target || !sum) return null;
  const span = target.length - 1;
  const out = new Float32Array(sum.length);
  let mean = 0;
  for (let i = 0; i < sum.length; i++) {
    out[i] = target[Math.round((i * span) / (sum.length - 1))] - sum[i];
    mean += out[i];
  }
  mean /= sum.length;
  let power = 0;
  for (let i = 0; i < out.length; i++) {
    out[i] -= mean;
    power += out[i] * out[i];
  }
  return { curve: out, rms: Math.sqrt(power / out.length) };
}

export function initFitRoom(el, hooks) {
  const titlebar = el.querySelector(".dock-title");
  const title = document.createElement("span");
  title.className = "sgi-title";
  const close = document.createElement("button");
  close.className = "sgi-box";
  close.appendChild(document.createElement("span"));
  close.onclick = () => api.close();
  titlebar.textContent = "";
  titlebar.appendChild(title);
  titlebar.appendChild(close);

  const body = document.createElement("div");
  body.className = "sgi-body";
  el.appendChild(body);

  const bays = document.createElement("div");
  bays.className = "sgi-bays";
  body.appendChild(bays);

  const status = document.createElement("div");
  status.className = "sgi-sunken sgi-status";
  body.appendChild(status);

  const left = document.createElement("div");
  left.className = "sgi-bay sgi-bay-left";
  bays.appendChild(left);

  const center = document.createElement("div");
  center.className = "sgi-bay sgi-bay-center";
  bays.appendChild(center);

  const right = document.createElement("div");
  right.className = "sgi-bay sgi-bay-right";
  bays.appendChild(right);

  const sections = panel("SECTIONS");
  sections.box.style.flex = "1";
  left.appendChild(sections.box);
  const rows = [];
  for (let i = 0; i < 7; i++) {
    const row = document.createElement("div");
    row.className = "sgi-row";
    const chip = document.createElement("i");
    chip.className = "sgi-chip";
    const name = document.createElement("span");
    name.className = "sgi-row-name";
    const freq = document.createElement("span");
    freq.className = "sgi-row-freq";
    const lock = document.createElement("span");
    lock.className = "sgi-row-lock";
    lock.onclick = (e) => {
      e.stopPropagation();
      hooks.onLock(i);
    };
    row.onclick = () => hooks.onSelect(i);
    row.append(chip, name, freq, lock);
    sections.body.appendChild(row);
    rows.push({ row, chip, name, freq, lock });
  }

  const inspector = panel("SECTION");
  left.appendChild(inspector.box);
  const editors = FIELDS.map(([key, group, unit]) => {
    const line = document.createElement("div");
    line.className = "sgi-edit";
    const label = document.createElement("span");
    label.className = "sgi-edit-label";
    label.textContent = `${group} ${unit}`;
    const shown = () => {
      const lane = view.lanes && view.lanes[selected];
      if (!lane) return 0;
      return unit === "dB" ? 20 * Math.log10(Math.max(lane[key], 1e-6)) : lane[key];
    };
    const box = valueBox({
      currentValue: shown,
      onCommit: (v) =>
        hooks.onEdit(selected, { [key]: unit === "dB" ? Math.pow(10, v / 20) : v }),
    });
    const wheel = thumbwheel({
      horizontal: true,
      onDelta: (d) => {
        const lane = view.lanes && view.lanes[selected];
        if (!lane) return;
        if (unit === "Hz") {
          hooks.onEdit(selected, { [key]: Math.min(16000, Math.max(20, lane[key] * Math.pow(2, d / 260))) });
        } else if (unit === "dB") {
          hooks.onEdit(selected, { [key]: lane[key] * Math.pow(10, d * 0.02 / 20) });
        } else {
          const top = key === "pole_r" ? view.ceiling : 1;
          hooks.onEdit(selected, { [key]: Math.min(top, Math.max(0, lane[key] + d * 0.0006)) });
        }
      },
    });
    line.append(label, box, wheel);
    inspector.body.appendChild(line);
    return { key, unit, box, line };
  });

  const scope = document.createElement("canvas");
  scope.className = "sgi-sunken sgi-scope";
  center.appendChild(scope);

  const target = panel("TARGET");
  right.appendChild(target.box);
  const targetName = document.createElement("div");
  targetName.className = "sgi-target-name";
  target.body.appendChild(targetName);
  const peakTable = document.createElement("div");
  peakTable.className = "sgi-peaks";
  target.body.appendChild(peakTable);

  const solver = panel("METHOD");
  right.appendChild(solver.box);
  solver.body.appendChild(button("1 PIN SKELETON", () => hooks.onAutoPin(), "sgi-btn-wide"));
  const scaffoldButton = button(
    "2 ZERO SCAFFOLD",
    () => {
      scaffold = !scaffold;
      hooks.onScaffold(scaffold);
      api.paint(view);
    },
    "sgi-btn-wide"
  );
  solver.body.appendChild(scaffoldButton);
  solver.body.appendChild(
    button("3 BOUND & NORMALIZE", () => hooks.onBound(), "sgi-btn-wide")
  );
  solver.body.appendChild(button("FIT", () => hooks.onFit(), "sgi-btn-wide sgi-btn-go"));
  const presets = document.createElement("div");
  presets.className = "sgi-presets";
  for (const [label, key] of [["ALL", "all"], ["FORMANTS", "formants"], ["S6·S7", "tail"]]) {
    presets.appendChild(button(label, () => hooks.onPreset(key)));
  }
  solver.body.appendChild(presets);

  const corners = panel("CORNERS");
  right.appendChild(corners.box);
  const matrix = document.createElement("div");
  matrix.className = "sgi-matrix";
  corners.body.appendChild(matrix);
  const cells = [];
  for (let i = 0; i < 8; i++) {
    const cell = button(`C${i}`, () => hooks.onCommit(i));
    cell.classList.add("sgi-cell");
    matrix.appendChild(cell);
    cells.push(cell);
  }

  const error = panel("ERROR");
  right.appendChild(error.box);
  const errorLines = [0, 1, 2].map(() => {
    const line = document.createElement("div");
    line.className = "sgi-error-line";
    error.body.appendChild(line);
    return line;
  });

  let open = false;
  let selected = 1;
  let view = {};
  let drag = null;
  let scaffold = false;

  function markers() {
    const list = [];
    if (!view.lanes) return list;
    for (let i = 0; i < 7; i++) {
      const lane = view.lanes[i];
      if (lane.pole_r > 0) list.push({ lane: i, kind: "pole", hz: lane.pole_hz, r: lane.pole_r });
      if (lane.zero_r > 0) list.push({ lane: i, kind: "zero", hz: lane.zero_hz, r: lane.zero_r });
    }
    return list;
  }

  function drawScope() {
    scope.width = scope.clientWidth;
    scope.height = scope.clientHeight;
    const ctx = scope.getContext("2d");
    const w = scope.width;
    const h = scope.height;
    ctx.fillStyle = css("--well");
    ctx.fillRect(0, 0, w, h);
    ctx.lineWidth = 1;
    for (let db = DB_LO; db <= DB_HI; db += 10) {
      ctx.strokeStyle = db === 0 ? css("--grat-major") : css("--grat-minor");
      ctx.beginPath();
      ctx.moveTo(0, yOf(db, h));
      ctx.lineTo(w, yOf(db, h));
      ctx.stroke();
    }
    for (const hz of [100, 1000, 10000]) {
      ctx.strokeStyle = hz === 1000 ? css("--grat-major") : css("--grat-minor");
      ctx.beginPath();
      ctx.moveTo(xOf(hz, w), 0);
      ctx.lineTo(xOf(hz, w), h);
      ctx.stroke();
    }
    ctx.strokeStyle = css("--alarm");
    ctx.setLineDash([6, 4]);
    ctx.beginPath();
    ctx.moveTo(0, yOf(CROWN, h));
    ctx.lineTo(w, yOf(CROWN, h));
    ctx.stroke();
    ctx.setLineDash([]);

    ctx.setLineDash([4, 4]);
    ctx.font = "10px monospace";
    for (let i = 0; i < 7; i++) {
      const role = roleOf(i, view.roles);
      if (!role.formant || !view.lanes || view.lanes[i].pole_r <= 0) continue;
      const x = xOf(view.lanes[i].pole_hz, w);
      ctx.strokeStyle = css(role.color);
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
      ctx.fillStyle = css(role.color);
      ctx.fillText(role.formant, x + 3, 11);
    }
    ctx.setLineDash([]);

    if (view.target) trace(ctx, view.target, w, h, css("--trace-ghost"), 1.2);
    if (view.candidate) trace(ctx, view.candidate, w, h, css("--role-chest"), 1);
    const res = residualOf(view.target, view.sum);
    if (res) {
      const zero = yOf(0, h);
      ctx.lineWidth = 1.2;
      for (let i = 1; i < res.curve.length; i++) {
        const bad = Math.abs(res.curve[i]) > 1;
        ctx.strokeStyle = bad ? css("--alarm") : css("--trace-live");
        ctx.beginPath();
        ctx.moveTo(((i - 1) / (res.curve.length - 1)) * w, zero - res.curve[i - 1] * 2);
        ctx.lineTo((i / (res.curve.length - 1)) * w, zero - res.curve[i] * 2);
        ctx.stroke();
      }
    }
    if (view.sum) trace(ctx, view.sum, w, h, css("--trace-cumulative"), 1.8);

    for (const m of markers()) {
      const role = roleOf(m.lane, view.roles);
      const x = xOf(m.hz, w);
      const y = yOf(sampleAt(view.sum, m.hz), h);
      const on = m.lane === selected;
      ctx.strokeStyle = css(role.color);
      ctx.fillStyle = css(role.color);
      ctx.lineWidth = on ? 2 : 1;
      if (m.kind === "pole") {
        ctx.fillRect(x - 4, y - 4, 8, 8);
      } else {
        ctx.beginPath();
        ctx.arc(x, y, 4.5, 0, 7);
        ctx.stroke();
        if (view.zeroHeld && view.zeroHeld[m.lane]) {
          ctx.beginPath();
          ctx.arc(x, y, 2, 0, 7);
          ctx.fill();
        }
      }
      if (on) {
        ctx.strokeStyle = css("--sgi-hi");
        ctx.strokeRect(x - 7, y - 7, 14, 14);
      }
    }
    return res;
  }

  function pick(px, py) {
    const w = scope.width;
    const h = scope.height;
    let best = null;
    let bestDist = 14;
    for (const m of markers()) {
      const d = Math.hypot(xOf(m.hz, w) - px, yOf(sampleAt(view.sum, m.hz), h) - py);
      if (d < bestDist) {
        bestDist = d;
        best = m;
      }
    }
    if (best) return best;
    for (let i = 0; i < 7; i++) {
      const role = roleOf(i, view.roles);
      if (!role.formant || !view.lanes || view.lanes[i].pole_r <= 0) continue;
      if (Math.abs(xOf(view.lanes[i].pole_hz, w) - px) < 5) {
        return { lane: i, kind: "pole", flag: true, hz: view.lanes[i].pole_hz, r: view.lanes[i].pole_r };
      }
    }
    return null;
  }

  scope.addEventListener("pointerdown", (e) => {
    const rect = scope.getBoundingClientRect();
    const hit = pick(e.clientX - rect.left, e.clientY - rect.top);
    if (!hit && scaffold) {
      const hz = hzOfX(e.clientX - rect.left, scope.width);
      const lane = hooks.onPlaceZero(hz);
      if (lane === null || lane === undefined) return;
      selected = lane;
      drag = {
        lane,
        kind: "zero",
        hz,
        r: view.lanes[lane].zero_r,
        y0: e.clientY - rect.top,
        rp0: rPrime(view.lanes[lane].zero_r),
      };
      scope.setPointerCapture(e.pointerId);
      return;
    }
    if (!hit) return;
    selected = hit.lane;
    hooks.onSelect(hit.lane);
    drag = { ...hit, y0: e.clientY - rect.top, rp0: rPrime(hit.r) };
    scope.setPointerCapture(e.pointerId);
  });

  scope.addEventListener("pointermove", (e) => {
    if (!drag) return;
    const rect = scope.getBoundingClientRect();
    const px = e.clientX - rect.left;
    const py = e.clientY - rect.top;
    const hz = hzOfX(px, scope.width);
    const patch = {};
    patch[drag.kind === "pole" ? "pole_hz" : "zero_hz"] = hz;
    if (!drag.flag) {
      const rp = drag.rp0 + (dbOfY(py, scope.height) - dbOfY(drag.y0, scope.height));
      const top = drag.kind === "pole" ? view.ceiling : 1;
      patch[drag.kind === "pole" ? "pole_r" : "zero_r"] = Math.min(
        top,
        Math.max(0, rOf(Math.max(0, rp)))
      );
    }
    hooks.onEdit(drag.lane, patch);
  });

  const endDrag = () => {
    if (!drag) return;
    const d = drag;
    drag = null;
    hooks.onEditEnd(d.lane);
  };
  scope.addEventListener("pointerup", endDrag);
  scope.addEventListener("pointercancel", endDrag);

  const api = {
    isOpen: () => open,
    open() {
      open = true;
      el.hidden = false;
      hooks.onPaint();
    },
    close() {
      open = false;
      el.hidden = true;
    },
    select(i) {
      selected = i;
    },
    setStatus(text) {
      status.textContent = text;
    },
    paint(next) {
      if (!open) return;
      view = next || {};
      if (view.selected !== undefined && view.selected !== null) selected = view.selected;
      title.textContent = `${view.name || "no target"} — 7-STAGE SERIAL CASCADE`;

      for (let i = 0; i < 7; i++) {
        const lane = view.lanes ? view.lanes[i] : null;
        const role = roleOf(i, view.roles);
        const locked = view.locks ? view.locks[i] : false;
        const r = rows[i];
        r.row.className = `sgi-row${i === selected ? " on" : ""}`;
        r.chip.style.background = css(role.color);
        r.name.textContent = `S${i + 1} ${role.label}`;
        r.name.style.color = css(role.color);
        const level = lane && lane.scale > 0 ? 20 * Math.log10(lane.scale) : 0;
        r.freq.textContent = !lane
          ? "—"
          : lane.pole_r > 0
            ? `${lane.pole_hz.toFixed(0)}  ${level >= 0 ? "+" : ""}${level.toFixed(1)}`
            : lane.zero_r > 0
              ? `z${lane.zero_hz.toFixed(0)}  ${level >= 0 ? "+" : ""}${level.toFixed(1)}`
              : "—";
        r.lock.textContent = locked ? "🔒" : "🔓";
        r.lock.style.opacity = locked ? "1" : "0.4";
      }

      const lane = view.lanes ? view.lanes[selected] : null;
      const role = roleOf(selected, view.roles);
      inspector.box.querySelector(".sgi-panel-title").textContent = `S${selected + 1} ${role.label}`;
      inspector.box.querySelector(".sgi-panel-title").style.color = css(role.color);
      const hasPole = !!lane && lane.pole_r > 0;
      const hasZero = !!lane && lane.zero_r > 0;
      for (const ed of editors) {
        const live =
          ed.key === "scale" ? hasPole || hasZero : ed.key.startsWith("pole") ? hasPole : hasZero;
        ed.line.hidden = !live;
        if (!live || document.activeElement === ed.box) continue;
        const v = lane[ed.key];
        ed.box.value =
          ed.unit === "Hz"
            ? v.toFixed(1)
            : ed.unit === "dB"
              ? (20 * Math.log10(Math.max(v, 1e-6))).toFixed(2)
              : v.toFixed(4);
      }
      scaffoldButton.className = scaffold ? "sgi-btn sgi-btn-wide on" : "sgi-btn sgi-btn-wide";
      scope.style.cursor = scaffold ? "copy" : "crosshair";

      targetName.textContent = view.name || "—";
      peakTable.textContent = "";
      for (const [i, p] of (view.peaks || []).slice(0, 5).entries()) {
        const line = document.createElement("div");
        line.className = "sgi-peak";
        line.innerHTML = "";
        const tag = document.createElement("span");
        tag.textContent = `F${i + 1}`;
        tag.style.color = css("--role-formant");
        const hz = document.createElement("span");
        hz.textContent = `${p.hz.toFixed(0)} Hz`;
        const q = document.createElement("span");
        q.textContent = `BW ${p.bandwidth_hz.toFixed(0)}  Q ${(p.hz / p.bandwidth_hz).toFixed(1)}`;
        line.append(tag, hz, q);
        peakTable.appendChild(line);
      }

      for (let i = 0; i < 8; i++) {
        const held = view.field && view.field[i];
        cells[i].textContent = held ? `C${i} ${held.name.slice(0, 9)}` : `C${i}`;
        cells[i].className = `sgi-btn sgi-cell${held ? " filled" : ""}${
          i === view.active ? " on" : ""
        }`;
      }

      drawScope();
      const crown = view.sum ? Math.max(...view.sum) : null;
      const rms = view.rms;
      errorLines[0].textContent =
        rms === null || rms === undefined
          ? "rms —"
          : `rms ${rms.toFixed(2)} dB${view.rmsStale ? " (edited since)" : ""}`;
      errorLines[0].style.color =
        rms !== null && rms !== undefined && rms < 1 && !view.rmsStale
          ? css("--trace-live")
          : css("--alarm");
      errorLines[1].textContent =
        crown === null ? "crown —" : `crown ${crown.toFixed(1)} / ${CROWN} dB`;
      errorLines[1].style.color =
        crown !== null && crown > CROWN ? css("--alarm") : css("--sgi-ink");
      errorLines[2].textContent =
        view.packing === null || view.packing === undefined
          ? "packing —"
          : `packing ${view.packing.toFixed(3)} dB`;
      errorLines[2].style.color = css("--sgi-ink");
    },
  };
  api.close();
  return api;
}
