import { button, element } from "./ui.js";

export type Space = "AUTHOR" | "ANALYZE" | "MORPH";
export const SPACES: Space[] = ["AUTHOR", "ANALYZE", "MORPH"];

export type ShellHooks = {
  onSpace: (space: Space) => void;
  onPlay: () => void;
  onFit: () => void;
  onPalette: () => void;
};

export type Shell = {
  toolbar: HTMLElement;
  browser: HTMLElement;
  work: HTMLElement;
  inspector: HTMLElement;
  foot: HTMLElement;
  status: HTMLElement;
  title: HTMLElement;
  setSpace: (space: Space) => void;
  setPlaying: (on: boolean) => void;
  setFitEnabled: (on: boolean) => void;
  onResize: (fn: () => void) => void;
};

const MIN = 132;
const MAX = 420;

// Application structure is ordinary DOM: a toolbar, three docks separated by draggable
// grips, and a footer. Panels inside the docks host either a measurement canvas or
// Trench's own widgets. Nothing here knows what a body is; it emits intent.
export function mountShell(root: HTMLElement, hooks: ShellHooks): Shell {
  root.replaceChildren();
  root.id = "shell";

  const toolbar = element("header");
  toolbar.id = "toolbar";
  const brand = element("span", "brand", "TRENCH");
  const title = element("span", undefined, "—");

  const object = element("div", "tool-group");
  object.append(title);

  const spaces = element("div", "tool-group");
  const spaceButtons = new Map<Space, HTMLButtonElement>();
  for (const space of SPACES) {
    const b = button(space, () => hooks.onSpace(space));
    spaces.appendChild(b);
    spaceButtons.set(space, b);
  }

  const find = element("div", "tool-group");
  find.append(button("FIND  /", hooks.onPalette));

  const gap = element("div", "gap");

  const docksGroup = element("div", "tool-group");
  const browserToggle = button("BROWSER", () => toggle("no-browser", browserToggle));
  const inspectorToggle = button("INSPECTOR", () => toggle("no-inspector", inspectorToggle));
  browserToggle.classList.add("on");
  inspectorToggle.classList.add("on");
  docksGroup.append(browserToggle, inspectorToggle);

  const transport = element("div", "tool-group");
  const play = button("PLAY", hooks.onPlay);
  const fit = button("FIT", hooks.onFit);
  transport.append(play, fit);

  toolbar.append(brand, object, spaces, find, gap, docksGroup, transport);

  const docks = element("div");
  docks.id = "docks";
  const browserDock = element("div", "dock");
  browserDock.id = "browser-dock";
  const browserGrip = element("div", "grip browser");
  const work = element("div");
  work.id = "work";
  const inspectorGrip = element("div", "grip inspector");
  const inspectorDock = element("div", "dock");
  inspectorDock.id = "inspector-dock";
  docks.append(browserDock, browserGrip, work, inspectorGrip, inspectorDock);

  const foot = element("footer");
  foot.id = "foot";
  const status = element("span");
  status.id = "status";

  root.append(toolbar, docks, foot);

  const resized: (() => void)[] = [];
  const announce = () => {
    for (const fn of resized) fn();
  };

  function toggle(flag: string, control: HTMLButtonElement) {
    const on = docks.classList.toggle(flag);
    control.classList.toggle("on", !on);
    announce();
  }

  // Grips write a CSS variable rather than inline widths, so the grid stays the single
  // description of the layout.
  function drag(grip: HTMLElement, variable: string, fromRight: boolean) {
    let start = 0;
    let base = 0;
    grip.addEventListener("pointerdown", (e) => {
      start = e.clientX;
      base = Number.parseFloat(getComputedStyle(docks).getPropertyValue(variable)) || 200;
      grip.setPointerCapture(e.pointerId);
      e.preventDefault();
    });
    grip.addEventListener("pointermove", (e) => {
      if (!grip.hasPointerCapture(e.pointerId)) return;
      const delta = (e.clientX - start) * (fromRight ? -1 : 1);
      docks.style.setProperty(variable, `${Math.min(MAX, Math.max(MIN, base + delta))}px`);
      announce();
    });
  }

  drag(browserGrip, "--browser-w", false);
  drag(inspectorGrip, "--inspector-w", true);
  window.addEventListener("resize", announce);

  return {
    toolbar,
    browser: browserDock,
    work,
    inspector: inspectorDock,
    foot,
    status,
    title,
    setSpace(space) {
      for (const [name, b] of spaceButtons) b.classList.toggle("on", name === space);
    },
    setPlaying(on) {
      play.classList.toggle("on", on);
      play.textContent = on ? "STOP" : "PLAY";
    },
    setFitEnabled(on) {
      fit.disabled = !on;
    },
    onResize(fn) {
      resized.push(fn);
    },
  };
}
