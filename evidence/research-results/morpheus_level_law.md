# Morpheus ("Vulcan" firmware) cube level law

Read 2026-09-04 from evidence/research-results/disassembly (vulcan_full.asm, unpack.txt,
range_post.txt, callback_annot.txt). Firmware image base 0x08020000.

## Gain decode (0x0803803A-0x08038066, constant 0x08038258 = 4097/2^36)
The unpacker at 0x080382FC makes an eighth pass over payload bytes 308..318 for the eight
corner gains; refill code15 = field ? (field<<4)|0x0F : 0.
    E = code>>11 ; M = code&0x7FF ; h = (E != 0)
    g = ((M | h<<11) << (E-h)) * 5.961918958519163e-08      range [0, 4.0)
Check: Piano LP.4 (cube 236) corner 0 field 1623 -> 0.421856 (matches cubes_decoded.json);
field 1787 -> 0.984371 (the corpus median, -0.14 dB); 2047 -> 3.999999.

## Position -> corner combination (0x08038002-0x08038038)
Trilinear on the 15-bit codes (not on decoded values), same SMUAD weight pairs as every
pole and zero parameter: bit0 of the corner index = Transform, bit1 = Morph, bit2 =
Frequency. Because the code is a minifloat, the corner gain crossfades in dB (within
0.52 dB of a true exponential).

## Where it multiplies (wet path, once, after all seven sections)
    out = corner_gain x user_trim(L/R, +-40 dB, default 1) x crossfade(mix, 1 at rest)
          x cascade_out
Nothing scales the cascade input. Inside the cascade: each section's own DC normaliser
b = D(1) = 1 - 2 Rp cos(theta_p) + Rp^2, gated by a bit31 flag the unpacker writes into
theta_z[corner 0]: stage 0 always, stages 1..6 only if payload byte 316 bit 0 is set.
Zeros are never normalised; section 7 has no zero stage. Output soft clip linear to 2^30
then quadratic to 2^31; the x2/3 -> trunc -> x1.5 at 0x080380B8 is a quantiser at unity.
The state limiter at 0x08037894 is the module's Distortion stage, off by default.
No 0.5, no headroom shift, no normalisation of the cascade product.

## Absolute level
Cascade DC gain is 0 dB (median over 289 x 8 corners) because each section's zeros sit on
the next section's poles and the DC terms telescope. The corner gain is the whole absolute
level. Piano LP.4 corner 0: raw cascade DC -0.03 dB, peak +19.5 dB at 64 Hz; corner gain
-7.5 dB; module output -7.5 dB at DC, +12.0 dB at the peak.

## Unresolved
The writer of the crossfade float at 0x20000028; the absolute threshold of the distortion
state limiter (struct +0x1C/+0x20); two per-channel constants at 0x080380A4/0x080380AE
taken as DC offsets by analogy.

Consequence for TRENCH: import Morpheus cubes with per-section DC normalisation as flagged
and the decoded corner gain applied once after the cascade; retire the P2K crown audit for
them.
