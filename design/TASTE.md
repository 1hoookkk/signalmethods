# Locked design decisions

Verdicts, not suggestions. Each entry is dated and stands until Tyson reverses
it. Agents: build to this file; when Tyson issues a new verdict, record it here
with the date and what it replaced.

## Aesthetic (locked 2026-08-15, from 8 reference images)

90s scientific-workstation instrument: SGI/IRIX Indigo Magic, ATLAS event
display, Mentor IC Station, Cadence Virtuoso, Petrel, MERLIN. Sculpted warm-gray
chrome with real 2px bevels — raised controls, sunken wells and fields. All
color belongs to data inside near-black wells (cyan target, green filter, lane
inks, hot orange for the live/selected thing). Numeric readouts live in sunken
light fields on chrome, never floating on the canvas. Live cursor coordinates in
a top strip. A Virtuoso-style mouse legend in the foot states current
interactions as fact. Data source engraved bottom-right. Dense, exact, visibly
built. Implemented in `editor/src/ui/theme.rs` and `paint.rs`.

Rejected on the way (do not resurrect): generic dark phosphor chrome, bone/ink
paper, cyanotype drafting plate, any near-black-plus-lone-accent plugin look.

## The trace (locked, from the plugin: trench-x3-clean GraphDisplay.h)

X3 display law: 4x oversample per pixel column with peak-hold decimation
(extremum by |dB|, sign kept — never an average); power-domain accumulation in
float32 across all sections, one 10*log10 at the end; no epsilon, no dB floor;
only NaN refused. One vertex per column; X pixel-centre locked; Y full precision
on slopes; flat runs re-lock both axes (0.15 px over one pixel of travel);
linear vertex connection; a single flat opaque 1.1 px stroke, butt caps, no
understroke, no bloom, ever. Trace clipped to its well.
Implemented in `editor/src/ui/paint.rs::x3_trace`.

## Words (locked 2026-08-15/16)

Plain words or exact DSP terms; nothing cryptic, no invented metaphors, no
codenames. On screen: target, filter, hz, at, bw, r, hold, range, gain, fit,
keep, pairing. In code and UI: PolePair, ZeroPair, Section, SectionIndex, Gain,
Constraint, FitTarget, FitResult. Banned vocabulary: "deal", "ask" (on screen),
stage roles (Air Cap etc.), any label for what geometry or selection already
states. Almost no instructional English; the mouse legend states interactions.

## Structure (locked 2026-08-15)

Selection defines scope; commands define operations; one inspector follows
selection; corners are alternate states of one persistent structure, never
modes. The cascade navigator is a thin strip (selection, order, one-word state,
sparklines) and never grows into permanent per-section panels. Home is the
field: the ride surface with corner responses at its corners and a draggable
puck; play/pause always present; audition never edits.

## The object (locked, measured)

Candidates are not sections: analysis produces PolePair/ZeroPair lists with
temporary p-IDs; a Section exists only by explicit assignment of pole pair,
zero pair and gain to a SectionIndex. No index inferred from frequency,
discovery order or strength without a named heuristic. Provisional previews are
deterministic and marked, never silently committed. Section correspondence is
authored; morphing only interpolates stored encoded words of the same section
between corners — never sort, renumber, re-pair or reinterpret during morph.
Sections are global factors, not parallel EQ bands; a corridor constrains only
where a pole may sit during a fit. Gain is one figure at the cascade output,
spread across sections on write. The last stage slot is the output/gain stage.

## Method (governing)

Measure first → discover invariants → name only what survives → build the
authoring GUI around those facts. Spectral analysis/reference → identify
important features → hand construct. UI work proceeds as vertical slices in the
real app with fixture + screenshot proof (`--ui-case`, `--fixture`, `--shot`);
a slice is not finished without its screenshot. Judge every screen by: where
does the eye go first · what can I manipulate · what changed · what can be
deleted.

## Corpus prior (measured 2026-08-16, pairing_census)

Factory pole-zero pairing across the null-verified corpus: 65% local pairs,
median interval +2.4 st (the zero rides just above its pole); 35% cross-spectrum
pairs; 17% of zeros exactly on the circle; zero radius median 0.9499. Use as
the design prior for factorization; never as an automatic rule.
