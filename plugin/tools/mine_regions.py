from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
PRAAT = Path(r"C:\Users\hooki\tools\praat\Praat.exe")

MIN_POSE_S = 0.12
POSE_CUT_S = 0.22
MIN_GLIDE_S = 0.25
GLIDE_TRAVEL_HZ = 350.0
STILL_SPEED = 900.0
MAX_POSES, MAX_GLIDES = 6, 4

PRAAT_SCRIPT = '''form Mine
  sentence wav
  sentence out
endform
Read from file: wav$
sound = Convert to mono
dur = Get total duration
select sound
pitch = To Pitch: 0, 60, 600
select sound
intensity = To Intensity: 60, 0, "yes"
select sound
formant = To Formant (burg): 0, 5, 5500, 0.025, 50
writeFileLine: out$, "t{TAB}int{TAB}f0{TAB}f1{TAB}f2{TAB}f3"
t = 0.02
while t < dur - 0.02
  selectObject: intensity
  i = Get value at time: t, "cubic"
  selectObject: pitch
  f0 = Get value at time: t, "Hertz", "linear"
  selectObject: formant
  f1 = Get value at time: 1, t, "hertz", "linear"
  f2 = Get value at time: 2, t, "hertz", "linear"
  f3 = Get value at time: 3, t, "hertz", "linear"
  appendFileLine: out$, fixed$(t, 3), tab$, fixed$(i, 2), tab$, fixed$(f0, 1), tab$, fixed$(f1, 1), tab$, fixed$(f2, 1), tab$, fixed$(f3, 1)
  t += 0.01
endwhile
'''.replace("{TAB}", "\\t")

def run_praat(wav: Path) -> np.ndarray:
    with tempfile.TemporaryDirectory() as td:
        script = Path(td) / "mine.praat"
        table = Path(td) / "mine.tsv"
        script.write_text(PRAAT_SCRIPT, encoding="utf-8")
        subprocess.run([str(PRAAT), "--run", str(script),
                        str(wav), str(table)],
                       check=True, capture_output=True, timeout=300)
        rows = []
        for line in table.read_text().splitlines()[1:]:
            vals = [np.nan if "undefined" in v else float(v)
                    for v in line.split("\t")]
            if len(vals) == 6:
                rows.append(vals)
    return np.array(rows)

def smooth(x: np.ndarray, n: int) -> np.ndarray:
    k = np.hanning(n); k /= k.sum()
    pad = np.concatenate([x[:n][::-1], x, x[-n:][::-1]])
    return np.convolve(pad, k, mode="same")[n:-n]

def track_smooth(x: np.ndarray, n: int = 11) -> np.ndarray:
    idx = np.arange(len(x))
    ok = np.isfinite(x)
    if ok.sum() < 2:
        return np.full_like(x, np.nan)
    return smooth(np.interp(idx, idx[ok], x[ok]), n)

def segments_of(mask: np.ndarray, t: np.ndarray, bridge_s=0.06):
    idx = np.where(mask)[0]
    if not len(idx):
        return []
    segs, start, prev = [], idx[0], idx[0]
    dt = float(np.median(np.diff(t)))
    for i in idx[1:]:
        if (i - prev) * dt > bridge_s:
            segs.append((start, prev))
            start = i
        prev = i
    segs.append((start, prev))
    return segs

def read_textgrid(path: Path):
    marks, tier = [], ""
    xmin = xmax = None
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if line.startswith("name = "):
            tier = line.split('"')[1]
        elif line.startswith("xmin = "):
            xmin = float(line.split("=")[1])
        elif line.startswith("xmax = "):
            xmax = float(line.split("=")[1])
        elif line.startswith("text = "):
            label = line.split('"')[1] if '"' in line else ""
            if label.strip() and xmin is not None and xmax is not None and xmax > xmin:
                marks.append((tier, label.strip(), xmin, xmax))
    return marks

