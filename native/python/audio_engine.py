import math
import os
import sys
import threading
from typing import List, Optional
import numpy as np
import sounddevice as sd

sys.path.insert(0, os.path.dirname(__file__))
import trench_core

class AudioEngine:
    def __init__(self, sample_rate_hz: float = 44100.0):
        self.sample_rate = sample_rate_hz
        self.runner = trench_core.Runner(sample_rate_hz)
        self.runner.set_bite(0.18)
        self.runner.set_ring_leveller(True)
        self._stream: Optional[sd.OutputStream] = None
        self._lock = threading.Lock()
        self._phase = 0.0
        self._freq = 110.0
        self._active = False
        self._volume = 0.15

    def set_cascade(self, biquads: List[List[float]], immediate: bool = False):
        with self._lock:
            if immediate:
                self.runner.set_immediate(biquads)
            else:
                self.runner.set_glide(biquads, 64)

    def set_bite(self, bite: float):
        with self._lock:
            self.runner.set_bite(bite)

    def set_frequency(self, hz: float):
        with self._lock:
            self._freq = max(20.0, min(1000.0, hz))

    def _audio_callback(self, outdata, frames_count, time_info, status):
        block = np.zeros(frames_count, dtype=np.float32)
        with self._lock:
            step = self._freq / self.sample_rate
            for i in range(frames_count):
                self._phase += step
                if self._phase >= 1.0:
                    self._phase -= 1.0
                val = 2.0 * self._phase - 1.0
                val -= (8.0 / (math.pi * math.pi)) * math.sin(2.0 * math.pi * self._phase) * 0.1
                block[i] = val * self._volume
            self.runner.process(block)
        outdata[:, 0] = block
        if outdata.shape[1] > 1:
            outdata[:, 1] = block

    def start(self):
        if self._stream is not None:
            return
        try:
            self._stream = sd.OutputStream(
                samplerate=self.sample_rate,
                channels=2,
                dtype="float32",
                blocksize=256,
                callback=self._audio_callback
            )
            self._stream.start()
            self._active = True
        except Exception:
            self._stream = None
            self._active = False

    def stop(self):
        if self._stream is not None:
            try:
                self._stream.stop()
                self._stream.close()
            except Exception:
                pass
            self._stream = None
            self._active = False

    def play_preview(self, biquads: List[List[float]], duration_s: float = 0.45, hz: float = 130.81):
        def _worker():
            runner = trench_core.Runner(self.sample_rate)
            runner.set_immediate(biquads)
            runner.set_bite(0.18)
            runner.set_ring_leveller(True)
            n_samples = int(self.sample_rate * duration_s)
            t = np.linspace(0, duration_s, n_samples, endpoint=False, dtype=np.float32)
            phase = (t * hz) % 1.0
            saw = (2.0 * phase - 1.0).astype(np.float32)
            env = np.ones(n_samples, dtype=np.float32)
            fade_in = int(self.sample_rate * 0.01)
            fade_out = int(self.sample_rate * 0.05)
            env[:fade_in] = np.linspace(0, 1, fade_in)
            env[-fade_out:] = np.linspace(1, 0, fade_out)
            buf = saw * env * 0.18
            runner.process(buf)
            try:
                sd.play(buf, self.sample_rate)
                sd.wait()
            except Exception:
                pass
        t = threading.Thread(target=_worker, daemon=True)
        t.start()
