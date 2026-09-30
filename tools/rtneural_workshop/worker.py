import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import numpy as np
import soundfile as sf
from references import load_pair, split_pair
from records import run_records, score_records, spans, training_layout, training_starts,prime_records

REPO = Path(__file__).resolve().parents[2]
DATA = REPO / 'output/rtneural-workshop'
RUNNER = DATA / 'bin/runner.exe'
RENDERER = REPO / 'out/build/vst3/plugin/TRENCH_Render_artefacts/Release/TRENCH_Render.exe'
FS = 48000
WARM = 12000


def emit(**event):
    print(json.dumps(event), flush=True)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read_audio(path):
    x, fs = sf.read(str(path), always_2d=True)
    if fs != FS or not np.isfinite(x).all() or len(x) == 0:
        raise ValueError('Use a finite, nonempty 48 kHz WAV')
    return x.mean(axis=1).astype(np.float32)


def native(job, x, mode, value, label):
    original_size = len(x)
    rate = FS
    if mode == 'model':
        rate = json.loads(Path(value).read_text()).get('workshop_runtime', {}).get('sample_rate', FS)
    if rate != FS:
        from scipy.signal import resample_poly
        from math import gcd
        divisor = gcd(rate, FS)
        x = resample_poly(x, rate // divisor, FS // divisor).astype(np.float32)
    source, target = job / f'{label}_in.f32', job / f'{label}_out.f32'
    np.asarray(x, dtype='<f4').tofile(source)
    result = subprocess.run([str(RUNNER), mode, str(source), str(target), str(value)], capture_output=True, text=True, check=True)
    (job / f'{label}.log').write_text(result.stdout + result.stderr)
    y = np.fromfile(target, dtype='<f4')
    if y.shape != x.shape or not np.isfinite(y).all():
        raise ValueError('Incomplete or nonfinite native render')
    if rate != FS:
        y = resample_poly(y, FS // divisor, rate // divisor)[:original_size].astype(np.float32)
        if len(y) != original_size:
            raise ValueError('Sample-rate conversion returned incomplete audio')
    return y


def teacher(job, config, x, label):
    if config['reference'] == 'model':
        return native(job, x, 'model', config['capture_model']['path'], label)
    return native(job, x, 'target', config['drive_db'], label)


def filtered(job, x, label):
    source, target = job / f'{label}_in.wav', job / f'{label}_out.wav'
    sf.write(str(source), x, FS, subtype='FLOAT')
    command = [str(RENDERER), '--in', str(source), '--out', str(target), '--body', 'Talking Hedz', '--rate', str(FS), '--input', '0', '--output', '0', '--q', '0.4', '--morph', '0:0.15,0.25:0.8,0.5:0.2,0.75:0.9,end:0.3', '--ring', '0', '--move', '0']
    duration = len(x) / FS
    command[command.index('--morph') + 1] = f'0:0.15,{duration*0.25}:0.8,{duration*0.5}:0.2,{duration*0.75}:0.9,end:0.3'
    result = subprocess.run(command, cwd=REPO, env=dict(os.environ, TRENCH_SLAM='0', TRENCH_NO_OUTPUT_MACKITY='1'), capture_output=True, text=True, check=True)
    (job / f'{label}.log').write_text(result.stdout + result.stderr)
    return read_audio(target)


def metrics(y):
    return {'peak_dbfs': float(20 * np.log10(max(float(abs(y).max()), 1e-12))), 'rms_dbfs': float(20 * np.log10(max(float(np.sqrt(np.mean(y.astype(np.float64) ** 2))), 1e-12)))}


def audible(job, config, source, mode, value, label):
    x = source
    if config['filter'] and config['placement'] == 'after':
        x = filtered(job, x, f'{label}_pre_filter')
    if mode == 'model' and config['reference'] == 'pair':
        record_samples=int(config.get('record_samples',0))
        if record_samples:
            y=run_records(lambda part,i:native(job,part,mode,value,f'{label}_record{i}'),x,record_samples)
        else:
            _, _, prefix, _, _, _ = split_pair(*load_pair(config))
            y = native(job, np.concatenate((prefix, x)), mode, value, f'{label}_stage')[len(prefix):]
    else:
        y = teacher(job, config, x, f'{label}_stage') if mode == 'teacher' else native(job, x, mode, value, f'{label}_stage')
    if config['filter'] and config['placement'] == 'before':
        y = filtered(job, y, f'{label}_post_filter')
    y = y * 10 ** (config['output_db'] / 20)
    sf.write(str(job / f'{label}.wav'), y, FS, subtype='FLOAT')
    return y


def stimulus(seconds, seed):
    rng = np.random.default_rng(seed)
    length = seconds * FS
    x = np.zeros(length, dtype=np.float64)
    for start in range(0, length, FS // 2):
        n = min(FS // 2, length - start)
        t = np.arange(n) / FS
        frequency = np.exp(rng.uniform(np.log(28), np.log(5000)))
        gain = 10 ** (rng.uniform(-48, 12) / 20)
        phase = rng.uniform(0, 2 * np.pi)
        envelope = np.minimum(t * 300, 1) * np.exp(-t * rng.uniform(0.1, 9))
        tone = 0.75 * np.sin(2 * np.pi * frequency * t + phase) + 0.15 * np.sin(2 * np.pi * 2.01 * frequency * t) + 0.1 * rng.uniform(-1, 1, n)
        if rng.random() < 0.15:
            tone = rng.uniform(-1, 1, n)
        if rng.random() < 0.12:
            tone *= 0
        x[start:start+n] = gain * tone * envelope
    return x.astype(np.float32)


def esr(prediction, target):
    return float(np.sum((prediction.astype(np.float64) - target) ** 2) / max(np.sum(target.astype(np.float64) ** 2), 1e-12))


def train(job, config):
    import torch
    from model import Net, predict, export,save_checkpoint
    from losses import AudioObjective
    torch.set_num_threads(4)
    torch.manual_seed(7)
    requested_device=config.get('device','auto')
    if requested_device not in ('auto','cpu','cuda'):
        raise ValueError('Use auto, cpu or cuda for training')
    if requested_device=='cuda' and not torch.cuda.is_available():
        raise RuntimeError('CUDA was requested but is unavailable in this Python environment')
    device=torch.device('cuda' if requested_device=='cuda' or (requested_device=='auto' and torch.cuda.is_available()) else 'cpu')
    if device.type=='cuda':
        torch.backends.cuda.matmul.allow_tf32=False
        torch.backends.cudnn.allow_tf32=False
    rng = np.random.default_rng(19)
    emit(event='progress', message='Preparing reference training and validation pairs', progress=0.03)
    if config['reference'] == 'pair':
        tx, ty, vx, vy, source, holdout_teacher = split_pair(*load_pair(config))
        provenance = {'training_source': 'First 70% of aligned pair', 'validation_source': 'Next 15% of aligned pair', 'holdout_source': 'Final 15% of aligned pair', 'hardware_description': config['pair_description'], 'delay_samples': config['delay_samples'], 'alignment': 'User-specified constant integer delay; clock drift and fractional delay are not corrected'}
    else:
        tx, vx = stimulus(36, 91), stimulus(10, 213)
        ty = teacher(job, config, tx, 'training_target')
        vy = teacher(job, config, vx, 'validation_target')
        source = read_audio(config['source'])
        holdout_teacher = teacher(job, config, source, 'holdout_teacher')
        provenance = {'training_source': '36 s seeded synthetic signals, seed 91', 'validation_source': '10 s independently seeded synthetic signals, seed 213', 'holdout_source': config['source_name']}
    for name, data in [('train_input', tx), ('train_target', ty), ('validation_input', vx), ('validation_target', vy)]:
        sf.write(str(job / f'{name}.wav'), data, FS, subtype='FLOAT')
    net = Net(config['hidden'], input_scale=config.get('input_scale', 16.0))
    checkpoint = config.get('initial_checkpoint')
    if checkpoint:
        net.load_state_dict(torch.load(checkpoint, weights_only=True,map_location='cpu'))
        provenance['initial_checkpoint_sha256'] = sha(checkpoint)
    net.to(device)
    provenance['training_device']={'type':device.type,'name':torch.cuda.get_device_name(device) if device.type=='cuda' else 'CPU','torch_version':torch.__version__,'cuda_build':torch.version.cuda,'precision':'float32; no mixed precision or TF32'}
    emit(event='progress',message=f'Training on {provenance["training_device"]["name"]}',progress=0.04,training_device=provenance['training_device'])
    learning_rate=float(config.get('learning_rate',0.0005 if checkpoint else 0.002))
    if not np.isfinite(learning_rate) or not 0<learning_rate<=.01:
        raise ValueError('Learning rate must be positive and at most 0.01')
    minimum_rate=min(learning_rate,.0001)
    optimizer = torch.optim.Adam(net.parameters(), lr=learning_rate)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, config['steps'], eta_min=minimum_rate)
    x, y = torch.from_numpy(tx).to(device), torch.from_numpy(ty).to(device)
    best = float('inf')
    history = []
    block, cycle, batch = 4096, 16, int(config.get('batch_size',8))
    if not 1<=batch<=64:
        raise ValueError('Training batch size must be between 1 and 64')
    record_samples=int(config.get('record_samples',0))
    if record_samples:
        if config['reference']!='pair' or config['delay_samples']!=0 or config.get('filter',False):
            raise ValueError('Independent records require an aligned pair with no additional filter')
        for part in (tx,vx,source):
            spans(len(part),record_samples)
        provenance.update(record_samples=record_samples,record_seconds=record_samples/FS,record_state_reset=True)
    cycle=training_layout(len(x),record_samples,WARM,block)
    random_context=bool(config.get('random_record_context',False))
    if random_context:
        if not record_samples:
            raise ValueError('Random capture context requires independent records')
        cycle=min(cycle,8)
        provenance['record_sampling']='Independent random training offsets, with complete preceding capture context and at most eight blocks per state refresh'
    objective = AudioObjective(waveform_weight=float(config.get('waveform_weight',0.0))).to(device)
    start_time = time.time()
    if checkpoint:
        prediction = run_records(lambda part,i:predict(net,part),vx,record_samples)
        initial_loss, initial_parts, initial_esr = score_records(objective,prediction,vy,record_samples,WARM)
        best = float(initial_loss)
        history.append({'step': 0, 'loss': None, 'validation_objective': best, 'validation_mr_spectral': float(initial_parts['mr_spectral']), 'validation_lf_esr': float(initial_parts['lf_esr']), 'validation_full_band_esr_diagnostic': initial_esr})
        save_checkpoint(net,job/'best.pt')
        export(net, job / 'model_rtneural.json')
        emit(event='progress', message='Evaluated starting checkpoint', progress=0.08, **history[-1])
    for step in range(config['steps']):
        if step % cycle == 0:
            indices = training_starts(rng,len(x),record_samples,WARM,block,cycle,batch,random_context)
            if random_context:
                state=prime_records(net,x,indices,record_samples,WARM)
            else:
                prefix = torch.stack([x[i:i+WARM] for i in indices]).unsqueeze(-1)
                with torch.no_grad():
                    if record_samples:
                        _, state=net(torch.zeros(batch,WARM,1,device=device))
                        _, state=net(prefix,state)
                    else:
                        _, state = net(prefix)
                indices += WARM
        inputs = torch.stack([x[i:i+block] for i in indices]).unsqueeze(-1)
        target = torch.stack([y[i:i+block] for i in indices])
        pred, state = net(inputs, state)
        state = tuple(v.detach() for v in state)
        indices += block
        pred = pred.squeeze(-1)
        loss, components = objective(pred, target)
        if not torch.isfinite(loss):
            raise RuntimeError('Nonfinite training objective; inspect reference levels and alignment')
        optimizer.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(net.parameters(), 1)
        optimizer.step()
        scheduler.step()
        if step % 100 == 0 or step == config['steps'] - 1:
            prediction = run_records(lambda part,i:predict(net,part),vx,record_samples)
            validation_loss, validation_components, validation_esr = score_records(objective,prediction,vy,record_samples,WARM)
            score = float(validation_loss)
            row = {'step': step + 1, 'loss': float(loss.detach()), 'validation_objective': score, 'validation_mr_spectral': float(validation_components['mr_spectral']), 'validation_lf_esr': float(validation_components['lf_esr']), 'validation_full_band_esr_diagnostic': validation_esr}
            history.append(row)
            if score < best:
                best = score
                save_checkpoint(net,job/'best.pt')
                export(net, job / 'model_rtneural.json')
            emit(event='progress', message=f'Training step {step+1} / {config["steps"]}', progress=0.08 + 0.78 * (step+1) / config['steps'], **row)
        elif step % 25 == 0:
            emit(event='progress', message=f'Training step {step+1} / {config["steps"]}', progress=0.08 + 0.78 * (step+1) / config['steps'], step=step+1, loss=float(loss.detach()))
    net.load_state_dict(torch.load(job / 'best.pt', weights_only=True,map_location='cpu'))
    prefix = vx if config['reference'] == 'pair' and not record_samples else np.empty(0, np.float32)
    evaluation_input = np.concatenate((prefix, source))
    cpp = run_records(lambda part,i:native(job,part,'model',job/'model_rtneural.json',f'holdout_cpp_{i}' if record_samples else 'holdout_cpp'),evaluation_input,record_samples)[len(prefix):]
    pytorch = run_records(lambda part,i:predict(net,part),evaluation_input,record_samples)[len(prefix):]
    parity = float(np.sqrt(np.mean((cpp.astype(np.float64)-pytorch) ** 2)))
    if parity > 1e-4:
        raise RuntimeError(f'Native export parity failed: RMSE {parity}')
    silence = native(job, np.zeros(FS, np.float32), 'model', job / 'model_rtneural.json', 'silence')
    best_row = min(history, key=lambda row: row['validation_objective'])
    test_loss, test_components, score = score_records(objective,cpp,holdout_teacher,record_samples,WARM)
    report = {'objective': objective.description(), 'validation_objective': best, 'validation_mr_spectral': best_row['validation_mr_spectral'], 'validation_lf_esr': best_row['validation_lf_esr'], 'holdout_objective': float(test_loss), 'holdout_mr_spectral': float(test_components['mr_spectral']), 'holdout_lf_esr': float(test_components['lf_esr']), 'holdout_esr': score, 'holdout_esr_db': 10*np.log10(max(score, 1e-12)), 'export_parity_rmse': parity, 'silence_peak': float(abs(silence).max()), 'elapsed_seconds': time.time()-start_time, 'history': history, 'training_source': '36 s seeded synthetic signals, seed 91', 'validation_source': '10 s independently seeded synthetic signals, seed 213', 'holdout_source': config['source_name'], 'source_used_for_training': False, 'model_drive_db': config['drive_db'], 'sample_rate': FS, 'hidden_units': config['hidden'], 'input_domain': 'Training stimulus peaks up to 4 digital units', 'runtime_drive': 'Fixed at captured drive; change drive and retrain to create another snapshot', 'status': 'Experimental; listening acceptance required'}
    report.update(provenance)
    report['reference_type'] = config['reference']
    report['model_drive_db'] = config['drive_db'] if config['reference'] == 'native' else None
    report['input_domain'] = 'Imported pair values' if config['reference'] == 'pair' else 'Synthetic training signals up to 4 digital units'
    report['error_warmup_excluded_samples'] = WARM
    report['optimizer_input_scale_folded_into_weights'] = net.input_scale
    report['held_out_input_context_samples'] = len(prefix)
    report['optimizer']={'type':'Adam','initial_learning_rate':learning_rate,'minimum_learning_rate':minimum_rate,'steps':config['steps'],'gradient_norm_limit':1,'batch_size':batch,'block_samples':block,'blocks_per_sequence':cycle}
    (job / 'training_report.json').write_text(json.dumps(report, indent=2))
    return report


def main(job):
    config = json.loads((job / 'config.json').read_text())
    source = read_audio(config['source'])
    emit(event='progress', message='Preparing reference audition', progress=0.01)
    if config['reference'] == 'pair':
        *_, source, target = split_pair(*load_pair(config))
        if config['filter']:
            target = filtered(job, target, 'recorded_reference_filter')
        target *= 10 ** (config['output_db'] / 20)
        sf.write(str(job / 'reference.wav'), target, FS, subtype='FLOAT')
    else:
        target = audible(job, config, source, 'teacher', config['drive_db'], 'reference')
    baseline = filtered(job, source, 'baseline_filter') if config['filter'] else source
    baseline = baseline * 10 ** (config['output_db'] / 20)
    sf.write(str(job / 'baseline.wav'), baseline, FS, subtype='FLOAT')
    report = train(job, config) if config['action'] == 'train' else None
    audio = {'baseline': metrics(baseline), 'reference': metrics(target)}
    if report:
        emit(event='progress', message='Rendering and checking the native RTNeural export', progress=0.92)
        model = audible(job, config, source, 'model', job / 'model_rtneural.json', 'model')
        audio['model'] = metrics(model)
    result = {'id': job.name, 'config': config, 'audio': audio, 'report': report, 'duration_seconds': len(source)/FS, 'source_sha256': sha(config['source']), 'mackity_source_sha256': sha(REPO / 'plugin/source/dsp/DeskDrive.h'), 'runner_sha256': sha(RUNNER), 'renderer_sha256': sha(RENDERER), 'normalization': False, 'automatic_compensation': False, 'manual_output_location': 'After the entire chain', 'sample_rate': FS}
    if config['reference'] == 'pair':
        result.pop('source_sha256')
        result['pair_sha256'] = {key: sha(config[key]) for key in ('pair_input', 'pair_target')}
    if config['reference'] == 'model':
        result['capture_model_sha256'] = sha(config['capture_model']['path'])
    (job / 'result.json').write_text(json.dumps(result, indent=2))
    emit(event='complete', message='Ready to audition', progress=1, result=result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('job')
    args = parser.parse_args()
    main(Path(args.job).resolve())
