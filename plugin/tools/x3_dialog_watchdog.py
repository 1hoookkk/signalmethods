import ctypes
import time

ctypes.windll.user32.SetProcessDPIAware()
from pywinauto import Desktop  # noqa: E402

while True:
    try:
        for w in Desktop(backend="win32").windows(class_name="#32770"):
            if "Emulator X" not in w.window_text():
                continue
            for c in w.children():
                if c.class_name() == "Button" and "No" in c.window_text():
                    c.click()
                    print("dismissed", flush=True)
                    break
    except Exception:
        pass
    time.sleep(0.5)
