# X3 movement — implementation spec

What the Emulator X3 does when the wheel moves, read out of `EmulatorX.dll`.
Every address below is a verified read, not an inference. Static parity
(`tools/x3_null_gate.py`, 360/360 sample-exact) constrains only the endpoints;
everything here is about what happens between them.

## The finding

The X3 updates filter coefficients **once per block** and **linearly ramps them
per sample across that block**. It does not update per sample, and it does not
install abruptly.

We do the opposite: `Engine::rebuild_coefficients` runs per sample and installs
whole (`Cascade::snap_targets`, `trench-core/src/engine.rs:875`). Under a fixed
wheel the two agree exactly — which is why the null gate passes and told us
nothing. The moment the wheel moves they diverge in three separate ways, listed
under **Deltas** below.

## The machine

`CPhantomRTFilter`, vtable at `1806d64f8` (immediately after the AGC table
`1806d64a0`).

### Object layout (verified in `FUN_1802bfb50`, the constructor)

    +0x28   int    N          host block length
    +0x2c   float  1/N        `1.0f / (float)N`
    +0x86   char   static     1 = install-and-run, 0 = ramp
    +0x2a0  float  morph      coordinate the words are interpolated at
    +0x2a4  float  q          ditto, Q axis
    +0x2a8  float  4.0        runtime_scale
    +0x2b0  float  morph'     block's smoothed morph, feeds +0x2a0
    +0x2b8  double            one-pole state behind +0x2b0
    +0x3b0  u16[30]           interpolated packed words (6 stages x 5)
    +0x3f0  ptr[6]            raw allocs, 0x50 bytes each
    +0x428  ptr[6]            16-byte-aligned WORKING coefficient rows
    +0x458  ptr[6]            raw allocs
    +0x488  ptr[6]            16-byte-aligned DELTA rows
    +0x4b8  float[6][5]       current[]   — coefficients at start of block
    +0x530  float[6][5]       target[]    — coefficients at end of block
    +0x5a8  float[6][5]       frozen[]    — snapshot for the voice-restart path
    +0x620  ptr               -> ptr[6] left  per-stage state {z1,z2}
    +0x628  ptr               -> ptr[6] right per-stage state {z1,z2}
    +0x420  uint  NSTAGES     1, 2, 3 or 6

### vtable slots that matter

    0x08  FUN_1802c3fc0   current[] <- target[]
    0x10  FUN_1802c4020   working[i] <- current[i]
    0x18  FUN_1802c4080   frozen[]  <- current[]
    0x20  FUN_1802c40f0   working[i] <- frozen[i]
    0x28  FUN_1802c41a0   delta[i][j] = (target[i][j] - current[i][j]) * (1/N)
    0x30  FUN_1802c4150   delta[] <- 0
    0x38  FUN_1802c0430   step the morph smoother once
    0x40..0x78            eight render loops, RAMPING
    0x80..0xb8            the same eight, STATIC

Within each group of eight the order is
`{1,2,3,6 stages} x {mono, stereo}` — `0x40`=1 mono, `0x48`=1 stereo,
`0x50`/`0x58`=2, `0x60`/`0x68`=3, `0x70`/`0x78`=6.

## The per-block sequence

From `FUN_1802d1600` (voice loop) and `FUN_1802c04e0` (per-voice render):

1. `vtable 0x38` — step the morph one-pole once per block. Result in `+0x2b0`.
   The law is `s = s - s*R + mod; out = R*s`, with `R = 1/DAT_180889ba8`.
   `DAT_180889ba8` has no static value — it is built at construction from the
   table behind `DAT_1808acba8`. Read live out of FL64 with EmulatorX.dll
   loaded (2026-08-13): the table is 20 entries `1, 7.07107, 10, 14.1421, 20,
   28.2843, 40 … 5120`, the constructor interpolates it at `(int)(20*0.01)=0`
   plus fraction 0.2, giving

       DAT_180889ba8 = 2.2142133712768555
       R             = 0.4516276717185974

   which is exactly the value found in memory. Time constant ~2.2 blocks.
