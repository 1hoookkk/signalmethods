from __future__ import annotations

import ctypes

import numpy as np

from .core import RATE, lib

lib.trench_engine_set_agc_enabled.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.trench_engine_set_saturation_enabled.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.trench_engine_set_dc_block_enabled.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.trench_engine_set_agc_drive.argtypes = [ctypes.c_void_p, ctypes.c_float]
lib.trench_engine_set_grit.argtypes = [ctypes.c_void_p, ctypes.c_float]
lib.trench_engine_set_input_preamp.argtypes = [ctypes.c_void_p, ctypes.c_float]


class Audition:
    def __init__(self, rate: float = 48_000.0):
        self.rate = rate
        self.morph = 0.0
        self.stream = None
        self.engine = lib.trench_engine_create()
        lib.trench_engine_prepare(self.engine, ctypes.c_double(rate))
        self._rng = np.random.default_rng()

    def load(self, body: bytes) -> bool:
        return lib.trench_engine_load_body_bytes_at(
            self.engine, body, len(body), ctypes.c_double(RATE)) == 0

    def playing(self) -> bool:
        return self.stream is not None

    def start(self) -> str:
        if self.stream is not None:
            return ""
        try:
            import sounddevice as sd
        except Exception as e:
            return f"no audio: {e}"

        def cb(outdata, frames, _t, _s):
            n = (self._rng.standard_normal(frames).astype(np.float32) * 0.15)
            left = np.ascontiguousarray(n)
            right = np.ascontiguousarray(n.copy())
            lib.trench_engine_process_block(
                self.engine,
                left.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                right.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                frames, ctypes.c_double(self.morph), ctypes.c_double(0.0))
            outdata[:, 0] = left
            outdata[:, 1] = right

        try:
            self.stream = sd.OutputStream(samplerate=self.rate, channels=2,
                                          dtype="float32", blocksize=512,
                                          callback=cb)
            self.stream.start()
        except Exception as e:
            self.stream = None
            return f"no audio: {e}"
        return ""

    def stop(self) -> None:
        if self.stream is not None:
            self.stream.stop()
            self.stream.close()
            self.stream = None

    def close(self) -> None:
        self.stop()
        if self.engine:
            lib.trench_engine_destroy(self.engine)
            self.engine = None

    def set_switch(self, name: str, on: bool) -> None:
        fn = {"agc": lib.trench_engine_set_agc_enabled,
              "saturation": lib.trench_engine_set_saturation_enabled,
              "dc block": lib.trench_engine_set_dc_block_enabled}[name]
        fn(self.engine, 1 if on else 0)

    def set_amount(self, name: str, v: float) -> None:
        fn = {"agc drive": lib.trench_engine_set_agc_drive,
              "bite": lib.trench_engine_set_grit,
              "input": lib.trench_engine_set_input_preamp}[name]
        fn(self.engine, ctypes.c_float(v))
