#!/usr/bin/env python3
"""Bake an individualized Palette rating session into Curves.h."""

from __future__ import annotations

import argparse
import csv
import math
import os
import re
import stat
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, Sequence


TABLE_SIZE = 129
AXIS_SYMBOLS = {
    "output": "kSlam",
    "slam": "kSlam",
    "output-trim": "kSlamTrim",
    "slam-trim": "kSlamTrim",
    "input": "kPreamp",
    "preamp": "kPreamp",
    "z": "kBite",
    "bite": "kBite",
    "morph": "kMorph",
    "q": "kQ",
    "follow": "kFollow",
    "track": "kTrack",
}


class CurveBakeError(ValueError):
    """A ratings session cannot be safely fitted or baked."""


@dataclass(frozen=True)
class RatingPoint:
    raw: float
    mean: float
    count: int


@dataclass(frozen=True)
class FitSummary:
    model: str
    evaluate: Callable[[float], float]
    description: str
    rmse: float
    normalized_rmse: float
    r_squared: float


@dataclass(frozen=True)
class BakeResult:
    symbol: str
    model: str
    low: float
    high: float
    points: tuple[RatingPoint, ...]
    fitted: tuple[float, ...]
    residuals: tuple[float, ...]
    table: tuple[float, ...]
    description: str
    rmse: float
    normalized_rmse: float
    r_squared: float


def _finite_number(text: str, label: str) -> float:
    try:
        value = float(text)
    except (TypeError, ValueError) as exc:
        raise CurveBakeError(f"{label} must be a number, got {text!r}") from exc
    if not math.isfinite(value):
        raise CurveBakeError(f"{label} must be finite, got {text!r}")
    return value


def read_ratings(path: Path | str) -> tuple[RatingPoint, ...]:
    path = Path(path)
    try:
        handle = path.open("r", encoding="utf-8-sig", newline="")
    except OSError as exc:
        raise CurveBakeError(f"cannot read ratings file {path}: {exc}") from exc
    grouped: dict[float, list[float]] = {}
    with handle:
        reader = csv.DictReader(handle)
        required = {"raw", "rating", "presentation"}
        if reader.fieldnames is None or not required.issubset(reader.fieldnames):
            found = ", ".join(reader.fieldnames or ()) or "none"
            raise CurveBakeError(
                f"ratings CSV requires columns raw,rating,presentation; found {found}"
            )
        for row_number, row in enumerate(reader, start=2):
            raw = _finite_number(row.get("raw", ""), f"row {row_number} raw")
            rating = _finite_number(
                row.get("rating", ""), f"row {row_number} rating"
            )
            presentation = row.get("presentation", "")
            if not presentation or not presentation.strip():
                raise CurveBakeError(f"row {row_number} presentation must be nonempty")
            if not 0.0 <= raw <= 1.0:
                raise CurveBakeError(f"row {row_number} raw {raw} is outside [0,1]")
            if not 1.0 <= rating <= 9.0:
                raise CurveBakeError(f"row {row_number} rating {rating} is outside [1,9]")
            grouped.setdefault(raw, []).append(rating)
    if len(grouped) < 3:
        raise CurveBakeError("ratings must contain at least 3 unique raw values")
    missing_repeats = [raw for raw, values in grouped.items() if len(values) < 2]
    if missing_repeats:
        values = ", ".join(f"{raw:g}" for raw in sorted(missing_repeats))
        raise CurveBakeError(f"every raw value needs a repeat; missing at {values}")
    return tuple(
        RatingPoint(raw, sum(values) / len(values), len(values))
        for raw, values in sorted(grouped.items())
    )


