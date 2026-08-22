import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CARGO = str(Path.home() / ".cargo" / "bin" / "cargo.exe")
PLUGINVAL = os.environ.get(
    "TRENCH_PLUGINVAL",
    str(Path.home() / "Downloads" / "pluginval_Windows" / "pluginval.exe"))
VST3 = r"C:\Program Files\Common Files\VST3\TRENCH.vst3"

results = []


def step(name, cmd, fail_marker=None, env=None):
    print(f"\n== {name} ==", flush=True)
    e = dict(os.environ)
    if env:
        e.update(env)
    r = subprocess.run(cmd, cwd=str(ROOT), env=e, capture_output=True, text=True)
    out = (r.stdout or "") + (r.stderr or "")
    tail = "\n".join(out.strip().splitlines()[-12:])
    print(tail)
    ok = r.returncode == 0 and (fail_marker is None or fail_marker not in out)
    results.append((name, ok))
    return ok


def main():
    step("cargo test (trench-core, release)",
         [CARGO, "test", "-p", "trench-core", "--release", "--no-fail-fast"])
    step("x3 null gate (360 states, sample-exact)",
         [sys.executable, "tools/x3_null_gate.py"])
    step("doctor (decisions + shipped-body freeze)",
         [sys.executable, "-m", "bench.doctor"])
    faceshot = ROOT / "build/TRENCH_FaceShot_artefacts/Release/TRENCH_FaceShot.exe"
    step("FaceShot build",
         ["cmake", "--build", "build", "--config", "Release", "--target", "TRENCH_FaceShot"])
    step("FaceShot proof battery", [str(faceshot)], fail_marker="FAIL")
    step("FaceShot NO FILTER state", [str(faceshot)], fail_marker="FAIL",
         env={"TRENCH_DRIVE_ONLY_ITER": "1"})
    step("VST3 build + install",
         ["powershell", "-ExecutionPolicy", "Bypass", "-File", "tools/build_install_vst3.ps1"])
    if Path(PLUGINVAL).exists():
        step("pluginval strictness 10",
             [PLUGINVAL, "--validate-in-process", "--strictness-level", "10",
              "--timeout-ms", "300000", VST3])
    else:
        print(f"\n== pluginval == SKIPPED (not found at {PLUGINVAL}; set TRENCH_PLUGINVAL)")
        results.append(("pluginval strictness 10", False))

    print("\n" + "=" * 60)
    all_ok = True
    for name, ok in results:
        print(f"  {'PASS' if ok else 'FAIL':4s}  {name}")
        all_ok = all_ok and ok
    print("=" * 60)
    print("SHIP GATE: " + ("PASS - this build is a ship candidate" if all_ok
                           else "FAIL - do not ship"))
    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
