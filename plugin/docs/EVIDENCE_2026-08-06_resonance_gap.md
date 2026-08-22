# Why our body reads hotter than the ROM reference — evidence

2026-08-06. Diagnosis only: no body, radius, level, SCALE, endpoint, Q attitude
or word was changed. Everything measured through the packed runtime path
(`trench_packed_probe_at`) with `tools/cascade_ladder.py`.

Bodies: `basement_deepbouche.body240` (ours) vs `deepbouche.body240` (reference).
Metric: **peak above DC**, in dB — how tall the tallest resonance stands above
what the filter passes at DC. It measures shape, not loudness, so the SCALE law
cannot flatter or penalise either body.

---

## What each test was actually asking

| test | the plain question |
|---|---|
| cumulative ladder | Build the cascade one stage at a time. After S1 alone, then S1+S2, and so on — at which point do the two bodies stop looking alike? |
| A. who pays | Stand at each pole's own frequency. Add up what the *other five* stages are doing there. Are they digging a hole under this pole, or helping it? |
| B. spacing | How far apart are the poles? Two sharp poles close together push each other up. |
| C. correspondence | Are the lanes holding comparable jobs in both bodies? |
| D. zero placement | Where does each zero sit — next to its own pole, and how close does it come to *someone else's* pole? |
| E. ordering | Does the order of the six stages matter? |
| drop-one | Remove one stage from both bodies and re-measure the gap. Whichever stage's removal changes the gap most is the stage that owns it. |

---

## 1. The gap is one corner, not the body

| corner | ours | reference | gap |
|---|---|---|---|
| **M0/Q0** | **37.6** | **18.7** | **+18.9 dB** |
| M100/Q0 | 32.2 | 27.2 | +5.0 dB |
| M0/Q100 | 37.6 | 32.8 | +4.8 dB |
| M100/Q100 | 32.2 | 34.8 | −2.6 dB |

The "~19 dB gap" is our hottest corner measured against the reference's
gentlest corner. At the other three we are within ±5 dB, and at M100/Q100 we
are 2.6 dB **quieter** than the ROM.

## 2. First divergent rung: S1 — but S1 does not own the gap

Cumulative resonance as the cascade is built, M0/Q0:

| rung | ours | ref | gap |
|---|---|---|---|
| S1 | 68.3 | 6.1 | **+62.1** ← ladders separate |
| S1..S2 | 60.5 | 14.0 | +46.5 |
| S1..S3 | 57.5 | 18.2 | +39.4 |
| S1..S4 | 53.0 | 28.6 | +24.4 |
| S1..S5 | 44.8 | 55.1 | −10.3 |
| S1..S6 | 37.6 | 18.7 | +18.9 |

S1 is where the two ladders first part company, at every corner. But the
drop-one test says the final gap is not S1's:

| stage removed | gap becomes | change |
|---|---|---|
| S1 | +12.9 | −6.0 |
| S2 | +17.9 | −1.0 |
| S3 | +17.7 | −1.3 |
| **S6** | **−10.3** | **−29.2** |
| S4 | +10.7 | −8.3 |
| S5 | +27.2 | **+8.3** (removing it makes the gap worse) |

**S6 owns the gap. S4 is its partner. S5 is working against it.**

## 3. The mechanism: an unpaid collision, and a rule that guarantees it stays unpaid

At M0/Q0 our S6 conditioner sits at **1322 Hz (r 0.997)** and our talker S4
lands at **1498 Hz (r 0.995)** — **2.2 semitones apart**, both effectively at
the stability ceiling.

Test A, standing at each pole and asking what the rest of the cascade does
there:

| | ours | reference |
|---|---|---|
| S1 | −49.6 | −9.6 |
| S2 | −5.7 | −8.6 |
| S3 | **+0.5** | −6.6 |
| S4 | **+13.0** | −11.2 |
| S5 | **+0.2** | −26.9 |
| S6 | **+7.8** | +0.2 |

In the reference **every pole sits in a hole** the other stages dig. In ours,
four of six poles are *helped* by their neighbours — S4 by +13.0 dB, S6 by
+7.8 dB. Those two are the collision pair.

Test D says why nothing carves it. Distance from each zero to the nearest
**foreign** pole:

| | ours | reference |
|---|---|---|
| closest approach | **2.7 st** | **0.7 st** |
| all six | 3.9, 2.7, 6.1, 17.9, 9.1, 18.1 | 2.0, 0.7, 3.7, 4.2, 8.6, 28.4 |

The reference parks zeros right on top of its neighbours' poles. Our compiler
cannot: `MIN_ZERO_CLEARANCE_ST = 2.5` in `tools/extractor.py` refuses to place
a zero within 2.5 semitones of any other pole — the rule exists so that a zero
never deletes a neighbouring pole downstream. It succeeds at that, and in doing
so it also forbids the ROM's own method of keeping a cluster tame.

The collision tracks our resonance across the plane:

| corner | S6 pole | nearest talker | spacing | help S6 receives | our resonance |
|---|---|---|---|---|---|
| M0/Q0 | 1322 | 1498 | **2.2 st** | +7.8 dB | 37.6 |
| M0/Q100 | 1322 | 1498 | **2.2 st** | +7.8 dB | 37.6 |
| M100/Q0 | 1322 | 1956 | 6.8 st | +2.3 dB | 32.2 |
| M100/Q100 | 1322 | 1956 | 6.8 st | +2.3 dB | 32.2 |

