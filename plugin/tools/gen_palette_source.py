import argparse
import struct

import numpy as np

SR = 44100
SEGMENT_SECONDS = 6.0
FADE_SECONDS = 0.5
PARTIALS = 5
NOISE_DB = -42.0
PEAK = 0.75

CHORDS = [
    (55.00, [110.00, 130.81, 164.81, 196.00]),
    (43.65, [87.31, 110.00, 130.81, 164.81]),
    (65.41, [130.81, 164.81, 196.00, 246.94]),
    (49.00, [98.00, 123.47, 146.83, 164.81]),
]


def additive_saw(frequency, length, phase_offset):
    n = np.arange(length, dtype=np.float64)
    out = np.zeros(length, dtype=np.float64)
    for k in range(1, PARTIALS + 1):
        f = frequency * k
        if f >= SR * 0.5:
            break
        out += np.sin(2.0 * np.pi * f * n / SR + phase_offset + k * 0.61803398875) / k
    return out


def fade_shape(fade):
    ramp = 0.5 - 0.5 * np.cos(np.pi * np.arange(fade) / max(fade - 1, 1))
    return ramp


def segment_envelope(length, fade):
    envelope = np.ones(length, dtype=np.float64)
    ramp = fade_shape(fade)
    envelope[:fade] = ramp
    envelope[-fade:] = ramp[::-1]
    return envelope


def build(duration, seed):
    rng = np.random.default_rng(seed)
    segment_length = int(SEGMENT_SECONDS * SR)
    fade = int(FADE_SECONDS * SR)
    step = segment_length - fade
    segments = int(np.ceil(duration / SEGMENT_SECONDS)) + 1
    total = segments * step + fade

    envelope = segment_envelope(segment_length, fade)
    left = np.zeros(total, dtype=np.float64)
    right = np.zeros(total, dtype=np.float64)

    for index in range(segments):
        root, notes = CHORDS[index % len(CHORDS)]
        span = slice(index * step, index * step + segment_length)

        bass = additive_saw(root, segment_length, rng.uniform(0.0, 2.0 * np.pi))
        left[span] += bass * envelope * 0.90
        right[span] += bass * envelope * 0.88

        for note in notes:
            l = additive_saw(note * 1.0012, segment_length, rng.uniform(0.0, 2.0 * np.pi))
            r = additive_saw(note * 0.9988, segment_length, rng.uniform(0.0, 2.0 * np.pi))
            left[span] += l * envelope * 0.55
            right[span] += r * envelope * 0.55

    noise = rng.standard_normal(total) * (10.0 ** (NOISE_DB / 20.0))
    left += noise
    right += rng.standard_normal(total) * (10.0 ** (NOISE_DB / 20.0))

    peak = max(np.max(np.abs(left)), np.max(np.abs(right)))
    scale = PEAK / peak
    left *= scale
    right *= scale
    return np.stack([left, right])


def write_float_wav(path, audio):
    channels = audio.shape[0]
    frames = audio.T.astype("<f4").tobytes()
    header = b"RIFF" + struct.pack("<I", 36 + len(frames)) + b"WAVE"
    header += b"fmt " + struct.pack("<IHHIIHH", 16, 3, channels, SR,
                                    SR * channels * 4, channels * 4, 32)
    header += b"data" + struct.pack("<I", len(frames))
    with open(path, "wb") as handle:
        handle.write(header + frames)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--duration", type=float, default=24.0)
    parser.add_argument("--seed", type=int, default=20260908)
    args = parser.parse_args()

    audio = build(args.duration, args.seed)
    write_float_wav(args.output, audio)
    peak = float(np.max(np.abs(audio)))
    rms = float(np.sqrt(np.mean(audio ** 2)))
    print("wrote %s  %.2f s  peak %.4f  rms %.4f  crest %.2f dB"
          % (args.output, audio.shape[1] / SR, peak, rms, 20.0 * np.log10(peak / rms)))


if __name__ == "__main__":
    main()
