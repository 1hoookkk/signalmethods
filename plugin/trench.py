from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent

COMMANDS = {
    "doctor":  ("is the repo healthy, and what is already decided",
                [sys.executable, "-m", "bench.doctor"]),
    "facts":   ("re-derive every measured constant from the corpus and report drift",
                [sys.executable, "-m", "bench.facts"]),
    "gate":    ("prove no shipped body changed",
                [str(ROOT / "target/release/topology-gate.exe"), str(ROOT / "target/gate.bin")]),
    "sheet":   ("open the editor",
                [sys.executable, "-m", "tools.wordsheet"]),
    "inspect": ("cascade plate for one body: BODY [RATE] [OUT.png]",
                [sys.executable, "tools/inspect_body.py"]),
    "make":    ("build a filter: ENDPOINT_A ENDPOINT_B SLUG [--install]",
                [sys.executable, "-m", "tools.make_filter"]),
    "install": ("build and install the VST3",
                ["powershell", "-ExecutionPolicy", "Bypass", "-File",
                 "tools/build_install_vst3.ps1"]),
    "test":    ("cargo test -p trench-core",
                ["cargo", "test", "-p", "trench-core"]),
    "ship":    ("the whole ship gate: tests, null gate, doctor, proof battery, install, pluginval",
                [sys.executable, "tools/ship_gate.py"]),
}

START_HERE = """
START HERE — the loop, in order

  1. trench doctor          what is decided, and whether the repo still obeys it
  2. trench facts           every number re-derived from the corpus; drift is a failure
  3. trench sheet           author. drag, hear, rate
  4. trench make ... --install
  5. trench install         then restart FL
  6. trench gate            prove nothing that already shipped moved

RULES THAT ARE NOT NEGOTIABLE

  A number in the authoring path must be in bench/facts.py with a derivation
  that re-runs. If you cannot derive it, it is UNSOURCED and it is a bug.

  Deep notches are the mechanism, not a defect. Factory bodies dip 69.8 dB
  below their own median.

  A pole lands where you put it: 0.02 semitones. Place, do not solve.

  Ears are the only shipping gate. Plots triage; they do not decide.
"""


def main() -> int:
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help", "help"):
        print(START_HERE)
        print("COMMANDS\n")
        for name, (what, _) in COMMANDS.items():
            print(f"  trench {name:9s} {what}")
        print()
        return 0
    name = sys.argv[1]
    if name not in COMMANDS:
        print(f"unknown command '{name}'. try: trench help")
        return 2
    what, cmd = COMMANDS[name]
    return subprocess.run(cmd + sys.argv[2:], cwd=str(ROOT)).returncode


if __name__ == "__main__":
    raise SystemExit(main())