def select_endpoints(
    points: Sequence[RatingPoint],
    endpoint_low: float | None,
    endpoint_high: float | None,
) -> tuple[tuple[RatingPoint, ...], float, float]:
    sampled_low, sampled_high = points[0].raw, points[-1].raw
    low = sampled_low if endpoint_low is None else endpoint_low
    high = sampled_high if endpoint_high is None else endpoint_high
    if not math.isfinite(low) or not math.isfinite(high):
        raise CurveBakeError("endpoints must be finite")
    if low < sampled_low or high > sampled_high:
        raise CurveBakeError(
            f"endpoints [{low:g},{high:g}] must lie inside sampled range "
            f"[{sampled_low:g},{sampled_high:g}]"
        )
    if not low < high:
        raise CurveBakeError("endpoint-low must be less than endpoint-high")
    selected = tuple(point for point in points if low <= point.raw <= high)
    if len(selected) < 3:
        raise CurveBakeError("chosen endpoints must bracket at least 3 unique raw values")
    return selected, low, high


def _linear_regression(x: Sequence[float], y: Sequence[float]) -> tuple[float, float]:
    n = len(x)
    sx = sum(x)
    sy = sum(y)
    sxx = sum(value * value for value in x)
    sxy = sum(a * b for a, b in zip(x, y))
    denominator = n * sxx - sx * sx
    if abs(denominator) <= 1.0e-20:
        return sy / n, 0.0
    b = (n * sxy - sx * sy) / denominator
    return (sy - b * sx) / n, b


def _fit_statistics(
    observed: Sequence[float], fitted: Sequence[float]
) -> tuple[float, float, float]:
    residual_sum = sum((actual - predicted) ** 2 for actual, predicted in zip(observed, fitted))
    rmse = math.sqrt(residual_sum / len(observed))
    span = max(observed) - min(observed)
    normalized = rmse / span if span > 0.0 else math.inf
    mean = sum(observed) / len(observed)
    total_sum = sum((actual - mean) ** 2 for actual in observed)
    r_squared = 1.0 - residual_sum / total_sum if total_sum > 0.0 else -math.inf
    return rmse, normalized, r_squared


def fit_power(
    points: Sequence[RatingPoint], low: float, high: float
) -> tuple[FitSummary, float]:
    observed = [point.mean for point in points]
    u = [(point.raw - low) / (high - low) for point in points]
    best: tuple[float, float, float, tuple[float, ...]] | None = None
    log_low, log_high = math.log(0.05), math.log(20.0)
    for index in range(8001):
        p = math.exp(log_low + (log_high - log_low) * index / 8000.0)
        transformed = [max(0.0, value) ** p for value in u]
        a, b = _linear_regression(transformed, observed)
        fitted = tuple(a + b * value for value in transformed)
        sse = sum((actual - predicted) ** 2 for actual, predicted in zip(observed, fitted))
        candidate = (sse, p, a, b, fitted)
        if best is None or candidate[0] < best[0]:
            best = candidate
    assert best is not None
    _, p, a, b, fitted = best
    rmse, normalized, r_squared = _fit_statistics(observed, fitted)

    def evaluate(raw: float) -> float:
        normalized_raw = min(1.0, max(0.0, (raw - low) / (high - low)))
        return a + b * normalized_raw**p

    summary = FitSummary(
        "power",
        evaluate,
        f"rating = {a:.9g} + {b:.9g} * u^{p:.9g}",
        rmse,
        normalized,
        r_squared,
    )
    return summary, b


def _pava(values: Sequence[float], weights: Sequence[int], increasing: bool) -> list[float]:
    sign = 1.0 if increasing else -1.0
    blocks: list[list[float | int]] = []
    for index, (value, weight) in enumerate(zip(values, weights)):
        blocks.append([index, index, sign * value * weight, weight])
        while len(blocks) >= 2:
            left, right = blocks[-2], blocks[-1]
            if float(left[2]) / int(left[3]) <= float(right[2]) / int(right[3]):
                break
            blocks[-2:] = [[left[0], right[1], float(left[2]) + float(right[2]), int(left[3]) + int(right[3])]]
    result = [0.0] * len(values)
    for start, end, total, weight in blocks:
        level = sign * float(total) / int(weight)
        for index in range(int(start), int(end) + 1):
            result[index] = level
    return result


