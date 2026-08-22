# TRENCH surface contract
2026-07-31. Law, not brainstorm. Supersedes the "nothing new at rest" phrasing
of WRAP_BRIEF.md; everything else there stands.

## The three axes

Every visible thing is classified on three independent axes:

- **Ownership** — which machine it belongs to: FILTER, CONSOLE, or SOURCE.
- **Interaction type** — parameter (continuous, automatable), modifier
  (alters an existing control, adds no processor family), command (an act,
  not a position), or disclosure (UI-only: drawers, menus, tour).
- **Visibility** — rest (always on the face), active (state-gated feedback),
  editing-only (visible only while the gesture/disclosure is open).

## Voices

- **Coral** = the sound (trace, FX lamp on the cool glass).
- **Teal** = your hands (wheel paint, knob rings, glints on the warm plate).
- **Amber** = the source (RESAMPLE/GEN only). Theme token `amber` is this
  colour and nothing else; no local copies.
- Engravings and readout boxes are materials, not voices: engraved words
  never change; readout (LCD family) numbers/letters may.
- Disclosure state (drawer open, menu open, tour visible) never acquires a
  voice and never writes on the glass.

## The entrance law

No status clutter at rest — but **every hidden capability keeps one quiet,
permanent entrance.** Onboarding may explain an entrance; it can never
replace one. A filter condition must never gate a console or source door.

## The map

| Function | Owns | Type | Rest affordance | Active feedback | Persists | NO FILTER behaviour |
|---|---|---|---|---|---|---|
| BODY | filter | parameter | selector well | name in readout | project | shows "NO FILTER" |
| Glass | filter | display | graticule + trace | trace/ghost/boot only | — | flat trace; never a status bar |
| MORPH / Q | filter | parameter | wheels + readouts | paint position | project | step back (dim, disabled) |
| KEY | filter | modifier | KEY box | letter in its box only | project | box hidden |
| Modulation | filter | modifier | via FX door | moving curve; chip lamp | project | inert; not offered |
| GRIT | filter | parameter | via FX drawer | ring teeth + engine flare | project | dim, disabled in drawer |
| PREAMP | console | parameter | via FX drawer | ring teeth | project | playable |
| SLAM | console | parameter | drag the glass; drawer knob | cursor readout while dragging | project | playable |
| MIX | console | parameter | rail + needle | needle position | project | playable |
| FX door | console | disclosure | dim chip + lamp (permanent) | lamp lit when mod/dist hot | drawer state: project | **visible; opens distortion directly** (modulation has nothing to act on) |
| RESAMPLE | source | command | machined button in the tab | amber ring + GEN tile at the control | generations: project | works (it is console-side) |
| GEN n | source | sound state | — (LIVE shows nothing) | amber tile on the control only | project | same |
| Onboarding / menus | — | disclosure | replay hotspot | spotlight card | shown-once flag | same |

## Consequences enforced in code

1. The FX chip is never hidden by the filter gate — it is the console's only
   door. In NO FILTER it opens the distortion drawer directly.
2. The glass never announces source state; GEN speaks from the RESAMPLE
   control alone.
3. `Theme::amber()` is the real amber (0xffb8862e) and ReentryChip consumes
   the token.
