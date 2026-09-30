import json
from pathlib import Path
import time
import numpy as np
import soundfile as sf
from worker import DATA, FS, main, native, stimulus


def verify():
    root = DATA / 'verification' / time.strftime('%Y%m%d-%H%M%S')
    root.mkdir(parents=True)
    x = stimulus(14, 667)
    y = native(root, x, 'target', 32, 'synthetic_fixture')
    sf.write(str(root / 'synthetic_input.wav'), x, FS, subtype='FLOAT')
    sf.write(str(root / 'synthetic_target.wav'), np.pad(y, (31, 0)), FS, subtype='FLOAT')
    config = {'reference': 'pair', 'pair_input': str(root / 'synthetic_input.wav'), 'pair_target': str(root / 'synthetic_target.wav'), 'delay_samples': 31, 'pair_description': 'SYNTHETIC verification fixture: native Mackity 1202 algorithm, not hardware', 'source': str(root / 'synthetic_input.wav'), 'source_name': 'Synthetic fixture', 'drive_db': 32, 'output_db': -24, 'filter': True, 'placement': 'before', 'hidden': 8, 'steps': 10, 'action': 'train', 'created_at': time.strftime('%Y-%m-%d %H:%M:%S')}
    (root / 'config.json').write_text(json.dumps(config, indent=2))
    main(root)
    result = json.loads((root / 'result.json').read_text())
    report = result['report']
    assert abs(report['validation_objective'] - min(row['validation_objective'] for row in report['history'])) < 1e-6
    assert np.isclose(report['validation_objective'], report['validation_mr_spectral'] + report['validation_lf_esr'], rtol=1e-6, atol=1e-6)
    assert report['export_parity_rmse'] < 1e-4
    assert report['reference_type'] == 'pair'
    assert set(result['pair_sha256']) == {'pair_input', 'pair_target'}
    assert report['source_used_for_training'] is False
    assert report['objective']['full_band_sample_error_in_objective'] is False
    for name in ('baseline', 'reference', 'model'):
        audio, rate = sf.read(str(root / f'{name}.wav'))
        assert rate == FS and len(audio) == int(FS*14) - int(FS*14*0.85)
        assert np.isfinite(audio).all()
    print(json.dumps({'verification': 'passed', 'directory': str(root), 'parity_rmse': report['export_parity_rmse']}))


if __name__ == '__main__':
    verify()
