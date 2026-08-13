# Import checklist

Import files deliberately. Do not copy the old repository wholesale.

- [ ] Add Station sources, excluding `laws.json`, `law.rs`, and
      `laws_panel.rs`.
- [ ] Remove law-related module declarations, command-line arguments, reload
      actions, layout regions, and UI copy from Station.
- [ ] Add the cascade view as an inspector, not a pass/fail policy panel.
- [ ] Add authoring tools that are on the current operator path.
- [ ] Add only references actually used, with provenance.
- [ ] Add recipes separately from generated bodies and renders.
- [ ] Pin `trench-core` to an existing commit from `trench-plugin`.
- [ ] Use canonical exported counts and sizes; do not preserve byte literals.
- [ ] Verify a clean Station build.
- [ ] Verify inspectors against the same body and sample rate.
- [ ] Commit the import in small, reviewable groups.

