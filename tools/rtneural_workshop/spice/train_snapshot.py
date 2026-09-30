import argparse
import json
from pathlib import Path
import sys
import time
import numpy as np
import soundfile as sf
import circuit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from worker import DATA, FS, main, metrics, sha, stimulus


def bass_stimulus(seconds, seed, levels=None):
    rng = np.random.default_rng(seed)
    x = np.zeros(seconds * FS, dtype=np.float64)
    levels = np.asarray(levels if levels is not None else [0.001, 0.003, 0.01, 0.03, 0.1, 0.3, 0.8])
    for index, start in enumerate(range(0, len(x), FS // 2)):
        n = min(FS // 2, len(x) - start)
        t = np.arange(n) / FS
        f = rng.choice([32.7, 43.65, 55, 73.42, 98, 130.81])
        phase = 2 * np.pi * f * t
        tone = np.sin(phase) + 0.3 * np.sin(phase * 2.003) + 0.12 * np.sin(phase * 5.01)
        env = np.minimum(1, t / 0.003) * np.exp(-t * rng.uniform(1, 6))
        x[start:start+n] = levels[index % len(levels)] * tone * env
    return x.astype(np.float32)


def rail_probe(folder):
    t = np.arange(FS//4) / FS
    y = circuit.render(np.sin(2*np.pi*10000*t), 1, 0, 'rail_probe_10k', save_nodes=True)
    nodes = np.load(circuit.DATA / 'rail_probe_10k_nodes.npz')
    result = {key: {'min': float(nodes[key].min()), 'max': float(nodes[key].max())} for key in ('xdesk.opout', 'out')}
    result['converted_48k_output'] = {'min': float(y.min()), 'max': float(y.max())}
    result['rail_check_location'] = 'Raw SPICE op-amp output, before coupling and antialias decimation'
    result['circuit_sha256'] = sha(circuit.HERE / 'mackie8bus.lib')
    (folder / 'rail_probe.json').write_text(json.dumps(result, indent=2))
    return result


def clipping_profile(folder):
    tones = []
    cases = []
    duration = 0.6
    t = np.arange(int(duration * FS)) / FS
    for hz in (50, 1000, 10000):
        for peak in (0.01, 0.05, 0.2, 1.0):
            cases.append((hz, peak))
            tones.append(peak * np.sin(2 * np.pi * hz * t))
            tones.append(np.zeros(int(0.4 * FS)))
    dry_volts = np.concatenate(tones)
    wet_volts = circuit.render(dry_volts, 1, 0, 'clipping_profile')
    rows = []
    for index, (hz, peak) in enumerate(cases):
        start = index * FS + int(0.3 * FS)
        y = wet_volts[start:start + int(0.3 * FS)]
        spectrum = np.fft.rfft(y) / len(y) * 2
        bins = np.fft.rfftfreq(len(y), 1 / FS)
        fundamental = abs(spectrum[np.argmin(abs(bins - hz))])
        harmonics = [abs(spectrum[np.argmin(abs(bins - h * hz))]) for h in range(2, min(10, int((FS / 2 - 1) / hz)) + 1)]
        rows.append({'frequency_hz': hz, 'input_peak_volts': peak, 'positive_peak_volts': float(y.max()), 'negative_peak_volts': float(y.min()), 'dc_volts': float(y.mean()), 'fundamental_gain_db': float(20 * np.log10(max(fundamental / peak, 1e-12))), 'harmonic_ratio_db': float(20 * np.log10(max(np.linalg.norm(harmonics) / max(fundamental, 1e-12), 1e-12)))})
    at_1k = [row for row in rows if row['frequency_hz'] == 1000]
    rails = rail_probe(folder)
    checks = {'finite': bool(np.isfinite(wet_volts).all()), 'raw_opamp_within_supply_rails_on_10k_overload_probe': rails['xdesk.opout']['min'] > -16 and rails['xdesk.opout']['max'] < 16, 'small_signal_gain_40_to_43_db': 40 < at_1k[0]['fundamental_gain_db'] < 43, 'overload_compresses_gain_by_more_than_10_db': at_1k[0]['fundamental_gain_db'] - at_1k[-1]['fundamental_gain_db'] > 10, 'positive_and_negative_clipping_beyond_14_volts': at_1k[-1]['positive_peak_volts'] > 14 and at_1k[-1]['negative_peak_volts'] < -14}
    result = {'reference': '8Bus schematic simulation; hardware accuracy unverified', 'rows': rows, 'checks': checks, 'passed': all(checks.values()), 'circuit_sha256': sha(circuit.HERE / 'mackie8bus.lib'), 'rail_check': rails, 'converted_peak_note': 'Antialias bandlimiting changes waveform peak height; rail limits apply to the raw analog node, not the decimated signal.'}
    (folder / 'clipping_profile.json').write_text(json.dumps(result, indent=2))
    print(json.dumps({'clipping_checks': checks}), flush=True)
    if not result['passed']:
        raise RuntimeError('Circuit clipping-profile check failed')


def run(job, steps):
    job.mkdir(parents=True, exist_ok=False)
    circuit.DATA = job / 'spice'
    circuit.DATA.mkdir()
    (job / 'mackie8bus.lib').write_bytes((circuit.HERE / 'mackie8bus.lib').read_bytes())
    source = DATA / 'sources/bass_phrase.wav'
    bass, rate = sf.read(str(source), dtype='float32')
    if rate != FS or bass.ndim != 1 or len(bass) < 3 * FS:
        raise ValueError('Held-out source must contain at least three seconds of mono 48 kHz audio')
    x = np.concatenate((stimulus(3, 1771) * 0.25, bass_stimulus(11, 9401), bass_stimulus(3, 811, [0.001, 0.008, 0.03, 0.1, 0.3, 0.8]), bass[:3 * FS])).astype(np.float32)
    sf.write(str(job / 'input.wav'), x, FS, subtype='FLOAT')
    print(json.dumps({'event': 'render', 'samples': len(x), 'seconds': len(x) / FS}), flush=True)
    y = (circuit.render(x, 4, 0, 'training20') / 16).astype(np.float32)
    sf.write(str(job / 'target.wav'), y, FS, subtype='FLOAT')
    for name, expected in [('input.wav', x), ('target.wav', y)]:
        actual, rate = sf.read(str(job / name), dtype='float32')
        if rate != FS or not np.array_equal(expected, actual) or not np.isfinite(actual).all():
            raise RuntimeError('Pair read-back verification failed')
    provenance = {'type': 'SPICE simulation, not physical hardware', 'topology': 'Mackie analog 8Bus balanced line input to insert send', 'estimated_semiconductors': ['2SA1084', 'NJM4560', '1N4148'], 'input_volts_per_unit': 4, 'output_volts_per_unit': 16, 'trim_ohms': 0, 'supply_volts': [-16, 16], 'sample_rate': FS, 'train_seconds': 14, 'validation_seconds': 3, 'holdout_seconds': 3, 'input_sha256': sha(job / 'input.wav'), 'target_sha256': sha(job / 'target.wav'), 'circuit_sha256': sha(job / 'mackie8bus.lib'), 'normalization': False, 'automatic_compensation': False}
    (job / 'target_provenance.json').write_text(json.dumps(provenance, indent=2))
    clipping_profile(job)
    finish(job, steps)


def finish(job, steps):
    provenance = json.loads((job / 'target_provenance.json').read_text())
    profile = json.loads((job / 'clipping_profile.json').read_text())
    if not profile['passed'] or profile['circuit_sha256'] != provenance['circuit_sha256']:
        raise ValueError('A passed clipping profile from the same circuit is required')
    for filename, key in [('input.wav', 'input_sha256'), ('target.wav', 'target_sha256'), ('mackie8bus.lib', 'circuit_sha256')]:
        if sha(job / filename) != provenance[key]:
            raise ValueError('Generated target provenance does not match the files')
    source = DATA / 'sources/bass_phrase.wav'
    previous = DATA / 'runs/20260929-222453-0eff3e10'
    prior_config = json.loads((previous / 'config.json').read_text())
    if prior_config['hidden'] != 32:
        raise ValueError('Starting model must be the 32-unit 8Bus snapshot')
    config = {'drive_db': 0, 'output_db': -6, 'steps': steps, 'hidden': 32, 'input_scale': 16, 'initial_checkpoint': str(previous / 'best.pt'), 'action': 'train', 'placement': 'before', 'filter': False, 'source': str(source), 'source_name': 'Held-out bass phrase', 'created_at': time.strftime('%Y-%m-%d %H:%M:%S'), 'reference': 'pair', 'delay_samples': 0, 'pair_input': str(job / 'input.wav'), 'pair_target': str(job / 'target.wav'), 'pair_description': 'Mackie analog 8Bus schematic simulation: line input to insert send, maximum trim, +/-16 V rails, 4 V/input unit, 16 V/output unit. Estimated semiconductor behavior; hardware fidelity unverified.'}
    (job / 'config.json').write_text(json.dumps(config, indent=2))
    main(job)
    reference, rate = sf.read(str(job / 'reference.wav'), dtype='float32')
    model, model_rate = sf.read(str(job / 'model.wav'), dtype='float32')
    if rate != model_rate or reference.shape != model.shape:
        raise RuntimeError('Audition pair dimensions disagree')
    audition = np.concatenate((reference, np.zeros(FS, np.float32), model))
    sf.write(str(job / '8Bus_SPICE_then_RTNeural.wav'), audition, FS, subtype='FLOAT')
    (job / 'audition.json').write_text(json.dumps({'order': ['SPICE', 'RTNeural'], 'start_seconds': [0, 4], 'manual_output_db': -6, 'normalization': False, 'metrics': metrics(audition)}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('job')
    parser.add_argument('--steps', type=int, default=3000)
    parser.add_argument('--train-existing', action='store_true')
    args = parser.parse_args()
    (finish if args.train_existing else run)(Path(args.job).resolve(), args.steps)
