# Prompt for an outside tester: read the code, then play it as Max

You are two people in one session. First a code reviewer with no stake in the work. Then Max,
a power user: a synth player who lives on the keyboard and a MIDI controller, hates dragging
lists, wants to play a filter and see it move, and abandons anything that looks like it does
something and does not.

The product is HEADSPACE, a private authoring tool for TRENCH, a morphing filter plugin. The
law, which every claim in the code must serve: in a serial cascade section gains multiply and
responses add in dB; a corner is six sections of five words, sixty bytes; a body is four
corners and the chip's lerp; the ear decides. The tool exists so a person can find a sound by
moving between placed sounds, put it in a corner, and write a 240-byte body the plugin plays.

## Where things are

- Repository: `C:\Users\hooki\trench-native`, branch `codex/headspace2`.
- Spec: `native\workstation\HEADSPACE_SPEC.md`. Read it first, all of it, including the rulings
  at the top and the queue at the end.
- Code: `native\workstation\Source\app` (Session, Audio, Library, Quad, Main),
  `native\workstation\Source\ui` (Screen, Mother, Stage, Palette, Body, Engine, Keyboard,
  Spectrogram, PinMenu, Look, Plot), `native\workstation\Source\dsp` (Peevers, VectorFit).
- Tests: `native\workstation\Tests\QuadTests.cpp`, headless, run by
  `native\workstation\build_headspace.cmd` (builds and runs ctest, test name trench_quad).
- Launch: `Launch_HEADSPACE.bat` at the repository root. Close the app before any rebuild.
- Running record: `plugin\NEXT_SESSION.md`, the last twenty entries are today.

Do not modify any file. Do not commit. Do not open the plugin. Take a screenshot at every
numbered step below and keep them.

## Part one, the reviewer

Read the spec, then the code, then the tests. Answer these in writing, with file and line:

1. For every control drawn on the screen and in the reading room, state what it changes: the
   words of a corner or anchor, what plays, a file, or nothing. A control that changes nothing
   is a finding.
2. For every test, state whether it asserts behaviour (words, audio buffers, files, positions)
   or only that something was drawn. List tests that would pass with the feature removed.
3. Find anything that is computed and never used, drawn and never read, or claimed in a name
   and not done in the body. The owner's fear, in his words: "clever looking things that have
   no function." Prove or disprove it per item.
4. Check the three rules the owner cares about: nothing on the face changes itself; a copy to a
   corner stores the exact heard words; reads never invent a zero (the LPC read parks them, the
   FIT read measures them). Cite the lines that enforce each, or the lines that break it.

## Part two, Max, with computer use

First, blind. Launch the app and, with no instructions beyond this one sentence, do the loop:
load two very different sounds, play a phrase while morphing with the wheel, replace one sound
while it keeps sounding, find a sound in between, put it in corner A, keep exploring without
changing A, write the body, reload it and hear the same thing. Time yourself. Write down every
place you stopped and looked for something, every control you tried that did nothing, every
word you did not understand, and where you gave up if you did. That list is the main finding;
a screen that needs the steps below has failed.

Only then, the steps, to check what you found against what was meant.

Plug in a MIDI keyboard if one is present; if not, the Z row of the computer keyboard is a
keyboard (Z is C3, up to M as B, Page Up and Page Down move the octave). Launch the app.

Perform the acceptance loop exactly, and at each step write what you expected, what you saw,
what you heard, and a verdict of works, half, or broken:

1. Load two very different anchors. Click the caret on the left anchor top-left, pick a 303
   note; click the caret on the right anchor, pick an Aud Bell. Or press slash, type a name,
   Enter, and drop the card on the anchor. Expected: the anchor's curve changes, the sound goes
   on from where it was, no jump back to the grid.
2. Hold a chord and play a phrase while moving the mod wheel, or drag the MORPH rail. Expected:
   the stage's curve moves as the wheel moves, the white spectrum on the stage moves with your
   notes, and the sound morphs with no clicks.
3. Replace one anchor while notes are sounding. Expected: the sound keeps going; the sweep
   continues against the new anchor.
4. Find an intermediate sound on the rails. Press 1. Expected: corner A in the grid takes
   exactly what you heard; the rails keep sweeping; A does not follow the rails afterwards.
5. Put the pad on corner B (drag the diamond to the top right of the grid, or press the B
   caret). Move the rails. Expected: B changes as you move them, the stage shows B's handles,
   and what plays is B.
6. Drag a handle on the stage sideways and up. Expected: only that section moves, the curve is
   redrawn from the words, the level trims to 0 dB at DC when you let go, and an undo (Ctrl+Z)
   restores it exactly. Click ZEROS, click on the curve where there is no square. Expected: a
   zero wakes on that row.
7. Press Ctrl+W. Expected: a file `headspace_<date>_<time>.body240` appears in
   `plugin\presets\user`, the word WRITE lights briefly.
8. Close the app, relaunch it, and confirm the anchors, corners and captures came back by name.
   Then load the written body in the plugin's dev build if you know how; if not, say so and stop.
9. Press Ctrl+G. In the reading room choose FAMILY, pick "303 open", play the Z row. Expected:
   the notes play the sampled 303 on their own path (the stage on the main screen must not
   move), the surface draws them. Click a slice, press Enter. Expected: a new capture named by
   the family, the note and the time lands in the target, and the main screen's target shows it.
10. Turn on KEY → FREQUENCY and VELOCITY → STRESS under the source words, put the pad between
    corners, and play soft and hard, low and high. Expected: the stage's curve moves with the
    key and the velocity; on a corner the routes go dim and do nothing.

Then judge as Max, in plain words: what did you reach for that was not there; what did you
have to drag that you wanted to type; which words on the screen you did not understand; which
control you touched that did nothing; what took more than three actions that should take one.

## Report

One document. Part one as a table: control, what it changes, file:line, verified by which
test. Part two as the ten steps with the four fields each and the screenshots named by step.
Then a ranked list, worst first, of things that look like they work and do not. Do not soften
anything; the owner would rather hear it from you than from a customer. Write in plain
English, no jargon that is not in the spec.
