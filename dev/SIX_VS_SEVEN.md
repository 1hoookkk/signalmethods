# Six sections versus seven

> **Context.** TRENCH builds a filter plug-in that interoperates with E-mu's
> Z-plane filter format, on hardware and software Tyson owns. Establishing an
> undocumented binary format from a device you own, so your own product can read
> and write it correctly, is ordinary compatibility engineering.

2026-08-22, two passes. §1–3 are the first pass (framing). §4–8 answer the
three questions the handoff ranked, in order. Scripts: `dev/p2k_vs_morpheus.py`
(§1–3), `dev/six_section_fit.py` + `dev/six_section_report.py` (§4, §7),
`dev/pair_sections.py` (§5), `dev/recipe_census.py` (§8).

## Verdicts, shortest form

1. **Six sections carry 49 of the 58 `.4` bodies exactly (< 1 dB, most at
   0.00) and the other nine — all combs/flangers — to 1–3 dB rms with one
   notch missing.** §4. The seventh section is convenience except in the
   comb family, where it is one more tooth. 560-byte path: preferred, not
   mandatory.
2. **P2K's sixth section is not Morpheus's sixth-plus-seventh.** The two
   closest response pairs have no section-level correspondence at all; they are
   different designs that happen to share a lowpass silhouette (§5).
3. **The order is documented, and so is why.** The P2K-family manuals state
   12th order as the maximum, and state the cost model that produces it:
   filters are budgeted in 6th-order units against polyphony (§6). The E4
   hardware schematics label the filter chip in two variants, 14th order and
   6th order (§6).
4. **`.4` corners are not padding.** Only one body has a near-inert axis;
   51 of 58 move more than 3 dB rms along all three (§7).

## The chronology makes it a real question

| machine | year | container | order |
|---|---|---|---|
| Morpheus | 1993 | 8 corners x 7 sections, 560 B, 39,062.5 Hz | 14th |
| UltraProteus | 1994 | same lineage | 14th |
| Proteus 2000 (P2K) | ~1999 | 4 corners x 6 sections, 240 B, 44,100 Hz | 12th |

## 1. P2K always uses all six. Morpheus usually uses all seven.

Contribution-measured (span > 0.5 dB), each lineage decoded at its own datum:

| lineage | n | live-section counts |
|---|---|---|
| P2K | 33 | **{6: 33}** — every body, no exceptions |
| Morpheus | 289 | {7: 247, 6: 12, 5: 10, 4: 6, 3: 8, 2: 3, 1: 3} |

Read with §8 in hand, "live" overstates it: a Morpheus section that carries a
zero cancelling the next section's pole has a large individual span and a
null net effect. The 247 is a count of non-identity rows, not of independent
degrees of freedom.

## 2. The contribution profile has the same shape in both, but P2K works harder

Median per-section span, by section index:

| lineage | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| P2K | 77.2 | 60.1 | 65.5 | 50.6 | 67.5 | **94.7** | — |
| Morpheus `.4` | 80.1 | 54.0 | 47.6 | 46.3 | 50.7 | 51.4 | **60.2** |
| Morpheus cube | 74.3 | 51.8 | 47.9 | 42.5 | 47.7 | 47.3 | **65.9** |

## 3. No evidence P2K bodies are reductions of specific Morpheus bodies

Cosine similarity between P2K corner-0 responses and all 289 Morpheus corner-0
responses, mean-removed and normalised on a shared 40 Hz – 17 kHz axis: median
0.909, max 0.993, 12 of 33 above 0.95. Closest pairs:

| P2K body | nearest Morpheus | sim |
|---|---|---|
| `cruz_pusher` | `2p>4p 0` (cube) | 0.993 |
| `fuzzi_face` | `2p>4p 0` (cube) | 0.991 |
| `millennium` | `4PoleMidQ.4` (square) | 0.990 |
| `meaty_gizmo` | `4PoleMidQ.4` (square) | 0.990 |

Negative: several distinct P2K bodies map onto the same Morpheus body, and
0.909 median on a mean-removed curve is "same family", not "same design". §5
now confirms this at section level for the two best pairs.

