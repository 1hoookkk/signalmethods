# Seeding recipes — proposed structure

Proposal only. No implementation. The ask: make the fitter's initialisation
phase pluggable per E-mu filter family, move the existing peel logic behind that
boundary, and keep the solver and loss ignorant of which recipe ran.

## Where this lands

Canonical fitter core is C++: `native/trench-core`, namespace
`trench::core::p2k`. The Rust `trench-core` in trench-authoring is reference
only and is not changed by this.

The relevant current declarations, `native/trench-core/include/trench/core/p2k.hpp`:

```cpp
struct SeedContinuous {};
struct SeedPeel {};
using Seed = std::variant<SeedContinuous, SeedPeel, CornerWords>;

CornerWords peel_seed(std::span<const double> target);
CornerWords peel_seed_by(std::span<const double> target, Cost cost);

std::optional<P2kFit> fit_corner(std::span<const double> target,
                                 std::span<const Seed> seeds,
                                 const FitOptions& opts);
```

So a dispatch variant already exists. The change is to give it one more
alternative whose payload is a family tag, and to move placement behind a single
narrow interface.

## Two decisions to settle first

**1. The recipe must not be section-count-bound.** The survey in
`NATIVE_CONTAINER_SURVEY.md` establishes that the shipping machine is seven
sections with **section 7 pole-only** — six zero slots, seven pole slots. The
six-stage P2K path is the narrower case, not the general one. Baking `6` into
the recipe interface now would have to be undone immediately. The interface
below therefore takes a `SeedShape` describing how many sections exist and which
of them may carry a zero, and returns geometry sized to it.

**2. The family tag is an input, never inferred.** Nothing should try to
classify a target curve into a family. The tag comes from the caller — the
authored session, or the operator's choice in the app. A misclassified target
silently seeding the wrong recipe is worse than no routing at all.

## Proposed additions

```cpp
// --- what the machine being seeded looks like -----------------------------

struct SeedShape {
  std::size_t sections{kStageCount};   // 6 for P2K legacy, 7 for native
  std::size_t zero_sections{kStageCount};  // leading sections that may hold a
                                           // zero; native = sections - 1
  double sample_rate_hz{kSr};
};

// --- routing ---------------------------------------------------------------

enum class Family {
  kVowel,      // DIPTHONGS: poles on formants, zeros inside as shoulders
  kPhaser,     // notches in a series, zeros near the circle
  kFlanger,    // as phaser, denser and deeper
  kLowpass,    // Butterworth-ish poles, zeros parked at DC/Nyquist
  kEqualiser,  // matched pole/zero per band, gain either sign
  kDistortion,
  kGeneric,    // no family knowledge; the loss-driven default
};

// --- the interface ---------------------------------------------------------

// A recipe's ONLY job is to produce starting coordinates.  It sees the target
// and the shape.  It does not see FitOptions, the loss, the freedom mask, the
// solver, or any previous fit result.  It must be pure and deterministic:
// same (target, shape) -> same words, no RNG, no global state.
class SeedRecipe {
 public:
  virtual ~SeedRecipe() = default;
  [[nodiscard]] virtual std::string_view name() const noexcept = 0;
  [[nodiscard]] virtual CornerWords seed(std::span<const double> target,
                                         const SeedShape& shape) const = 0;
};

// Registry.  Returns a reference to a process-wide immutable instance; there is
// no per-call state to own.  kGeneric is the fallback and must always resolve.
const SeedRecipe& recipe_for(Family family) noexcept;

// --- how it reaches the fitter ---------------------------------------------

struct SeedFamily {
  Family family{Family::kGeneric};
};

using Seed = std::variant<SeedContinuous, SeedPeel, SeedFamily, CornerWords>;
```

`fit_corner` and `fit_corner_watched` signatures are **unchanged**. Inside, the
existing switch over the seed variant gains one arm:

```cpp
// pseudocode, inside the seed-materialising step
if (const auto* f = std::get_if<SeedFamily>(&seed)) {
  words = recipe_for(f->family).seed(target, shape);
}
```

Everything after that point — `enter`, `polish`, `stage_moves`, the gain pass,
packing — is untouched and cannot tell which arm ran. That is the separation the
brief asks for, and it is enforced by the recipe never receiving `FitOptions`.

