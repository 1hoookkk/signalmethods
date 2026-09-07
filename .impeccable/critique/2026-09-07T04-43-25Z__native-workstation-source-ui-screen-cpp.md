---
target: HEADSPACE main screen and spectrogram window
total_score: 17
max_score: 40
na_heuristics: 
p0_count: 1
p1_count: 3
target_identity: "file:C:\\Users\\hooki\\trench-native\\native\\workstation\\Source\\ui\\Screen.cpp"
target_fingerprint: "sha256:ed9eb18d04fbc6306816d40d5a0492e24ea55de66c59e4389248cea8ea43affe"
target_path: "C:\\Users\\hooki\\trench-native\\native\\workstation\\Source\\ui\\Screen.cpp"
timestamp: 2026-09-07T04-43-25Z
slug: native-workstation-source-ui-screen-cpp
---
Method: dual-agent (A: Opus design review, B: Opus detector and measurement). Detector clean (CSS rules on C++; not meaningful). Target: native/workstation/Source/ui/Screen.cpp, renders headspace.png, headspace_mother.png, spectrogram.png.

## Design Health Score: 17 / 40 (Poor)

| # | Heuristic | Score | Key issue |
|---|---|---|---|
| 1 | Visibility of system status | 1 | state line clipped at the window edge; no dirty mark, no MIDI/audio indicator |
| 2 | Match with the real world | 3 | vocabulary right; MORPH, FREQUENCY, STRESS never shown |
| 3 | User control and freedom | 3 | undo covers the model; no visible undo; Escape does not abort a drag |
| 4 | Consistency and standards | 2 | three rails, three right-hand grammars; diamond means two things; A/B double meaning |
| 5 | Error prevention | 1 | plain W writes; bake tick 6 px from a drag handle |
| 6 | Recognition rather than recall | 1 | ~30 keys shown nowhere; ten glyphs, no tooltips, no cursor change |
| 7 | Flexibility and efficiency | 3 | every action keyed; contextual wheel; drag to any cell |
| 8 | Aesthetic and minimalist | 2 | fourteen plots in one blue; five sizes one weight; no hierarchy |
| 9 | Error recovery | 1 | two strings, 10 px dim at the bottom |
| 10 | Help and documentation | 0 | none in the app |

## Design specificity
~25% authored: numbered section handles on the fixed frame, the vowel chart with schwa crosshair, H raw words, the 240-byte write. ~75% generic: three-tab card list with thumbnails, icon rows, chevron carets, drop box, unlabelled hairlines. Measured luminance: keyboard 22% of light on 5.5% of area (mean L 145 vs page 36.6); stage below page mean; all other regions within 10% of the mean.

## Priority issues
- [P0] The wrong hero: keyboard owns the light, stage does not. Fix: keyboard as a rule of keys (hairline, ticks, ink on the sounding key); stage curve thicker, the only full-strength blue; other curves muted. bolder.
- [P1] Three things at once on the left: mother, chart, cards, drop box. Fix: sources move to the G window (chart as landmarks on its surface); the left keeps the mother alone, larger. distill.
- [P1] Rails unlabelled; bake is a tick beside a handle. Fix: MORPH/FREQUENCY/STRESS 10 px small caps in the gutter, numeric readout at each right end, rails aligned to the cards' left edge, BAKE as a word 20 px clear of the diamond's travel. typeset.
- [P1] Bottom right ambiguous: plot reads as corner A and as what plays; arrows copy A onto itself; icon sources. Fix: live plot (output over response, input dimmer) draggable onto a cell; arrows, Keep, file name removed; sources as words. clarify.
- [P2] Spectrogram slab: one blue, axes under the fills, colliding time labels. Fix: axes last, amplitude colour ramp blue to orange, newest in ink, 32 slices. clarify.

## Persona red flags
Alex (power): no QWERTY notes, no MIDI status, no cursor change over ~40 hit regions, Carve an invisible 240 px drag, no numeric entry, pin menu 50 rows unsearchable, W unmodified.
Jordan (first-timer): empty state is "+", three unlabelled rails, ~30 keys with no legend or menu, ten uncaptioned glyphs (tick reads OK), disabled controls at ~2:1 contrast, chart without title or units, stray drop becomes the loop.

## Minor observations
Look::yellow unused; H words in a proportional face; corner C caret 8 px from letter D; stage curve leaves the frame with no clip mark; rows 4 and 5 absent with no hint; card names truncated where they differ while anchor B shows the full name; Bodies underline 71 px for a 42 px word.

## Questions to consider
- Why does an inert keyboard own a fifth of the light?
- With sources in the G window, is the four-quarter grid still the structure?
- If the ear decides, what are fourteen unreadable thumbnails for?
