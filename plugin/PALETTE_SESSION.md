# Tyson's TRENCH Palette session

This is Tyson's individualized Palette pipeline: shipping knob travel is calibrated from Tyson's own ratings and matches, not from an assumed generic listener. The observations attributed below to Martens describe the published Palette method. Choices such as one TRENCH axis per pass, the fitter order, the 129-entry tables, the real plug-in renderer, and the axis order are TRENCH adaptations.

The goal is not to make distortion disappear into the filter. INPUT and OUTPUT remain two independent Mackie-derived desks around the filter cascade. With **No filter** selected, both desks remain available as a distortion engine. Z/BITE is the filter-owned nonlinearity. That separation lets a user choose desk color without being forced to choose a filter, while the Palette tables make every control's travel Tyson-shaped.

## The three-stage flow

### 1. Sound selection: choose the usable range

Choose one axis, the supplied loop, a body, monitoring level, speakers or headphones, and the musical context. Listen broadly before collecting ratings. Move the target control until the bottom and top are both useful, distinct, and free of accidental failure modes. **Write down those chosen raw-parameter endpoints before the rating session.** They are the session's `low` and `high`; they must not be inferred later from favorite ratings.

The selection record should include:

- date, plug-in build identifier, loop path and BPM;
- body display name;
- axis and chosen low/high raw values;
- monitoring path and fixed playback level;
- a short reason each endpoint is usable and where the unusable region begins.

Sound selection is contextual. A range selected on the supplied loop with No filter is evidence for that loop/body/context, not a universal hearing law.

### 2. Unidimensional scaling: rate five levels, fit, invert, listen

Render five inclusive knob positions across the chosen endpoints through the real processor and the current shipping curve. Preview all five before rating. Randomize presentation order outside the renderer, allow replay, ask for a 1–9 rating along only the named dimension, repeat presentations, and preserve every raw trial. Do not replace the raw trial file with averages.

Martens used a 3-by-5 contextual stimulus matrix, adaptive passes, and polynomial fits while building an individualized Palette. TRENCH adapts that method to one axis per pass: average repeated ratings at each raw value; fit a power-law psychophysical function; use the monotone fallback when that law misfits; normalize between the session's recorded endpoints; then invert the fitted function by bisection at all 129 knob positions. The result is baked into the axis table in `source/parameters/Curves.h`.

Rebuild and listen after every baked pass. Five initial judgments are a starting scale, not automatically a final shipping law. Repeat the pass when adjacent steps collapse perceptually, endpoints no longer feel useful, residuals show structure, or the rebuilt knob does not progress evenly in context.

The raw ratings CSV has one presentation per row:

```csv
raw,rating,presentation
0.000,1,1
0.000,2,2
0.250,3,1
0.250,3,2
```

Keep the CSV, the fitter's printed function and residuals, its PNG quick plot, and the baked `Curves.h` together as the audit trail. A typical bake command is:

```powershell
python plugin\tools\gen_curves.py --axis slam --ratings C:\path\to\output-drive-ratings.csv --curves plugin\source\parameters\Curves.h --plot C:\path\to\scratchpad\output-drive-fit.png
```

Use the command-line help from the checked-in script if its final option spelling differs; never hand-edit the 129 numbers.

### 3. Timbre matching: standard versus adjustable comparison

Matching is a separate judgment. Hold a standard fixed and let Tyson adjust a comparison along only the dimension being matched. Record the match, repeat it from different starting positions, and preserve the individual trials before averaging or baking anything.

For OUTPUT, **OUTPUT = 0 is the standard**. `TRENCH_Stimuli` whole-loop RMS-matches the other four renders to that standard to remove gross loudness as an easy cue. Tyson then judges drive character, not “which one is louder.” RMS equality is not perceptual equality and is never acceptance by itself. The shipping `slamTrim` table remains unity until Tyson records a subjective trim match and that match is reviewed and baked.

## Axis order

Run one complete sound-selection, scaling, matching, rebuild, and listening pass before moving on:

1. OUTPUT drive, then OUTPUT trim
2. INPUT
3. Z
4. MORPH
5. Q
6. FOLLOW depth

OUTPUT and INPUT belong to the independent post- and pre-filter desks. Z/BITE belongs to the filter. MORPH and Q describe the body's filter geography. FOLLOW depth scales the envelope-driven movement while Movement itself stays at its declared default OFF during isolated stimulus rendering.

Sandell and Martens (1992) provide prototype/PCA/interpolation background for MORPH's lineage only. That paper does not define the current TRENCH MORPH law.

## Render the supplied loop

Build the headless tool in the VS 2022 developer shell:

```powershell
cmake --build out\build\vst3 --config Release --target TRENCH_Stimuli
```

Render the first OUTPUT set with the selected body and recorded endpoints:

```powershell
out\build\vst3\plugin\TRENCH_Stimuli_artefacts\Release\TRENCH_Stimuli.exe `
  --input "C:\Users\hooki\Downloads\d rich loop 140.wav" `
  --bpm 140 `
  --axis output `
  --body "No filter" `
  --output-dir "C:\Users\hooki\Downloads\palette-output-session" `
  --low 0 --high 1
```

The output directory contains five 32-bit floating-point WAV files named by axis, level, and knob position, plus `manifest.json`. The manifest records the absolute source and output paths, BPM, body, audio format, endpoint range, processor build identifier, shipping-curve internal value, pre-match RMS, level-match gain, common anti-clip scale, final RMS, and final peak for every level. Keep the manifest with the randomized rating sheet; it is the link from an opinion back to the exact sound that prompted it.

For another axis, change only `--axis`, the body when the axis requires a real filter, the recorded endpoints, and the output directory. Supported axis names are `output`, `input`, `z`, `morph`, `q`, and `follow`.

## Session checklist

- [ ] Freeze the loop, BPM, body, monitoring path, playback level, and build identifier.
- [ ] Explore broadly and record usable low/high raw endpoints before rating.
- [ ] Render five levels and archive the WAVs plus `manifest.json`.
- [ ] Preview all five; randomize trial order outside the renderer; allow replay.
- [ ] Collect repeated 1–9 judgments and retain every raw CSV row.
- [ ] Inspect the fitted function, point residuals, monotonicity, endpoints, and PNG.
- [ ] Bake the 129-entry inverse table, rebuild, and listen to the real knob.
- [ ] Run fixed-standard matching separately; for OUTPUT start from OUTPUT = 0.
- [ ] Bake trim only after subjective matching; do not treat RMS as Tyson's verdict.
- [ ] Stop the pass when the rebuilt travel is ordered, useful at both ends, and has no unexplained perceptual pile-ups or jumps. Otherwise revise the range or collect another pass.
- [ ] Move to the next axis only after Tyson accepts the current axis by ear.

The final acceptance question is practical: does every part of the rebuilt control produce a deliberate, repeatable musical choice on the fixed session material? A clean fit and green tests establish mechanics; Tyson's listening establishes the palette.
