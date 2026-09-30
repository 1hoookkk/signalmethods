import argparse
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf


def plot(job):
    report = json.loads((job / 'clipping_profile.json').read_text())
    audio, fs = sf.read(str(job / 'spice/clipping_profile_volts.wav'))
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10})
    fig, axes = plt.subplots(2, 2, figsize=(11, 7.5), constrained_layout=True)
    for hz, color in [(50, '#246969'), (1000, '#a25324'), (10000, '#64599c')]:
        rows = [row for row in report['rows'] if row['frequency_hz'] == hz]
        x = [row['input_peak_volts'] for row in rows]
        axes[0, 0].semilogx(x, [row['fundamental_gain_db'] for row in rows], 'o-', color=color, label=f'{hz:,} Hz')
        axes[0, 1].semilogx(x, [row['harmonic_ratio_db'] for row in rows], 'o-', color=color)
        axes[1, 0].semilogx(x, [row['positive_peak_volts'] for row in rows], 'o-', color=color)
        axes[1, 0].semilogx(x, [-row['negative_peak_volts'] for row in rows], 'x--', color=color)
    axes[0, 0].set(title='Fundamental gain compresses with drive', ylabel='Gain (dB)')
    axes[0, 0].legend()
    axes[0, 1].set(title='Harmonics rise with clipping', ylabel='Harmonics / fundamental (dB)')
    axes[1, 0].set(title='48 kHz peaks: positive (solid), negative (dashed)', ylabel='Output peak (V equivalent)')
    for axis in [axes[0, 0], axes[0, 1], axes[1, 0]]:
        axis.set_xlabel('Differential input peak (V)')
        axis.grid(alpha=0.25)
    for index, color in [(4, '#246969'), (6, '#a25324'), (7, '#64599c')]:
        start = index * fs + int(0.4 * fs)
        y = audio[start:start+int(0.003 * fs)]
        axes[1, 1].plot(np.arange(len(y))/fs*1000, y, color=color, label=f"{report['rows'][index]['input_peak_volts']:g} V input")
    axes[1, 1].set(title='1 kHz waveform at increasing drive', xlabel='Time (ms)', ylabel='Output (V)', ylim=(-17, 17))
    axes[1, 1].grid(alpha=0.25)
    axes[1, 1].legend()
    fig.suptitle('Mackie 8-Bus schematic simulation — clipping profile\nMaximum trim, ±16 V rails, 10 kΩ load; semiconductor estimates, hardware fidelity unverified', fontsize=13)
    fig.supxlabel('Harmonics: orders 2–10 below Nyquist. Bandlimiting can raise sampled peaks above the raw analog peaks.\nThese checks establish simulated behavior, not a hardware match.', fontsize=9)
    fig.savefig(job / 'clipping_profile.png', dpi=150)
    plt.close(fig)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('job')
    plot(Path(parser.parse_args().job))
