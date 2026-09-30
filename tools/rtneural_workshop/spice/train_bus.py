import argparse
from concurrent.futures import ProcessPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import shutil
import sys
import time
import numpy as np
import soundfile as sf
import bus_output
from train_snapshot import bass_stimulus

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from worker import DATA, FS, filtered, main, metrics, native, sha, stimulus, esr
from records import run_records
from references import split_pair

BASELINE = DATA / 'runs/20260930-0535-8bus-schematic'
STUDY = DATA / 'bus-output-20260930'


def write_audio(path, audio):
    audio = np.asarray(audio, dtype=np.float32)
    if not np.isfinite(audio).all():
        raise ValueError('Nonfinite output')
    sf.write(str(path), audio, FS, subtype='FLOAT')
    readback, rate = sf.read(str(path), dtype='float32')
    if rate != FS or not np.array_equal(audio, readback):
        raise RuntimeError('Audio readback does not match')


def percussion(seconds, seed, levels=(.002,.008,.03,.1,.35)):
    rng=np.random.default_rng(seed)
    x=np.zeros(seconds*FS)
    for i,start in enumerate(range(0,len(x),FS//4)):
        n=min(FS//4,len(x)-start)
        t=np.arange(n)/FS
        pitch=rng.uniform(45,90)+120*np.exp(-t*45)
        kick=np.sin(2*np.pi*np.cumsum(pitch)/FS)*np.exp(-t*22)
        noise=rng.standard_normal(n)
        snare=(noise-np.concatenate(([0],noise[:-1])))*np.exp(-t*65)
        tone=kick if i%2==0 else .15*snare
        x[start:start+n]=tone*rng.choice(levels)
    return x.astype(np.float32)


def soften(x):
    from scipy.signal import butter, sosfilt
    return sosfilt(butter(2,12000/(FS/2),output='sos'),np.clip(x,-.8,.8)).astype(np.float32)


def onsets(seconds, seed):
    rng=np.random.default_rng(seed)
    x=np.zeros(seconds*FS)
    for start in range(0,len(x),FS//2):
        n=min(FS//2,len(x)-start)
        t=np.arange(n)/FS
        f=rng.choice([41.2,55,82.4,110,220,440,880])
        tone=np.sin(2*np.pi*f*t)+.2*np.sin(2*np.pi*2.01*f*t)
        quiet=rng.choice([.03,.08,.15])
        hot=rng.choice([.4,.6,.8])
        step=np.clip((t-.12)/.006,0,1)
        level=quiet+(hot-quiet)*step
        edge=np.minimum(t*500,1)*np.minimum(np.maximum(.32-t,0)*500,1)
        x[start:start+n]=level*tone*np.where(t<.32,edge,0.0)
    return x.astype(np.float32)


def ramps(seconds, seed):
    rng=np.random.default_rng(seed)
    x=np.zeros(seconds*FS)
    for start in range(0,len(x),FS):
        n=min(FS,len(x)-start)
        t=np.arange(n)/FS
        f=rng.choice([55,110,220,440,1000])
        tone=np.sin(2*np.pi*f*t)+.15*np.sin(2*np.pi*3.02*f*t)
        top=rng.choice([-6.0,-4.0,-2.0])
        db=np.where(t<.5,-30+(top+30)*t/.5,top-(top+30)*(t-.5)/.5)
        x[start:start+n]=10**(db/20)*tone*np.minimum(t*500,1)
    return x.astype(np.float32)


def recovery(seconds, seed, levels=(.03,.2,.8)):
    rng=np.random.default_rng(seed)
    x=np.zeros(seconds*FS)
    for start in range(0,len(x),FS):
        n=min(FS//3,len(x)-start)
        t=np.arange(n)/FS
        x[start:start+n]=rng.choice(levels)*np.sin(2*np.pi*rng.choice([35,55,110,440])*t)*np.minimum(t*200,1)*np.minimum(np.maximum(n/FS-t,0)*200,1)
    return x.astype(np.float32)


def verify():
    checks = json.loads((STUDY/'checks-loaded/checks.json').read_text())
    if not checks['passed']:
        raise RuntimeError('Circuit checks must pass before training')
    folder = STUDY/'convergence'
    t = np.arange(FS//4)/FS
    x = .2*np.sin(2*np.pi*10000*t)
    a = bus_output.render(folder,x,1,'overload_4x',oversample=4)
    b = bus_output.render(folder,x,1,'overload_8x',oversample=8)
    error = esr(a[FS//10:],b[FS//10:])
    result = {'probe':'10 kHz, 0.2 V peak differential line input', 'error_power_ratio':error, 'error_db':float(10*np.log10(max(error,1e-15))), 'threshold':.001, 'passed':error<.001}
    (folder/'result.json').write_text(json.dumps(result,indent=2))
    if not result['passed']:
        raise RuntimeError('Transient timestep convergence failed')
    compact = bus_output.render(folder,x,1,'overload_uniform',oversample=4,storage='uniform')
    error = esr(compact,a)
    result = {'error_power_ratio':error, 'threshold':1e-8, 'passed':error<1e-8, 'method':'ngspice INTERP versus adaptive output interpolated offline; same internal timestep and circuit'}
    (folder/'storage.json').write_text(json.dumps(result,indent=2))
    if not result['passed']:
        raise RuntimeError('Uniform storage equivalence failed')
    t = np.arange(FS//2)/FS
    x = .06*(np.sin(2*np.pi*55*t)+.4*np.sin(2*np.pi*1000*t)+.1*np.sin(2*np.pi*7000*t))
    nominal = bus_output.render(STUDY/'sensitivity',x,1,'nominal',storage='uniform')
    rows=[]
    for name,parameters in [('lower_headroom','unity=5.5Meg slew=3Meg margin=3'),('higher_headroom','unity=27Meg slew=9Meg margin=1')]:
        y=bus_output.render(STUDY/'sensitivity',x,1,name,parameters=parameters,storage='uniform')
        rows.append({'name':name,'parameters':parameters,'esr_relative_to_nominal':esr(y,nominal),'peak_volts':float(abs(y).max())})
    (STUDY/'sensitivity/result.json').write_text(json.dumps({'scope':'NJM2068 estimate sensitivity only; does not bound all circuit uncertainty','rows':rows},indent=2))


def prepare(job,workers=1):
    checks=json.loads((STUDY/'checks-loaded/checks.json').read_text())
    if not checks['passed']:
        raise RuntimeError('Circuit checks must pass')
    if any(sha(bus_output.HERE/name)!=expected for name,expected in checks['library_hashes'].items()):
        raise RuntimeError('Circuit changed after profile verification')
    if not json.loads((STUDY/'convergence/result.json').read_text())['passed']:
        raise RuntimeError('Numerical convergence must pass')
    if not json.loads((STUDY/'convergence/storage.json').read_text())['passed']:
        raise RuntimeError('Memory-bounded output storage must match adaptive output')
    job.mkdir(parents=True,exist_ok=False)
    for name in ('mackie8bus.lib','bus_output.lib'):
        (job/name).write_bytes((bus_output.HERE/name).read_bytes())
    source=DATA/'sources/bass_phrase.wav'
    bass,rate=sf.read(str(source),dtype='float32')
    if rate!=FS or bass.ndim!=1 or len(bass)<3*FS:
        raise ValueError('Need three seconds of mono 48 kHz held-out bass')
    train_filtered=filtered(job,bass_stimulus(10,16503),'training_filter')*.25
    validation_filtered=filtered(job,bass_stimulus(3,27051,[.002,.01,.04,.2,.6,0]),'validation_filter')*.25
    validation=np.concatenate((stimulus(2,55063)*.25,bass_stimulus(3,87215,[.002,.01,.04,.2,.6,0]),validation_filtered,np.zeros(FS)))
    heldout_bass=filtered(job,bass[:3*FS],'heldout_filter')*.25
    heldout=np.concatenate((heldout_bass,percussion(3,506791),bass_stimulus(2,909531,[.004,.02,.12,.5]),np.zeros(FS)))
    train=np.concatenate((stimulus(10,55109)*.25,bass_stimulus(10,66203),train_filtered,percussion(6,97173),recovery(4,78945),np.zeros(2*FS)))
    x=np.concatenate((train,validation,heldout)).astype(np.float32)
    write_audio(job/'input.wav',x)
    write_audio(job/'heldout_filter.wav',heldout)
    provenance={'type':'Schematic simulation, not hardware capture','route':'Balanced line input -> channel fader -> hard-left pan -> main L sum -> main master -> balanced XLR output','eq':'Bypassed; residual loading from unused EQ/low-cut branches omitted','settings':{'line_trim_ohms':0,'channel_fader_electrical_wiper_fraction':.366,'main_fader_electrical_wiper_fraction':1,'channel_rail_volts':18,'preamp_and_main_rail_volts':16},'input_volts_per_unit':4,'output_volts_per_unit':16,'output_connection':'pin2 minus pin3, each leg loaded by 10 kohm to ground','training_seconds':42,'validation_seconds':9,'holdout_seconds':9,'heldout_description':'0-3 s Talking Hedz filtered bass at explicit -12.0412 dB before desk; 3-6 s independent percussion; 6-8 s independent bass; 8-9 s recovery. No training or selection on these targets.','stimulus_filter_gain':.25,'normalization':False,'automatic_compensation':False,'library_hashes':{name:sha(job/name) for name in ('mackie8bus.lib','bus_output.lib')},'schematic_sha256':sha(DATA/'research/Mackie-8BUS-schematics.pdf'),'source_sha256':sha(source),'input_sha256':sha(job/'input.wav'),'input_metrics':{name:metrics(v) for name,v in [('train',train),('validation',validation),('holdout',heldout)]},'estimated_semiconductors':['2SA1084','NJM4560','NJM2068','1N4148'],'omissions':['PSU sag and coupling','noise and device variation','other channels and stereo crosstalk','EQ and low-cut branch residual loads','meter and monitor branch loads','insert detection circuit']}
    (job/'target_provenance.json').write_text(json.dumps(provenance,indent=2))
    render_existing(job,workers)


def verify_sources(job,source_kind='clocked'):
    x,rate=sf.read(str(job/'input.wav'),dtype='float32')
    if rate!=FS:
        raise ValueError('Need the saved 48 kHz stimulus')
    folder=STUDY/('native-source-verification' if source_kind=='pwl' else 'clocked-source-verification')
    tests=[('percussion',x[34*FS:35*FS],4,STUDY/'convergence-failure/isolated_default_volts.wav'),('overload_10k',.2*np.sin(2*np.pi*10000*np.arange(FS//4)/FS),1,STUDY/'convergence/overload_4x_volts.wav')]
    rows=[]
    for name,signal,volts,reference_path in tests:
        y=bus_output.render(folder,signal,volts,name,storage='uniform',source_kind=source_kind)
        reference,_=sf.read(str(reference_path))
        error=esr(y,reference)
        rows.append({'probe':name,'esr':error,'threshold':.001,'passed':error<.001,'reference_sha256':sha(reference_path)})
    result={'rows':rows,'passed':all(row['passed'] for row in rows),'source_kind':source_kind,'change':'Explicit sample-transition breakpoints; same audio waveform, circuit and solver tolerances','library_hashes':{name:sha(bus_output.HERE/name) for name in ('mackie8bus.lib','bus_output.lib')}}
    (folder/'result.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result),flush=True)
    if not result['passed']:
        raise RuntimeError('Source comparison failed')


def render_record(folder,part,index,library_hashes,total=20):
    folder=Path(folder)
    label=f'record{index:02d}'
    audio_path=folder/f'{label}_volts.wav'
    cache_path=folder/f'{label}_cache.json'
    fingerprint=hashlib.sha256(part.astype('<f4').tobytes()).hexdigest()
    expected={'input_float_sha256':fingerprint,'library_hashes':library_hashes,'record_samples':len(part),'source_kind':'filesource','rail_smoothing_volts':0}
    cached=json.loads(cache_path.read_text()) if cache_path.exists() else {}
    reusable=audio_path.exists() and all(cached.get(key)==value for key,value in expected.items()) and cached.get('audio_sha256')==sha(audio_path)
    print(json.dumps({'record':index+1,'of':total,'event':'reuse' if reusable else 'render'}),flush=True)
    if reusable:
        volts,rate=sf.read(str(audio_path),dtype='float32')
        if rate!=FS or volts.shape!=part.shape or not np.isfinite(volts).all():
            raise ValueError('Cached record is incomplete')
    else:
        bus_output.render(folder,part,4,label,storage='uniform')
        volts,rate=sf.read(str(audio_path),dtype='float32')
        if rate!=FS or volts.shape!=part.shape or not np.isfinite(volts).all():
            raise ValueError('Rendered record is incomplete')
        cached={**expected,'audio_sha256':sha(audio_path)}
        cache_path.write_text(json.dumps(cached,indent=2))
    return index,(volts/16).astype(np.float32),cached


def render_existing(job,workers=1):
    if not 1<=workers<=4:
        raise ValueError('Use one to four isolated SPICE processes')
    provenance=json.loads((job/'target_provenance.json').read_text())
    if sha(job/'input.wav')!=provenance['input_sha256']:
        raise ValueError('Input provenance mismatch')
    for name,expected in provenance['library_hashes'].items():
        if sha(bus_output.HERE/name)!=expected or sha(job/name)!=expected:
            raise ValueError('Circuit provenance mismatch')
    x,rate=sf.read(str(job/'input.wav'),dtype='float32')
    if rate!=FS:
        raise ValueError('Pair sample rate mismatch')
    record_samples=3*FS
    if len(x)<60*FS or len(x)%record_samples:
        raise ValueError('This study requires at least the 60-second stimulus in whole three-second records')
    count=len(x)//record_samples
    train_end,validation_end=len(x)*7//10,len(x)*17//20
    folder=job/'spice-records'
    folder.mkdir(exist_ok=True)
    parts={}
    manifest={}
    def collect(result):
        index,audio,cached=result
        start=index*record_samples
        parts[index]=audio
        manifest[index]={'index':index,'start_sample_in_pair':start,'samples':record_samples,'role':'training' if start<train_end else 'validation' if start<validation_end else 'heldout',**cached}
        (job/'records.json').write_text(json.dumps([manifest[i] for i in sorted(manifest)],indent=2))
        print(json.dumps({'event':'captured','complete':len(parts),'of':count,'record':index+1}),flush=True)
    tasks=[(folder,x[start:start+record_samples],index,provenance['library_hashes'],count) for index,start in enumerate(range(0,len(x),record_samples))]
    if workers==1:
        for task in tasks:
            collect(render_record(*task))
    else:
        with ProcessPoolExecutor(max_workers=workers) as executor:
            futures=[executor.submit(render_record,*task) for task in tasks]
            for future in as_completed(futures):
                collect(future.result())
    y=np.concatenate([parts[i] for i in range(count)])
    write_audio(job/'target.wav',y)
    provenance['target_sha256']=sha(job/'target.wav')
    provenance['target_metrics']=metrics(y)
    provenance['solver']='Gear, reltol 1e-4, abstol 1e-12, vntol 1e-7, itl4 10; original filesource, 4x maximum timestep and uniform storage'
    provenance['record_samples']=record_samples
    provenance['record_seconds']=3
    provenance['record_count']=count
    provenance['record_state']='Independent circuit operating point and 0.5 seconds of zero input per record. Training and inference reset recurrent state at the same record boundaries.'
    (job/'target_provenance.json').write_text(json.dumps(provenance,indent=2))


def validate_records(job):
    provenance=json.loads((job/'target_provenance.json').read_text())
    records=json.loads((job/'records.json').read_text())
    x,rate=sf.read(str(job/'input.wav'),dtype='float32')
    y,target_rate=sf.read(str(job/'target.wav'),dtype='float32')
    if rate!=FS or target_rate!=FS or x.shape!=y.shape or len(x)<60*FS or len(x)%(3*FS) or not np.isfinite(y).all():
        raise ValueError('Incomplete reference pair')
    count=len(x)//(3*FS)
    train_end,validation_end=len(x)*7//10,len(x)*17//20
    if [record['index'] for record in records]!=list(range(count)):
        raise ValueError('Capture ordering is incomplete')
    signature={'input_volts_per_unit':4,'seconds':3,'oversample':4,'solver_iterations':10,'source_kind':'filesource','storage':'uniform','rail_smoothing_volts':0,'library_hashes':provenance['library_hashes']}
    for record in records:
        index=record['index']
        label=f'record{index:02d}'
        folder=Path(provenance.get('capture_directory',job/'spice-records'))
        report=json.loads((folder/f'{label}_render.json').read_text())
        if any(report.get(key)!=value for key,value in signature.items()):
            raise ValueError(f'{label}: reference settings changed')
        if sha(folder/f'{label}_library.lib')!=report['effective_library_sha256']:
            raise ValueError(f'{label}: effective circuit changed')
        a,b=index*3*FS,(index+1)*3*FS
        expected_role='training' if a<train_end else 'validation' if a<validation_end else 'heldout'
        if record['start_sample_in_pair']!=a or record['samples']!=3*FS or record['role']!=expected_role:
            raise ValueError(f'{label}: capture boundary or split changed')
        if hashlib.sha256(x[a:b].astype('<f4').tobytes()).hexdigest()!=record['input_float_sha256']:
            raise ValueError(f'{label}: capture input changed')
        volts,record_rate=sf.read(str(folder/f'{label}_volts.wav'),dtype='float32')
        if record_rate!=FS or sha(folder/f'{label}_volts.wav')!=record['audio_sha256'] or not np.array_equal(y[a:b],volts/16):
            raise ValueError(f'{label}: target assembly changed')
    roles=[record['role'] for record in records]
    result={'passed':True,'records':count,'training_records':roles.count('training'),'validation_records':roles.count('validation'),'heldout_records':roles.count('heldout'),'record_samples':3*FS,'reference_settings':signature,'input_sha256':sha(job/'input.wav'),'target_sha256':sha(job/'target.wav'),'verification':'Exact record ordering, split boundaries, input and audio hashes, effective circuit hashes, solver settings and unnormalized 16 V per unit target assembly'}
    (job/'pair_validation.json').write_text(json.dumps(result,indent=2))
    return result


def prepare_extended(job,source,workers=1):
    checks=json.loads((STUDY/'checks-loaded/checks.json').read_text())
    if not checks['passed']:
        raise RuntimeError('Circuit checks must pass')
    if any(sha(bus_output.HERE/name)!=expected for name,expected in checks['library_hashes'].items()):
        raise RuntimeError('Circuit changed after profile verification')
    validate_records(source)
    parent=json.loads((source/'target_provenance.json').read_text())
    x,rate=sf.read(str(source/'input.wav'),dtype='float32')
    if rate!=FS or len(x)!=60*FS:
        raise ValueError('Extension starts from the 60-second study stimulus')
    job.mkdir(parents=True,exist_ok=False)
    for name in ('mackie8bus.lib','bus_output.lib'):
        (job/name).write_bytes((bus_output.HERE/name).read_bytes())
    old_train,old_validation,old_heldout=x[:42*FS],x[42*FS:51*FS],x[51*FS:]
    new_train=np.concatenate((onsets(14,311007),ramps(10,311013),recovery(8,311019,(.5,.65,.8)),percussion(6,311023,(.2,.4,.6,.8)),bass_stimulus(4,311029,[.3,.5,.8,.05,0])))
    new_validation=np.concatenate((onsets(3,422003),ramps(3,422009),recovery(2,422011,(.5,.65,.8)),np.zeros(FS)))
    new_heldout=np.concatenate((onsets(3,533001),ramps(2,533007),percussion(2,533011,(.2,.4,.6,.8)),recovery(1,533017,(.5,.65,.8)),np.zeros(FS)))
    new_train,new_validation,new_heldout=(soften(v) for v in (new_train,new_validation,new_heldout))
    x=np.concatenate((old_train,new_train,old_validation,new_validation,old_heldout,new_heldout)).astype(np.float32)
    if len(x)!=120*FS:
        raise RuntimeError('Extended stimulus must be 120 seconds')
    write_audio(job/'input.wav',x)
    if (source/'heldout_filter.wav').exists():
        shutil.copyfile(source/'heldout_filter.wav',job/'heldout_filter.wav')
    folder=job/'spice-records'
    folder.mkdir()
    old_folder=Path(parent.get('capture_directory',source/'spice-records'))
    for old_index,new_index in list(zip(range(14),range(14)))+list(zip(range(14,17),range(28,31)))+list(zip(range(17,20),range(34,37))):
        for suffix in ('_volts.wav','_cache.json','_render.json','_library.lib','_source.txt'):
            src=old_folder/f'record{old_index:02d}{suffix}'
            if src.exists():
                shutil.copyfile(src,folder/f'record{new_index:02d}{suffix}')
    provenance=dict(parent)
    provenance.pop('capture_directory',None)
    provenance.update({'input_sha256':sha(job/'input.wav'),'training_seconds':84,'validation_seconds':18,'holdout_seconds':18,
        'extended_from':source.name,'extension':'The 60-second study stimulus keeps its records; each split gains the same length again of onset bursts stepping from below to above the clip point into silence, 1-second dB ramps through the clip point and back, hot recovery bursts and hot percussion, none above 0.8 units, every burst with a 5 ms attack and the new material rolled off above 12 kHz. Old records 14-19 keep their circuit renders at their new positions 28-30 and 34-36.',
        'heldout_description':parent.get('heldout_description','')+' 9-18 s: independent onset bursts, ramps, hot percussion and hot recovery.',
        'input_metrics':{name:metrics(v) for name,v in [('train',x[:84*FS]),('validation',x[84*FS:102*FS]),('holdout',x[102*FS:])]}})
    (job/'target_provenance.json').write_text(json.dumps(provenance,indent=2))
    render_existing(job,workers)
    shutil.copyfile(source/'best.pt',job/'initial.pt')
    import torch
    weights=torch.load(job/'initial.pt',weights_only=True,map_location='cpu')
    provenance=json.loads((job/'target_provenance.json').read_text())
    provenance['parent_run']=source.name
    provenance['initial_checkpoint_sha256']=sha(job/'initial.pt')
    provenance['refinement_hidden_units']=int(weights['out.weight'].shape[1])
    provenance['refinement_initialization']='Copied parent checkpoint'
    (job/'target_provenance.json').write_text(json.dumps(provenance,indent=2))


def prepare_refinement(job,source,hidden=None):
    import torch
    from model import Net,widen
    validate_records(source)
    job.mkdir(parents=True,exist_ok=False)
    for name in ('input.wav','target.wav','mackie8bus.lib','bus_output.lib','records.json','pair_validation.json'):
        shutil.copyfile(source/name,job/name)
    shutil.copyfile(source/'best.pt',job/'initial.pt')
    weights=torch.load(job/'initial.pt',weights_only=True,map_location='cpu')
    original_hidden=weights['out.weight'].shape[1]
    if hidden and hidden!=original_hidden:
        torch.manual_seed(7)
        original=Net(original_hidden,input_scale=16)
        original.load_state_dict(weights)
        torch.save(widen(original,hidden).state_dict(),job/'initial.pt')
    provenance=json.loads((source/'target_provenance.json').read_text())
    provenance['capture_directory']=str(Path(provenance.get('capture_directory',source/'spice-records')).resolve())
    provenance['parent_run']=source.name
    provenance['initial_checkpoint_sha256']=sha(job/'initial.pt')
    provenance['refinement_hidden_units']=hidden or original_hidden
    provenance['refinement_initialization']='Response-preserving widening with added nonlinear features' if hidden and hidden!=original_hidden else 'Copied parent checkpoint'
    (job/'target_provenance.json').write_text(json.dumps(provenance,indent=2))


def train(job,steps,initial_checkpoint=None,learning_rate=None,device='auto',batch_size=8,random_context=False,waveform_weight=0.0):
    provenance=json.loads((job/'target_provenance.json').read_text())
    if initial_checkpoint is None and provenance.get('parent_run'):
        initial_checkpoint=job/'initial.pt'
    for name in ('input','target'):
        if sha(job/f'{name}.wav')!=provenance[f'{name}_sha256']:
            raise ValueError('Training pair provenance mismatch')
    for name,expected in provenance['library_hashes'].items():
        if sha(job/name)!=expected:
            raise ValueError('Circuit provenance mismatch')
    validate_records(job)
    config={'drive_db':0,'output_db':-12,'steps':steps,'hidden':32,'input_scale':16,'initial_checkpoint':str(BASELINE/'best.pt'),'action':'train','placement':'after','filter':False,'source':str(DATA/'sources/bass_phrase.wav'),'source_name':'Held-out filtered bass, percussion and recovery','created_at':time.strftime('%Y-%m-%d %H:%M:%S'),'reference':'pair','delay_samples':0,'pair_input':str(job/'input.wav'),'pair_target':str(job/'target.wav'),'pair_description':'Mackie 8Bus schematic candidate including channel fader/pan, main L summing and balanced output. Maximum line trim and main master; channel fader near unity; EQ and low-cut bypassed. Estimated semiconductors. Not a hardware capture.'}
    config['record_samples']=provenance['record_samples']
    config['device']=device
    config['batch_size']=batch_size
    config['random_record_context']=random_context
    config['waveform_weight']=waveform_weight
    config['hidden']=provenance.get('refinement_hidden_units',32)
    if initial_checkpoint:
        config['initial_checkpoint']=str(initial_checkpoint)
    if learning_rate is not None:
        config['learning_rate']=learning_rate
    config['pair_description']+=' Independent 3-second captures. First held-out capture has Talking Hedz already applied before the desk.'
    (job/'config.json').write_text(json.dumps(config,indent=2))
    main(job)
    finish(job)


def finish(job):
    x,_=sf.read(str(job/'input.wav'),dtype='float32')
    y,_=sf.read(str(job/'target.wav'),dtype='float32')
    *_,holdout,target=split_pair(x,y)
    config=json.loads((job/'config.json').read_text())
    record_samples=config['record_samples']
    if config['output_db']!=-12 or config['filter']:
        raise ValueError('These auditions require the declared fixed OUTPUT -12 dB and the saved filter stimulus')
    gain=10**(-12/20)
    reference=target*gain
    model=run_records(lambda part,i:native(job,part,'model',job/'model_rtneural.json',f'final_model_{i}'),holdout,record_samples)*gain
    baseline=holdout*gain
    for name,audio in [('reference',reference),('model',model),('baseline',baseline)]:
        write_audio(job/f'{name}.wav',audio)
    result=json.loads((job/'result.json').read_text())
    result['audio']={name:metrics(audio) for name,audio in [('reference',reference),('model',model),('baseline',baseline)]}
    result['audition_gain_routing']='Original held-out input enters the model unchanged; fixed OUTPUT -12 dB is applied afterward to baseline, reference and model.'
    (job/'result.json').write_text(json.dumps(result,indent=2))
    old=run_records(lambda part,i:native(job,part,'model',BASELINE/'model_rtneural.json',f'accepted_preamp_{i}'),holdout,record_samples)*gain
    write_audio(job/'accepted_preamp.wav',old)
    gap=np.zeros(FS,np.float32)
    auditions={'8Bus_MAIN_SPICE_then_RTNeural.wav':([reference,model],['Full route SPICE','Full route RTNeural']), '8Bus_preamp_then_main_output.wav':([old,model],['Accepted preamp model','New main-output model']), '8Bus_preamp_SPICE_new_model.wav':([old,reference,model],['Accepted preamp model','Full route SPICE','Full route RTNeural'])}
    manifest={}
    for name,(parts,labels) in auditions.items():
        sequence=[]
        for i,part in enumerate(parts):
            if i: sequence.append(gap)
            sequence.append(part)
        y=np.concatenate(sequence)
        write_audio(job/name,y)
        manifest[name]={'order':labels,'start_seconds':[i*(1+len(holdout)/FS) for i in range(len(parts))],'manual_output_db':-12,'normalization':False,'metrics':metrics(y),'sha256':sha(job/name)}
    for name,parts,labels in [('8Bus_bass_SPICE_then_RTNeural.wav',[reference[:3*FS],model[:3*FS]],['Full route SPICE','Full route RTNeural']),('8Bus_bass_preamp_then_main.wav',[old[:3*FS],model[:3*FS]],['Accepted preamp model','New main-output model'])]:
        y=np.concatenate((parts[0],gap,parts[1]))
        write_audio(job/name,y)
        manifest[name]={'order':labels,'start_seconds':[0,4],'manual_output_db':-12,'normalization':False,'metrics':metrics(y),'sha256':sha(job/name)}
    (job/'auditions.json').write_text(json.dumps(manifest,indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('job',nargs='?')
    parser.add_argument('--verify',action='store_true')
    parser.add_argument('--verify-source',action='store_true')
    parser.add_argument('--source-kind',choices=['pwl','clocked'],default='clocked')
    parser.add_argument('--train-existing',action='store_true')
    parser.add_argument('--render-existing',action='store_true')
    parser.add_argument('--finish',action='store_true')
    parser.add_argument('--render-only',action='store_true')
    parser.add_argument('--steps',type=int,default=3000)
    parser.add_argument('--workers',type=int,default=1)
    parser.add_argument('--refine-from')
    parser.add_argument('--extend-from')
    parser.add_argument('--waveform-weight',type=float,default=0.0)
    parser.add_argument('--learning-rate',type=float)
    parser.add_argument('--hidden',type=int,choices=[32,64])
    parser.add_argument('--device',choices=['auto','cpu','cuda'],default='auto')
    parser.add_argument('--batch-size',type=int,choices=[8,16,32,64],default=8)
    parser.add_argument('--random-record-context',action='store_true')
    args=parser.parse_args()
    if args.hidden and not args.refine_from:
        parser.error('--hidden requires --refine-from')
    if args.verify:
        verify()
    elif args.verify_source:
        verify_sources(Path(args.job).resolve(),args.source_kind)
    else:
        job=Path(args.job).resolve()
        if args.finish:
            finish(job)
        else:
            if args.refine_from:
                prepare_refinement(job,Path(args.refine_from).resolve(),args.hidden)
            elif args.extend_from:
                prepare_extended(job,Path(args.extend_from).resolve(),args.workers)
            elif args.render_existing:
                render_existing(job,args.workers)
            elif not args.train_existing:
                prepare(job,args.workers)
            if not args.render_only:
                train(job,args.steps,job/'initial.pt' if args.refine_from or args.extend_from else None,args.learning_rate,args.device,args.batch_size,args.random_record_context,args.waveform_weight)
