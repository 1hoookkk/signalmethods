const KINDS = [
  ["mouths", "MOUTHS"],
  ["poses", "POSES"],
  ["scaffolds", "SCAFFOLDS"],
  ["frames", "KEPT FRAMES"],
  ["architectures", "FACTORY"],
  ["bodies", "HERO BODIES"],
];

export function renderLibrary(el, lib, onPick) {
  el.textContent = "";
  for (const [kind, label] of KINDS) {
    const items = lib[kind] || [];
    if (!items.length) continue;
    const head = document.createElement("div");
    head.className = "lib-kind";
    head.textContent = `${label} (${items.length})`;
    el.appendChild(head);
    for (const item of items) {
      const row = document.createElement("div");
      row.className = "lib-item";
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
