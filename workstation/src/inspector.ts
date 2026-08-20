import type { Doc } from "./doc.js";
import { currentSlot, lawState } from "./doc.js";
import type { Lane } from "./dsp.js";
import { element, key, keys, numeric, panel } from "./ui.js";

export type InspectorHooks = {
  onEdit: (patch: Partial<Lane>) => void;
  onCommit: () => void;
  onHold: () => void;
  onBypass: () => void;
  onFlip: () => void;
  onClear: () => void;
  onSeat: (rect: DOMRect) => void;
  onClearTarget: () => void;
  onClearReference: () => void;
};

export type Inspector = { root: HTMLElement; paint: (doc: Doc, residual: number | null) => void };

// Properties of the current selection, as editable numbers rather than a readout. The
// same variables the response canvas drags are typed here; there is one owner and two
// ways to reach it.
export function mountInspector(hooks: InspectorHooks): Inspector {
  const root = element("div");
  root.style.cssText = "display:flex;flex-direction:column;min-height:0;flex:1";

  const section = panel("SECTION", { scroll: true });
  const poleHz = numeric("POLE", {
    unit: "Hz",
    digits: 1,
    min: 20,
    max: 16000,
    step: 0.004,
    ratio: true,
    onChange: (v) => hooks.onEdit({ pole_hz: v }),
  });
  const poleR = numeric("r", {
    digits: 4,
    min: 0,
    max: 0.9999,
    step: 0.0009,
    onChange: (v) => hooks.onEdit({ pole_r: v }),
  });
  const zeroHz = numeric("ZERO", {
    unit: "Hz",
    digits: 1,
    min: 20,
    max: 16000,
    step: 0.004,
    ratio: true,
    onChange: (v) => hooks.onEdit({ zero_hz: v }),
  });
  const zeroR = numeric("r", {
    digits: 4,
    min: 0,
    max: 1,
    step: 0.0009,
    onChange: (v) => hooks.onEdit({ zero_r: v }),
  });
  const scale = numeric("SCALE", { unit: "dB", digits: 2, min: -96, max: 24, step: 0.08 });
  const wordLine = element("div", "num");
  const wordLabel = element("label", undefined, "WORDS");
  const wordValue = element("div", "field flat");
  const wordText = element("span", "v", "—");
  wordText.style.fontSize = "9px";
  wordValue.append(wordText);
  wordLine.append(wordLabel, wordValue);

  const holdKey = key("HOLD", hooks.onHold);
  const bypassKey = key("BYPASS", hooks.onBypass);
  const flipKey = key("P↔Z", hooks.onFlip);
  const clearKey = key("CLEAR", hooks.onClear);
  const seatKey = key("SEAT FACTORY SECTION", () => hooks.onSeat(seatKey.getBoundingClientRect()));

  section.body.append(
    poleHz.root,
    poleR.root,
    zeroHz.root,
    zeroR.root,
    scale.root,
    wordLine,
    keys(2, holdKey, bypassKey, flipKey, clearKey),
    keys(1, seatKey),
  );

  const target = panel("TARGET", { scroll: true });
  const targetName = element("div", "num");
  const targetLabel = element("label", undefined, "SOURCE");
  const targetValue = element("div", "field flat");
  const targetText = element("span", "v", "—");
  targetValue.append(targetText);
  targetName.append(targetLabel, targetValue);
  const residualLine = numeric("RESIDUAL", { unit: "dB", digits: 2 });
  const clearTarget = key("CLEAR TARGET", hooks.onClearTarget);
  const clearReference = key("CLEAR REFERENCE", hooks.onClearReference);
  target.body.append(targetName, residualLine.root, keys(1, clearTarget, clearReference));

  root.append(section.root, target.root);

  return {
    root,
    paint(doc, residual) {
      const i = doc.selected;
      const lane = doc.lanes[i];
      const slot = currentSlot();
      const held = lawState(doc.laws[i]) !== "FREE";
      const bypassed = !!slot?.off?.[i];
      section.value.textContent = `S${i + 1}`;
      section.value.style.color = `var(--s${i + 1})`;
      poleHz.set(lane.pole_r > 0 ? lane.pole_hz : null);
      poleR.set(lane.pole_r > 0 ? lane.pole_r : null);
      zeroHz.set(lane.zero_r > 0 ? lane.zero_hz : null);
      zeroR.set(lane.zero_r > 0 ? lane.zero_r : null);
      scale.set(20 * Math.log10(Math.max(lane.scale, 1e-6)));
      const packed = (slot?.words ?? doc.words)?.[i];
      wordText.textContent = packed ? packed.map((v) => v.toString(16).padStart(4, "0")).join(" ") : "—";
      holdKey.textContent = held ? "FREE" : "HOLD";
      holdKey.classList.toggle("on", held);
      bypassKey.textContent = bypassed ? "ENABLE" : "BYPASS";
      bypassKey.classList.toggle("on", bypassed);

      targetText.textContent = slot?.targetName ?? "—";
      residualLine.set(residual);
      clearTarget.disabled = !slot?.target;
      clearReference.disabled = !doc.reference;
    },
  };
}
