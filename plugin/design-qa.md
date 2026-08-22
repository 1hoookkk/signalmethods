# MIX wheel revert QA

- source visual truth: `C:\Users\hooki\OneDrive\Pictures\Screenshots\Screenshot 2026-08-09 204310.png`
- implementation screenshot: `C:\Users\hooki\trench-x3-clean\trench_face_mix50.png`
- viewport: source face cropped from 400 x 569 to 326 x 503; implementation captured natively at 326 x 503
- density normalization: both compared at 326 x 503 logical pixels, 1x
- state: 2-Pole Lowpass face; the source MIX value is not exposed, so material and geometry are compared in the full view while travel is verified separately at 0, 50, and 100 percent
- full-view comparison: `C:\Users\hooki\trench-x3-clean\scratchpad\mix_revert_qa\source_vs_revert.png`
- focused MIX comparison: `C:\Users\hooki\trench-x3-clean\scratchpad\mix_revert_qa\mix_focus_compare.png`

## Findings

- No P0, P1, or P2 mismatch remains within the requested MIX-wheel scope.
- The implementation is back on the original neutral 64-frame strip at 12 x 94 pixels per frame. Its live blob is byte-identical to the first solved MIX implementation at commit `7841c2d9`.
- The wheel is again independent of the active palette: no curve colour, theme tint, or code-painted position light is drawn over it.
- The original interaction is restored: direct vertical drag, Shift fine adjustment, mouse-wheel adjustment, double-click reset, and one sprite pose per normalized parameter value.
- FaceShot's real component gesture set MIX to 0.500 and reported `MIX wheel 0.500 cue capture PASS`. Captures at 0, 50, and 100 percent were also produced.
- The surrounding palette, typography, graph, MORPH/Q wheels, and lower-room layout were intentionally left untouched.

## Comparison history

1. Earlier drift added palette state to `ThinWheel`, a painted position rung, and an oversampled replacement asset.
2. The asset had already been restored to the historic 64-frame strip; this pass restored the component boundary itself by removing the theme argument/state and retaining the original frame drawing and gesture law.
3. Post-fix evidence is the rebuilt 1x FaceShot capture plus the passing MIX gesture and three travel-state captures.

## Required fidelity surfaces

- fonts and typography: MIX label unchanged and aligned with the source; typography outside MIX is out of scope.
- spacing and layout rhythm: MIX aperture, wheel width, and vertical relationship to its label match the normalized source.
- colors and visual tokens: the wheel remains neutral black/grey hardware and does not consume the active accent token.
- image quality and asset fidelity: exact historic raster asset restored; no procedural paint or replacement graphic is used.
- copy and content: `MIX` remains the sole visible label; the value is reported on the glass only during interaction.

## Final result

final result: passed
