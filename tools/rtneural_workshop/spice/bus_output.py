import argparse
import hashlib
import json
from pathlib import Path
import re
import time
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly
import circuit

HERE = Path(__file__).resolve().parent
FS = circuit.FS


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def library(rail_smoothing=0):
    result='\n'.join((HERE / name).read_text() for name in ('mackie8bus.lib', 'bus_output.lib'))
    if rail_smoothing:
        if not 0 < rail_smoothing <= .01:
            raise ValueError('Rail regularization must be at most 10 mV')
        e=f'{rail_smoothing:.17g}'
        result=f'.func railpos(x) {{x>{e} ? x : (x<(-{e}) ? 0 : ((x+{e})*(x+{e})/(4*{e})))}}\n'+result
        for headroom in ('headroom','margin'):
            high=f'(v(vp)-{headroom})'
            low=f'(v(vn)+{headroom})'
            result=result.replace(f'max(v(dominant)-{high},0)',f'railpos(v(dominant)-{high})')
            result=result.replace(f'min(v(dominant)-{low},0)',f'(-railpos({low}-v(dominant)))')
            result=result.replace(f'min(max(v(dominant),v(vn)+{headroom}),v(vp)-{headroom})',f'({low}+railpos(v(dominant)-{low})-railpos(v(dominant)-{high}))')
    return result


def deck(source, route='full', parameters='', rail_smoothing=0):
    if route == 'full':
        path = f'RsourceP hot tip 50\nRsourceM cold ring 50\nXdesk tip ring pin2 pin3 unbalanced MACKIE8_LINE_MAIN {parameters}\n'
        nodeset = '.nodeset v(xdesk.xpre.ep)=.6 v(xdesk.xpre.em)=.6 v(xdesk.xpre.en)=-9 v(xdesk.xpre.epout)=-9 v(xdesk.xpre.np)=-8.3 v(xdesk.xpre.nm)=-8.3 v(xdesk.xpre.opout)=0 v(xdesk.xchannel.fader_out)=0 v(xdesk.xchannel.pan_amp)=0 v(xdesk.xchannel.amp_in)=0 v(xdesk.xchannel.fader_fb)=0 v(xdesk.xchannel.pan_in)=0\n'
    elif route == 'main':
        path = f'R242 signal sum 5.1k\nXdesk sum pin2 pin3 unbalanced MACKIE8_MAIN {parameters}\n'
        nodeset = ''
    else:
        raise ValueError('Use full or main route')
    prefix = 'xdesk.xmain' if route == 'full' else 'xdesk'
    nodeset += '.nodeset ' + ' '.join(f'v({prefix}.{name})=0' for name in ('sum_out','fader_out','fader_in','fader_fb','hot_amp','cold_amp','unbal_amp','hot_fb','unbal_fb')) + '\n'
    return 'Mackie 8Bus selected main mix path - estimated semiconductor simulation\n' + library(rail_smoothing) + '\n' + source + '\n' + path + 'RreceiverP pin2 0 10k\nRreceiverM pin3 0 10k\nEout out 0 pin2 pin3 1\n.options method=gear reltol=1e-4 abstol=1e-12 vntol=1e-7\n' + nodeset


def simulate(folder, text, label):
    circuit.DATA = Path(folder)
    engine=circuit.CircuitSpice.new_instance()
    uniform=bool(re.search(r'(?im)^\.options?\s+.*\binterp\b',text))
    transient=bool(re.search(r'(?im)^\.tran\s',text))
    if transient and not uniform and getattr(engine,'_workshop_uniform_storage_used',False):
        raise RuntimeError('ngspice retains INTERP inside this library instance; use a fresh process for adaptive samples')
    if uniform:
        engine._workshop_uniform_storage_used=True
    try:
        values = circuit.simulate(text, label)
    except Exception:
        try:
            if engine.last_plot.startswith('tran'):
                plot=engine.plot(None,engine.last_plot)
                np.savez_compressed(Path(folder)/f'{label}.partial.npz',**{key:np.asarray(plot[key]._data).copy() for key in plot})
        except Exception as diagnostic_error:
            print(json.dumps({'partial_save_error':str(diagnostic_error)}),flush=True)
        raise
    circuit.CircuitSpice.new_instance().destroy()
    return values