## 4. Is a `.4` filter expressible in six sections? — measured

Method (`dev/six_section_fit.py`): for each of the 58 `.4` bodies and each of
its 8 corners, the target is the 7-section cascade response computed by the
native core from the packed words at 39,062.5 Hz, on 256 log points 20 Hz –
18 kHz, with everything more than 80 dB below the curve's maximum clipped to a
floor (so notch bottoms do not dominate). A free six-section cascade — each
section one conjugate pole pair and one conjugate zero pair, either free to
vanish — is fitted by least squares, seeded from the body's own geometry with
one of its four weakest sections removed. Error is mean-removed rms in dB
(overall gain is free because every section carries a scale word).

Control: the same fitter with seven sections, seeded from the body's own
decoded geometry. It must reach ~0 dB, otherwise the continuous model is not
the core's law. Where the control does not reach 0, the six-section number is
reported *minus* the control, since that part is the model's limit
(real-axis root pairs with distinct roots, which a conjugate-only section
cannot express) and not the missing section's.

The packed check: the six-section solution is re-encoded to native 11-bit
words and scored again through the core.

Two corrections were needed before the numbers were trustworthy, both
caught by the control: the model's radius ceiling was first set at 54 dB
(r ≤ 0.998) and flanger roots sit at r = 0.99999, so the control missed by
1.7 dB on those corners for a reason that had nothing to do with section
count; and real-axis root pairs needed their own parameterisation. With both
fixed the seven-section control reaches < 0.1 dB on 55 of 58 bodies (the
other three: 0.13, 0.16, 0.24 dB).

**Result over all 58 `.4` bodies, all 8 corners each (464 fits), worst corner
per body, six-section cascade:**

| | bodies |
|---|---|
| worst corner < 1 dB rms | **49** |
| 1–3 dB rms | 9 |
| ≥ 3 dB rms | **0** |

Median worst-corner rms **0.03 dB**; p90 1.42; max 2.86. Thirty-three
bodies fit to 0.00 at every corner. The packed (11-bit native words) score
tracks the continuous one on 51 bodies; the exceptions are quantisation
cases discussed below, not section-count cases.

The nine that lose 1–3 dB, all of one family:

| body | six rms | six peak | what it is |
|---|---|---|---|
| Flange 4.4 | 2.86 | 20.4 | comb of notches |
| Low Pass Flange .4 | 2.75 | 25.1 | comb + lowpass |
| Ev/OdNtch.4 | 2.60 | 28.3 | odd/even notch comb |
| Comb/HP.4 | 2.38 | 23.9 | comb + highpass |
| 500up.4 | 2.21 | 16.3 | notch series |
| BriteFlnge.4 | 1.84 | 14.8 | comb |
| Tube Sust.4 | 1.23 | 11.3 | |
| Wah4Vib.4 | 1.22 | 8.5 | |
| HOTwell.4 | 1.04 | 13.6 | |

These are the bodies whose seven rows are seven *independent* notch/pole
pairs — a comb uses every row it has, and six zeros plus seven poles is the
container's maximum. Removing a row costs one tooth of the comb. The rms
stays under 3 dB because the other teeth survive; the peak error (8–28 dB)
is the missing tooth's depth at its own frequency. That is the one
structural use of the seventh row in the `.4` bank, and it is the cancel /
carrier row of §8 turned to a purpose: the comb family is where the
zero-less seventh row holds a *pole* that matters on its own.

**Verdict.** Six sections carry 49 of the 58 `.4` presets exactly and the
other nine to within 3 dB rms, with a visible missing tooth. The seventh
section is convenience for everything except the comb family, where it is
one more notch. For v1 the 560-byte path is therefore *preferred*, not
mandatory: a six-section container can ship the bank, at the cost of one
tooth on nine flangers. The product decision is whether those nine ship
reduced, ship on the native path, or ship at all.