def main():
    wav = Path(sys.argv[1]).resolve()
    stem = wav.stem.lower().replace(" ", "_")
    outdir = ROOT / "mined" / stem
    outdir.mkdir(parents=True, exist_ok=True)

    data = run_praat(wav)
    t, inten, f0, f1, f2, f3 = data.T
    voiced = np.isfinite(f0) & np.isfinite(f1) & np.isfinite(f2) \
        & (inten > np.nanmax(inten) - 30.0)

    dt = float(np.median(np.diff(t)))
    f1s, f2s = track_smooth(f1), track_smooth(f2)
    speed = np.hypot(np.gradient(f1s, dt), np.gradient(f2s, dt))
    speed = np.where(np.isfinite(speed), speed, STILL_SPEED * 4)

    poses, glides = [], []
    for a, b in segments_of(voiced, t):
        if (t[b] - t[a]) < MIN_POSE_S:
            continue
        for sa, sb in segments_of(speed[a:b + 1] < STILL_SPEED, t[a:b + 1]):
            sa, sb = sa + a, sb + a
            if (t[sb] - t[sa]) < MIN_POSE_S:
                continue
            w = max(1, int(POSE_CUT_S / dt))
            if sb - sa <= w:
                c = (sa + sb) // 2
            else:
                means = smooth(speed[sa:sb + 1], 3)
                c = sa + int(np.argmin([means[i:i + w].mean()
                                        for i in range(0, sb - sa - w + 1)])) + w // 2
            t0, t1 = max(t[sa], t[c] - POSE_CUT_S / 2), min(t[sb], t[c] + POSE_CUT_S / 2)
            score = float(np.nanmean(inten[sa:sb])) + 10.0 * (t[sb] - t[sa])
            poses.append((score, t0, t1, float(np.nanmedian(f1[sa:sb])),
                          float(np.nanmedian(f2[sa:sb]))))
        travel = (np.nanmax(f1s[a:b + 1]) - np.nanmin(f1s[a:b + 1])) \
            + (np.nanmax(f2s[a:b + 1]) - np.nanmin(f2s[a:b + 1]))
        if (t[b] - t[a]) >= MIN_GLIDE_S and travel >= GLIDE_TRAVEL_HZ:
            glides.append((travel, float(t[a]), float(t[b])))

    poses = sorted(poses, reverse=True)[:MAX_POSES]
    glides = sorted(glides, reverse=True)[:MAX_GLIDES]

    marked = []
    tg = Path(sys.argv[2]) if len(sys.argv) > 2 else wav.with_suffix(".TextGrid")
    if tg.exists():
        for tier, label, t0, t1 in read_textgrid(tg):
            kind = "glide" if "glide" in (tier + label).lower() else "pose"
            marked.append((kind, label, t0, t1))
        print(f"TextGrid: {len(marked)} marked interval(s) from {tg.name} - yours outrank the detector")

    from scipy.io import wavfile
    sr, x = wavfile.read(wav)
    chosen, candidates = [], []
    safe = "abcdefghijklmnopqrstuvwxyz0123456789_"
    for kind, label, t0, t1 in marked:
        tag = "".join(c for c in label.lower().replace(" ", "_") if c in safe) or "mark"
        f = outdir / f"{stem}_{tag}_{t0:.2f}-{t1:.2f}s.wav"
        wavfile.write(f, sr, x[int(t0 * sr):int(t1 * sr)])
        chosen.append((label, t0, t1, f))
    n = 0
    for item in sorted(poses + [(g[0], g[1], g[2]) for g in glides], key=lambda i: i[1]):
        t0, t1 = item[1], item[2]
        if any(not (t1 < m[2] or t0 > m[3]) for m in marked):
            continue
        n += 1
        f = outdir / f"{stem}_cut{n}_{t0:.2f}-{t1:.2f}s.wav"
        wavfile.write(f, sr, x[int(t0 * sr):int(t1 * sr)])
        candidates.append((t0, t1, f))
    cuts = [("choice", lbl, t0, t1, f) for lbl, t0, t1, f in chosen] \
        + [("candidate", "", t0, t1, f) for t0, t1, f in candidates]

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    xm = x if x.ndim == 1 else x.mean(axis=1)
    xm = xm.astype(float) / (np.abs(xm).max() + 1e-9)
    fig, (ax0, ax1) = plt.subplots(2, 1, figsize=(16, 8), sharex=True,
                                   gridspec_kw={"height_ratios": [1, 2.4]})
    ax0.plot(np.arange(len(xm)) / sr, xm, color="0.35", lw=0.4)
    ax0.set_ylim(-1.05, 1.05); ax0.set_yticks([])
    ax1.specgram(xm, NFFT=1024, Fs=sr, noverlap=768, cmap="Greys",
                 vmin=-120)
    for track, shade in ((f1, "#c0392b"), (f2, "#e67e22"), (f3, "#7f8c8d")):
        ax1.plot(t, track, ".", ms=2.5, color=shade)
    for kind, label, t0, t1, f in cuts:
        if kind == "choice":
            for ax in (ax0, ax1):
                ax.axvspan(t0, t1, color="#b8860b", alpha=0.20)
            ax1.text((t0 + t1) / 2, 5100, label, ha="center",
                     fontsize=10, color="#b8860b", weight="bold")
        else:
            ax1.plot([t0, t1], [90, 90], color="#222222", lw=3,
                     solid_capstyle="butt")
            for edge in (t0, t1):
                ax1.plot([edge, edge], [90, 260], color="#222222", lw=1.2)
    ax1.set_ylim(0, 5500); ax1.set_xlabel("seconds")
    ax1.set_ylabel("Hz  (dots = formant paths)")
    fig.suptitle(f"{wav.name} - gold = your marks, gray brackets = unnamed "
                 f"candidate cuts. All cuts in mined/{stem}/", fontsize=12)
    fig.tight_layout()
    plate = outdir / f"{stem}_mining.png"
    fig.savefig(plate, dpi=110)

    lines = [f"{wav.name}: {len(chosen)} chosen by you, "
             f"{len(candidates)} unnamed candidate cut(s)", ""]
    for lbl, t0, t1, f in chosen:
        lines.append(f"  {lbl:<12} {t0:6.2f}-{t1:5.2f}s   {f.name}")
    for t0, t1, f in candidates:
        lines.append(f"  {'-':<12} {t0:6.2f}-{t1:5.2f}s   {f.name}")
    lines += ["", "  every cut is a WAV: audition it, then Ctrl+drop (a held pose)",
              "  or Ctrl+Shift+drop (a moving gesture) on the Workstation.",
              "  You decide which is which - the cut is just the region."]
    queue = "\n".join(lines)
    (outdir / "queue.txt").write_text(queue, encoding="utf-8")
    print(queue)
    print(f"\nplate: {plate}")
    os.startfile(plate)

main()
