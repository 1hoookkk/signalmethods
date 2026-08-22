"""THE CAPTURE RIG: the X3 renders itself; we fit everything.

Drives the standalone Emulator X over a loopMIDI virtual port and records
its output by WASAPI loopback - no cable, no pixel automation. Per preset:
four parked corner takes -> corners_from_wavs, one slow wheel ride ->
fit_field. Certified bodies land in bodies/candidates/, plates alongside.

Setup (once): X3 standalone open, MIDI input set to "loopMIDI Port", and a
CAPTURE preset that is the filter and NOTHING else - one voice, a looped
bright/rich sample, amp env full-on instant (no attack/release shaping), no
LFOs, no filter envelope, no cords except wheel->MORPH (+ Q wheel), all
insert/master FX off. Anything else in the path gets measured as if it were
the filter. Swap only the filter type between captures. Then:

  python tools/capture_x3.py --list                      # devices/ports
  python tools/capture_x3.py --probe --program 3         # CC sanity take
  python tools/capture_x3.py NAME --program 3            # full capture

Batch: a text file of "program name" lines via --batch FILE.

Wheel cords default to mod wheel (CC1) for MORPH and CC21 for Q - override
with --morph-cc / --q-cc if the preset cords differ. Capture rules
(SOURCE_PLAN): parked knobs, take level < 0 dBFS - the rig warns on clip.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from scipy.io import wavfile
import rtmidi
import soundcard as sc

ROOT = Path(__file__).resolve().parents[1]
TAKES = ROOT / "evidence" / "x3_takes"

def open_midi(port_hint: str) -> rtmidi.MidiOut:
    out = rtmidi.MidiOut()
    ports = out.get_ports()
    for i, p in enumerate(ports):
        if port_hint.lower() in p.lower():
            out.open_port(i)
            return out
    sys.exit(f"MIDI port matching '{port_hint}' not found; have {ports}")

class Rig:
    def __init__(self, args):
        self.midi = open_midi(args.midi_port)
        self.ch = args.channel - 1
        self.velocity = args.velocity
        self.morph_cc = args.morph_cc
        self.q_cc = args.q_cc
        self.rate = args.rate
        spks = sc.all_speakers()
        matches = [s for s in spks if args.device.lower() in s.name.lower()]
        if not matches:
            sys.exit(f"no output device matching '{args.device}'; have {[s.name for s in spks]}")
        self.spk = matches[0]
        self.mic = sc.get_microphone(str(self.spk.name), include_loopback=True)
        self._pin_volume()

    def _pin_volume(self):
        try:
            from ctypes import cast, POINTER
            from comtypes import CLSCTX_ALL
            from pycaw.utils import AudioUtilities
            from pycaw.api.endpointvolume import IAudioEndpointVolume
            for dev in AudioUtilities.GetAllDevices():
                if dev.FriendlyName and self.spk.name in dev.FriendlyName:
                    vol = cast(dev._dev.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None),
                               POINTER(IAudioEndpointVolume))
                    vol.SetMasterVolumeLevelScalar(1.0, None)
                    vol.SetMute(0, None)
                    print(f"  capture device '{self.spk.name}' volume pinned to 100%")
                    return
            print(f"  NOTE: could not pin volume for '{self.spk.name}' - keep it steady by hand")
        except Exception as e:
            print(f"  NOTE: volume pin failed ({e}) - keep the device volume steady by hand")

    def cc(self, num: int, val: int):
        self.midi.send_message([0xB0 | self.ch, num, val])

    def park(self, morph: int, q: int):
        self.cc(self.morph_cc, morph)
        self.cc(self.q_cc, q)
        bend = 8192 + (q * 8191) // 127
        self.midi.send_message([0xE0 | self.ch, bend & 0x7F, (bend >> 7) & 0x7F])
        time.sleep(0.25)

    def program(self, n: int):
        self.midi.send_message([0xC0 | self.ch, n])
        time.sleep(1.0)

    def note(self, n: int, on: bool):
        self.midi.send_message([(0x90 if on else 0x80) | self.ch, n, self.velocity if on else 0])

    def record(self, seconds: float, path: Path, phrase, ride=None):
        frames = []
        t0 = time.monotonic()
        with self.mic.recorder(samplerate=self.rate, channels=1) as rec:
            for n, _ in phrase:
                self.note(n, True)
            while (t := time.monotonic() - t0) < seconds:
                frames.append(rec.record(numframes=int(self.rate * 0.05)))
                if ride is not None:
                    ride(min(t / seconds, 1.0))
            for n, _ in phrase:
                self.note(n, False)
            frames.append(rec.record(numframes=int(self.rate * 0.3)))
        x = np.concatenate(frames)[:, 0]
        peak = float(np.abs(x).max())
        if peak >= 1.0:
            print(f"  CLIP WARNING: {path.name} peak {peak:.2f} >= 0 dBFS - retake wanted")
        elif peak < 1e-4:
            print(f"  SILENT TAKE: {path.name} peak {peak:.6f} - is the X3 hearing MIDI?")
        else:
            print(f"  {path.name}: peak {20*np.log10(peak):+.1f} dBFS, {len(x)/self.rate:.1f} s")
        wavfile.write(str(path), int(self.rate), x.astype(np.float32))
        return peak

CORNERS = [("m0q0", 0, 0), ("m100q0", 127, 0), ("m0q100", 0, 127), ("m100q100", 127, 127)]

def capture_preset(rig: Rig, name: str, program: int | None, args):
    outdir = TAKES / name
    outdir.mkdir(parents=True, exist_ok=True)
    if program is not None:
        rig.program(program)
    phrase = [(n, args.hold) for n in args.notes]

    wavs = []
    for label, m, q in CORNERS:
        rig.park(m, q)
        p = outdir / f"{name}_{label}.wav"
        peak = rig.record(args.hold, p, phrase)
        if peak < 1e-4:
            print(f"  ABORT {name}: silent take - fix MIDI/audio, nothing fitted")
            return
        wavs.append(p)
        time.sleep(0.4)

    ride_wav = outdir / f"{name}_ride.wav"
    rig.park(0, 0)
    rig.record(args.ride_seconds, ride_wav, phrase,
               ride=lambda t01: rig.cc(rig.morph_cc, int(round(t01 * 127))))
    rig.cc(rig.morph_cc, 0)

    if args.no_fit:
        return
    py = sys.executable
    subprocess.run([py, str(ROOT / "tools" / "corners_from_wavs.py"), name,
                    *map(str, wavs), str(args.rate)], check=False)
    subprocess.run([py, str(ROOT / "tools" / "fit_field.py"), f"{name}_ride",
                    str(ride_wav), str(args.rate)], check=False)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("name", nargs="?", help="body name for a single capture")
    ap.add_argument("--batch", type=Path, help="file of 'program name' lines")
    ap.add_argument("--program", type=int, help="MIDI program number to select")
    ap.add_argument("--list", action="store_true", help="list MIDI ports + audio devices")
    ap.add_argument("--probe", action="store_true",
                    help="one M0Q0 take then one M100Q0 take - listen/diff to confirm the CC cords")
    ap.add_argument("--midi-port", default="loopMIDI")
    ap.add_argument("--device", default="MSI",
                    help="output device substring to record (default MSI - the unheard monitor)")
    ap.add_argument("--channel", type=int, default=1)
    ap.add_argument("--morph-cc", type=int, default=1)
    ap.add_argument("--q-cc", type=int, default=21)
    ap.add_argument("--notes", type=lambda s: [int(n) for n in s.split(",")],
                    default=[60], help="excitation notes, comma-separated (default 60)")
    ap.add_argument("--hold", type=float, default=8.0, help="seconds per parked take")
    ap.add_argument("--ride-seconds", type=float, default=16.0)
    ap.add_argument("--velocity", type=int, default=100)
    ap.add_argument("--rate", type=float, default=48_000.0)
    ap.add_argument("--no-fit", action="store_true", help="capture only, fit later")
    args = ap.parse_args()

    if args.list:
        print("MIDI out:", rtmidi.MidiOut().get_ports())
        print("Speakers:", [s.name for s in sc.all_speakers()])
        return

    rig = Rig(args)
    if args.probe:
        outdir = TAKES / "_probe"
        outdir.mkdir(parents=True, exist_ok=True)
        if args.program is not None:
            rig.program(args.program)
        phrase = [(n, args.hold) for n in args.notes]
        for label, m, q in (CORNERS[0], CORNERS[1]):
            rig.park(m, q)
            rig.record(4.0, outdir / f"probe_{label}.wav", phrase)
        print(f"Probe takes in {outdir} - if the two sound identical, the MORPH cord"
              f" is not CC{args.morph_cc}; try --morph-cc.")
        return

    jobs = []
    if args.batch:
        for line in args.batch.read_text().splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                prog, name = line.split(None, 1)
                jobs.append((name.strip(), int(prog)))
    elif args.name:
        jobs.append((args.name, args.program))
    else:
        ap.error("give a name, --batch, --probe, or --list")

    for name, prog in jobs:
        print(f"== {name} (program {prog}) ==")
        capture_preset(rig, name, prog, args)

if __name__ == "__main__":
    main()
