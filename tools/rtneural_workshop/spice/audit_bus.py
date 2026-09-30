import argparse
import json
from pathlib import Path
import time
import numpy as np
import soundfile as sf
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from train_bus import STUDY, FS, native, metrics, esr, sha
from bus_output import harmonics_at


def audit(job):
    model=job/'model_rtneural.json'
    folder=job/'audit'
    folder.mkdir(exist_ok=True)
    checks=json.loads((STUDY/'checks-loaded/checks.json').read_text())
    source=np.loadtxt(STUDY/'checks-loaded/full_profile_source.txt')[:,1]
    x=source[FS//2:-FS//20].astype(np.float32)/4
    reference,rate=sf.read(str(STUDY/'checks-loaded/full_profile_volts.wav'))
    if rate!=FS or len(reference)!=len(x):
        raise ValueError('Profile sample count mismatch')
    predicted=native(folder,x,'model',model,'profile')*16
    rows=[]
    offset=0
    for reference_row in checks['rows']:
        hz=reference_row['hz']
        duration=.5 if hz<10000 else .1
        a=round((offset+duration*.5)*FS)
        b=round((offset+duration)*FS)
        fundamental,harmonics=harmonics_at(predicted[a:b],hz)
        rows.append({'hz':hz,'input_peak_volts':reference_row['input_peak_volts'],'reference_gain_db':reference_row['gain_db'],'model_gain_db':float(20*np.log10(max(fundamental/reference_row['input_peak_volts'],1e-12))),'reference_harmonics_db':reference_row['harmonics_db'],'model_harmonics_db':float(20*np.log10(max(np.linalg.norm(harmonics)/max(fundamental,1e-12),1e-12))),'esr':esr(predicted[a:b],reference[a:b])})
        offset+=duration+.1
    start=time.perf_counter()
    silence=native(folder,np.zeros(20*FS,np.float32),'model',model,'long_silence')
    elapsed=time.perf_counter()-start
    training_input,input_rate=sf.read(str(job/'train_input.wav'),dtype='float32')
    if input_rate!=FS:
        raise ValueError('Training-input sample rate mismatch')
    continuous=native(folder,np.concatenate((training_input,np.zeros(20*FS,np.float32))),'model',model,'continuous_drive_recovery')
    target,_=sf.read(str(job/'reference.wav'))
    actual,_=sf.read(str(job/'model.wav'))
    sections=[]
    for name,a,b in [('filtered_bass',.25,3),('percussion',3.25,6),('bass',6.25,8),('recovery',8,9)]:
        a,b=round(a*FS),round(b*FS)
        sections.append({'name':name,'esr':esr(actual[a:b],target[a:b]),'reference':metrics(target[a:b]),'model':metrics(actual[a:b])})
    training=json.loads((job/'training_report.json').read_text())
    result={'model_sha256':sha(model),'clipping_profile':rows,'heldout_sections':sections,'long_zero_input':{'seconds':20,'all':metrics(silence),'final_second':metrics(silence[-FS:]),'finite':bool(np.isfinite(silence).all())},'runtime':{'wall_seconds_including_process_startup':elapsed,'audio_seconds':20,'realtime_factor':elapsed/20,'qualification':'Standalone mono render only; not plugin host CPU or latency certification'},'validation':{'starting_checkpoint_objective':training['history'][0]['validation_objective'],'best_objective':training['validation_objective'],'best_step':min(training['history'],key=lambda r:r['validation_objective'])['step']},'export_parity_rmse':training['export_parity_rmse'],'selection':'Checkpoint selected only on validation; these diagnostics are not optimization targets','reference':'Estimated-semiconductor schematic simulation; hardware accuracy not established'}
    result['continuous_drive_recovery']={'driven_seconds':len(training_input)/FS,'silence_seconds':20,'state_resets':0,'finite':bool(np.isfinite(continuous).all()),'all':metrics(continuous),'final_second':metrics(continuous[-FS:]),'scope':'Continuous model stability only; no continuous SPICE reference is available for this signal'}
    result['heldout_section_warmup']='First 0.25 seconds excluded after each three-second capture reset; recovery retains its preceding bass context.'
    (folder/'audit.json').write_text(json.dumps(result,indent=2))
    fig,axes=plt.subplots(2,2,figsize=(11,7),constrained_layout=True)
    rows1k=[r for r in rows if r['hz']==1000]
    for key,label in [('reference_gain_db','SPICE'),('model_gain_db','RTNeural')]:
        axes[0,0].semilogx([r['input_peak_volts'] for r in rows1k],[r[key] for r in rows1k],'o-',label=label)
    axes[0,0].set(title='1 kHz clipping profile',xlabel='Differential line input peak (V)',ylabel='Fundamental gain (dB)')
    axes[0,0].legend()
    a,b=round(2.65*FS),round(2.655*FS)
    axes[0,1].plot(np.arange(b-a)/FS*1000,reference[a:b],label='SPICE')
    axes[0,1].plot(np.arange(b-a)/FS*1000,predicted[a:b],label='RTNeural',alpha=.8)
    axes[0,1].set(title='0.2 V peak line input, 1 kHz',xlabel='Time (ms)',ylabel='Balanced output (V)')
    axes[0,1].legend()
    a,b=round(.4*FS),round(.5*FS)
    axes[1,0].plot(np.arange(b-a)/FS*1000,target[a:b],label='SPICE')
    axes[1,0].plot(np.arange(b-a)/FS*1000,actual[a:b],label='RTNeural',alpha=.8)
    axes[1,0].set(title='Held-out filtered bass, OUTPUT -12 dB',xlabel='Time (ms)',ylabel='Digital amplitude')
    axes[1,0].legend()
    for data,label in [(target,'SPICE'),(actual,'RTNeural')]:
        tail=data[8*FS:9*FS].reshape(100,480)
        axes[1,1].plot((np.arange(100)+.5)/100,20*np.log10(np.maximum(np.sqrt(np.mean(tail**2,axis=1)),1e-9)),label=label)
    axes[1,1].set(title='Recovery after held-out bass stops',xlabel='Time since input stopped (s)',ylabel='10 ms RMS (dBFS)')
    axes[1,1].legend()
    for ax in axes.flat:
        ax.grid(alpha=.2)
    fig.suptitle('8Bus selected main-output route: simulation and trained model\nEstimated semiconductor behavior; no hardware-match claim',fontsize=13)
    fig.savefig(folder/'comparison.png',dpi=150)
    plt.close(fig)
    print(json.dumps(result,indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('job')
    audit(Path(parser.parse_args().job).resolve())
