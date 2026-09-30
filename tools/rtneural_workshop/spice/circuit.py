import argparse
import json
from pathlib import Path
import time
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly
from PySpice.Spice.NgSpice.Shared import NgSpiceShared

HERE = Path(__file__).resolve().parent
DATA = HERE.parents[2] / 'output/rtneural-workshop/spice8bus'
FS = 48000


class CircuitSpice(NgSpiceShared):
    def send_stat(self, message, ngspice_id):
        now = time.time()
        if now - getattr(self, '_last_status', 0) >= 20:
            self._last_status = now
            print(json.dumps({'spice_status': message}), flush=True)
        return 0


def netlist(source, trim=100, parameters=''):
    return 'Mackie 8Bus line input to insert send - schematic derived candidate\n' + (HERE / 'mackie8bus.lib').read_text() + f'\n{source}\nRsourceP hot tip 50\nRsourceM cold ring 50\nXdesk tip ring out MACKIE8_LINE trim={trim} {parameters}\nRload out 0 10k\n.options method=gear reltol=1e-4 abstol=1e-12 vntol=1e-7\n.nodeset v(xdesk.ep)=.6 v(xdesk.em)=.6 v(xdesk.en)=-9 v(xdesk.epout)=-9 v(xdesk.np)=-8.3 v(xdesk.nm)=-8.3 v(xdesk.opout)=0\n'


def simulate(text, label):
    DATA.mkdir(parents=True, exist_ok=True)
    (DATA / f'{label}.cir').write_text(text)
    ng = CircuitSpice.new_instance()
    ng.destroy()
    ng.load_circuit(text)
    start = time.time()
    try:
        ng.run()
    except Exception:
        (DATA / f'{label}.log').write_text(ng.stdout + '\n' + ng.stderr)
        raise
    (DATA / f'{label}.log').write_text(ng.stdout + '\n' + ng.stderr)
    plot = ng.plot(None, ng.last_plot)
    values = {key: np.asarray(plot[key]._data).copy() for key in plot}
    print(json.dumps({'simulation': label, 'seconds': time.time()-start}), flush=True)
    return values


def probe():
    result = {}
    for trim in (5200, 100, 0):
        source = 'Vp hot 0 dc 0 ac .5\nVm cold 0 dc 0 ac -.5'
        op = simulate(netlist(source, trim) + '.op\n.end\n', f'op_trim{trim}')
        points = {key: float(value[0].real) for key, value in op.items() if key in ('xdesk.bp','xdesk.bm','xdesk.ep','xdesk.em','xdesk.en','xdesk.epout','xdesk.opout','out')}
        ac = simulate(netlist(source, trim) + '.ac dec 60 10 100000\n.end\n', f'ac_trim{trim}')
        freq = ac['frequency'].real
        response = ac['out']
        result[str(trim)] = {'operating_point_volts': points, 'gain_db': {str(hz): float(np.interp(np.log(hz), np.log(freq), 20*np.log10(abs(response)))) for hz in (20, 50, 1000, 10000, 20000)}}
        np.savez(DATA / f'ac_trim{trim}.npz', frequency=freq, response=response)
    (DATA / 'probe.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


def render(x, volts_per_unit, trim, label, oversample=4, parameters='', save_nodes=False):
    DATA.mkdir(parents=True, exist_ok=True)
    pad = FS//2
    padded = np.concatenate((np.zeros(pad), x, np.zeros(FS//20)))
    times = np.arange(len(padded))/FS
    infile = DATA / f'{label}_source.txt'
    np.savetxt(infile, np.column_stack((times, padded)), fmt='%.12g')
    source = f'Asrc [%vd(signal 0)] filesrc\n.model filesrc filesource (file="{infile.as_posix()}" amploffset=[0] amplscale=[{volts_per_unit}] timeoffset=0 timescale=1 timerelative=false amplstep=false)\nEp hot 0 signal 0 .5\nEm cold 0 signal 0 -.5'
    step = 1 / (FS*oversample)
    nodes = 'v(out) v(xdesk.opout) v(xdesk.bp) v(xdesk.bm) v(xdesk.en) v(xdesk.epout)' if save_nodes else 'v(out)'
    text = netlist(source, trim, parameters) + f'.save {nodes}\n.tran {step:.12g} {times[-1]:.12g} 0 {step:.12g}\n.end\n'
    values = simulate(text, label)
    timepoints, volts = values['time'].real, values['out'].real
    if len(timepoints) < 2 or timepoints[-1] < times[-1] - step or not np.isfinite(volts).all():
        raise RuntimeError('Incomplete or nonfinite SPICE render')
    uniform = np.arange((len(padded)-1)*oversample+1)/(FS*oversample)
    high_rate = np.interp(uniform, timepoints, volts)
    y = resample_poly(high_rate, 1, oversample)[pad:pad+len(x)]
    sf.write(str(DATA / f'{label}_volts.wav'), y, FS, subtype='FLOAT')
    if save_nodes:
        np.savez(DATA / f'{label}_nodes.npz', **values)
    return y


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', action='store_true')
    args = parser.parse_args()
    if args.probe:
        probe()