class _Pchip:
    def __init__(self, x: Sequence[float], y: Sequence[float]):
        self.x = tuple(x)
        self.y = tuple(y)
        n = len(x)
        h = [x[index + 1] - x[index] for index in range(n - 1)]
        delta = [(y[index + 1] - y[index]) / h[index] for index in range(n - 1)]
        slopes = [0.0] * n
        if n == 2:
            slopes[:] = [delta[0], delta[0]]
        else:
            for index in range(1, n - 1):
                if delta[index - 1] == 0.0 or delta[index] == 0.0 or delta[index - 1] * delta[index] <= 0.0:
                    slopes[index] = 0.0
                else:
                    w1 = 2.0 * h[index] + h[index - 1]
                    w2 = h[index] + 2.0 * h[index - 1]
                    slopes[index] = (w1 + w2) / (w1 / delta[index - 1] + w2 / delta[index])
            slopes[0] = self._endpoint_slope(h[0], h[1], delta[0], delta[1])
            slopes[-1] = self._endpoint_slope(h[-1], h[-2], delta[-1], delta[-2])
        self.slopes = tuple(slopes)

    @staticmethod
    def _endpoint_slope(h0: float, h1: float, d0: float, d1: float) -> float:
        slope = ((2.0 * h0 + h1) * d0 - h0 * d1) / (h0 + h1)
        if slope * d0 <= 0.0:
            return 0.0
        if d0 * d1 < 0.0 and abs(slope) > abs(3.0 * d0):
            return 3.0 * d0
        return slope

    def __call__(self, value: float) -> float:
        if value <= self.x[0]:
            return self.y[0] + self.slopes[0] * (value - self.x[0])
        if value >= self.x[-1]:
            return self.y[-1] + self.slopes[-1] * (value - self.x[-1])
        lo, hi = 0, len(self.x) - 1
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if value < self.x[mid]:
                hi = mid
            else:
                lo = mid
        width = self.x[hi] - self.x[lo]
        t = (value - self.x[lo]) / width
        t2, t3 = t * t, t * t * t
        return (
            (2.0 * t3 - 3.0 * t2 + 1.0) * self.y[lo]
            + (t3 - 2.0 * t2 + t) * width * self.slopes[lo]
            + (-2.0 * t3 + 3.0 * t2) * self.y[hi]
            + (t3 - t2) * width * self.slopes[hi]
        )


def fit_monotone_spline(
    points: Sequence[RatingPoint], low: float, high: float, increasing: bool
) -> FitSummary:
    x = [point.raw for point in points]
    observed = [point.mean for point in points]
    isotonic = _pava(observed, [point.count for point in points], increasing)
    spline = _Pchip(x, isotonic)
    fitted = [spline(value) for value in x]
    rmse, normalized, r_squared = _fit_statistics(observed, fitted)
    direction = "increasing" if increasing else "decreasing"
    return FitSummary(
        "monotone-spline",
        spline,
        f"{direction} PAVA + shape-preserving cubic Hermite",
        rmse,
        normalized,
        r_squared,
    )


def _invert_table(
    evaluate: Callable[[float], float], low: float, high: float, increasing: bool
) -> tuple[float, ...]:
    y_low, y_high = evaluate(low), evaluate(high)
    if not math.isfinite(y_low) or not math.isfinite(y_high):
        raise CurveBakeError("fitted endpoint response is not finite")
    span = y_high - y_low
    tolerance = 1.0e-10 * max(1.0, abs(y_low), abs(y_high))
    if abs(span) <= tolerance or (span > 0.0) != increasing:
        raise CurveBakeError("fitted endpoint response is flat or uninvertible")
    values: list[float] = []
    for index in range(TABLE_SIZE):
        target = y_low + span * index / (TABLE_SIZE - 1)
        left, right = low, high
        for _ in range(64):
            middle = (left + right) * 0.5
            y_middle = evaluate(middle)
            if (y_middle < target) == increasing:
                left = middle
            else:
                right = middle
        values.append((left + right) * 0.5)
    values[0], values[-1] = low, high
    if any(not math.isfinite(value) for value in values):
        raise CurveBakeError("inverse table contains a non-finite value")
    epsilon = 1.0e-12
    if any(values[index] > values[index + 1] + epsilon for index in range(TABLE_SIZE - 1)):
        raise CurveBakeError("inverse table is not monotone")
    return tuple(values)


