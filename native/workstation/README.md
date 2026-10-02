# HEADSPACE

The authoring screen for TRENCH bodies. One ImGui window on D3D11, linked straight to
`native/core` through `Source/slice`.

The authority is the code and its acceptance suite:

1. `Tests/SliceTests.cpp` — the executable contract
2. `Source/slice/` — the state model it asserts

Everything else in this folder is history. `HEADSPACE_SPEC.md`, `DECISIONS.md`,
`FIELD_PUSH.md`, `MAX_TEST_PROMPT.md` and `archive/` describe earlier surfaces (a WebView2
page, a FIELD/PUSH cube, a four-quarter hand-painted screen) that are no longer in the tree.
They are not authority and several of their claims about the tree are false.

## Build, test, run

    native\workstation\build_headspace.cmd          # configure, build, ctest
    cmake --build --preset headspace                # build only
    ctest --preset headspace                        # acceptance only
    Launch_HEADSPACE.bat                            # start the built app

The app is `out/build/vst3/plugin/workstation/HEADSPACE.exe`. `--smoke` runs a scripted
interaction and writes `headspace-*.bmp` plus a failure log; `--compact` opens a smaller
window. Close any running `HEADSPACE.exe` before relinking.

## The screen

Connect a MIDI input before launch; HEADSPACE opens the first available input on channel 1.
Notes play a monophonic saw at MIDI pitch, velocity controls excitation level and Q,
and CC1 (mod wheel) controls Morph. Last-note priority, sustain (CC64), two-semitone
pitch bend, and all-notes-off are supported. Escape releases MIDI and returns to the
110 Hz audition source. Hover the source label to see the connected device.
MIDI messages are consumed on the UI frame before the next audio publication.

Shift+1–4 recalls saved corners without stamping. Plain 1–4 still stamps the audible
candidate. Performance updates the transient candidate; stamping remains explicit.
Audio traverses packed words over 256 samples, decoding each step through the same
runtime resolver as the plot, while retaining cascade delay state. A new target
starts from the running words. The plot shows the destination during this short glide.
The separate rate-compensation gain glides logarithmically between endpoint values;
its frequency search runs when resolving the destination, not on every audio sample.
Monitor protection remains active; preserved state alone does not prove click-free
modulation or physical-model behaviour.

Click the source label to switch SAW / IMPULSE. In IMPULSE, each MIDI note-on injects
one velocity-scaled impulse; note-off leaves the cascade to decay. The STRIKE button
and Shift+1–4 also strike the body. Its resonances determine the ringing pitch;
MIDI note number does not transpose the body in this mode. Events received within
one UI frame are combined into one strike at the next audio block.

While playing or in IMPULSE mode, 1–4 captures a coherent 30-word snapshot from the
last rendered audio block, including a glide in progress. It stamps only the chosen
corner, with normal undo and byte-exact export. This records geometry, not delay
registers, rhythm or audio. The snapshot follows software rendering, before device
output latency; it is not a timestamped capture of the sample reaching your speakers.
While idle in SAW mode, stamping continues to capture the draft candidate.

Clock-armed four-beat capture, chord-to-pole voicing, and live source/filter
deconvolution are not implemented in this slice.

- **Pad** — MORPH by Q over the four corners. Dragging auditions the canonical packed-word
  interpolation; the 1–4 keys or the corner chips stamp the live sound into a corner.
- **Cascade plot** — the response of what plays, DC-referenced, with the live spectrum drawn
  from the same audio (Span's 1995 analysis chain, `Source/slice/Span.cpp`).
- **Acoustic map** — the twelve Klatt vowels placed by F1/F2; hovering previews, clicking
  picks the body.
- **Macros** — tract scale, tension, stress, tilt. Draft-only until a stamp.
- **Stage view** — per-stage pole and zero editing, with the stage diagnostics.
- **Export** — the four corners as 240 bytes.

## Files

- `Source/slice/Model.*`: the corners, the packed-word grammar, `resolve` at any sample rate.
- `Source/slice/Audition.*`: the live draft — pad, macros, stage edits, selection.
- `Source/slice/Audio.*`: the device callback, the monitor, the live spectrum tap.
- `Source/slice/Span.*`: Peevers's 1995 analysis chain, ported from the decompilation.
- `Source/slice/Response.h`: the response curve and the listening reference.
- `Source/slice/History.h`: whole-gesture undo and redo.
- `Source/app/Main.cpp`: the window, the screen, the smoke script.
- `Tests/SliceTests.cpp`, `Tests/RenderChecks.h`: the acceptance suite, headless.

## Reports

The suite writes its evidence beside the working directory: `headspace-source-admission.txt`,
`headspace-span-port.txt`, `headspace-rate-agreement.txt`, `headspace-monitor.txt`,
`headspace-audio-levels.txt`, `headspace-dc-reference.txt`.