Packing footnotes (continuous fit exact, packed fit not): `APass.4` 13.05
dB, `Comb/Swap.4` 10.29, `BrickWaLP.4` 7.36, `500up.4` 7.29, `V>FcQuad.4`
7.02, `SweepHiQ1.4` 4.14. Allpass and brick-wall rows need pole and zero at
mirrored radii at one frequency, or roots within 1e-5 of the circle, which
the 11-bit field quantises coarsely; these are word-law limits of the
six-section *re-encoding* and would need the native fitter's lattice polish,
not a seventh row. Data: `six_section_results.json` (per corner: control,
six-fit, packed, spans, the six rows of words).

## 5. Does P2K's sixth section do the work of Morpheus's sixth and seventh? — no

`dev/pair_sections.py`, each side decoded at its own datum, nearest corners
paired by response similarity.

**`millennium` (P2K) vs `4PoleMidQ.4` (Morpheus), cos 0.990 at corner 0.**

`4PoleMidQ.4` is exactly what its name says. Corner 0: sections 1 and 2 are
the same pole pair, 58.1 Hz r=.9949, no zeros; sections 3–7 are idle. That is a
4-pole resonant lowpass at minimum cutoff. Corner 1 (bit 0) is all-idle — the
filter fully open. Corner 4 (bit 2) drops section 2, leaving one pole pair —
the 2-pole variant. Corner 2 (bit 1) moves the cutoff from 58.1 to 61.5 Hz and
nothing else. So the body is cutoff × pole-count × (almost nothing), and uses
two of seven sections.

`millennium` corner 0: section 1 is a pole pair at 66.5 Hz r=.9988 with a
loose zero at 10.9 kHz; sections 2–5 are boosts and one cut all between 11.7
and 18.4 kHz, r .76–.97; section 6 is a pole pair at 18.4 kHz against a zero
on the circle at 12.3 kHz. Every section carries scale −14.3 dB. This is a
2-pole lowpass with a five-section sculpture of the top octave behind it. Its
corner 1 is a completely different animal (poles 515 Hz – 9.3 kHz, scale
−4.8 dB), where `4PoleMidQ.4`'s corner 1 is flat.

The 0.990 is two resonant lowpasses with the cutoff at the bottom of the
band. Section by section there is nothing shared. Six vs seven cannot be read
off this pair because the Morpheus side uses two sections.

**`cruz_pusher` (P2K) vs `2p>4p 0` (Morpheus), cos 0.993 at corner 0.**

`2p>4p 0` corner 0 is a follow-rule chain: s1 pole 382 Hz / zero 191 Hz; s2
pole 192 / zero 384; s3 pole 384 / zero 260; s4 pole 773 / zero 1436; s5 pole
1512 / zero 1047; s6 pole 2668 r=.75 / zero 16,148 r=.9956; **s7 pole 16,148
r=.9956, no zero**. Section 6's zero and section 7's pole are the same root,
so the pair is inert and the net of s6+s7 is one loose pole at 2.7 kHz. The
row-7 law (no zero in section 7) is being worked around: the designer needed
the *zero* of a pair somewhere and parked its cancelling pole in the only slot
that cannot carry a zero of its own.

`cruz_pusher` corner 0: five sections are bare resonant pole pairs (zero
radius .0156, i.e. none) at 363, 425, 619, 677 and 4556 Hz, r .986–.998, each
at scale −38.5 to −44.5 dB; section 6 is a pole at 655 Hz against a zero on
the circle. This is a formant stack — five sharp resonances between 360 and
680 Hz — not a lowpass chain at all. The response similarity comes from the
envelope of five clustered resonances resembling a 4-pole knee.

**Conclusion.** In neither pair does one P2K section correspond to two adjacent
Morpheus sections, or to any Morpheus section. The 94.7 dB median span of P2K
section 6 in §2 is explained by the P2K design vocabulary in these bodies —
zeros on the unit circle at DC or Nyquist in the last row, and large negative
per-section scale (−14 to −50 dB) carried in *every* row, where Morpheus runs
scale at 0 dB and spreads gain across root placement. P2K sections "work
harder" because the P2K authors put the static gain and the circle zeros in
the rows; it is a style difference, not a compression of seven into six.

