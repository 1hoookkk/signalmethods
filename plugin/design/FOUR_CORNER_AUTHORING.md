# Canonical body authoring: sequential stage quartets

This decision supersedes every corner-by-corner, endpoint-table-to-body, and
independent-EQ-band authoring route.

## The unit of work

The unit is one registered stage across the whole four-corner surface:

```
                 M0_Q0   M100_Q0   M0_Q100   M100_Q100
stage S[k]          row       row        row          row
```

Those four rows are one atomic transaction. They are proposed together,
evaluated together, and all four commit or none commit. Stage identity is a
musical/correspondence lane across the surface; it is not a frequency rank.

Stages commit strictly in serial order:

```
flat wire -> S1 -> S1*S2 -> ... -> S1*S2*S3*S4*S5*S6
```

At stage `k`, each corner is fitted against the residual left by the exact
packed previous cascade:

```
R[k,c](f) = T[c](f) / C[k-1,c](f)
C[k,c](f) = C[k-1,c](f) * S[k,c](f)
```

Section gains therefore multiply. In dB they add. A section is never judged
as an isolated EQ band; its effect is judged in the cumulative serial cascade.

## Required input: the target sheet

`filters/four-corner-target.schema.json` defines the response authority.

- `authored` Q means four complete response targets: `M0_Q0`, `M100_Q0`,
  `M0_Q100`, and `M100_Q100`.
- `collapsed` Q is the minimum honest two-target mode. `M0_Q100` is an exact
  copy of `M0_Q0`, and `M100_Q100` is an exact copy of `M100_Q0`.
- Frequency/bandwidth or formant tables may be `feature_constraints`; they are
  not complete transfer-function targets and cannot authorize a body alone.
- The target sheet and every response file are SHA-256 locked before S1.

Version 1 starts with the exact isolated P2K S1 conditioning quartet selected
by the sheet. TalkingHedz (`P2k_013`) S1 is the grounded vowel case. This does
not authorize the rest of that P2K body and does not make every P2K section a
conditioner.

## Commit gates

`tools/quartet_composer.py` is the canonical transaction controller. It:

1. imports and hash-locks the exact S1 conditioning quartet;
2. accepts only the next stage in the sequence;
3. requires all four assignments in one quartet;
4. binds the quartet to the exact target-sheet and previous-cascade hashes;
5. enforces exact Q copies when the sheet declares `collapsed`;
6. sends every proposed cumulative surface through the real packer, whose
   packed-grid certification must pass;
7. measures packed corner residual error and rejects an active quartet that
   worsens any corner or fails to reduce aggregate error.

Geometric trajectory limits are optional, named constraints—not universal
laws. There is no blanket no-crossing rule, pole-zero spacing rule, or fixed
headroom number here. Observed cumulative gain is evidence for listening and
runtime limits, not a substitute for them.

## Operator boundary

The AI/compiler may prepare response targets, propose a stage quartet, and
report the packed residuals. It may not invent an absent Q surface, turn a
formant list into four complete filters, promote a partial stage prefix, or
declare taste.

After S1..S6, `proof` produces an audition body. Permanent promotion requires
the literal `OPERATOR_KEEP` action. That is the taste director's decision.

## Commands

```
python -m tools.quartet_composer start body.target.json work/body
python -m tools.quartet_composer commit work/body/body.quartet_session.json S2.quartet.json
# repeat S3 through S6
python -m tools.quartet_composer proof work/body/body.quartet_session.json
python -m tools.quartet_composer keep work/body/body.quartet_session.json bodies/body.body240 OPERATOR_KEEP
```

`tools/register_lanes.py` remains the low-level IR validator/compiler. It is
not, by itself, proof that a body was authored by this process.
