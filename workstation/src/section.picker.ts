import type { StageSource, VocabularyState } from "./api.js";
import { curveInto } from "./curves.js";
import { css, curveEval, SECTION_DB_HI, SECTION_DB_LO, scope, trace, panel as wellPanel } from "./render.js";

const CORNER_INKS = ["--s1", "--s2", "--s3", "--s4", "--s5", "--s6", "--s7", "--s1"];
const PAGE = 24;

export type Grain = "track" | "cell";

export type PickerHooks = {
  corner: () => number;
  onDetail: (text: string) => void;
  onSeat: (grain: Grain, source: StageSource, stage: number, corner: number, destination: number) => void;
  onSeatState: (state: VocabularyState, destination: number) => void;
};

export function createSectionPicker(host: HTMLElement, hooks: PickerHooks) {
  const panel = document.createElement("div");
  panel.className = "section-picker";
  panel.hidden = true;
  host.appendChild(panel);

  const cache = new Map<string, Float32Array>();
  let sources: StageSource[] = [];
  let states: VocabularyState[] = [];
  let grain: Grain = "track";
  let family = "recurring";
  let query = "";
  let shown = PAGE;
  let destination = 0;
  let anchor: DOMRect | null = null;

  function curve(source: StageSource, stage: number, corner: number): Float32Array {
    const key = `${source.id}/${stage}/${corner}`;
    let found = cache.get(key);
    if (!found) {
      found = curveInto([source.tracks[stage][corner]], null);
      cache.set(key, found);
    }
    return found;
  }

  function draw(canvas: HTMLCanvasElement, source: StageSource, stage: number) {
    const corners = grain === "track" ? source.tracks[stage].length : 1;
    const values = Array.from({ length: corners }, (_, c) =>
      curve(source, stage, grain === "track" ? c : activeCorner(source)),
    );
    scope(canvas).frame((g, w, h) => {
      const yOf = wellPanel(g, w, h, SECTION_DB_LO, SECTION_DB_HI);
      values.forEach((v, c) => {
        g.globalAlpha = corners === 1 ? 1 : 0.85;
        trace(g, w, curveEval(v), yOf, css(CORNER_INKS[c % CORNER_INKS.length]), 1.2);
      });
      g.globalAlpha = 1;
    });
  }

  function drawState(canvas: HTMLCanvasElement, state: VocabularyState) {
    const v = stateCurve(state);
    scope(canvas).frame((g, w, h) => {
      const yOf = wellPanel(g, w, h, SECTION_DB_LO, SECTION_DB_HI);
      trace(g, w, curveEval(v), yOf, css("--s3"), 1.3);
    });
  }

  function activeCorner(source: StageSource): number {
    return Math.min(hooks.corner(), source.tracks[0].length - 1);
  }

  function matches(): StageSource[] {
    const needle = query.trim().toLocaleLowerCase();
    return sources.filter((s) => {
      if (family !== "all" && s.family !== family) return false;
      if (!needle) return true;
      return `${s.name} ${s.id}`.toLocaleLowerCase().includes(needle);
    });
  }

  function close() {
    panel.hidden = true;
  }

  function place() {
    if (!anchor) return;
    const width = panel.offsetWidth;
    const left = Math.max(2, Math.min(window.innerWidth - width - 2, anchor.left));
    const top = Math.max(2, Math.min(window.innerHeight - 140, anchor.bottom + 2));
    panel.style.left = `${left}px`;
    panel.style.top = `${top}px`;
    panel.style.maxHeight = `${Math.max(140, window.innerHeight - top - 2)}px`;
  }

  function chip(label: string, active: boolean, onclick: () => void): HTMLButtonElement {
    const b = document.createElement("button");
    b.className = active ? "pick-chip on" : "pick-chip";
    b.textContent = label;
    b.onclick = onclick;
    return b;
  }

  function stateCurve(state: VocabularyState): Float32Array {
    const key = `state/${state.words.join(",")}`;
    let found = cache.get(key);
    if (!found) {
      found = curveInto([state.words], null);
      cache.set(key, found);
    }
    return found;
  }

  function renderStates(body: HTMLElement) {
    const q = query.trim().toLowerCase();
    const list = states.filter((st) => !q || st.presets.some((n) => n.toLowerCase().includes(q)));
    if (!list.length) {
      const empty = document.createElement("div");
      empty.className = "sheet-type";
      empty.textContent = "no recurring section matches";
      body.appendChild(empty);
      return;
    }
    const label = document.createElement("div");
    label.className = "sheet-type";
    label.textContent = `${list.length} SECTIONS RECURRING ACROSS PRESETS · RANKED BY USE`;
    body.appendChild(label);
    const strip = document.createElement("div");
    strip.className = "sheet-strip";
    for (const state of list.slice(0, shown)) {
      const tile = document.createElement("div");
      tile.className = "sheet-tile";
      tile.tabIndex = 0;
      tile.setAttribute("role", "button");
      const cite = `x${state.count} · ${state.presets.join(", ")} · P ${Math.round(state.pole.hz)} Hz r ${state.pole.r.toFixed(4)}`;
      tile.title = cite;
      tile.setAttribute("aria-label", cite);
      const tag = document.createElement("span");
      tag.className = "count";
      tag.textContent = `x${state.count}`;
      const canvas = document.createElement("canvas");
      tile.append(canvas, tag);
      const seat = () => {
        hooks.onSeatState(state, destination);
        close();
      };
      tile.onclick = seat;
      tile.onmouseenter = () => hooks.onDetail(cite);
      tile.onfocus = () => hooks.onDetail(cite);
      tile.onkeydown = (e) => {
        if (e.key === "Enter" || e.key === " ") {
          e.preventDefault();
          seat();
        }
      };
      strip.appendChild(tile);
      requestAnimationFrame(() => drawState(canvas, state));
    }
    body.appendChild(strip);
  }

  function render(keepFocus = false) {
    panel.textContent = "";

    const head = document.createElement("div");
    head.className = "section-picker-head";
    const title = document.createElement("strong");
    title.textContent = `S${destination + 1} ←`;
    const closeButton = document.createElement("button");
    closeButton.textContent = "×";
    closeButton.onclick = close;
    head.append(title, closeButton);

    const bar = document.createElement("div");
    bar.className = "pick-bar";
    bar.append(
      chip("TRACK", grain === "track", () => {
        grain = "track";
        render();
      }),
      chip("CELL", grain === "cell", () => {
        grain = "cell";
        render();
      }),
      chip("ALL", family === "all", () => {
        family = "all";
        shown = PAGE;
        render();
      }),
      chip("RUNTIME", family === "x3", () => {
        family = "x3";
        shown = PAGE;
        render();
      }),
      chip("FORMS", family === "md", () => {
        family = "md";
        shown = PAGE;
        render();
      }),
      chip("RECURRING", family === "recurring", () => {
        family = "recurring";
        shown = PAGE;
        render();
      }),
    );

    const search = document.createElement("input");
    search.className = "pick-search";
    search.type = "text";
    search.value = query;
    search.placeholder = "FILTER BY OBJECT";
    search.oninput = () => {
      query = search.value;
      shown = PAGE;
      render(true);
    };

    const body = document.createElement("div");
    body.className = "section-sheet";
    if (family === "recurring") {
      renderStates(body);
      panel.append(head, bar, search, body);
      place();
      if (keepFocus) panel.querySelector<HTMLInputElement>(".pick-search")?.focus();
      return;
    }
    const list = matches();
    for (const source of list.slice(0, shown)) {
      const label = document.createElement("div");
      label.className = "sheet-type";
      label.textContent = `${source.name} · ${source.family} · ${source.tracks[0].length} corners`;
      body.appendChild(label);

      const strip = document.createElement("div");
      strip.className = "sheet-strip";
      for (let stage = 0; stage < source.stage_count; stage++) {
        const tile = document.createElement("div");
        tile.className = "sheet-tile";
        tile.tabIndex = 0;
        tile.setAttribute("role", "button");
        const corner = activeCorner(source);
        const cite =
          grain === "track"
            ? `${source.id} / S${stage + 1} / all ${source.tracks[stage].length} corners`
            : `${source.id} / C${corner} / S${stage + 1}`;
        tile.title = cite;
        tile.setAttribute("aria-label", cite);

        const tag = document.createElement("span");
        tag.className = "count";
        tag.textContent = `S${stage + 1}`;
        const canvas = document.createElement("canvas");
        tile.append(canvas, tag);

        const seat = () => {
          hooks.onSeat(grain, source, stage, corner, destination);
          close();
        };
        tile.onclick = seat;
        tile.onmouseenter = () => hooks.onDetail(cite);
        tile.onfocus = () => hooks.onDetail(cite);
        tile.onkeydown = (e) => {
          if (e.key !== "Enter" && e.key !== " ") return;
          e.preventDefault();
          seat();
        };
        strip.appendChild(tile);
        requestAnimationFrame(() => draw(canvas, source, stage));
      }
      body.appendChild(strip);
    }

    if (list.length > shown) {
      const more = document.createElement("button");
      more.className = "pick-more";
      more.textContent = `MORE — ${list.length - shown} of ${list.length} objects left`;
      more.onclick = () => {
        shown += PAGE;
        render();
      };
      body.appendChild(more);
    }
    if (!list.length) {
      const empty = document.createElement("div");
      empty.className = "section-empty";
      empty.textContent = "NO MATCH";
      body.appendChild(empty);
    }

    panel.append(head, bar, search, body);
    place();
    if (keepFocus) {
      const s = panel.querySelector<HTMLInputElement>(".pick-search");
      if (s) {
        s.focus();
        s.setSelectionRange(query.length, query.length);
      }
    }
  }

  panel.addEventListener("keydown", (e) => {
    if (e.key === "Escape") close();
  });

  return {
    setSources(next: StageSource[] | undefined) {
      sources = (next || []).filter((s) => s.tracks && s.tracks.length);
      cache.clear();
    },
    setStates(next: VocabularyState[] | undefined) {
      states = (next || []).filter((s) => s && s.words && s.presets);
    },
    open(nextDestination: number, nextAnchor: DOMRect | null) {
      destination = nextDestination;
      anchor = nextAnchor || null;
      shown = PAGE;
      panel.hidden = false;
      render();
      panel.querySelector<HTMLInputElement>(".pick-search")?.focus();
    },
    close,
  };
}
