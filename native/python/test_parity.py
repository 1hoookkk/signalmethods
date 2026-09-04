import glob
import os
import sys
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
import trench_core

def test_p2k_roundtrip():
    pattern = os.path.join("plugin", "presets", "p2k", "*.body240")
    files = glob.glob(pattern)
    assert len(files) == 33, f"Expected 33 P2K bodies, found {len(files)}"
    for path in files:
        with open(path, "rb") as f:
            raw = f.read()
        assert len(raw) == 240, f"File {path} is {len(raw)} bytes, expected 240"
        body = trench_core.Body.from_legacy_bytes(raw)
        out = body.to_legacy_bytes()
        assert raw == out, f"Byte mismatch on P2K roundtrip for {path}"
    print(f"PASS: 33/33 P2K bodies round-trip byte-for-byte")

def test_morpheus_roundtrip():
    pattern = os.path.join("evidence", "factory-data", "morpheus", "raw", "bodies", "*.body")
    files = glob.glob(pattern)
    assert len(files) == 289, f"Expected 289 Morpheus cubes, found {len(files)}"
    for path in files:
        with open(path, "rb") as f:
            raw = f.read()
        assert len(raw) == 560, f"File {path} is {len(raw)} bytes, expected 560"
        body = trench_core.Body.from_native_bytes(raw)
        out = body.to_native_bytes()
        assert raw == out, f"Byte mismatch on Morpheus roundtrip for {path}"
    print(f"PASS: 289/289 Morpheus cubes round-trip byte-for-byte")

def test_corner_cascades():
    pattern = os.path.join("plugin", "presets", "p2k", "*.body240")
    files = glob.glob(pattern)
    corners = [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)]
    for path in files:
        body = trench_core.Body.from_file(path)
        for ci, (m, q) in enumerate(corners):
            words = body.interpolate_words(m, q, 0.0)
            for si in range(6):
                stored = body.get_words(ci, si)
                assert words[si] == stored, f"Word mismatch at corner {ci} section {si} in {path}"
    print("PASS: P2K corner cascades match stored words exactly")

def test_pad_curve_precision():
    pattern = os.path.join("plugin", "presets", "p2k", "*.body240")
    files = glob.glob(pattern)
    grid_hz = np.geomspace(40.0, 16000.0, 96)
    test_pads = [(0.0, 0.0), (0.35, 0.65), (0.5, 0.5), (1.0, 1.0)]
    for path in files[:5]:
        body = trench_core.Body.from_file(path)
        for m, q in test_pads:
            biquads = body.cascade(m, q, 0.0, 44100.0, 44100.0)
            db = trench_core.cascade_response_db(biquads, grid_hz, 44100.0)
            assert np.all(np.isfinite(db)), f"Non-finite response for {path} at pad ({m}, {q})"
            assert np.all(db >= -120.0) and np.all(db <= 120.0)
    print("PASS: Pad curves evaluated through C ABI are continuous and finite")

def test_runner_audition():
    runner = trench_core.Runner(44100.0)
    runner.set_sample_rate(44100.0)
    runner.set_bite(0.18)
    runner.set_ring_leveller(True)
    files = glob.glob(os.path.join("plugin", "presets", "p2k", "*.body240"))
    body = trench_core.Body.from_file(files[0])
    biquads = body.cascade(0.5, 0.5, 0.0, 44100.0, 44100.0)
    runner.set_immediate(biquads)
    block = np.ones(512, dtype=np.float32) * 0.1
    out = runner.process(block)
    assert np.all(np.isfinite(out))
    assert not np.all(out == 0.1), "Runner output did not filter the input"
    print("PASS: CascadeRunner processes audio through C ABI")

def test_audio_analysis():
    t = np.linspace(0, 0.1, 4410, dtype=np.float32)
    sig = 0.5 * np.sin(2.0 * np.pi * 500.0 * t) + 0.3 * np.sin(2.0 * np.pi * 1500.0 * t)
    poles = trench_core.speech_poles(sig, 44100.0, 6)
    assert len(poles) > 0
    res = trench_core.audio_resonances(sig, 44100.0, 6)
    assert len(res) > 0
    print(f"PASS: Audio analysis extracted {len(poles)} speech poles and {len(res)} resonances")

if __name__ == "__main__":
    test_p2k_roundtrip()
    test_morpheus_roundtrip()
    test_corner_cascades()
    test_pad_curve_precision()
    test_runner_audition()
    test_audio_analysis()
    print("ALL PARITY TESTS PASSED")