def render(folder, x, volts_per_unit, label, route='full', parameters='', oversample=4, save_nodes=False, storage='adaptive', solver_iterations=10, source_kind='filesource',rail_smoothing=0):
    if storage not in ('adaptive', 'uniform') or (storage == 'uniform' and save_nodes):
        raise ValueError('Uniform storage saves output only; rail checks require adaptive nodes')
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    x = np.asarray(x, dtype=np.float64)
    pad = FS // 2
    padded = np.concatenate((np.zeros(pad), x, np.zeros(FS // 20)))
    times = np.arange(len(padded)) / FS
    source_path = folder / f'{label}_source.txt'
    np.savetxt(source_path, np.column_stack((times, padded)), fmt='%.17g' if source_kind=='clocked' else '%.12g')
    source = f'Asrc [%vd(signal 0)] filesrc\n.model filesrc filesource (file="{source_path.as_posix()}" amploffset=[0] amplscale=[{volts_per_unit}] timeoffset=0 timescale=1 timerelative=false amplstep=false)\nEp hot 0 signal 0 .5\nEm cold 0 signal 0 -.5'
    if source_kind=='pwl':
        rows=[' '.join(f'{times[i]:.17g} {padded[i]*volts_per_unit:.17g}' for i in range(start,min(start+8,len(times)))) for start in range(0,len(times),8)]
        source='Vsignal signal 0 DC 0 PWL(\n+ '+'\n+ '.join(rows)+'\n+ )\nEp hot 0 signal 0 .5\nEm cold 0 signal 0 -.5'
        del rows
    elif source_kind=='clocked':
        period=1/FS
        source+=f'\nVsampleclock sampleclock 0 DC 0 PULSE(0 0 0 {period/4:.17g} {period/4:.17g} {period/2:.17g} {period:.17g})'
    elif source_kind!='filesource':
        raise ValueError('Use native pwl, clocked filesource, or XSPICE filesource')
    step = 1 / (FS * oversample)
    prefix = 'xdesk.xmain' if route == 'full' else 'xdesk'
    nodes = ['out'] if storage == 'uniform' else ['out', 'pin2', 'pin3', 'unbalanced']
    if save_nodes:
        nodes += [prefix + '.' + name for name in ('sum_out', 'fader_out', 'hot_amp', 'cold_amp', 'unbal_amp')]
        if route == 'full':
            nodes += ['xdesk.xpre.opout', 'xdesk.insert', 'xdesk.xchannel.fader_out', 'xdesk.xchannel.pan_amp']
    options = '.options interp\n' if storage == 'uniform' else ''
    options += f'.options itl4={int(solver_iterations)}\n'
    effective_library=library(rail_smoothing)
    (folder/f'{label}_library.lib').write_text(effective_library)
    text = deck(source, route, parameters,rail_smoothing) + options + '.save ' + ' '.join(f'v({node})' for node in nodes) + f'\n.tran {step:.12g} {times[-1]:.12g} 0 {step:.12g}\n.end\n'
    start = time.time()
    values = simulate(folder, text, label)
    t = values['time'].real
    if len(t) < 2 or t[-1] < times[-1] - step or any(not np.isfinite(v).all() for v in values.values()):
        raise RuntimeError('Incomplete or nonfinite SPICE render')
    uniform = np.arange((len(padded)-1)*oversample+1) / (FS*oversample)
    audio = resample_poly(np.interp(uniform, t, values['out'].real), 1, oversample)[pad:pad+len(x)]
    sf.write(str(folder / f'{label}_volts.wav'), audio, FS, subtype='FLOAT')
    bounds = {key: {'min': float(v.real.min()), 'max': float(v.real.max())} for key, v in values.items() if key != 'time'}
    correlation = float(np.corrcoef(values['pin2'].real,values['pin3'].real)[0,1]) if 'pin2' in values and np.std(values['pin2'].real)>1e-10 else None
    bounds_key = 'raw_node_bounds_volts' if storage == 'adaptive' else 'interpolated_output_bounds_volts'
    report = {'route': route, 'parameters': parameters, 'input_volts_per_unit': volts_per_unit, 'seconds': len(x)/FS, 'elapsed_seconds': time.time()-start, 'oversample': oversample, 'solver_iterations':solver_iterations, 'source_kind':source_kind, 'storage': storage, 'stored_samples':len(t), bounds_key: bounds, 'raw_balanced_leg_correlation':correlation, 'output': 'pin2 minus pin3, differential volts', 'library_hashes': {name: sha(HERE/name) for name in ('mackie8bus.lib','bus_output.lib')}}
    report.update(rail_smoothing_volts=rail_smoothing,effective_library_sha256=sha(folder/f'{label}_library.lib'))
    (folder / f'{label}_render.json').write_text(json.dumps(report, indent=2))
    if save_nodes:
        indices=np.linspace(0,len(t)-1,min(50000,len(t))).astype(int)
        np.savez_compressed(folder / f'{label}_node_excerpt.npz', **{key:v[indices] for key,v in values.items()})
    return audio


def probes(folder):
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    checks = {}
    ac_results = {}
    for route in ('main', 'full'):
        source = 'Vsignal signal 0 dc 0 ac 1\nEp hot 0 signal 0 .5\nEm cold 0 signal 0 -.5'
        ac = simulate(folder, deck(source, route) + '.ac dec 60 10 100000\n.end\n', route + '_ac')
        f = ac['frequency'].real
        z = ac['out']
        ac_results[route] = {'gain_db': {str(hz): float(np.interp(np.log(hz), np.log(f), 20*np.log10(abs(z)))) for hz in (20,50,1000,10000,20000)}, 'phase_1k_degrees': float(np.interp(np.log(1000),np.log(f),np.unwrap(np.angle(z))*180/np.pi))}
        np.savez(folder/f'{route}_ac.npz', frequency=f, response=z)
    expected = (2700/5100)*(1+2400/1100)*2
    measured = 10**(ac_results['main']['gain_db']['1000']/20)
    checks['main_small_signal_gain_within_5_percent_of_resistor_formula'] = abs(measured/expected-1) < .05
    checks['full_small_signal_gain_50_to_53_db'] = 50 < ac_results['full']['gain_db']['1000'] < 53
    rows = []
    tones = []
    cases = [(50,.001),(1000,.001),(1000,.01),(1000,.05),(1000,.2),(1000,1),(10000,.2)]
    segments=[]
    offset=0
    for hz, peak in cases:
        duration=.5 if hz<10000 else .1
        t=np.arange(round(FS*duration))/FS
        tones += [peak*np.sin(2*np.pi*hz*t),np.zeros(FS//10)]
        segments.append((offset+duration*.5,offset+duration))
        offset+=duration+.1
    y = render(folder,np.concatenate(tones),1,'full_profile',save_nodes=True)
    for i,(hz,peak) in enumerate(cases):
        part=y[round(segments[i][0]*FS):round(segments[i][1]*FS)]
        fundamental,harmonics=harmonics_at(part,hz)
        rows.append({'hz':hz,'input_peak_volts':peak,'gain_db':float(20*np.log10(max(fundamental/peak,1e-12))),'harmonics_db':float(20*np.log10(max(np.linalg.norm(harmonics)/max(fundamental,1e-12),1e-12))),'peak_volts':float(abs(part).max())})
    bounds=json.loads((folder/'full_profile_render.json').read_text())['raw_node_bounds_volts']
    checks['opamp_outputs_within_own_supply_rails'] = all(b['min'] > (-18 if '.xchannel.' in n else -16) and b['max'] < (18 if '.xchannel.' in n else 16) for n,b in bounds.items() if n.endswith(('opout','sum_out','fader_out','hot_amp','cold_amp','unbal_amp','pan_amp')))
    checks['main_fader_stage_reaches_overload'] = bounds['xdesk.xmain.fader_out']['max'] > 14 and bounds['xdesk.xmain.fader_out']['min'] < -14
    checks['balanced_driver_legs_opposite_polarity'] = json.loads((folder/'full_profile_render.json').read_text())['raw_balanced_leg_correlation'] < -.99
    checks['gain_compresses_by_more_than_20_db'] = rows[1]['gain_db']-rows[5]['gain_db'] > 20
    result={'ac':ac_results,'analytic_main_gain':expected,'rows':rows,'checks':checks,'passed':all(checks.values()),'library_hashes':{name:sha(HERE/name) for name in ('mackie8bus.lib','bus_output.lib')},'harmonic_method':'Least-squares sine/cosine fit with DC; fractional cycles allowed','raw_nodes':'Bounds use every adaptive solver sample; NPZ contains an indexed excerpt'}
    (folder/'checks.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2),flush=True)
    if not result['passed']:
        raise RuntimeError('Bus-output circuit checks failed')
    return result


def harmonics_at(y,hz):
    t=np.arange(len(y))/FS
    count=min(10,int((FS/2-1)/hz))
    columns=[np.ones(len(y))]
    for h in range(1,count+1):
        columns += [np.sin(2*np.pi*hz*h*t),np.cos(2*np.pi*hz*h*t)]
    coefficients=np.linalg.lstsq(np.column_stack(columns),y,rcond=None)[0][1:]
    amplitudes=np.hypot(coefficients[::2],coefficients[1::2])
    return float(amplitudes[0]),amplitudes[1:]


if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('folder')
    args=parser.parse_args()
    probes(Path(args.folder).resolve())
