# Inflator source

`inflatorCurve` is the waveshaper polynomial published in RCInflator 2 (Oxford Edition), author lewloiwc, ReaTeam JSFX collection:
https://github.com/ReaTeam/JSFX/blob/master/Distortion/RCInflator2_Oxford.jsfx
(discussion: https://forum.cockos.com/showthread.php?t=256286)

For |y| < 1 that file computes `A*y + B*y^2 + C*y^3 - D*(y^2 - 2*y^3 + y^4)` with
`A = curve*0.01 + 1.5`, `B = -curve*0.02`, `C = curve*0.01 - 0.5`,
`D = 0.0625 - curve*0.0025 + curve^2*0.000025`, and mixes it with the dry sample by the
Effect amount. TRENCH uses curve 0 only (A 1.5, B 0, C -0.5, D 0.0625) and no band split.

The equation was written out here from the published file; no JSFX code was copied. The
file carries no licence line and the collection's licence has not been confirmed.

TRENCH's own choices: the curve is applied to the signal normalised to the final ceiling
(-0.1 dBFS) and scaled back, so the stage cannot exceed the ceiling; the input is held to
that ceiling as the JSFX "Clip 0 dB" option holds it to 0 dBFS; Effect equals the OUTPUT
knob position, and OUTPUT also sets the Mackity desk's input gain (1 + 9 x knob) and its
static compensation (DeskCompensation.h). Curve 0 has slope 1.5 at zero, is monotone on
[0, 1] and ends at exactly 1, so it lifts quiet material by up to 3.5 dB and never the peak.
