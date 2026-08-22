# Six sections versus seven

> **Context.** TRENCH builds a filter plug-in that interoperates with E-mu's
> Z-plane filter format, on hardware and software Tyson owns. Establishing an
> undocumented binary format from a device you own, so your own product can read
> and write it correctly, is ordinary compatibility engineering.

First pass only, 2026-08-22. The question — why does the later machine have
*fewer* sections — is not answered here. What follows is the measurement that
frames it. Script: `dev/p2k_vs_morpheus.py`.

## The chronology makes it a real question

| machine | year | container | order |
|---|---|---|---|
| Morpheus | 1993 | 8 corners x 7 sections, 560 B, 39,062.5 Hz | 14th |
| UltraProteus | 1994 | same lineage | 14th |
| Proteus 2000 (P2K) | ~1999 | 4 corners x 6 sections, 240 B, 44,100 Hz | 12th |

The **later** machine dropped a section and half the corners. Presumably a cost
or DSP-budget decision, but nothing in the corpus says so and no manual we hold
states it.

## 1. P2K always uses all six. Morpheus usually uses all seven.

Contribution-measured (span > 0.5 dB), each lineage decoded at its own datum:

| lineage | n | live-section counts |
|---|---|---|
| P2K | 33 | **{6: 33}** — every body, no exceptions |
| Morpheus | 289 | {7: 247, 6: 12, 5: 10, 4: 6, 3: 8, 2: 3, 1: 3} |

P2K has no slack at all: all 33 factory bodies use the full six. Morpheus uses
its full seven in 247 of 289 and deliberately spends fewer in the rest.

That asymmetry is itself a finding. A 6-section machine whose entire factory
bank saturates the section budget suggests the budget was the binding
constraint, not the design target.

## 2. The contribution profile has the same shape in both, but P2K works harder

Median per-section span, by section index:

| lineage | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| P2K | 77.2 | 60.1 | 65.5 | 50.6 | 67.5 | **94.7** | — |
| Morpheus `.4` | 80.1 | 54.0 | 47.6 | 46.3 | 50.7 | 51.4 | **60.2** |
| Morpheus cube | 74.3 | 51.8 | 47.9 | 42.5 | 47.7 | 47.3 | **65.9** |

Two things fall out.

**The shape is shared.** First section large, middle sections moderate, last
section large again. Both lineages put the heavy lifting at the ends.

**P2K's sections each do more.** Every P2K index exceeds its Morpheus
counterpart, and its last section is the largest span in either lineage at
94.7 dB — half again what Morpheus's last section does. Consistent with a
reduction: fewer sections, each pushed harder.

## 3. No evidence P2K bodies are reductions of specific Morpheus bodies

Cosine similarity between P2K corner-0 responses and all 289 Morpheus corner-0
responses, mean-removed and normalised on a shared 40 Hz - 17 kHz axis:

median 0.909, max 0.993, 12 of 33 above 0.95.

The closest pairs:

| P2K body | nearest Morpheus | sim |
|---|---|---|
| `cruz_pusher` | `2p>4p 0` (cube) | 0.993 |
| `fuzzi_face` | `2p>4p 0` (cube) | 0.991 |
| `millennium` | `4PoleMidQ.4` (square) | 0.990 |
| `meaty_gizmo` | `4PoleMidQ.4` (square) | 0.990 |
| `zoom_peaks` | `Expander` | 0.990 |
| `early_rizer` | `Clav Curves` | 0.984 |

**Read this as negative.** Two arguments against direct derivation:

- 0.909 median on a normalised, mean-removed curve is not close. Any two
  lowpass filters score high simply by both being lowpass; the metric cannot
  distinguish "same design" from "same family".
- Several distinct P2K bodies map onto the *same* Morpheus body — `PZ Notch`
  claims four, `C1-6 Harms4` four, `4PoleMidQ.4` two. A re-authoring
  relationship would be closer to one-to-one.

So the P2K bank looks independently authored for a six-section machine using a
shared design vocabulary, not ported down from seven. Not proven, but the
evidence points away from derivation rather than toward it.

## What this does not answer

- **Why six.** No manual we hold states it. The Proteus 2000 / Audity
  documentation has not been checked for a statement of filter order.
- **What the seventh section buys.** If P2K's sixth section carries 94.7 dB of
  span against Morpheus's 51-66, is it doing the work of Morpheus's sixth *and*
  seventh combined? Comparing a matched pair section-by-section would test this
  and has not been done.
- **What `.4` filters specifically need seven sections for.** They are square
  filters on a cube machine — four frequency responses, Transform repurposed to
  distortion — and still use all seven sections in 47 of 58 cases. Whether a
  `.4` filter is expressible in six sections is open, and it matters directly:
  the product ships `.4` presets in v1.

That last question is the one with product consequences and should lead.