2. `+0x86 = 0`.
3. If `+0x2a0 == +0x2b0` **and** `+0x2a4 == q_dest` (exact float compare),
   set `+0x86 = 1` and skip to step 6. Nothing moved; nothing is recomputed.
4. Else commit `+0x2a0`/`+0x2a4`, then
   `FUN_1802c3d40` (interpolate packed words at that coordinate) ->
   `FUN_1802c3600` (decode into `target[]`).
5. `vtable 0x28` — `delta = (target - current) * (1/N)`.
6. Per voice: `vtable 0x10` re-seeds `working[i]` from `current[i]`, then
   `FUN_1802c04e0` dispatches on `NSTAGES`, stereo, and `+0x86`.
7. After all voices: `vtable 0x08` — `current[] <- target[]`.

Step 6 re-seeds because the render loop mutates `working[]` in place, so every
voice in the block traverses the identical coefficient trajectory.

The `param_5 != 0` branch in `FUN_1802c04e0` (byte `+0x71` on the voice, set
when the sample player hit its end/loop this block) takes `vtable 0x20` instead
— `working[] <- frozen[]` — and forces static. That is the voice-restart path,
not the movement path.

## The ramp, exactly

`FUN_1802c1550` (6-stage mono, ramping) — the increment is at the **end** of
the sample body, after the output is written. So for sample `i` of `N`:

    coef(i) = current + i * (target - current) / N        i = 0 .. N-1

Sample 0 uses `current` untouched. `target` is first heard at sample 0 of the
*next* block. **The wheel position sampled during block k does not fully reach
the filter until block k+1.** That one-block lag is part of the machine.

## The five ramped numbers

CORRECTED 2026-08-14 against the live Ghidra decompile. The earlier read of the
state update (`z2 = z1; z1 = w`) and the `c0 = b1/b0 …` mapping were both
wrong.

The render loop's per-stage row is `c[0..4]`, and it is these five that get the
linear increment. Read straight off `FUN_1802c1550` (first stage; `pfVar9` is
the working row, `fVar1`/`fVar2` are `z1`/`z2`):

    fVar13 = (fVar2 * pfVar9[3] - fVar1 * pfVar9[2]) + *pfVar7;   // w
    *pfVar8 = (fVar1 * 2.0 + fVar13) - fVar2;                     // z1'
    ... ((fVar1 * fVar15 + fVar13) - fVar2 * fVar3) * fVar14      // y

which is, per sample per stage:

    w   = x - c2*z1 + c3*z2
    y   = (w + c0*z1 - c1*z2) * c4
    z1' = w + 2*z1 - z2
    z2' = z1
    (y is the input x of the next stage; c4 is applied before the handoff)

The `+ 2*z1 - z2` in the state update shifts the whole basis: coefficients are
stored as deviations from a double root at z = 1. Against our
`[b0, b1, b2, a1, a2]`:

    b0 = c4    b1 = (c0 - 2)*c4    b2 = (1 - c1)*c4    a1 = c2 - 2    a2 = 1 - c3

This is exactly `minifloat.rs::kernel_to_biquad`, and the row `c[0..4]` is
exactly what `minifloat.rs::stage_words_to_kernel` builds: the repo already
calls this row the KERNEL. `FUN_1802c3600` confirms the decode end: minifloat-
decode each of the five words `d0..d4`, then

    c0 = 4*d0 + d1      c2 = 4*d2 + d3      c4 = 4*d4
    (the 4.0 is runtime_scale at +0x2a8 — COMBINE_K in minifloat.rs)

so `c1 = d1 = 1 - r_zero^2`, `c3 = d3 = 1 - r_pole^2` (the r^2 words), and
SCALE rides `c4` — it ramps with the other four, it is not a separate stepped
gain.

