# The TRENCH workstation and the owner's intent

## THE INTENT (the owner's own recorded words, 2026-08)

Why these presets exist: the project began with the MORPH wheel on the
X3's Talking Hedz — a filter that felt like a mouth. TRENCH's presets are
ORIGINALS IN THE SPECIES of the loved ROM references, never copies: each
is authored from a behavior spec, NULLED against its reference to prove
the species, then pushed for daylight, gated by measured distance from
the ROM atlas, and finally gated by ears — "ears are the only shipping
gate."

The product goal, verbatim: "I want to curate a select few rhythms
[and bodies] that give producers the sound of their dreams." The test for
every shipped body: a producer on stock sounds loads it, turns the wheel,
and says "how do I do THAT". A small hero rail where every slot is
earned; names are literal or culture-literal — the name states the
wheel's travel (X to Y) or decodes instantly for trap producers.

The authoring law: the owner ALWAYS picks the endpoints. The tool shows
candidates; it never chooses. He is a producer, not a coder; the bench is
his console and it speaks the compiler's language — lanes, sections,
poles, radii — never metaphors. His stated lightbulb: "seeing how each
section is broken down" — the cascade inspector's per-lane decomposition
is what made authoring thinkable.

## THE HOUSE DISCOVERY (verbatim from the intent dossier)

House discovery that frames everything: **E-MU authored from a shared pose
vocabulary.** Identical section rows recur across characters — the 79 Hz sub
pole, the 479+13367 pair, the 17.6 k air pole appear verbatim in BolandBass,
LucifersQ, BassTracer and BassBox-303; KlubKlassik and AcidRavage share their
entire M0 pose with the M100 poles RE-PAIRED to different sections (same
destinations, different journeys); Millennium and MeatyGizmo share both Q0
corners outright. Characters are recombinations + one new axis. That is
literally our LIBRARY rail + COPY POSE + re-pair workflow.

## THE WORKSTATION (its own agent brief, verbatim)

# Workstation bench — agent brief (tight scope)

FIRST, before any code: read the patents and papers in `ref/` — the Rossum
ARMAdillo paper (the founding coordinate system), the Ding/Rossum morphing
paper, and the IIR patent. Then `recipes/INTENT.md`. The bench exists so Tyson
can author the INTENT recipes by hand; every control speaks the tooling's
language (`batch_compiler.plan_body` is the one planning path; the FFI is
the only math).

## Laws (violations are rework)
- No teaching text, no lore, no technobabble, no invented shorthand.
- Nothing hidden behind right-click; menus are shortcuts only.
- No invented constants: every number from a table, the FFI, or a doc.
- Only measured-bandwidth sources appear. Ever.
- Q model: morph C0->C1 exists at BOTH Q rows; both chords visible.
- Refusals loud; certify at save and at engine load; plate beside body.
- Hit targets are hands-sized; click-anywhere beats precision grabs.
- Tyson picks the endpoints. Candidates are shown, never chosen for him.

## ALLOWED (nothing else)
1. ARMADILLO PLOT: a pane in the ARMAdillo log-polar coordinates (the
   paper's space: log2 Hz x R' dB) showing the six lanes' poles and zeros
   at the scrub position, both Q rows distinguishable. Read coordinates
   through the FFI probe only.
2. LAYOUT: compact the panes - the score is stretched; plots sized to
   content, no dead vertical space.
3. HAND-FEEL: verify every drag (landing wire, scrub, seat picks) under a
   real mouse; fix dead zones without fattening lines.
4. FIRST BODY: drive one TalkingHedz-species body end to end from the
   INTENT recipe (two vowel poses, wire the relay, uniform-gentle Q),
   certify PASS, plate written. Leave it in bodies/candidates for ears.

## Proof
- `python tools/workstation/audit.py` must exit 0 (every check).
- Every claim backed by a run, not a read.


## FACTUAL INVENTORY (what exists today)

Python/Qt app at tools/workstation: score / buildup / container panes,
real-engine audition through the trench_core FFI (the only math), session
save/load, shapes, an audit harness (audit.py must exit 0). The authoring
loop it serves: two measured poses -> make-body compile -> certify ->
taste linter (5 axioms: register architecture, voicing, motion taxonomy,
zero grammar, Q attitude) -> null vs reference -> distance gate -> ears.
Known gaps recorded elsewhere: the six-row source material is not fully
fed; the joint packed-path optimizer (proven 3-4 dB better than
corner-only fits, which collapse mid-morph) is not yet the default path;
the hero specs are not yet compiled into lane targets drawn in the panes.
