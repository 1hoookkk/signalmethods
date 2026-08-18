const KINDS = [
  ["mouths", "MOUTHS"],
  ["recordings", "RECORDINGS"],
  ["poses", "POSES"],
  ["scaffolds", "SCAFFOLDS"],
  ["architectures", "FACTORY (study only)"],
];

export function renderLibrary(el, lib, onPick, seedMenu) {
  el.textContent = "";
  const templates = (lib.vocabulary && lib.vocabulary.templates) || [];
  if (templates.length) {
    const head = document.createElement("div");
    head.className = "lib-kind";
    head.textContent = "TEMPLATES";
    el.appendChild(head);
    for (const entry of templates) {
      if (!entry.template) continue;
      const row = document.createElement("div");
      row.className = "lib-item";
      row.textContent = entry.type;
      row.title = `${entry.members.length} filters · ${entry.template.sentence}\nclick: seat into active corner · right-click: import all four corners`;
      const pick = (right) => {
        el.querySelectorAll(".lib-item.active").forEach((n) => n.classList.remove("active"));
        row.classList.add("active");
        onPick("templates", { ...entry.template, type: entry.type }, right);
      };
      row.onclick = () => pick(false);
      row.oncontextmenu = (e) => {
        e.preventDefault();
        pick(true);
      };
      el.appendChild(row);
    }
  }
  if (seedMenu && seedMenu.length) {
    const head = document.createElement("div");
    head.className = "lib-kind";
    head.textContent = "SEED FROM TABLE";
    el.appendChild(head);
    for (const cat of seedMenu) {
      const catHead = document.createElement("div");
      catHead.className = "lib-item";
      catHead.style.cssText = "color:var(--axis-ink);font-size:10px;cursor:default;padding-left:12px";
      catHead.textContent = cat.category;
      el.appendChild(catHead);
      for (const entry of cat.entries) {
        const row = document.createElement("div");
        row.className = "lib-item";
        row.textContent = entry.label;
        row.title = cat.category;
        row.onclick = () => {
          el.querySelectorAll(".lib-item.active").forEach((n) => n.classList.remove("active"));
          row.classList.add("active");
          onPick("seed", entry, false);
        };
        el.appendChild(row);
      }
    }
  }
  for (const [kind, label] of KINDS) {
    const items = lib[kind] || [];
    if (!items.length) continue;
    const head = document.createElement("div");
    head.className = "lib-kind";
    head.textContent = `${label} (${items.length})`;
    el.appendChild(head);
    for (const item of items) {
      const row = document.createElement("div");
      row.className = kind === "architectures" ? "lib-item lib-study" : "lib-item";
      row.textContent = item.name;
      row.title = item.gloss || item.id;
      const pick = (right) => {
        el.querySelectorAll(".lib-item.active").forEach((n) => n.classList.remove("active"));
        row.classList.add("active");
        onPick(kind, item, right);
      };
      row.onclick = () => pick(false);
      row.oncontextmenu = (e) => {
        e.preventDefault();
        pick(true);
      };
      el.appendChild(row);
    }
  }
}
