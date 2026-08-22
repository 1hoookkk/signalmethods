from __future__ import annotations
import copy
import json
import time
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]

@dataclass
class ArchiveEntry:
    name: str
    category: str
    cutoff_travel: float
    resonance_change: float
    quality_score: float
    trenchsrc_dict: dict
    body240_b64: str = ""
    generation: int = 0
    timestamp: float = 0.0

    def behaviour(self) -> np.ndarray:
        return np.array([self.cutoff_travel, self.resonance_change])

class SurfaceArchive:

    def __init__(self, name: str = "default",
                 cutoff_bins: int = 8, resonance_bins: int = 6,
                 cutoff_range: tuple[float, float] = (0.0, 8.0),
                 resonance_range: tuple[float, float] = (0.0, 40.0),
                 output_dir: Path | None = None):
        self.name = name
        self.cutoff_bins = cutoff_bins
        self.resonance_bins = resonance_bins
        self.cutoff_edges = np.linspace(cutoff_range[0], cutoff_range[1], cutoff_bins + 1)
        self.resonance_edges = np.linspace(resonance_range[0], resonance_range[1], resonance_bins + 1)

        self.grid: list[list[ArchiveEntry | None]] = [
            [None] * resonance_bins for _ in range(cutoff_bins)
        ]
        self.total_submissions = 0
        self.total_accepted = 0
        self.generation = 0
        self.output_dir = output_dir or (ROOT / "evidence" / "surface_archives")

    def _bin(self, cutoff_travel: float, resonance_change: float) -> tuple[int, int]:
        ci = max(0, min(self.cutoff_bins - 1,
               int(np.digitize(cutoff_travel, self.cutoff_edges) - 1)))
        ri = max(0, min(self.resonance_bins - 1,
               int(np.digitize(resonance_change, self.resonance_edges) - 1)))
        return ci, ri

    def submit(self, src: "TrenchSrc", metrics: "SurfaceMetrics",
               quality_score: float | None = None) -> bool:
        from tools.surface_evaluator import SurfaceMetrics

        self.total_submissions += 1

        if not metrics.is_valid():
            return False

        ci, ri = self._bin(metrics.cutoff_travel_octaves, metrics.resonance_change)

        if quality_score is None:
            quality_score = (
                metrics.axis_independence * 0.35
                + (1.0 - min(metrics.smoothness_penalty, 1.0)) * 0.25
                + metrics.surface_area * 0.20
                + min(metrics.cutoff_travel_octaves / 6.0, 1.0) * 0.10
                + min(metrics.resonance_change / 30.0, 1.0) * 0.10
            )

        existing = self.grid[ci][ri]
        if existing is not None and quality_score <= existing.quality_score:
            return False

        import base64
        body = src.compile()
        entry = ArchiveEntry(
            name=src.name,
            category=src.category,
            cutoff_travel=metrics.cutoff_travel_octaves,
            resonance_change=metrics.resonance_change,
            quality_score=quality_score,
            trenchsrc_dict=src.to_dict(),
            body240_b64=base64.b64encode(body).decode("ascii"),
            generation=self.generation,
            timestamp=time.time(),
        )
        self.grid[ci][ri] = entry
        self.total_accepted += 1
        return True

    def best(self) -> ArchiveEntry | None:
        best = None
        best_score = -1.0
        for row in self.grid:
            for entry in row:
                if entry and entry.quality_score > best_score:
                    best = entry
                    best_score = entry.quality_score
        return best

    def filled_bins(self) -> int:
        return sum(1 for row in self.grid for entry in row if entry is not None)

    def all_entries(self) -> list[ArchiveEntry]:
        entries = [e for row in self.grid for e in row if e is not None]
        entries.sort(key=lambda e: e.quality_score, reverse=True)
        return entries

    def report(self) -> str:
        lines = [
            f"Surface Archive: {self.name}",
            f"  bins: {self.cutoff_bins}×{self.resonance_bins} "
            f"(cutoff {self.cutoff_edges[0]:.1f}-{self.cutoff_edges[-1]:.1f} oct, "
            f"resonance {self.resonance_edges[0]:.0f}-{self.resonance_edges[-1]:.0f} dB)",
            f"  submissions: {self.total_submissions}, accepted: {self.total_accepted}",
            f"  filled bins: {self.filled_bins()}/{self.cutoff_bins * self.resonance_bins}",
            f"  generation: {self.generation}",
            "",
        ]
        lines.append("  Behaviour grid (· = empty, # = occupied):")
        for ci in range(self.cutoff_bins):
            row_str = "    "
            for ri in range(self.resonance_bins):
                row_str += "# " if self.grid[ci][ri] else "· "
            row_str += f"  cutoff {self.cutoff_edges[ci]:.1f}-{self.cutoff_edges[ci+1]:.1f} oct"
            lines.append(row_str)
        lines.append("    " + "  ".join(
            f"r{self.resonance_edges[i]:.0f}" for i in range(self.resonance_bins)))

        entries = self.all_entries()[:10]
        if entries:
            lines.append("")
            lines.append(f"  Top {len(entries)} entries:")
            for e in entries:
                lines.append(
                    f"    {e.name:24s}  cutoff={e.cutoff_travel:.2f}oct  "
                    f"res={e.resonance_change:.1f}dB  q={e.quality_score:.3f}"
                )
        return "\n".join(lines)

    def to_json(self, path: Path | None = None) -> Path:
        if path is None:
            self.output_dir.mkdir(parents=True, exist_ok=True)
            path = self.output_dir / f"{self.name}.json"
        data = {
            "name": self.name,
            "cutoff_bins": self.cutoff_bins,
            "resonance_bins": self.resonance_bins,
            "cutoff_range": [float(self.cutoff_edges[0]), float(self.cutoff_edges[-1])],
            "resonance_range": [float(self.resonance_edges[0]), float(self.resonance_edges[-1])],
            "total_submissions": self.total_submissions,
            "total_accepted": self.total_accepted,
            "generation": self.generation,
            "filled_bins": self.filled_bins(),
            "grid": [[
                {
                    "name": e.name,
                    "category": e.category,
                    "cutoff_travel": e.cutoff_travel,
                    "resonance_change": e.resonance_change,
                    "quality_score": e.quality_score,
                    "generation": e.generation,
                    "trenchsrc": e.trenchsrc_dict,
                } if e else None
                for e in row
            ] for row in self.grid],
        }
        path.write_text(json.dumps(data, indent=2))
        return path

    @classmethod
    def from_json(cls, path: Path) -> "SurfaceArchive":
        data = json.loads(path.read_text())
        archive = cls(
            name=data["name"],
            cutoff_bins=data["cutoff_bins"],
            resonance_bins=data["resonance_bins"],
            cutoff_range=(data["cutoff_range"][0], data["cutoff_range"][1]),
            resonance_range=(data["resonance_range"][0], data["resonance_range"][1]),
        )
        archive.total_submissions = data["total_submissions"]
        archive.total_accepted = data["total_accepted"]
        archive.generation = data["generation"]
        for ci, row in enumerate(data["grid"]):
            for ri, e_data in enumerate(row):
                if e_data:
                    archive.grid[ci][ri] = ArchiveEntry(
                        name=e_data["name"],
                        category=e_data["category"],
                        cutoff_travel=e_data["cutoff_travel"],
                        resonance_change=e_data["resonance_change"],
                        quality_score=e_data["quality_score"],
                        trenchsrc_dict=e_data["trenchsrc"],
                        generation=e_data.get("generation", 0),
                    )
        return archive

    def seed_random(self, count: int, category: str = "WORKHORSE",
                    rng: np.random.Generator | None = None):
        from tools.trenchsrc import TrenchSrc
        from tools.surface_evaluator import evaluate_surface

        if rng is None:
            rng = np.random.default_rng()

        for i in range(count):
            src = TrenchSrc.random_seed(
                f"{category.lower()}_seed_{i:04d}", category, rng=rng)
            body = src.compile()
            metrics = evaluate_surface(body)
            self.submit(src, metrics)

        self.generation = 1

    def save_bodies(self, output_dir: Path | None = None):
        if output_dir is None:
            output_dir = self.output_dir / self.name
        output_dir.mkdir(parents=True, exist_ok=True)

        for entry in self.all_entries():
            stem = entry.name.replace(" ", "_").lower()
            trenchsrc_path = output_dir / f"{stem}.trenchsrc"
            import yaml
            trenchsrc_path.write_text(
                yaml.dump(entry.trenchsrc_dict, default_flow_style=False, sort_keys=False))

            import base64
            body = base64.b64decode(entry.body240_b64)
            (output_dir / f"{stem}.body240").write_bytes(body)