Parity therefore means: ramp the KERNEL row linearly, and derive the biquad
from it (or run the kernel recurrence directly). Ramping `[b0,b1,b2,a1,a2]`
linearly is a different filter mid-block — `b1 = (c0-2)*c4` is a product of two
independently ramping quantities, quadratic in time.

## Deltas from what we ship

STATUS 2026-08-21, commit `d6ae46a25`: all five deltas are closed on the
shipping default (`x3Movement` ON). The one-pole runs in
`FilterEngine::process_span`, once per 32-sample control tick, on the
COMPLETE summed Morph destination — trajectory + FOLLOW + GROWL — before
packed-word interpolation. `Movement.h` renders only the raw clamped
`base + function-generator` trajectory; its hand-only smoother, base block
ramp, and 1 ms output slew are deleted (they were three TRENCH-only
smoothing laws stacked on the machine's one). The pole seeds to steady
state at the current target on the first tick and on the A/B toggle, so
parked positions pass through exactly; the machine's cold-start glide from
zero is a voice event we do not have. The per-sample path stays reachable
via `x3Movement` OFF.

1. **Rate.** CLOSED. We rebuild every sample (`engine.rs:1107`); the X3 rebuilds per
   block. Our wheel has no stair-step and no one-block lag; the X3's has both.
2. **Interpolant.** CORRECTED 2026-08-14: the X3 interpolates the *packed u16
   words* too. `FUN_1802c3d40` bilinearly lerps the corner words in u16 space
   with float-multiply-then-truncate-toward-zero — the identical law to our
   `lerp_u16` — and decodes ONCE per block. The interpolation law is the same;
   only the rate (per block vs per sample) differs. The earlier claim that it
   blends decoded coefficients was wrong.
3. **Basis.** `Cascade` ramps `[b0,b1,b2,a1,a2]` (well: snaps them — no ramp
   at all today). The X3 ramps the KERNEL row above.
4. **Change test.** The X3 skips the whole recompute on exact float equality of
   morph and Q. We have `coeff_stamp` (`engine.rs:796`), which is the same idea
   but keyed on words + boost + KEY + ratio + amount.
5. **Pre-smoothing.** CLOSED 2026-08-21. The X3 runs morph through a one-pole
   (`FUN_1802c0430`, one step per block, coefficient `1/DAT_180889ba8`)
   *before* the coefficient ramp. Ours now does the same, on the summed
   target, in the engine. An interim revision put this pole upstream in
   `Movement.h` on the hand only, with patterns bypassing it plus a 1 ms
   output slew and a per-buffer base ramp — that split one machine law into
   three placement-dependent ones and is gone.

## What to build

### Scope

A parity mode for the movement path. Endpoints must not move: `trench gate`
and `tools/x3_null_gate.py` stay green throughout.

### 1. Kernel ramp basis in `Cascade`

CORRECTED 2026-08-14. The X3's state recurrence (`z1' = w + 2*z1 - z2`) is NOT
bit-compatible with our DF-II at fixed coefficients — different arithmetic,
different rounding — so the earlier plan (swap the inner loop, demand
`rossum_reference` bit-exact) was self-contradictory. What parity actually
requires is the coefficient TRAJECTORY: the kernel row moves linearly, so the
biquad the filter runs at sample i is `kernel_to_biquad(current + i*delta)`.

So: keep the DF-II inner loop and its nonlinearities untouched. Add a gated
kernel ramp alongside the existing biquad snap:

- `Stage` gains `kernel[5]` + `kernel_deltas[5]` (the row from
  `stage_words_to_kernel`, which IS the X3 row).
- Per sample in kernel mode: derive `coeffs = kernel_to_biquad(kernel)` at the
  top, add `kernel_deltas` at the END of the sample (the X3 increments after
  the output is written — sample 0 uses `current` untouched).
- `biquad_to_kernel` (new, `minifloat.rs`) seeds the kernel after a snap;
  guard `b0 == 0` by carrying `c0 = 2, c1 = 1` (the zero pair vanishes).
- `rossum_reference` stays bit-exact trivially — the inner loop is untouched.

### 2. Block-rate update in `Engine`

`trench-core/src/engine.rs::process_span`.

- Hoist `rebuild_coefficients` out of the per-sample loop to the
  `control_phase == 0` branch. Evaluate the wheel once, at the block's first
  sample.
- Install with `set_targets(&corner, n)` rather than `snap_targets`, where `n`
  is the number of samples this control block will actually cover
  (`min(BLOCK_SIZE, len - i)`) — the X3's `1/N` and its loop count are the same
  `N`, and they must stay the same here or the ramp over/undershoots.
- `snap_next_targets` (body switch, reset) keeps calling `snap_targets`. That
  is the X3's `frozen[]`/static path.
- SCALE itself is `d4` and rides `c4` — in the X3 it ramps with the other four
  automatically (the `* 4.0` in `FUN_1802c3600` is runtime_scale/COMBINE_K,
  part of decode, not a separate gain). Our extra per-corner `output_gain`
  (interpolated boost) has no X3 counterpart; ramp it on the same block
  schedule — ramp it, do not step it.

### 3. One-block lag

`set_targets` as written starts from the *current* coefficients, which is
already the X3 behaviour: block k travels from target(k-1) to target(k). No
extra delay line is needed. Verify it rather than assume it (test below).

### 4. Gate it — and bake in nothing

This changes every moving sound we ship, so it lands as a switch and **the
default is not decided by this document.** The X3 doing it one way is evidence,
not a verdict.

- Both paths stay live and reachable: per-sample rebuild + whole install
  (what ships today), and block-rate rebuild + per-sample ramp (the X3). Neither
  is deleted when the other works. If one is removed the comparison in proof 5
  can never be re-run.
- The switch's default is Tyson's call after seeing proof 5, not a consequence
  of implementing this.
- Item 1 (ramp basis) is the exception and is *not* a bake-in: at fixed
  coefficients it must be bit-exact, so it changes no sound at all. It only
  makes the X3's ramp expressible. If `rossum_reference` does not stay
  bit-exact through it, the refactor is wrong, not the test.
- The one-block lag and the block-rate morph pre-smoother are part of the
  parity path, not separate opinions — they arrive and leave with the switch.
- `AGC_DRIVE = 2.0` (`trench-core/src/engine.rs:173`) stays a separate,
  still-wrong, still-gated number. Do not fold it into this switch.

### Modulated morph goes through the same path

GROWL is a function generator on the wheel, and the X3 has those. `+0x98` is
the morph destination's modulation accumulator — what the patchcord matrix sums
LFOs and function generators into — and `FUN_1802c0430` reads it once per block
and runs it through the one-pole before `+0x2b0` becomes the block's morph.

So GROWL is not a departure and needs no exception. It lands on `+0x98`, gets
the one-pole, gets the block-rate quantisation, gets the coefficient ramp, same
as every other modulation source. The one-pole is the X3's anti-zipper for
exactly this case — it is why a modulated morph does not stair-step there.

Consequence to expect and to plot: a fast function generator on the wheel is
low-passed by that one-pole, so its depth falls off with rate. That is the
machine's behaviour, not a defect to compensate. FOLLOW's offset lands the same
way.

## Proof required before this is called done

No claim lands without one of these run and shown. Status 2026-08-21:

1. `tools/x3_null_gate.py` — RUN: 360/360 sample-exact after the ownership
   move. Static positions did not move.
2. `trench gate` — RUN: 184 bodies, 0 refused, fnv1a64 `5187ee1fb8f8f27d`.
   CAVEAT found during review: the gate stores NO baseline — it prints a
   hash and writes `gate.bin`, so alone it only proves nothing moved since
   the last person compared hashes. It needs a committed expected hash.
3. `cascade.rs::rossum_reference` — passing (basis change landed earlier).
4. Ramp shape — `cascade.rs::x3_kernel_ramp_shape_matches_the_dll`, passing.
   Engine-level: `x3_coefficients_arrive_one_block_late_exactly` proves
   sample 0 of block k+1 sits exactly on block k's smoothed target.
5. The moving-wheel A/B plot — STILL NOT RUN. The A/B switch exists for it.

Added 2026-08-21, `dev/experiments/x3_moving_null.py`: the engine's moving
path (KEY OFF, filter only, wheel 0→1 over 3 s, raw native-rate, no gain
fitting) is SAMPLE-EXACT against an independent Python implementation of
this spec's law — one-pole, u16 lerp at the smoothed coordinate, kernel
ramp, change-test skip. That pins our implementation to the recovered law.
It is NOT machine parity: no X3 moving-wheel capture exists and the
DLL-in-host render path (current.md round-trip steps 3–5) was never built.
Engine tests also pin: the pole steps once per tick and a mid-block change
is not read until the next tick; GROWL and FOLLOW enter through the pole.

## Still open

The 12.2 dB. Ruled out so far, each by reading rather than reasoning:

- `FUN_1802d44a0` — the voice pan/amp matrix, not a gain stage. It applies two
  (mono) or four (stereo) per-sample-ramped gains from `FUN_1802d4750`, with
  the same ramp idiom as the filter (`delta = (new-old) * 1/N`, `1/N` at voice
  `+0x50`, `N` at voice `+0x4c`).
- `FUN_1802d4350` — the voice fade envelope. Returns 1.0 in steady state. Its
  rate table (`DAT_1808acb28`, 255 entries `1, 1/6, 1/12, 1/18 …`) is a
  per-block increment, nothing broadband.
- Both pan tables, read live out of FL64: `DAT_1808acb80` is constant-power
  (center 0.707107 = -3.01 dB, max 1.0); `DAT_1808acb70` peaks at 1.11751
  (+0.97 dB), center 0.823618 (-1.69 dB). Neither carries a fixed offset.

What is left on that path is `voice+0x48` and `(voice+0x20)+0x24` — per-voice
and per-preset volume. Those are preset data, so they cannot be a constant
"flat across all morphs" unless the preset itself sets it, which makes the
capture chain the likelier suspect than the DLL. Reading them needs a live
voice pointer, i.e. a breakpoint in the audio thread; that was not run.

Worth weighing first: 12.2 dB is within measurement noise of 12.04 dB = exactly
4.0x, and `runtime_scale` at `+0x2a8` is exactly 4.0. The null gate is
sample-exact against the DLL, so the factor is not missing from our filter —
which points at how the reference captures were made, not at the code. Check
the capture chain before breakpointing anything.

RESOLVED 2026-08-14: the capture chain was checked, and the 4.0x coincidence is
dead. The 12.2 dB is TWO uncontrolled gains, one per side of the comparison,
from the report of `dev/experiments/phaser2_capture_diagnostic_20260813` plus a
re-render of the actual dry reference through the engine at capture level:

- The X3 capture sits +4.13 dB ABOVE the pure filter math (the report's own
  48k bank match, correlation 0.9998) — EmulatorX voice/preset gain staging,
  the unread `voice+0x48` / preset volume, being a sampler.
- The TRENCH capture sits -8.09 dB BELOW the math. The engine's whole factory
  chain accounts for only -1.7 dB at that input level (measured, AGC drive 1.0
  and 2.0 identical); the remaining ~6 dB is FL session state on the TRENCH
  channel (channel volume default is 78% = -2.2 dB; the rest unrecorded).

Neither side is off by 12.04, so no single 4x exists anywhere. Both captures
were FL MASTER-bus recordings taken ~90 s apart with no gain accounting. If
the number ever needs to die completely: one controlled recapture, both FL
channels at 100%, record the insert not the master, panel state written down.
Nothing in the engine is owed 12 dB.
