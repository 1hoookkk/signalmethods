# HEADSPACE handoff, 2026-09-06 late night

Read this before anything else. It is where the discussion with Tyson stands.

## The model, in Tyson's terms, agreed at the end of the night

Massie's cube is not four found sounds. It is one sound and directions of variation, and the
chip's lerp is the machine that makes every version in between.

1. The base. One anchor, thirty words, the prototype. The PCA paper's centroid.
2. The axes. Each axis is a variation of the base as a vector in word space: semitones per
   row on the frequency words, dB per row on the resonance and gain words. The factory bodies
   show E-mu's two: the MORPH axis is mostly the skeleton shifted with zeros re-placed, the
   Q axis is a sharpening. A P2K body is a two-axis cube around a base; Morpheus was three.
3. The cube. Corners are generated, not chosen: base, base plus X, base plus Y, base plus X
   plus Y. The puck mixes amounts, and because the codes are logarithmic the mix is musical.
   Every point is a six-section serial cascade the chip plays.

Picking four unrelated anchors is the degenerate case where the axes are differences to
other sounds. It is what the current app does.

Axes, from most grounded: the factory's own (principal directions in word space of the 66
M0-to-M1 deltas and the 66 Q0-to-Q1 deltas, rows matched by frequency), hand axes
(transpose, sharpen, cut, open, front, back), and toward another sound (B minus A).

Picker makes or finds the base. Axes make the cube. Stage plays it. Hopper keeps cubes that
share a base or an axis. One model.

First slice proposed, not yet ruled: a base, two amount sliders on the factory's own two
axes, the four corners generated, the puck, W.

## What is built and green, commit 73cbdae1 plus vowelWords

`native/workstation/Source/app`: Quad (words, corners, lerp, file, pack), Library (132
factory corners, 12 Klatt vowels from the bank, wav reader through the core's speech_poles,
vowelWords for four formants), Session (corners A B C D = M0 Q1, M1 Q1, M0 Q0, M1 Q0,
PRESET, keep, reads, undo, keys), Screen (Filter Factory look: dark, corners named, puck,
response fills the stage, rails). Tyson rejected the dark look and does not want it verbatim;
he wants a light, hyper-optimised, intuitive surface with the picker and the hopper. 30 checks
green: `native\workstation\build_headspace.cmd`. Close HEADSPACE.exe before relinking.

## Rulings that stand

Word space is the only truth. The tool measures nothing. Six sections, all active, not six
bells. Only four sounds ever blend, the chip's way. Magnitude plots are 3:2 boxes on the fixed
30 dB grid. One font. Stop at the smallest end-to-end surface. No verdicts, no gain rules on
the path. Refused: hop verdicts, DC normalisation on write, stage-to-formant roles, many-way
blends, physics gestures, hairlines above the frame, neural inversion (vector fitting is the
reader when a reader is built). Cherry-pick research pastes against these.

## Words

anchor, base, axis, cube, corner, puck, keep, write, body. Not column, hop, lattice, field,
star.