def fit_and_invert(
    points: Sequence[RatingPoint],
    low: float,
    high: float,
    power_rmse: float = 0.05,
) -> tuple[FitSummary, tuple[float, ...]]:
    if not math.isfinite(power_rmse) or power_rmse < 0.0:
        raise CurveBakeError("power-rmse must be a finite nonnegative number")
    endpoint_trend = points[-1].mean - points[0].mean
    if abs(endpoint_trend) <= 1.0e-10:
        raise CurveBakeError("endpoint ratings are flat; response cannot be inverted")
    increasing = endpoint_trend > 0.0
    power, power_direction = fit_power(points, low, high)
    power_ok = (
        power.normalized_rmse <= power_rmse
        and power.r_squared >= 0.90
        and abs(power.evaluate(high) - power.evaluate(low)) > 1.0e-10
        and (power_direction > 0.0) == increasing
    )
    selected = power if power_ok else fit_monotone_spline(points, low, high, increasing)
    return selected, _invert_table(selected.evaluate, low, high, increasing)


def _format_float(value: float) -> str:
    if value == 0.0:
        value = 0.0
    return f"{value:.9g}f"


def _table_body(values: Sequence[float], newline: str) -> str:
    lines = []
    for start in range(0, len(values), 8):
        row = ", ".join(_format_float(value) for value in values[start : start + 8])
        lines.append(f"    {row},")
    return newline + newline.join(lines) + newline


def _replace_unique_body(text: str, pattern: re.Pattern[str], body: str, label: str) -> str:
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        raise CurveBakeError(f"expected exactly one {label} declaration, found {len(matches)}")
    match = matches[0]
    return text[: match.start("body")] + body + text[match.end("body") :]


def patch_curves(
    curves_path: Path | str,
    symbol: str,
    table: Sequence[float],
    low: float,
    high: float,
) -> None:
    path = Path(curves_path)
    try:
        original = path.read_bytes()
        text = original.decode("utf-8")
    except (OSError, UnicodeDecodeError) as exc:
        raise CurveBakeError(f"cannot read UTF-8 Curves.h {path}: {exc}") from exc
    newline = "\r\n" if b"\r\n" in original else "\n"
    table_pattern = re.compile(
        rf"(?P<prefix>inline\s+constexpr\s+Table\s+{re.escape(symbol)}\s*=\s*\{{)"
        rf"(?P<body>.*?)(?P<suffix>\}};)",
        re.DOTALL,
    )
    endpoint_symbol = f"{symbol}Endpoints"
    endpoint_pattern = re.compile(
        rf"(?P<prefix>inline\s+constexpr\s+std::array\s*<\s*float\s*,\s*2\s*>\s+"
        rf"{re.escape(endpoint_symbol)}\s*=\s*\{{)"
        rf"(?P<body>.*?)(?P<suffix>\}};)",
        re.DOTALL,
    )
    # Resolve and validate both regions before touching the file.
    if len(list(table_pattern.finditer(text))) != 1:
        count = len(list(table_pattern.finditer(text)))
        raise CurveBakeError(f"expected exactly one {symbol} declaration, found {count}")
    if len(list(endpoint_pattern.finditer(text))) != 1:
        count = len(list(endpoint_pattern.finditer(text)))
        raise CurveBakeError(
            f"expected exactly one {endpoint_symbol} declaration, found {count}"
        )
    patched = _replace_unique_body(text, table_pattern, _table_body(table, newline), symbol)
    endpoint_body = f" {_format_float(low)}, {_format_float(high)} "
    patched = _replace_unique_body(
        patched, endpoint_pattern, endpoint_body, endpoint_symbol
    )
    encoded = patched.encode("utf-8")
    mode = stat.S_IMODE(path.stat().st_mode)
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as temporary:
            temporary.write(encoded)
            temporary.flush()
            os.fsync(temporary.fileno())
        os.chmod(temporary_name, mode)
        os.replace(temporary_name, path)
    except Exception:
        try:
            os.unlink(temporary_name)
        except OSError:
            pass
        raise


