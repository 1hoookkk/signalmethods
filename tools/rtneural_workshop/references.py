import json
from pathlib import Path
import numpy as np
import soundfile as sf


def convert_model(value, sample_rate):
    if not isinstance(value, dict):
        raise ValueError('Model JSON must be an object')
    value = json.loads(json.dumps(value))
    residual = bool(value.get('workshop_runtime', {}).get('residual_input', False))
    kind = 'RTNeural JSON'
    if 'state_dict' in value:
        kind = 'Proteus snapshot'
        if value.get('model_data', {}).get('input_size') != 1:
            raise ValueError('Import a single-input Proteus snapshot; conditioned knob models are not supported yet')
        state = value['state_dict']
        ih = np.asarray(state['rec.weight_ih_l0'], dtype=float)
        hh = np.asarray(state['rec.weight_hh_l0'], dtype=float)
        bi = np.asarray(state['rec.bias_ih_l0'], dtype=float)
        bh = np.asarray(state['rec.bias_hh_l0'], dtype=float)
        dw = np.asarray(state['lin.weight'], dtype=float)
        db = np.asarray(state['lin.bias'], dtype=float)
        hidden = hh.shape[1]
        if ih.shape != (hidden*4, 1) or hh.shape != (hidden*4, hidden) or bi.shape != (hidden*4,) or bh.shape != bi.shape or dw.shape != (1, hidden) or db.shape != (1,):
            raise ValueError('Inconsistent Proteus weight dimensions')
        value = {'in_shape': [None, None, 1], 'layers': [{'type': 'lstm', 'activation': '', 'shape': [None, None, hidden], 'weights': [ih.T.tolist(), hh.T.tolist(), (bi+bh).tolist()]}, {'type': 'dense', 'activation': '', 'shape': [None, None, 1], 'weights': [dw.T.tolist(), db.tolist()]}]}
        residual = True
    if sample_rate not in (44100, 48000, 88200, 96000):
        raise ValueError('Choose the original model sample rate')
    layers = value.get('layers', [])
    if value.get('in_shape', [None])[-1] != 1 or len(layers) != 2 or [v.get('type') for v in layers] != ['lstm', 'dense']:
        raise ValueError('Supported model: mono LSTM followed by one dense output')
    if layers[0].get('activation', '') not in ('', 'tanh') or layers[1].get('activation', '') not in ('', 'linear'):
        raise ValueError('Only standard LSTM gates and a linear dense output are supported')
    hidden = layers[0]['shape'][-1]
    if not isinstance(hidden, int) or not 1 <= hidden <= 128 or layers[1]['shape'][-1] != 1:
        raise ValueError('Unsupported model dimensions')
    expected = [(1, hidden*4), (hidden, hidden*4), (hidden*4,), (hidden, 1), (1,)]
    weights = layers[0]['weights'] + layers[1]['weights']
    if len(weights) != 5:
        raise ValueError('Missing layer weights')
    for weight, shape in zip(weights, expected):
        a = np.asarray(weight)
        if a.shape != shape or not np.isfinite(a).all():
            raise ValueError('Invalid or nonfinite model weights')
    value['workshop_runtime'] = {'sample_rate': sample_rate, 'residual_input': residual}
    return value, {'format': kind, 'sample_rate': sample_rate, 'residual_input': residual, 'hidden_units': hidden, 'provenance': 'User-supplied capture; hardware identity and fidelity not independently verified'}


def align_pair(x, y, delay):
    start_x, start_y = max(-delay, 0), max(delay, 0)
    size = min(len(x)-start_x, len(y)-start_y)
    if size < 48000*12:
        raise ValueError('Need at least 12 seconds of overlapping mono audio after alignment')
    return x[start_x:start_x+size], y[start_y:start_y+size]


def load_pair(config):
    data = []
    for key in ('pair_input', 'pair_target'):
        x, fs = sf.read(str(config[key]))
        if fs != 48000 or x.ndim != 1 or not np.isfinite(x).all():
            raise ValueError('Training pairs must be mono, finite, and 48 kHz')
        data.append(x.astype(np.float32))
    return align_pair(*data, int(config['delay_samples']))


def split_pair(x, y):
    train_end, validation_end = len(x)*7//10, len(x)*17//20
    return x[:train_end], y[:train_end], x[train_end:validation_end], y[train_end:validation_end], x[validation_end:], y[validation_end:]
