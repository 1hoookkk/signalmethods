from __future__ import annotations
import time
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

from tools.trenchsrc import TrenchSrc
from tools.surface_evaluator import evaluate_surface, SurfaceMetrics
from tools.surface_archive import SurfaceArchive
from tools.mutation import (
    random_mutation, random_mutation_chain, random_pairing_mutation,
    swap_stages,
)

ROOT = Path(__file__).resolve().parents[1]

@dataclass
class GenerationStats:
    generation: int = 0
    submissions: int = 0
    accepted: int = 0
    best_quality: float = 0.0
    filled_bins: int = 0
    elapsed_s: float = 0.0

class SurfaceEvolution:

    def __init__(self,
                 archive: SurfaceArchive,
                 population_size: int = 16,
                 tournament_size: int = 3,
                 mutation_strength: float = 1.0,
                 crossover_rate: float = 0.3,
                 sample_rate: float = 48000.0,
                 rng: np.random.Generator | None = None):
        self.archive = archive
        self.population_size = population_size
        self.tournament_size = tournament_size
        self.mutation_strength = mutation_strength
        self.crossover_rate = crossover_rate
        self.sample_rate = sample_rate
        self.rng = rng or np.random.default_rng()

        self.generation = archive.generation
        self.history: list[GenerationStats] = []
        self.start_time = time.time()
        self._last_report_time = self.start_time

    def _select_parent(self) -> TrenchSrc | None:
        entries = self.archive.all_entries()
        if not entries:
            return None

        n = min(self.tournament_size, len(entries))
        candidates = [entries[self.rng.integers(0, len(entries))]
                      for _ in range(n)]
        winner = max(candidates, key=lambda e: e.quality_score)
        return TrenchSrc.from_dict(winner.trenchsrc_dict)

    def _select_two_parents(self) -> tuple[TrenchSrc | None, TrenchSrc | None]:
        a = self._select_parent()
        if a is None:
            return None, None
        b = self._select_parent()
        return a, b

    def step(self) -> GenerationStats:
        t0 = time.time()
        accepted = 0

        if self.archive.filled_bins() == 0:
            from tools.trenchsrc import TrenchSrc as TS
            seed = TS.random_seed(
                f"{self.archive.name}_gen0_seed",
                "WORKHORSE",
                rng=self.rng,
            )
            body = seed.compile()
            metrics = evaluate_surface(body, sr=self.sample_rate)
            if self.archive.submit(seed, metrics):
                accepted += 1

        for _ in range(self.population_size):
            if self.rng.random() < self.crossover_rate and self.archive.filled_bins() >= 2:
                parent_a, parent_b = self._select_two_parents()
                if parent_a is None or parent_b is None:
                    continue
                child = random_pairing_mutation(parent_a, parent_b, rng=self.rng)
            else:
                parent = self._select_parent()
                if parent is None:
                    continue
                child = random_mutation(parent, rng=self.rng)

            if self.rng.random() < 0.1 and len(child.voices) >= 2:
                i = self.rng.integers(0, len(child.voices))
                j = self.rng.integers(0, len(child.voices))
                if i != j:
                    child = swap_stages(child, i, j)

            child.name = f"{self.archive.name}_g{self.generation:04d}_{accepted:04d}"

            body = child.compile()
            metrics = evaluate_surface(body, sr=self.sample_rate)

            if self.archive.submit(child, metrics):
                accepted += 1

        self.generation += 1
        self.archive.generation = self.generation

        elapsed = time.time() - t0
        stats = GenerationStats(
            generation=self.generation,
            submissions=self.population_size,
            accepted=accepted,
            best_quality=self.archive.best().quality_score if self.archive.best() else 0.0,
            filled_bins=self.archive.filled_bins(),
            elapsed_s=elapsed,
        )
        self.history.append(stats)
        return stats

    def run(self, generations: int = 500,
            target_fill: float = 0.75,
            target_quality: float = 0.85,
            stall_generations: int = 50) -> list[GenerationStats]:
        stall_counter = 0
        prev_filled = self.archive.filled_bins()

        for gen in range(generations):
            stats = self.step()

            if stats.filled_bins == prev_filled:
                stall_counter += 1
            else:
                stall_counter = 0
                prev_filled = stats.filled_bins

            if gen < 5 or gen % 10 == 0:
                self._progress_line(stats)

            total_bins = self.archive.cutoff_bins * self.archive.resonance_bins
            fill_frac = stats.filled_bins / total_bins
            quality_ok = stats.best_quality >= target_quality

            if fill_frac >= target_fill and quality_ok:
                print(f"\n  Target reached: fill={fill_frac:.1%}, quality={stats.best_quality:.3f}")
                break

            if stall_counter >= stall_generations:
                print(f"\n  Stalled: no new bins in {stall_generations} generations")
                break

        return self.history

    def _progress_line(self, stats: GenerationStats):
        total = self.archive.cutoff_bins * self.archive.resonance_bins
        fill_pct = stats.filled_bins / total * 100
        elapsed = time.time() - self.start_time
        rate = stats.generation / max(elapsed, 0.1)
        print(
            f"  gen {stats.generation:4d} | "
            f"bins {stats.filled_bins:2d}/{total} ({fill_pct:4.0f}%) | "
            f"accepted {stats.accepted:2d}/{stats.submissions:2d} | "
            f"best q={stats.best_quality:.3f} | "
            f"{elapsed:.0f}s ({rate:.1f} gen/s)"
        )

    def report(self) -> str:
        elapsed = time.time() - self.start_time
        total_accepted = self.archive.total_accepted
        total_submitted = self.archive.total_submissions
        lines = [
            f"Surface-Elites Evolution: {self.archive.name}",
            f"  generations: {self.generation}",
            f"  wall time: {elapsed:.0f}s",
            f"  total submissions: {total_submitted}",
            f"  total accepted: {total_accepted}",
            f"  acceptance rate: {total_accepted / max(total_submitted, 1):.1%}",
            f"  filled bins: {self.archive.filled_bins()}/{self.archive.cutoff_bins * self.archive.resonance_bins}",
            f"  best quality: {self.archive.best().quality_score if self.archive.best() else 0:.3f}",
        ]
        if self.history:
            acc_rates = [s.accepted / max(s.submissions, 1) for s in self.history]
            lines.append(f"  acceptance trend: {min(acc_rates):.1%} → {acc_rates[-1]:.1%} (last gen)")

        lines.append("")
        lines.append(self.archive.report())
        return "\n".join(lines)