## Moving the existing logic

`peel_seed` / `peel_seed_by` are the Bell-1961 residual-peak hunt. They become
the body of one recipe:

```cpp
class VowelRecipe final : public SeedRecipe {
 public:
  std::string_view name() const noexcept override { return "vowel"; }
  CornerWords seed(std::span<const double> target,
                   const SeedShape& shape) const override;   // = today's peel
};
```

Keep the free functions `peel_seed` / `peel_seed_by` and keep `SeedPeel` in the
variant. They are covered by the parity fixture and the 132-corner bank refit,
and those tests are load-bearing. `VowelRecipe::seed` calls `peel_seed_by`
rather than reimplementing it, so there is exactly one copy of the placement
maths and the existing acceptance evidence still applies unchanged.

`SeedPeel` and `SeedFamily{kVowel}` are then deliberate synonyms for now. When
the vowel recipe later diverges from raw peel, the divergence is visible as a
diff inside `VowelRecipe` and the old path stays pinned for the tests.

## Stubs for the unwritten recipes

Per the brief, no placement maths is invented here. Each unimplemented family
gets a declaration and a body that defers:

```cpp
class PhaserRecipe final : public SeedRecipe {
 public:
  std::string_view name() const noexcept override { return "phaser"; }
  CornerWords seed(std::span<const double> target,
                   const SeedShape& shape) const override;
  // Until the ROM survey defines notch placement, this returns
  // GenericRecipe's result.  It must NOT guess an even-notch series.
};
```

A stub that defers to generic is honest and testable. A stub that guesses would
become a project rule by accident, which the repository instructions forbid.

`GenericRecipe` is the fallback. **It must not be matching pursuit.** That was
proposed, implemented and measured, and it is roughly three times worse than the
existing peak picker across all 44 DVTD mouths, winning on one — see
`DVTD_VOWEL_FIT.md` §3e. Greedy loss reduction spends sections carving broad
valleys instead of placing resonances, and cannot revise an early choice.

Until something beats it, `GenericRecipe` should be today's peak-picking seeder:
rank spectral maxima, place poles on them, sweep how many bells start as cuts.
The domain knowledge in "poles go on peaks" is load-bearing, not a shortcut.

## PCA: valid over responses, invalid over geometry

Measured, not asserted (`dev/pca_probe.py`, `dev/pca_check.py`).

**Responses are genuinely low-dimensional.** Cumulative explained variance, one
corner per body so correlated corners cannot inflate it, per-curve level and
family mean removed first:

| family | n bodies | PC1 | PC2 | PC3 |
|---|---|---|---|---|
| STANDARD | 26 | **97.0%** | 98.7% | 99.6% |
| COMPLEX | 81 | **93.1%** | 97.2% | 98.2% |
| FLANGERS | 21 | 87.0% | 90.6% | 93.9% |
| DIPTHONGS | 23 | 77.0% | 90.7% | 94.2% |

The structure survives dropping from 184 corners to 23 bodies, so it is real and
not an artefact of counting eight correlated corners per filter. Caveat kept
honest: with n in the 20s against 256 frequency bins, PCA is rank-limited to n,
so the multi-component figures flatter themselves. PC1 alone is the trustworthy
number, and COMPLEX at n=81 is the most trustworthy row. Dipthongs is the least
compressible family, which is what one would expect of vowels.

**Section index is not correspondence across bodies.** Pole frequency at a fixed
section index, corner 0, geometric spread:

| family | geometric sd at fixed section index |
|---|---|
| DIPTHONGS | 1.39 - 3.16 octaves |
| FLANGERS | 1.56 - 2.22 octaves |
| STANDARD | 1.15 - 3.33 octaves |
| COMPLEX | 1.97 - 2.86 octaves |

Section 7 of the dipthongs spans 22 Hz to 16148 Hz — nine and a half octaves.
Section index carries essentially no frequency meaning from one body to the
next.

