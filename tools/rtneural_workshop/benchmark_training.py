import argparse
import json
from pathlib import Path
import time
import numpy as np
import soundfile as sf
import torch

from losses import AudioObjective
from model import Net


def benchmark(source,folder,device,batches):
    torch.set_num_threads(4)
    if device=='cuda' and not torch.cuda.is_available():
        raise RuntimeError('CUDA is unavailable')
    torch.backends.cuda.matmul.allow_tf32=False
    torch.backends.cudnn.allow_tf32=False
    folder.mkdir(parents=True,exist_ok=True)
    config=json.loads((source/'config.json').read_text())
    weights=torch.load(source/'initial.pt',weights_only=True,map_location='cpu')
    a,rate=sf.read(str(source/'train_input.wav'),dtype='float32')
    b,target_rate=sf.read(str(source/'train_target.wav'),dtype='float32')
    if rate!=48000 or target_rate!=rate:
        raise ValueError('Use the verified 48 kHz training pair')
    x,y=torch.from_numpy(a).to(device),torch.from_numpy(b).to(device)
    rows=[]
    def synchronize():
        if device=='cuda':
            torch.cuda.synchronize()
    for batch in batches:
        if device=='cuda':
            torch.cuda.empty_cache()
            torch.cuda.reset_peak_memory_stats()
        net=Net(config['hidden'],input_scale=config['input_scale']).to(device)
        net.load_state_dict(weights)
        objective=AudioObjective().to(device)
        optimizer=torch.optim.Adam(net.parameters(),lr=.0002)
        indices=np.arange(batch)%(len(x)//config['record_samples'])*config['record_samples']
        prefix=torch.stack([x[i:i+12000] for i in indices]).unsqueeze(-1)
        synchronize()
        start=time.perf_counter()
        with torch.no_grad():
            _,state=net(torch.zeros(batch,12000,1,device=device))
            _,state=net(prefix,state)
        synchronize()
        context_seconds=time.perf_counter()-start
        indices+=12000
        elapsed=[]
        for step in range(9):
            synchronize()
            start=time.perf_counter()
            inputs=torch.stack([x[i:i+4096] for i in indices]).unsqueeze(-1)
            targets=torch.stack([y[i:i+4096] for i in indices])
            pred,state=net(inputs,state)
            state=tuple(v.detach() for v in state)
            loss,_=objective(pred.squeeze(-1),targets)
            if not torch.isfinite(loss):
                raise ValueError('Nonfinite benchmark loss')
            optimizer.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(net.parameters(),1)
            optimizer.step()
            synchronize()
            if step:
                elapsed.append(time.perf_counter()-start)
            indices+=4096
        seconds=float(np.mean(elapsed))
        row={'batch_size':batch,'context_seconds':context_seconds,'seconds_per_step':seconds,'samples_per_second':batch*4096/seconds,'peak_allocated_mib':torch.cuda.max_memory_allocated()/2**20 if device=='cuda' else None}
        rows.append(row)
        print(json.dumps(row),flush=True)
        del net,objective,optimizer,state,prefix,inputs,targets,pred,loss
    report={'device':device,'device_name':torch.cuda.get_device_name() if device=='cuda' else 'CPU','torch':torch.__version__,'hidden_units':config['hidden'],'precision':'float32; TF32 disabled','steps_measured':8,'rows':rows,'scope':'Actual LSTM, MR spectral + LF ESR and Adam training steps; excludes periodic validation and native export. Other machine activity affects timings.'}
    (folder/f'{device}_training_benchmark.json').write_text(json.dumps(report,indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('source')
    parser.add_argument('folder')
    parser.add_argument('--device',choices=['cpu','cuda'],required=True)
    parser.add_argument('--batches',type=int,nargs='+',default=[8,16,32])
    args=parser.parse_args()
    if any(not 1<=batch<=64 for batch in args.batches):
        parser.error('Batch sizes must be 1 to 64')
    benchmark(Path(args.source).resolve(),Path(args.folder).resolve(),args.device,args.batches)
