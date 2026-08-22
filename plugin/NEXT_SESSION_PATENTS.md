TRENCH — read the patents. Settle what we guessed.

Repo: C:\Users\hooki\trench-x3-clean. Run `python trench.py doctor` first; it
prints what is already decided and whether the repo still obeys it. Tyson is a
producer, not a coder. Lead with the result. Terse.

THE ONLY GOAL: every law the runtime obeys is either a QUOTE from a primary
source or an explicit measurement. Nothing else survives the session. Where a
source settles a question, the answer lands in `bench/facts.py` as a fact with
its citation, and the code is changed to agree or the disagreement is recorded.

This is not a reading session. It is an audit with a diff at the end.

## THE RULE

Every extracted law is a VERBATIM quote with its location — patent number plus
column and line, or claim number, or page. No paraphrase, no "the patent
suggests", no reasoning from what a filter designer would plausibly do. If a
source does not answer a question, the answer is "NOT IN THE SOURCES" and it
stays a measurement, not a law.

Half this repo's wrong turns came from a plausible mechanism written in the
same voice as a measured fact. `design/`, `docs/` and the commentary in
`recipes/INTENT.md` are agent narration and E-MU's real descriptors are mixed
into INTENT with invention in identical formatting. Treat everything that is
not in `ref/` as suspect.

## THE SOURCES, ON DISK

    ref/patents/US5170369.pdf                          Rossum, interpolation
                                                       architecture. Claim 5 is
                                                       the coefficient encoding,
                                                       claim 6 the interpolation.
    ref/patents/rossum_armadillo_coefficient_encoding.pdf
                                                       ARMAdillo, WASPAA 1991.
                                                       The encoding AND the
                                                       pole-analysis plot.
    ref/patents/US5943427_Massie_et_al.pdf             3D audio, 1995. Col. 12
                                                       is the name-the-feature
                                                       method already quoted in
                                                       AUTHORING_SPEC.
    ref/patents/US5952599_Dolby_et_al.pdf              never read
    ref/inputs/making_digital_filters_sound_analog.pdf  Rossum, ICMC 1992
    ref/morpheus_manual_zplane_descriptions.txt        1661 lines, E-MU's own
                                                       filter descriptions
    ref/emu/, ref/heritage/, ref/x3_menu/              unread
    ref/martens/                                       8 papers, PALETTE

## THE QUESTIONS, IN PRIORITY ORDER

Priority is by how much the answer changes what we build next.

1. THE ARMADILLO PLANE. The editor is being built on it, so it must be exact.
   AUTHORING_SPEC records `R' = 20 log10(1/(1-R))` and
   `theta' = pi(10 + log2 Omega)/10`, excluding theta below pi/1024, quoted from
   the 1991 paper. Verify both formulas against the source character by
   character, get the definition of Omega, and find out what he says the plot is
   FOR. "Even spacing means even perception" is the claim we are relying on —
   find where he says it, or stop repeating it.

2. THE INTERPOLATION AXIS ORDER. We interpolate packed words Morph-first, then
   Q, and `trench-core/src/minifloat.rs:611` says in its own output that this is
   UNSOURCED. Measured harmless (median 0.0006 dB) but unsourced. US5170369
   claim 6 is interpolation — does it specify an order, or an order-independent
   form? With three axes now the question gets worse, not better.

3. CORNER ADDRESSING. "2^N sets of coefficients, where N is the number of
   control parameters" is quoted from Symbolic Sound, not from Rossum. Find the
   primary statement. Then: is the corner index bit order specified anywhere?
   We chose `m | q<<1 | z<<2`. If a source says otherwise, we change it now,
   while nothing has shipped in the native format.

4. WHY 24% OF POLES MAKE NO PEAK. Measured: 710 conjugate poles in the 33
   character bodies, 76% produce a peak in the total, 24% produce nothing. The
   cause is a zero of one section landing on a later section's pole. Does any
   source state a placement rule, a minimum separation, or a section ordering
   law? This is the single biggest obstacle to authoring by placement, and a
   stated rule beats a heuristic.

5. THE SEVENTH SECTION. Symbolic Sound says "seven two-pole IIR sections
   connected in series", "first lowpass and six parametric". Is that in a
   primary source? If the first section is genuinely a lowpass by construction,
   the editor should say so and the recipe language should have it.

6. SCALE. We treat level as one value per corner spread across sections, on the
   evidence that Talking Hedz has b0 = 0.56189 identical in all six. Is the law
   stated, or is one-per-corner an inference from one body?

7. ZERO PLACEMENT. Corpus says 51.5% of rows put the zero more than 18 semitones
   from their own section's pole, and 17.5% of conjugate zeros sit at r = 1.0000
   exactly. Both are measurements with no stated law behind them. Does a source
   describe what the zeros are for?

8. THE NONLINEARITY. `cascade.rs` implements saturate-the-state with pole-radius
   modulation from the ICMC 1992 paper. Re-read it against the implementation
   and confirm we did what he wrote, including where the output is tapped.

9. US5952599 (Dolby) has never been opened. Find out in one pass whether it is
   relevant at all, and if not, say so in writing so nobody opens it again.

10. THE MORPHEUS MANUAL, 1661 lines, is E-MU's own filter descriptions and is
    the ground truth for filter-type language. It has never been read end to
    end. Extract the descriptor vocabulary verbatim - it is what the editor
    should call things, and `docs/` currently uses invented names.

## WHAT TO PRODUCE

Not a document. Three things:

1. Facts in `bench/facts.py`, each with `kind="SOURCED"`, the verbatim quote,
   and the citation. `python trench.py facts` must run clean afterwards.
2. A diff list: for every law found, does the code agree? Where it disagrees,
   either change the code and re-run `python trench.py gate` to show what moved,
   or record the disagreement as a fact with both values.
3. A kill list: claims currently in `AUTHORING_SPEC.md`, `NEXT_TASK.md`,
   `design/` or `docs/` that the sources do not support. Delete them. A wrong
   law written confidently is worse than an open question.

## WHAT NOT TO DO

- Do not implement anything from a patent that is not already a question above.
  Patents describe machines nobody built. We have a working runtime that is
  bit-identical on 184 reference bodies — `python trench.py gate`. It does not
  get rebuilt because a claim is interesting.
- Do not re-open a decision in `python trench.py doctor` unless a SOURCE
  contradicts it. Preference is not evidence.
- Do not paraphrase. If a quote is long, quote it long.

## STATE YOU NEED

- Runtime is 7 sections x 8 corners, `BODY_BYTES = 560`, 240-byte bodies load
  bit-identical through a compatibility path. `python trench.py gate` proves it
  against `5187ee1fb8f8f27d` over 184 reference bodies.
- `python trench.py facts` re-derives every measured constant from the corpus.
  Five are currently UNSOURCED and were invented on 2026-08-13:
  CROWN_SPREAD_DB, NOTCH_R, GLOBAL_ITERS, SHOULDER_OCT, FRAME_R_MAX. If a source
  settles any of them, that is a win. If not, they should be deleted along with
  the solver they belong to.
- The next build after this is the editor: Rust + mlua recipes + egui, one
  binary. See `NEXT_SESSION_EDITOR.md`. Everything found here should be aimed at
  making that editor obey the machine rather than obey me.

## HOUSE RULES

- Never fill a data gap with a number.
- Quote, never paraphrase from memory.
- The measurement beats the plausible mechanism.
- PowerShell for cargo; Git Bash picks the wrong linker.
- Ears are the only shipping gate.