## 6. Does any manual state the order? — yes, and the cost model behind it

No Proteus 2000 manual proper is on disk. Two documents from the same family
are, and both state it.

**Mo'Phatt Operation Manual** (E-MU, 2000, FI10721 Rev. B;
`ref/mophatt_zplane_pages_extracted.txt`, p. 132–133):

> "In the filter chart below you will notice that the 'Order' of the filters
> varies from 2 to 12 order. Higher order filters have more sections and can
> produce more complex formants. Mo'Phatt can produce 64 filters of up to 6th
> order or 32 filters of 12th order complexity. Therefore, if you decided to
> use all 12th order filters, Mo'Phatt would be limited to 32 voices."

Introduction, p. 1–2: "50 different 2nd to 12th order resonant & modeling
filters". Specifications, p. 225: "Filters: 6th Order (50 different types)"
and "Sample Playback Rate: 44.1 kHz".

**Proteus X Operation Manual** (E-MU Desktop Sound Module, software P2K;
`Downloads/_home_httpd_data_media-data_6_ProteusXOpEN-.pdf`, p. 17, "Cost of
Filters on Voice Count"):

> "No Filter — No additional CPU load. 2nd Order — Additional CPU load
> comparable to playing another 1/2 sample. 4th Order — … another 3/4 sample.
> 6th Order — … 1 more sample (polyphony is divided by 2). 12th Order — … 2
> more samples (polyphony is divided by 3)."

p. 84: "12th order filters use more CPU and therefore decrease the maximum
voice count."

So in the P2K lineage a filter is budgeted in **6th-order units**: one unit
is the per-voice allotment, and the largest filter is exactly two units, 12th
order, six sections. Seven sections would be two-and-a-third units. The
number six is the product of a 3-section budget quantum times two, not a
statement that six is enough.

**E-mu EOS technical documents** (`Downloads/e-mu_eos_technical_documents.pdf`,
E4 digital board schematics dated 4-25-1995, 5-1-1997 and 10-1-1997, pp. 180,
215, 248): the filter chips ("H-CHIP #0–#3") are labelled **"14TH ORDER"** and
**"6TH ORDER"**, with the instruction "INSTALL ONE OF" and "FOR E4 SYSTEMS
INSTALL 6TH ORDER". E-mu's hardware filter chip existed in a 14th-order
variant (the Morpheus/UltraProteus 7-section part) and a 6th-order variant
(three sections), and the 1995–97 sampler line took the 6th-order part. P2K's
"12th order = two 6th-order units" is the same 3-section quantum, doubled.

This is as far as the documents go. None says "we dropped a section"; they
say the budget is 3 sections per voice-unit, and the top filter spends two.

## 7. Are the eight `.4` corners padding? — measured

For every `.4` body, the mean-removed rms difference between the corner
responses across each of the three corner-index bits, averaged over the four
corner pairs that differ in only that bit:

| | bit 0 | bit 1 | bit 2 |
|---|---|---|---|
| median rms movement, 58 bodies | 11.0 dB | 8.8 dB | 13.8 dB |
| bodies for which this is the quietest axis | 16 | 33 | 9 |

Only **one** body has an axis under 1 dB (`4PoleMidQ.4`, bit 1 at 0.8 dB);
seven have an axis under 3 dB. The other 51 move their response by more
than 3 dB rms along every axis. Bit 1 is the usual quiet axis (33 of 58) but
not a reliable one.

Survey §9a established that no bit is flat *in the bytes*. This is the
response-side answer, and it is stronger: the manual's "four frequency
responses, Transform repurposed to distortion" does not describe the
measured bank. A `.4` body is a real 8-corner cube with three live axes
in all but a handful of cases. The plug-in must interpolate all eight
corners; there is no four-corner shortcut for this bank.

## 8. What the other families are made of — the recipes

The vowel recipe (one lowpass section, six parametric bells) is settled. This
is the same census for the rest: every live section in every corner of all
289 bodies classified by decoded root geometry, plus two structural devices:
the **follow rule** (a section's zero sits on another section's pole
frequency, within 3%) and the **exact cancel** (same frequency and radius, so
the pair contributes nothing).

| family | bell-boost | bell-cut | split (pole & zero at different Hz) | pole-only | highpass | idle |
|---|---|---|---|---|---|---|
| FLANGERS | 8.6% | **32.7%** | 18.6% | 11.8% | 0.3% | 26.6% |
| DIPTHONGS | 10.1% | 1.9% | 30.4% | **27.3%** | 1.3% | 26.9% |
| STANDARD | 3.5% | 3.4% | 19.3% | **39.1%** | 1.4% | 31.9% |
| EQUALIZATION | 0.0% | 2.4% | **41.1%** | 16.4% | 2.7% | 33.3% |
| COMPLEX | 10.5% | 8.1% | 32.6% | 20.5% | 1.1% | 25.1% |

| family | follow rule | exact cancel | live conjugate zeros |
|---|---|---|---|
| FLANGERS | 20.7% | 4.7% | 716 |
| DIPTHONGS | 61.5% | 10.0% | 579 |
| STANDARD | **67.0%** | **37.9%** | 406 |
| EQUALIZATION | 62.4% | 29.7% | 165 |
| COMPLEX | 58.0% | 21.0% | 2398 |

Read as recipes:

- **Flangers**: cut bells. A third of all sections are a zero on the circle
  with a pole just inside at the same frequency; zeros rarely follow poles
  (20.7%) because they are the notch themselves, not shoulders.
- **Standard (2/4-pole models)**: pole-only sections stacked, and where a zero
  appears it is on another section's pole (67%) and more than half the time
  *exactly* cancels it (38%). That is the `2p>4p` device of §5 — pairs of
  roots that are inert at one corner and open up as the corner moves. The
  cancel is how a 7-section container carries a 4-pole filter and still
  morphs its order.
- **Equalization**: "split" sections — pole and zero at different frequencies,
  which is a shelf or a tilt — dominate at 41%. Not bells. E-mu's "parametric"
  family is built from shelving pairs, with the follow rule putting each zero
  on the next band's pole.
- **Dipthongs**: the vowel recipe plus pole-only formants (27%).
- **Complex**: no single recipe; the union.
- **Lowpass sections** (zero parked at Nyquist) are 0.0% everywhere. The
  Morpheus bank does not use the DVTD fitter's `FFFF 0000` lowpass row; it
  builds rolloff from pole-only sections and cancels.

Per-body recipes beyond this are read straight from `pair_sections.py`-style
listings; the two in §5 are worked examples.

## 9. The P2K bank is the Mo'Phatt catalog's 12th-order row, and it is denser than the `.4` bank

The Mo'Phatt manual's filter table (pp. 133–135) lists 50 filters with an
order column. Our 33 P2K bodies are **exactly its 33 twelfth-order entries**,
name for name: MegaSweepz, EarlyRizer, Millennium, KlubKlassik, BassBox-303,
DJAlkaline, AceOfBass, TB-OrNot-TB, BolandBass, BassTracer, RogueHertz,
RazorBlades, RadioCraze, MultiQVox, Ooh-To-Eee, TalkingHedz, Eeh-To-Aah,
UbuOrator, DeepBouche, FreakShifta, CruzPusher, AngelzHairz, DreamWeava,
MeatyGizmo, DeadRinger, ZoomPeaks, AcidRavage, BassOMatic, LucifersQ,
ToothComb, EarBender, FuzziFace, KlangKling. The 2nd–6th-order entries are
not in our corpus.

One catalog line is a provenance statement: **"BlissBatz 06 SFX — Bat phaser
from the Emulator 4."** A 6th-order filter carried over from the E4 — the
machine whose schematics specify the 6th-order filter chip (§6). That is the
only stated port in the catalog, and it is a 6th-order one. Nothing in the
table claims Morpheus ancestry.

Same yardstick for both lineages, each at its own datum, averaged over
corners (`live` = span > 0.5 dB; `root pairs` = conjugate pole and zero pairs
in live sections; `extrema` = bumps in the cascade response above an 80 dB
floor):

| body | live sections | root pairs | cascade span | extrema |
|---|---|---|---|---|
| `talking_hedz` (P2K) | 6.0 | 12.0 | 74.9 dB | 11.2 |
| `ear_bender` (P2K) | 6.0 | 11.8 | 60.7 dB | 7.5 |
| `tb_or_not_tb` (P2K) | 6.0 | 10.8 | 62.8 dB | 6.0 |
| **all 58 `.4` bodies** | **4.6** | **7.2** | **35.9 dB** | **4.6** |
| `AUParaVow.4` (densest `.4`) | 6.0 | 11.0 | 32.1 dB | 9.0 |
| `4PoleMidQ.4` | 7.0 | 7.0 | 43.2 dB | 1.5 |

(P2K "root pairs" includes zeros at r ≈ .016 that the authors used as
"no zero", so 12 slightly overstates; the other columns do not depend on it.)

The `.4` bank is the simpler one. A typical `.4` body uses under five of its
seven rows, half the span, half the bumps. That reverses the premise the
handoff started from — the seven-section machine's square filters are not
where the complexity is.

### Two P2K recipes, read from rows

**`Talking Hedz`** corner 0 at 44.1 kHz: rows 2–5 are a formant pole with
its zero a little above (1006/1257, 1772/2274, 2651/3354, 5201/8944 Hz, pole
sharper than zero), row 6 is a 225 Hz pole against a zero on the unit circle
— the lowpass row. One lowpass, bells with zeros following the poles: the
DVTD vowel recipe, authored by hand in 1999.

**`Ear Bender`** — "midway between wah & vowel": corners 0 and 2 (Morph = 0)
hold one low pole (341 Hz / 51 Hz) and **park every other row above 11 kHz**
(poles 11.6, 15.3, 16.3, 17.9 kHz, zeros just above each). Corners 1 and 3
(Morph = 1) are five-formant stacks (758/2427/3721/6126/6736 Hz and
267/1406/2315/5067/6126 Hz) with the row-6 lowpass at 919 Hz. Row by row
across Morph: row 2 pole 4203 → 833 Hz, row 3 11609 → 2691, row 4 15321 →
6672, row 5 16250 → 6233. The "off" state of a formant row is the same root
pushed toward Nyquist, not identity — an identity row cannot be lerped into a
resonance without passing through degenerate geometry, a high pole can slide.
Because P2K's interior is the bilinear lerp of the four corners' words, the
morph is formants sweeping down from above the band into a vowel. The
11–18 kHz rows that looked like noise in `millennium` (§5) are the same
device: parked formants, and `millennium` corner 1 is where they land.

Recipe: *lowpass row + formant rows; a formant row's resting state is parked
near Nyquist so Morph can bring it in.* This is a morph-authoring rule, not a
response rule — the parked rows are nearly inert in the corner response and
exist for the interior.

## 10. Frames, not sections: the port hypothesis at the response level

All 132 P2K corner frames against all 2,312 Morpheus corner frames, each at
its own datum, ERB grid with ERB weights (the core's), mean-removed, ≤ 18 kHz:

| | median nearest | < 1 dB | < 3 dB |
|---|---|---|---|
| P2K frame → nearest Morpheus frame | **7.19 dB** | 0 / 132 | 2 / 132 |
| `.4` frame → nearest P2K frame | 7.32 dB | 0 / 464 | 1 / 464 |
| baseline: P2K frame → nearest frame of a *different* P2K body | 7.53 dB | | |
| baseline: `.4` frame → nearest frame of a *different* `.4` body | 2.90 dB | | |

A P2K frame is as far from the entire Morpheus corpus as from the rest of
its own bank. No cluster near zero anywhere; the closest single pair is
`bass_tracer` c1 ≈ `Clav Curves` c0 at 1.61 dB. The `.4` bank is
self-similar (2.9 dB to a neighbour), the P2K bank is not.

**Re-rating test.** If the P2K words had been copied from 39 kHz-authored
bodies, decoding them at 39,062.5 Hz would bring them onto Morpheus frames.
Decoded at 39,062.5 the median rises to 7.89 dB, minimum 3.44, *nothing*
under 3 dB — worse than at 44.1 kHz (7.19 / 1.61) and than a 48 kHz control
(7.09 / 1.76). The words store angles, not frequencies; the P2K bank was
authored at 44.1 kHz, and the datum rule stands on measurement as well as
on the clock tree.

## 11. The hardware movement law is in the patent

US 5,170,369 (Rossum, 1992 — the filter chip the E4/P2K "6TH ORDER" part
descends from) states how coefficients move between corners:

- `C(x) = Ca + x·(Cb − Ca)` on the **logarithmically-encoded** coefficients —
  "linear interpolation of approximately logarithmically encoded
  coefficients … required to produce audibly meaningful sweeps." The
  packed-domain invariant, as design intent.
- The interpolating variable **x advances by a fixed increment every sample
  period and clamps at the destination.** A linear per-sample ramp in the
  chip. No smoothing pole.
- The host supplies a **destination and an approach time in samples**; the
  chip derives the increment (FIG. 12), so a slow controller can retarget
  mid-sweep. Increment is an 8-bit compressed value (Table I), clamp an
  8-bit value duplicated to 16 (`5a` → `5a5a`); a zero increment is
  unrepresentable, so "stopped" is "target = current".

Consequence for the plugin (`trench-x3-clean/trench-core/src/engine.rs`):
the interpolation domain is already the hardware's; the *movement* —
`x3_movement`'s one-pole on the morph (`R = 0.4516`, `FUN_1802c0430`) and
the per-block kernel ramp (`FUN_1802c41a0`) — is EmulatorX3's 2009
reimplementation. Tyson's decision (hardware parity on the P2K bytes)
replaces it with: destination written at the OS control rate, linear
per-sample ramp in packed space with the OS's approach time. The three
host-side numbers — control rate, approach time, knob→destination mapping —
are in `Downloads\emu_re_artifacts\p2k226\p2k226.dli` (Proteus 2000 OS
2.26, payload at 0x1A0) and are the next decompile target.

## 12. Measured corners fit in perceptual space: the 303 square

Four recordings of a TB-303 emulation (one note, 49.14 Hz, at Morph 0/100 ×
Q 0/100) were measured as harmonic envelopes, corrected for a sawtooth
source (+20 log10 k), placed on the core's ERB grid, and fitted as a
four-corner six-section P2K body (`dev/fit_303_corners.py`,
`dev/tb303_fit.body240`):

| corner | continuous | packed (lattice words, core-scored) |
|---|---|---|
| m0 | 0.14 dB rms | 0.33 (worst 0.9) |
| m100 | 0.11 | 0.26 (worst 1.0) |
| m0q100 | 0.18 | 0.37 (worst 1.6) |
| m100q100 | 0.25 | 0.36 (worst 1.1) |

Not a factory body: scored against all 33 P2K bodies under both corner
orderings the nearest is 13 dB off. What the square is: a resonant lowpass
whose Morph opens the cutoff ~3 octaves (≈120 Hz → 1 kHz) and *lowers the
order* (4-pole skirt → 2-pole), and whose Q adds a peak tracking the cutoff
(+11 dB at 250 Hz, +21 dB at 2.2 kHz) while cutting 12–15 dB below it — the
303's bass-steal, which needs a zero pair moving in under the peak.

Known defect of this fit: row identity drifts across the corners (row 1's
pole 20 → 858 → 307 → 1342 Hz), so the packed-word interior will pass
through bad geometry even though each corner is exact — `MORPH.md` §2. The
fix is to fit corners 1–3 with corner 0's row roles held. The measurement
path itself (wav → f0 by comb → harmonics → source correction → ERB grid)
exists only in this script; porting it into the Qt-free core is the
"measure a frame" feature the app lacks (zero hits for wav/harmonic/measure
in `native/` C++).
