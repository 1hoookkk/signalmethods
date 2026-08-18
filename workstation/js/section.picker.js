import { curveInto } from "./curves.js";
import { css, scope, curveEval, trace, yMap, SECTION_DB_LO, SECTION_DB_HI } from "./render.js";


function fhz(hz) {
  if (!(hz > 0)) return "0";
  if (hz >= 1000) return `${(hz / 1000).toFixed(2)}k`;
  return `${Math.round(hz)}`;
}

function rootText(tag, root) {
  if (!root || root.kind === "off") return `${tag} —`;
  if (root.kind === "real") return `${tag} real ${root.pair[0].toFixed(3)}/${root.pair[1].toFixed(3)}`;
  return `${tag} ${fhz(root.hz)} Hz r ${root.r.toFixed(4)}`;
}

function detail(state) {
  const stages = state.stages.map((s) => `S${s + 1}`).join("/");
  return `×${state.count} across ${state.presets.length} filters at ${stages} — ${rootText("P", state.pole)} · ${rootText("Z", state.zero)} · ${state.scale_db.toFixed(2)} dB · ${state.presets.join(", ")}`;
}

export function createSectionPicker(host, hooks) {
  const panel = document.createElement("div");
  panel.className = "section-picker";
  panel.hidden = true;
  host.appendChild(panel);

  const cache = new Map();
  let states = [];
  let destination = 0;
  let anchor = null;

  function curve(state) {
    const key = state.words.join(",");
    if (!cache.has(key)) cache.set(key, curveInto([state.words]));
    return cache.get(key);
  }

  function drawState(canvas, state) {
    const values = curve(state);
    scope(canvas).frame((g, w, h) => {
      const { yOf } = yMap(SECTION_DB_LO, SECTION_DB_HI, h);
      g.fillStyle = css("--well");
      g.fillRect(0, 0, w, h);
      g.strokeStyle = css("--grat-major");
      g.beginPath();
      g.moveTo(0, Math.floor(yOf(0)) + 0.5);
      g.lineTo(w, Math.floor(yOf(0)) + 0.5);
      g.stroke();
      trace(g, w, h, curveEval(values), yOf, css("--trace-live"), 1.4);
    });
  }

  function groups() {
    const here = states.filter((state) => state.stages.includes(destination));
    const elsewhere = states.filter((state) => !state.stages.includes(destination));
    return [
      { kind: `SEEN AT S${destination + 1}`, items: here },
      { kind: "ELSEWHERE IN THE CASCADE", items: elsewhere },
    ].filter((group) => group.items.length);
  }

  function close() {
    panel.hidden = true;
  }

  function place() {
    if (!anchor) return;
    const width = panel.offsetWidth;
    const left = Math.max(2, Math.min(window.innerWidth - width - 2, anchor.left));
    const top = Math.max(2, Math.min(window.innerHeight - 120, anchor.bottom + 2));
    panel.style.left = `${left}px`;
    panel.style.top = `${top}px`;
    panel.style.maxHeight = `${Math.max(118, window.innerHeight - top - 2)}px`;
  }

  function render() {
    panel.textContent = "";

    const head = document.createElement("div");
    head.className = "section-picker-head";
    const title = document.createElement("strong");
    title.textContent = `S${destination + 1}`;
    const closeButton = document.createElement("button");
    closeButton.textContent = "×";
    closeButton.setAttribute("aria-label", "Close section picker");
    closeButton.onclick = close;
    head.append(title, closeButton);

    const sheet = document.createElement("div");
    sheet.className = "section-sheet";
    const found = groups();

    for (const { kind, items } of found) {
      const label = document.createElement("div");
      label.className = "sheet-type";
      label.textContent = kind;
      sheet.appendChild(label);

      const strip = document.createElement("div");
      strip.className = "sheet-strip";
      for (const state of items) {
        const tile = document.createElement("div");
        tile.className = "sheet-tile";
        tile.tabIndex = 0;
        tile.setAttribute("role", "button");
        tile.setAttribute("aria-label", detail(state));
        tile.title = detail(state);

        const count = document.createElement("span");
        count.className = "count";
        count.textContent = `×${state.count}`;
        const canvas = document.createElement("canvas");
        tile.append(canvas, count);

        const show = () => hooks.onDetail(detail(state));
        const seat = () => {
          hooks.onState(state, destination);
          close();
        };
        tile.onmouseenter = show;
        tile.onfocus = show;
        tile.onclick = seat;
        tile.onkeydown = (event) => {
          if (event.key !== "Enter" && event.key !== " ") return;
          event.preventDefault();
          seat();
        };
        strip.appendChild(tile);
        requestAnimationFrame(() => drawState(canvas, state));
      }
      sheet.appendChild(strip);
    }

    if (!found.length) {
      const empty = document.createElement("div");
      empty.className = "section-empty";
      empty.textContent = "NONE RECURRING";
      sheet.appendChild(empty);
    }

    panel.append(head, sheet);
    place();
  }

  panel.addEventListener("keydown", (event) => {
    if (event.key === "Escape") close();
  });

  return {
    setVocabulary(vocabulary) {
      states = (vocabulary && vocabulary.states) || [];
      cache.clear();
    },
    open(nextDestination, nextAnchor) {
      destination = nextDestination;
      anchor = nextAnchor || null;
      panel.hidden = false;
      render();
      panel.querySelector(".sheet-tile")?.focus();
    },
    close,
  };
}
