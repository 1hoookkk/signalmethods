import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time
import numpy as np

from worker import DATA,FS


def compare(model,folder,seconds):
    folder.mkdir(parents=True,exist_ok=True)
    document=json.loads(model.read_text())
    snapshot=folder/'model_snapshot.json'
    snapshot.write_text(json.dumps(document))
    t=np.arange(round(seconds*FS))/FS
    amplitude=np.geomspace(.0001,1,len(t))
    source=(amplitude*(.7*np.sin(2*np.pi*55*t)+.2*np.sin(2*np.pi*997*t)+.1*np.random.default_rng(953).uniform(-1,1,len(t)))).astype(np.float32)
    source=np.concatenate((source,np.zeros(2*FS,np.float32)))
    path=folder/'input.f32'
    source.astype('<f4').tofile(path)
    outputs={}
    rows=[]
    for name,filename in [('stl','runner.exe'),('xsimd_avx2','runner_simd.exe')]:
        runner=DATA/'bin'/filename
        target=folder/f'{name}_output.f32'
        times=[]
        for trial in range(3):
            start=time.perf_counter()
            result=subprocess.run([str(runner), 'model',str(path),str(target),str(snapshot)],capture_output=True,text=True,check=True)
            times.append(time.perf_counter()-start)
            (folder/f'{name}_{trial}.log').write_text(result.stdout+result.stderr)
        y=np.fromfile(target,dtype='<f4')
        if y.shape!=source.shape or not np.isfinite(y).all():
            raise ValueError('Backend returned incomplete or nonfinite output')
        outputs[name]=y
        rows.append({'backend':name,'runner_sha256':hashlib.sha256(runner.read_bytes()).hexdigest(),'elapsed_seconds':times,'median_seconds':float(np.median(times)),'audio_seconds':len(source)/FS,'realtime_factor':float(np.median(times)/(len(source)/FS))})
    difference=outputs['xsimd_avx2'].astype(np.float64)-outputs['stl']
    rmse=float(np.sqrt(np.mean(difference**2)))
    peak=float(np.max(abs(difference)))
    report={'passed':rmse<1e-4 and peak<1e-3,'rmse':rmse,'peak_absolute_difference':peak,'thresholds':{'rmse':1e-4,'peak_absolute_difference':1e-3},'model_snapshot_sha256':hashlib.sha256(snapshot.read_bytes()).hexdigest(),'hidden_units':document['layers'][0]['shape'][-1],'rows':rows,'speedup_stl_over_simd':rows[0]['median_seconds']/rows[1]['median_seconds'],'scope':'Native backend agreement on a separate amplitude sweep and silence; timings include process startup and are affected by other machine activity. Not a DAW CPU or latency certification.'}
    (folder/'backend_comparison.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))
    if not report['passed']:
        raise RuntimeError('Backend parity failed')


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('model')
    parser.add_argument('folder')
    parser.add_argument('--seconds',type=float,default=20)
    args=parser.parse_args()
    if args.seconds<=0:
        parser.error('--seconds must be positive')
    compare(Path(args.model).resolve(),Path(args.folder).resolve(),args.seconds)
