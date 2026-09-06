# HEADSPACE

One vowel space, one sound. Drag the gold mark or type F1 and F2 in Hz. The canvas uses note spacing: F2 decreases to the right, F1 increases downwards. Reference names sit at their measured formants: 12 Klatt 1980 vowels, 48 Hillenbrand 1995 vowels, and 16 DVTD subject-1 vowels.

The mark sets the first two formants. The surrounding references supply the other formants, widths, gains and row-6 low shelf through the existing chord blend. The response beside the canvas is the sound sent to the audio engine.

## Run

In MATLAB R2025b:

```matlab
cd('C:\Users\hooki\trench-native\native\workstation')
addpath toolbox
trench.setup;
app = trench.launch;
```

- PLAY starts or stops sound; SAW and PINK NOISE choose the source.
- Click a reference name to hear it exactly; dragging within six pixels of its position also snaps to it.
- Arrow keys move the mark one semitone in the indicated screen direction. Ctrl+arrows move 0.2 semitone. Typing in an Hz box keeps the normal text-editing keys.
- Keys 1 to 4, or the four bottom buttons, choose the current corner. Ctrl+S saves the current chord into that corner. Saved corners persist in `banks/HEADSPACE.bank.json`.
- WRITE BODY FILE becomes available when four corners are filled. It writes 240 bytes to `plugin/presets/user/headspace_<stamp>.body240`; hover over the button to see the written path.

Use a fresh MATLAB session after updating the class files. `delete(app)` closes the tool.

## Build and test

The existing `trench_bridge` and `trench_audio` MEX files and the model are unchanged by this screen reduction. If a build is needed, run `build_mex.cmd` through its MSVC environment wrapper, or `buildtool mex` in MATLAB.

All tests use invisible figures. `buildtool check` runs `tHeadspace`; `buildtool test` runs the C++ and MATLAB suites. For this slice:

```matlab
r = runtests({'Tests/tHeadspace.m','Tests/tShot.m'});
assertSuccess(r);
```

`tShot` writes `artifacts/shots/headspace.png`. CTest registrations remain `trench_workstation_headspace` and `trench_workstation_shot`.

## Data and decisions

Reference frames come from the existing three factory banks. Table rows without measured F4/F5 keep those slots inactive; no upper formants are invented. The four-corner bank is separate from the reference data.

See [DECISIONS.md](DECISIONS.md) for the quadrilateral bounds, interpolation and source evidence.