**So PCA over geometry is not available.** Averaging word vectors, root
coordinates, or per-section parameters across bodies would blend section 3 of
one filter with section 3 of an unrelated one. This is the correspondence
invariant in `native/CLAUDE.md` — "PCA may provide a corpus prototype and
deviation coordinates; it does not assign sections or override packed
correspondence" — and it is now measured rather than assumed. PCA over
*responses* is unaffected, because a cascade response is a product and does not
care what order the sections are in.

**Where it does fit this architecture.** PCA yields curves, not geometry, so it
cannot be a seeding recipe by itself. The usable route is one step removed:

1. Take a family's mean response, or a low-PC reconstruction of it.
2. Fit it **once, offline**, with generous effort.
3. Store the resulting single concrete cascade as that family's prototype.
4. `VowelRecipe` / `FlangerRecipe` / etc. seed from that stored cascade,
   optionally shifted to the target's dominant frequency.

Step 3 is what keeps this legal: the stored artefact is one real fitted cascade,
not an average of geometry, so no cross-body correspondence is ever assumed. And
it fits `SeedRecipe` exactly — the recipe stays pure and deterministic, and the
solver still cannot tell which recipe ran.

Note this makes PCA a **seed** source, not a target source. The workstation
brief caps targets at two (a pasted reference curve, and the push). Introducing
a PCA-derived target would be a third and would amend the brief; introducing a
PCA-derived seed does not, because seeds were never enumerated. That is the
clean way in, and it is worth preferring for that reason alone.

Untested, and the obvious first experiment: whether a family-prototype seed
actually beats the peak picker on in-family targets. Given that matching pursuit
lost badly to the peak picker, this should be measured before it is believed.

**Flag before anyone builds this.** The step-2 wording above — "fit the family
mean response once offline" — is the exact construction Sandell & Martens tried
and reported against. See `dev/CITATIONS_PCA_MARTENS.md`; the relevant passages
are on p. 37 of the 1992 paper, where a simple average is said to carry
"objectionable idiosyncrasies" that a PC-based prototype does not, and where the
prototype itself is described as "bland" and "perhaps not musically useful".
They also give a specific alternative construction — a MANOVA split into
pooled-within-groups and between-groups SSCP matrices — rather than a mean.
The citations are recorded; the reasoning over them is deliberately left open
and is not settled here.

## What the evidence already says each recipe will need

From `DVTD_VOWEL_FIT.md` §7, measured over the decoded bodies — recorded here so
the heuristics are written from data, not from the family name:

| family | median pole r | median zero r | paired zeros cutting |
|---|---|---|---|
| Flangers | 0.968 | 0.993 | 69% |
| Dipthongs | 0.979 | 0.944 | 8% |
| Standard | 0.7071 | 0.994 | 24% |
| Equalisation | 0.948 | 0.992 | 28% |

Flangers and vowels are near-inverses: zero-led with zeros on the circle, versus
pole-led with zeros pulled inside. Standard filters sit at the Butterworth pole
radius with zeros parked at the band edges. These are the starting points for
the placement maths, to be settled against the ROM data later.

## Testing boundary

- One test per recipe asserting determinism: same input twice, identical words.
- One test asserting every `Family` resolves through `recipe_for` without
  throwing, so adding an enumerator cannot silently fall through.
- One test asserting `SeedFamily{kVowel}` and `SeedPeel` produce byte-identical
  words, which pins the move as a pure refactor.
- The existing parity fixture and 132-corner bank refit must pass untouched. If
  either moves, the refactor is wrong.

## Open, and deliberately not decided here

- Whether `SeedShape.zero_sections` is the right way to express the row-7 law,
  or whether it should be a per-section mask. A mask is more general; the survey
  only justifies the simpler form so far.
- Whether recipes should be allowed to return several candidate seeds rather
  than one. `fit_corner` already takes a span of seeds, so the plumbing exists,
  but nothing yet needs it.
- The seven-stage solver path itself. This design makes the seeding side ready
  for it; the solver is separate work.

## Next session

The user's stated focus, ahead of implementing any of the above: the manuals,
and `.4` versus cubes analysed **separately** rather than pooled. The family
statistics in this document pool square and cube bodies together, which the
survey shows differ in corner structure — those numbers should be re-derived
split by geometry before any recipe's placement maths is written from them.
