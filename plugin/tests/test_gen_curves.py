import csv
import importlib.util
import math
import re
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR_PATH = ROOT / "plugin" / "tools" / "gen_curves.py"
SPEC = importlib.util.spec_from_file_location("gen_curves", GENERATOR_PATH)
gen_curves = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
import sys
sys.modules[SPEC.name] = gen_curves
SPEC.loader.exec_module(gen_curves)

SYMBOLS = (
    "kSlam",
    "kSlamTrim",
    "kPreamp",
    "kBite",
    "kMorph",
    "kQ",
    "kFollow",
    "kTrack",
)


def fixture_text():
    identity = ", ".join(f"{index / 128:.9g}f" for index in range(129))
    declarations = []
    for symbol in SYMBOLS:
        declarations.append(f"inline constexpr Table {symbol} = {{\n    {identity},\n}};\n")
        declarations.append(
            f"inline constexpr std::array<float, 2> {symbol}Endpoints = {{ 0f, 1f }};\n"
        )
    return (
        "#pragma once\nSENTINEL BEFORE -- byte stable\n"
        + "".join(declarations)
        + "SENTINEL AFTER -- byte stable\n"
    )


def write_ratings(path, raw_values, mean_function):
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=("raw", "rating", "presentation"))
        writer.writeheader()
        for raw in raw_values:
            mean = mean_function(raw)
            for presentation, noise in (("A", -0.06), ("B", 0.0), ("C", 0.06)):
                writer.writerow(
                    {"raw": raw, "rating": mean + noise, "presentation": presentation}
                )


def declaration_bytes(text, symbol):
    match = re.search(
        rf"inline constexpr Table {symbol} = \{{.*?\}};\n"
        rf"inline constexpr std::array<float, 2> {symbol}Endpoints = \{{.*?\}};\n",
        text,
        re.DOTALL,
    )
    assert match is not None
    return match.group(0)


def parse_table(text, symbol):
    matches = re.findall(
        rf"inline\s+constexpr\s+Table\s+{symbol}\s*=\s*\{{(.*?)\}};",
        text,
        re.DOTALL,
    )
    if len(matches) != 1:
        raise AssertionError(f"expected one {symbol} table, found {len(matches)}")
    values = [float(token) for token in re.findall(r"([-+0-9.eE]+)f", matches[0])]
    if len(values) != 129:
        raise AssertionError(f"{symbol} has {len(values)} values, expected 129")
    return values


def parse_endpoints(text, symbol):
    match = re.search(
        rf"inline\s+constexpr\s+std::array\s*<\s*float\s*,\s*2\s*>\s+"
        rf"{symbol}Endpoints\s*=\s*\{{(.*?)\}};",
        text,
        re.DOTALL,
    )
    if match is None:
        raise AssertionError(f"missing {symbol}Endpoints")
    values = [float(token) for token in re.findall(r"([-+0-9.eE]+)f", match.group(1))]
    if len(values) != 2:
        raise AssertionError(f"{symbol}Endpoints has {len(values)} values, expected 2")
    return values


class GeneratorTests(unittest.TestCase):
    def test_power_law_round_trip_and_byte_preservation(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            curves = directory / "Curves.h"
            ratings = directory / "ratings.csv"
            plot = directory / "scratch" / "output.png"
            original = fixture_text()
            curves.write_text(original, encoding="utf-8", newline="")
            raw_values = (0.1, 0.3, 0.5, 0.7, 0.9)
            forward = lambda raw: 1.2 + 7.6 * ((raw - 0.1) / 0.8) ** 2.0
            write_ratings(ratings, raw_values, forward)

            result = gen_curves.bake(
                "output",
                ratings,
                curves,
                plot,
                endpoint_low=0.1,
                endpoint_high=0.9,
                print_output=False,
            )

            self.assertEqual(result.model, "power")
            baked = curves.read_text(encoding="utf-8")
            table = parse_table(baked, "kSlam")
            self.assertEqual(len(table), 129)
            self.assertTrue(all(math.isfinite(value) for value in table))
            self.assertTrue(all(a <= b for a, b in zip(table, table[1:])))
            for index, raw in enumerate(table):
                normalized_forward = ((raw - 0.1) / 0.8) ** 2.0
                self.assertLessEqual(abs(normalized_forward - index / 128.0), 0.01)
            self.assertEqual(parse_endpoints(baked, "kSlam"), [0.1, 0.9])
            self.assertEqual(table[0], 0.1)
            self.assertEqual(table[-1], 0.9)
            self.assertTrue(plot.exists())
            self.assertGreater(plot.stat().st_size, 0)
            self.assertEqual(
                declaration_bytes(baked, "kQ"), declaration_bytes(original, "kQ")
            )
            self.assertIn("SENTINEL BEFORE -- byte stable", baked)
            self.assertIn("SENTINEL AFTER -- byte stable", baked)

    def test_non_power_shape_selects_monotone_spline(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            curves = directory / "Curves.h"
            ratings = directory / "ratings.csv"
            plot = directory / "fallback.png"
            curves.write_text(fixture_text(), encoding="utf-8", newline="")
            means = {0.1: 1.2, 0.3: 1.35, 0.5: 6.8, 0.7: 7.0, 0.9: 8.8}
            write_ratings(ratings, tuple(means), means.__getitem__)

            result = gen_curves.bake(
                "output",
                ratings,
                curves,
                plot,
                power_rmse=0.001,
                endpoint_low=0.1,
                endpoint_high=0.9,
                print_output=False,
            )

            self.assertEqual(result.model, "monotone-spline")
            baked = curves.read_text(encoding="utf-8")
            table = parse_table(baked, "kSlam")
            self.assertTrue(all(math.isfinite(value) for value in table))
            self.assertTrue(all(a <= b for a, b in zip(table, table[1:])))
            self.assertEqual(table[0], 0.1)
            self.assertEqual(table[-1], 0.9)
            self.assertEqual(parse_endpoints(baked, "kSlam"), [0.1, 0.9])


class ShippingCurvesTests(unittest.TestCase):
    def test_all_shipping_tables_are_finite_and_monotone(self):
        path = ROOT / "plugin" / "source" / "parameters" / "Curves.h"
        text = path.read_text(encoding="utf-8")
        for symbol in SYMBOLS:
            with self.subTest(symbol=symbol):
                values = parse_table(text, symbol)
                self.assertTrue(all(math.isfinite(value) for value in values))
                nondecreasing = all(a <= b for a, b in zip(values, values[1:]))
                nonincreasing = all(a >= b for a, b in zip(values, values[1:]))
                self.assertTrue(
                    nondecreasing or nonincreasing,
                    f"shipping table {symbol} is not monotone in either direction",
                )
                parse_endpoints(text, symbol)


if __name__ == "__main__":
    unittest.main()