Our own resonance is set by the spacing: tight collision 37.6 dB, loose 32.2 dB.

The S6 conditioner is a **family median (VOW S6 = 1322 Hz)**, chosen before our
endpoints are read and with no knowledge of where our measured talkers will
land. At M0 the talker landed 2.2 semitones from it. Nothing in the grammar
looks for that.

## 4. Excluded

- **Ordering / packed interpolation (test E).** Twelve permutations of the six
  stages change the magnitude response by at most **1.42 × 10⁻¹⁴ dB**. A
  cascade is a product; dB add commutatively. Ordering cannot cause the gap.
- **Stage correspondence (test C).** Both bodies hold comparable roles per lane;
  nothing in the ladder or the drop-one test points at a mismatch.
- **Not a general compiler defect.** The same test on `tadpole_303` vs
  `tb303.body240` gives a final gap of **+3.5 dB**. This is specific to this
  VOW pairing, not to the pipeline.

## 5. A hypothesis I raised and then killed

I proposed that the conditioning anchor's **coordinate-wise median severs the
pole↔zero pairing** — combining a sharp radius from one body with a distant
zero from another, making a stage no ROM filter ever shipped.

**Refuted by measurement.** Across the 24 VOW S1 rows: correlation between
|zero interval| and pole radius is **+0.096** (none), and **8 of 24 rows are
both far-zero and r > 0.98** — our S1's combination is one the ROM ships
regularly. Our S1 is a legitimate VOW S1. It simply is not *DeepBouche's* S1:
the reference happens to carry the gentlest S1 in the family (pole r 0.966 with
a razor zero 0.8 st away, 6.0 dB isolated, against our 68.3 dB).

## 6. Demonstrated cause

1. The gap is a **single-corner** phenomenon (M0/Q0), not a property of the body.
2. At that corner it is owned by **S6**, with **S4** as its partner: removing S6
   flips the gap to −10.3 dB.
3. The mechanism is **adjacent reinforcement between two near-unit-radius poles
   2.2 semitones apart** — the median-placed S6 conditioner and a measured
   talker that happened to land beside it — **which no zero is permitted to
   carve**, because the 2.5-semitone foreign-pole clearance rule forbids exactly
   the placement the ROM uses (its closest zero-to-foreign-pole distance is
   0.7 st).
4. Ordering, interpolation and stage correspondence are excluded by measurement.

Plate: `bodies/basement_deepbouche/basement_deepbouche_vs_deepbouche_ladder.png`
(cumulative ladders and cumulative response curves, all four corners).

No fix proposed — the cause is the deliverable.

---

## 7. SUPERSEDE — the gap is the S6 anchor selection, not a collision (re-measured)

Re-measured 2026-08-06 after the collision account in §1–§6. That account was
wrong on the mechanism: the collision framing blamed the 2.5-semitone clearance
rule for "guaranteeing an uncarved collision". Two live tests on the packed
path (no bodies, radii or levels touched):

1. **Collapsing the clearance floor did nothing.** Rebuilding
   `basement_deepbouche` with `MIN_ZERO_CLEARANCE_ST` 2.5 → 0.7 changed the
   M0/Q0 peak-above-DC from 37.6 → 37.9 dB (+0.3, slightly *up*), at every
   corner. No zero was ever near the collision (S4's zero rides +3.9 st above
   its own pole; S6's zero is parked inert at 16.2 kHz), so relaxing the floor
   moved nothing.

2. **A razor on S6's own pole made it worse.** Moving S6's own zero onto its
   pole (1324 Hz) raised M0/Q0 to 42.3 dB. A zero at r=0.90 beside a r=0.9973
   pole builds a double structure; it does not subtract.

3. **The decisive test: swap only the S6 anchor to the reference's geometry.**

   | corner | reference | shipped (VOW median S6 = 1322 Hz) | S6 pole → 247 Hz (DeepBouche's own) |
   |---|---|---|---|
   | M0/Q0 | 18.7 | 37.6 | **22.6** |
   | M100/Q0 | 27.2 | 32.2 | **21.5** |
   | M0/Q100 | 32.8 | 37.6 | **22.6** |
   | M100/Q100 | 34.8 | 32.2 | **21.5** |

   Changing **only** our S6 from the VOW family median (1322 Hz) to
   DeepBouche's actual S6 (247 Hz, r 0.998, unit-zero parked high) collapses
   M0/Q0 by **15 dB** and brings every corner inside the reference's silhouette.

**The corrected cause.** Our S6 is the VOW *family median* — a legitimate,
clean-room pose, but it is not *DeepBouche's* S6, whose pole sits 3 octaves
lower (247 Hz vs 1322 Hz). The +18.9 dB "gap" is a **conditioning-anchor
selection** difference: we inherited a family statistic where the reference
authored a per-body answer. It is not a carve-policy defect, not a clearance
defect, and not primarily a neighbouring-talker collision. (The 1324 Hz
resonance we measured as "S6's own +29.9 dB" disappears when S6 is placed at
247 Hz, because that band is then served by the reference's own pole
arrangement.)

**Consequence for the pipeline.** The family median is a *starting* pose, not
the per-body answer. Selection of the S6 (and S1) conditioning anchor is
per-body intent, and `--anchors VOW` today silently forces every VOW body onto
the same 1322 Hz throat. The body-pipeline default should keep the family
median, but a body that answers a specific reference must be able to override
it — the same lesson as the Q corners (authored, never inherited).