def make_plot(
    path: Path | str,
    result: BakeResult,
    evaluate: Callable[[float], float],
) -> None:
    import matplotlib

    matplotlib.use("Agg", force=True)
    from matplotlib import pyplot as plt

    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    figure, axes = plt.subplots(2, 1, figsize=(9, 8), constrained_layout=True)
    samples = [result.low + (result.high - result.low) * index / 512 for index in range(513)]
    axes[0].plot(samples, [evaluate(value) for value in samples], label=result.model)
    axes[0].scatter(
        [point.raw for point in result.points],
        [point.mean for point in result.points],
        color="black",
        zorder=3,
        label="mean rating",
    )
    for point, residual in zip(result.points, result.residuals):
        axes[0].annotate(f"{residual:+.3f}", (point.raw, point.mean), xytext=(3, 4), textcoords="offset points", fontsize=7)
    axes[0].set(xlabel="raw parameter", ylabel="rating (1-9)", title=result.description)
    axes[0].legend()
    knob = [index / (TABLE_SIZE - 1) for index in range(TABLE_SIZE)]
    axes[1].plot(knob, result.table, label="inverse table")
    axes[1].scatter(knob[::8], result.table[::8], s=10)
    axes[1].set(xlabel="perceptual knob", ylabel="raw parameter", title="129-point inverse lookup")
    axes[1].grid(alpha=0.25)
    figure.savefig(path, dpi=150)
    plt.close(figure)


def bake(
    axis: str,
    ratings_path: Path | str,
    curves_path: Path | str,
    plot_path: Path | str,
    power_rmse: float = 0.05,
    endpoint_low: float | None = None,
    endpoint_high: float | None = None,
    print_output: bool = True,
) -> BakeResult:
    try:
        symbol = AXIS_SYMBOLS[axis.lower()]
    except KeyError as exc:
        raise CurveBakeError(
            f"unknown axis {axis!r}; choose one of {', '.join(sorted(AXIS_SYMBOLS))}"
        ) from exc
    all_points = read_ratings(ratings_path)
    points, low, high = select_endpoints(all_points, endpoint_low, endpoint_high)
    fit, table = fit_and_invert(points, low, high, power_rmse)
    fitted = tuple(fit.evaluate(point.raw) for point in points)
    residuals = tuple(point.mean - predicted for point, predicted in zip(points, fitted))
    result = BakeResult(
        symbol,
        fit.model,
        low,
        high,
        points,
        fitted,
        residuals,
        table,
        fit.description,
        fit.rmse,
        fit.normalized_rmse,
        fit.r_squared,
    )
    # Render first, then perform the single atomic source mutation only after all
    # fitting and plotting work has succeeded.
    make_plot(plot_path, result, fit.evaluate)
    patch_curves(curves_path, symbol, table, low, high)
    if print_output:
        print(f"selected model: {fit.model}")
        print(f"function: {fit.description}")
        print(
            f"RMSE={fit.rmse:.9g} normalized_RMSE={fit.normalized_rmse:.9g} "
            f"R2={fit.r_squared:.9g}"
        )
        for point, predicted, residual in zip(points, fitted, residuals):
            print(
                f"raw={point.raw:.9g} mean={point.mean:.9g} fit={predicted:.9g} "
                f"residual={residual:+.9g} count={point.count}"
            )
        for index, value in enumerate(table):
            print(f"{index:03d} {value:.9g}")
        print(f"plot: {Path(plot_path)}")
        print(f"patched: {Path(curves_path)} ({symbol})")
    return result


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--axis", required=True, choices=sorted(AXIS_SYMBOLS))
    parser.add_argument("--ratings", required=True, type=Path)
    parser.add_argument("--curves", required=True, type=Path)
    parser.add_argument("--plot", required=True, type=Path)
    parser.add_argument("--power-rmse", type=float, default=0.05)
    parser.add_argument("--endpoint-low", type=float)
    parser.add_argument("--endpoint-high", type=float)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        bake(
            args.axis,
            args.ratings,
            args.curves,
            args.plot,
            args.power_rmse,
            args.endpoint_low,
            args.endpoint_high,
        )
    except (CurveBakeError, OSError, RuntimeError, ImportError) as exc:
        print(f"error: {exc}", file=__import__("sys").stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
