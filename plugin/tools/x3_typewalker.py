from __future__ import annotations

import ctypes
import subprocess
import sys
import time
from pathlib import Path

ctypes.windll.user32.SetProcessDPIAware()
from pywinauto import Desktop, mouse                      # noqa: E402

ROOT = Path(__file__).resolve().parents[1]

DROPDOWN_REL = (685, 163)
COL_DX = [109, 279, 449, 559]
ROW0_DY, ROW_DY = 17, 19.0
LABEL_REL = (-85, -9, 49, 8)

TARGETS = [
    (0, 1,  "X3_2pole_lowpass"),
    (0, 2,  "X3_4pole_lowpass"),
    (0, 3,  "X3_6pole_lowpass"),
    (0, 4,  "X3_2pole_highpass"),
    (0, 5,  "X3_4pole_highpass"),
    (0, 6,  "X3_2pole_bandpass"),
    (0, 7,  "X3_4pole_bandpass"),
    (0, 8,  "X3_contrary_bandpass"),
    (0, 9,  "X3_swept_eq_1oct"),
    (0, 10, "X3_swept_eq_2_1oct"),
    (0, 11, "X3_swept_eq_3_1oct"),
    (0, 12, "X3_phaser1"),
    (0, 13, "X3_phaser2"),
    (1, 0,  "X3_bat_phaser"),
    (1, 1,  "X3_flanger_lite"),
    (1, 2,  "X3_vocal_ah_ay_ee"),
    (1, 3,  "X3_vocal_oo_ah"),
    (1, 4,  "X3_dual_eq_morph"),
    (1, 5,  "X3_dual_eq_lp_morph"),
    (1, 6,  "X3_dual_eq_morph_expr"),
    (1, 7,  "X3_peak_shelf_morph"),
]

DROPDOWN = (0, 0)

def label_pixels():
    from PIL import ImageGrab
    dx, dy = DROPDOWN
    l, t, r, b = LABEL_REL
    return ImageGrab.grab((dx + l, dy + t, dx + r, dy + b)).tobytes()

def dismiss_save_dialog() -> bool:
    from pywinauto import Desktop
    dismissed = False
    for _ in range(12):
        dialogs = [w for w in Desktop(backend="win32").windows(class_name="#32770")
                   if "Emulator X" in w.window_text()]
        if not dialogs:
            break
        btns = [c for c in dialogs[0].children()
                if c.class_name() == "Button" and "No" in c.window_text()]
        if not btns:
            break
        btns[0].click()
        time.sleep(0.5)
        dismissed = True
    return dismissed

def editor_window():
    for w in Desktop(backend="win32").windows():
        t = w.window_text()
        if t.endswith("- Emulator X") or ("EmulatorX" in t and "Master" in t):
            return w
    return None

def select_type(col: int, row: int) -> bool:
    w = editor_window()
    if w is None:
        print("ABORT: X3 editor window not found")
        return False
    r = w.rectangle()
    global DROPDOWN
    DROPDOWN = (r.left + DROPDOWN_REL[0], r.top + DROPDOWN_REL[1])
    w.set_focus()
    time.sleep(0.4)
    for attempt in range(3):
        dismiss_save_dialog()
        before = label_pixels()
        mouse.click(coords=DROPDOWN)
        time.sleep(0.7)
        if dismiss_save_dialog():
            continue
        mouse.click(coords=(DROPDOWN[0] + COL_DX[col],
                            int(round(DROPDOWN[1] + ROW0_DY + ROW_DY * row))))
        time.sleep(0.6)
        dismiss_save_dialog()
        if label_pixels() != before:
            return True
        print(f"  type switch attempt {attempt + 1} did not change the label, retrying")
    print("ABORT: TYPE label never changed - clicks are not landing")
    return False

def main():
    py = sys.executable
    redo = "--redo" in sys.argv
    done = failed = 0
    for col, row, name in TARGETS:
        body = ROOT / "bodies" / "candidates" / f"{name}.body240"
        if body.exists():
            fresh = time.time() - body.stat().st_mtime < 6 * 3600
            if not redo or fresh:
                why = "fresh capture" if fresh else "already in the rail"
                print(f"-- {name}: {why}, skipping")
                continue
        print(f"== {name} ==", flush=True)
        if not select_type(col, row):
            sys.exit(1)
        extra = []
        if "--velocity" in sys.argv:
            extra = ["--velocity", sys.argv[sys.argv.index("--velocity") + 1]]
        rc = subprocess.run(
            [py, str(ROOT / "tools" / "capture_x3.py"), name, *extra],
            capture_output=True, text=True)
        tail = "\n".join(l for l in (rc.stdout + rc.stderr).splitlines()
                         if l.strip() and "Warning" not in l and "warn" not in l)
        print(tail, flush=True)
        if body.exists():
            done += 1
        else:
            failed += 1
            print(f"** {name}: no certified body produced", flush=True)
    print(f"\nWALK COMPLETE: {done} captured+certified, {failed} failed")

if __name__ == "__main__":
    main()
