import argparse
import json
from pathlib import Path
import sys
import numpy as np
import soundfile as sf
from circuit import DATA, FS, render
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from worker import filtered, metrics, stimulus, sha

SOURCE = DATA.parents[1] / 'tube-rtneural-20260929/holdout_input.wav'


def bass(volts):
    x, rate = sf.read(str(SOURCE))
    assert rate == FS
    label = 'bass_' + str(volts).replace('.', 'p') + 'V'
    y = render(x, volts, 0, label) / 16
    alone = y * 10**(-6/20)
    after_filter = filtered(DATA, y.astype(np.float32), label+'_filter') * 10**(-30/20)
    sf.write(str(DATA / f'{label}_stage_only.wav'), alone, FS, subtype='FLOAT')
    sf.write(str(DATA / f'{label}_before_filter.wav'), after_filter, FS, subtype='FLOAT')
    report = {'reference': 'Schematic-derived SPICE candidate; not a hardware recording', 'input_volts_per_digital_unit': volts, 'output_volts_per_digital_unit': 16, 'trim_ohms': 0, 'supply_volts': [-16,16], 'external_load_ohms': 10000, 'input_source_ohms_per_leg': 50, 'stage_only_manual_output_db': -6, 'filtered_manual_output_db': -30, 'normalization': False, 'stage_only': metrics(alone), 'before_filter': metrics(after_filter), 'source_sha256': sha(SOURCE), 'circuit_sha256': sha(Path(__file__).with_name('mackie8bus.lib'))}
    (DATA/f'{label}_report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report), flush=True)


def training():
    bass_audio, rate = sf.read(str(SOURCE))
    assert rate == FS
    x = np.concatenate((stimulus(28, 877), stimulus(6, 339), bass_audio[:FS*6])).astype(np.float32)
    y = render(x, 4, 0, 'training40') / 16
    sf.write(str(DATA/'training_input.wav'), x, FS, subtype='FLOAT')
    sf.write(str(DATA/'training_target.wav'), y, FS, subtype='FLOAT')
    (DATA/'training_provenance.json').write_text(json.dumps({'type': 'SPICE simulation', 'input_volts_per_unit': 4, 'output_volts_per_unit': 16, 'trim_ohms': 0, 'train_seconds': 28, 'validation_seconds': 6, 'held_out_bass_seconds': 6, 'normalization': False, 'circuit_sha256': sha(Path(__file__).with_name('mackie8bus.lib')), 'input_sha256': sha(DATA/'training_input.wav'), 'target_sha256': sha(DATA/'training_target.wav')}, indent=2))


def after_filter():
    x, rate = sf.read(str(SOURCE))
    assert rate == FS
    x = filtered(DATA, x.astype(np.float32), 'bass_filter_first')
    y = render(x, 4, 0, 'bass_4p0V_after_filter') / 16 * 10**(-30/20)
    sf.write(str(DATA/'bass_4p0V_after_filter.wav'), y, FS, subtype='FLOAT')
    before, _ = sf.read(str(DATA/'bass_4p0V_before_filter.wav'))
    sf.write(str(DATA/'8Bus_SPICE_before_then_after_filter.wav'), np.concatenate((before, np.zeros(FS), y)), FS, subtype='FLOAT')
    print(json.dumps({'placement': 'filter then circuit', 'input_volts_per_unit': 4, 'output_volts_per_unit': 16, 'manual_output_db': -30, 'audio': metrics(y)}), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--volts', type=float)
    parser.add_argument('--training', action='store_true')
    parser.add_argument('--after-filter', action='store_true')
    args = parser.parse_args()
    if args.training:
        training()
    elif args.after_filter:
        after_filter()
    else:
        bass(args.volts)
