# Prompt for a new chat: the TRENCH workstation as a marimo notebook

You are working in C:\Users\hooki\trench-native (a git worktree; run everything from there). Read CLAUDE.md
first and obey it: evidence/ is the only evidence root; no code comments in any language; every cmake build
runs through a .cmd wrapper that calls
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"; all tests
headless; never open windows on my screen; commit native work with explicit pathspecs only, other chats stage
plugin/. Read plugin/NEXT_SESSION.md (the running brief, newest entries at the bottom) and native/REWRITE_PLAN.md.
Then read native/app (the Qt workstation) for what it does, not how.

## The idea the tool exists for
A TRENCH body is a small set of snapshots. Each snapshot is a frame: a complex filter response, six
second-order sections in series, read from a real sound or a measured body or authored by hand. Frames sit on
the corners of a square (MORPH x Q; the Morpheus cubes add a third axis). Playing the square interpolates
between the frames in the engine's word space, which is a log-frequency glide of paired resonances, so the
sound morphs through a space of real spectra with one wheel. That is E-mu's method and it is the whole
product: interpolating between complex frames is what makes snapshots into an instrument. The workstation's
job is to make collecting frames and placing them on corners fast, and to let you hear the interpolation
while you do it.

## What exists
TRENCH is a musical filter (the plugin ships; the workstation is the authoring tool only). The engine truth is
native/core (C++): the 16-bit minifloat word codec, corner interpolation in word space, rewarp from a body's
datum rate, the level rule, transposition, the audition runner with BITE. Bodies: 33 Proteus 2000 / X3 at
plugin/presets/p2k/*.body240 (4 corners, 240 bytes, datum 44,100 Hz), 289 Morpheus cubes under
evidence/factory-data/morpheus (8 corners, datum 39,062.5 Hz). Frames: 132 bank corners, 14 type templates,
158 keyframes from audio in native/app/templates/keyframes_from_audio.json (heads, XL-1 waves, measured
vowels, Hillenbrand vowel medians, bodies, instruments) plus the Hillenbrand vowel space
(evidence/factory-data/hillenbrand-1995). The Qt app (native/app) already has: the FRAMES grid (304 frames as
tiles with a response sparkline, tabs by group, search, hover ghosts the plot and plays the frame, click
lands it on the editing corner with ANCHOR), the VOWEL SPACE tab (F1/F2 trapezoid, mouse over it plays the
vowel, click lands four poles), FROM AUDIO (SIX BELLS; SPEECH = order-12 LPC at 11,025 Hz, poles only),
ANCHOR across the square, a SO FAR column per row, one black curve on a fixed +-30 dB grid with a hard 0 dB
line, six rows with typed entry. It got the ideas right and the hands wrong: ambiguous sliders, no zero layer
on the plot, a surface that does not read as an instrument. Tyson's words: "the faders are the real friction
point", "there only needs to be one line", "where's the zero layer? dragging ambiguous sliders is not fun",
"it should be solely, first and foremost, the grid we talked about, and how interpolating between complex
frames is the key to snapshots".

## The task, in this order
1. Engine access first. The plan in native/REWRITE_PLAN.md: a small C ABI over native/core built as
   trench_core_c.dll, loaded with ctypes. Parity suite before any surface: all 33 bodies and 289 cubes
   round-trip byte for byte, every corner cascade matches the C++ path, the notebook's curve for a body at a
   pad matches the Qt app's within 0.01 dB. Never reimplement the engine in Python.
2. The grid. The frames library is the centre of the notebook: a matrix of snapshots (tabs by group, search,
   each tile drawing its own response), hover to hear the frame through the core runner (sounddevice), click
   to place it on a corner. The vowel space is a second way into the same grid. FROM AUDIO makes new frames:
   drop a sound in, get a frame (pre-emphasis 0.96 and Burg order 12 at 11,025 Hz for SPEECH, Tyson's
   MATLAB corner.m in evidence/research-results/matlab is the reference; SIX BELLS as in
   native/core/src/body_from_audio.cpp).
3. The square. Four corners (eight for cubes) holding frames; the pad plays the interpolation and you hear it
   move; ANCHOR pairs slots across corners so the glide is a log-frequency line, shown under the axis for the
   selected row; the same frame can be dropped on several corners to hold an anchor. For the Morpheus cubes
   show the cube itself: a rotatable 3-D wireframe (plotly's 3-D traces run natively in marimo, or three.js
   inside an anywidget when you need to drag the play point inside it) with a sparkline of each corner's
   frame at its vertex and the play point moving inside, so the eight snapshots and the space between them
   are one picture.
4. The plot as the editor of a frame, when you need to touch one: one line, fixed grid, poles and zeros as two
   visible layers of handles (peaks and valleys), drag across = frequency on a log scale, drag up = height,
   shift-drag = bandwidth. A custom anywidget (canvas, pointer events, Python callbacks), not plotly, not
   sliders. Numbers beside it in the Morph Designer grammar (TYPE, Frequency, Q, Peak Gain per row, LO and HI
   ends), typed entry lands on the nearest word.
5. Out: the SLOT -> TRENCH DEV file so the real plugin in FL hot-reloads the working body (native/app writes it;
   keep the format), and .body240 export with the datum rate.

## MATLAB
Tyson works in MATLAB R2025b (Signal Processing, DSP System and Audio toolboxes) and has his own scripts in
C:\Users\hooki\OneDrive\Documents\MATLAB (copies with a source note in evidence/research-results/matlab):
corner.m reads one WAV, takes a 50 ms slice, pre-emphasises at 0.96, fits Burg order 12 and prints the pole
lanes as frequency, radius and bandwidth; sound_to_skeleton.m, voice_body.m, fit_body.m, match_body.m do
the surrounding work. Integrate them, do not port them: install the MATLAB Engine for Python
(pip install matlabengine, matching the notebook's Python) and call the scripts from the notebook as
functions, `eng = matlab.engine.start_matlab()`, `eng.addpath(...)`, `lanes = eng.corner_lanes(path)`, with
a thin .m wrapper per script that returns arrays instead of printing. MATLAB is then the reference for FROM
AUDIO: when the engine is present the frame comes from his script, and the notebook's own Burg (numpy) must
match it within 1 Hz and 1 % bandwidth on the sung ah and the XL-1 lead before it is trusted without
MATLAB. Keep MATLAB optional: the notebook runs without it, with the core's FROM AUDIO.

Keep the Qt app until the notebook passes equivalents of the 62 native tests. Tyson has ADHD and is a
musician: short replies, one decision per message, decide routine things yourself, E-mu manual voice (frames,
morphing filter, Morph sweeps, Q raises resonance), no technobabble. Report progress as pictures (marimo can
export a widget to PNG headlessly through the browser) and numbers, never dumps.
